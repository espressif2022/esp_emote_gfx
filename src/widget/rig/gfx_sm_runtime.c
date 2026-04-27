/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layer 3 — UNIFIED RUNTIME
 *
 * One gfx_mesh_img per segment; dispatch on segment kind:
 *
 *   GFX_SM_SEG_CAPSULE      → s_apply_capsule()
 *   GFX_SM_SEG_RING         → s_apply_ring()
 *   GFX_SM_SEG_BEZIER_STRIP → s_apply_bezier(loop=false)
 *   GFX_SM_SEG_BEZIER_LOOP  → s_apply_bezier(loop=true)
 *
 * All four paths share the same coord-transform, solid-pixel source, and
 * mesh_img update pattern.  No "stickman backend" vs "face backend" switch.
 */

#include <math.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/gfx_disp.h"
#include "core/display/gfx_disp_priv.h"
#include "widget/gfx_mesh_img.h"
#include "widget/gfx_sm_scene.h"

static const char *TAG = "gfx_sm_runtime";

/* Forward declaration — NVG helpers below use this type. */
typedef struct { int32_t x; int32_t y; } s_spt_t;

/* ------------------------------------------------------------------ */
/*  Optional: NanoVG adaptive path flattener                          */
/*                                                                     */
/*  Set GFX_SM_HAVE_NANOVG=1 (compiler flag or Kconfig) to enable.   */
/*  When disabled every Bézier path falls back to the fixed-step      */
/*  s_tess_cubic_bezier / s_cubic_bezier samplers.                    */
/* ------------------------------------------------------------------ */
#ifndef GFX_SM_HAVE_NANOVG
#define GFX_SM_HAVE_NANOVG 0
#endif

#if GFX_SM_HAVE_NANOVG
#include "nanovg.h"

/* Maximum polygon points from one NVG flatten call.
 * Adaptive subdivision at typical emote scales (~50-200px) gives
 * ≤ ~60 pts per cubic segment; 256 covers any reasonable path. */
#define SM_NVG_MAX_PTS 256U

typedef struct {
    int32_t count;
    float   x[SM_NVG_MAX_PTS];
    float   y[SM_NVG_MAX_PTS];
} s_nvg_cap_t;

/* One shared context + capture buffer (rendering is single-threaded). */
static s_nvg_cap_t  s_nvg_cap;
static NVGcontext  *s_nvg_ctx    = NULL;
static uint8_t      s_nvg_refcnt = 0;

/* ── Minimal NVG capture-backend callbacks ── */
static int  s_nvgcb_create(void *u) { (void)u; return 1; }
static int  s_nvgcb_create_tex(void *u, int t, int w, int h, int f, const unsigned char *d)
            { (void)u;(void)t;(void)w;(void)h;(void)f;(void)d; return 1; }
static int  s_nvgcb_del_tex(void *u, int i)  { (void)u;(void)i; return 1; }
static int  s_nvgcb_upd_tex(void *u, int i, int x, int y, int w, int h, const unsigned char *d)
            { (void)u;(void)i;(void)x;(void)y;(void)w;(void)h;(void)d; return 1; }
static int  s_nvgcb_tex_size(void *u, int i, int *w, int *h)
            { (void)u;(void)i; if (w) *w=1; if (h) *h=1; return 1; }
static void s_nvgcb_viewport(void *u, float w, float h, float r) { (void)u;(void)w;(void)h;(void)r; }
static void s_nvgcb_cancel(void *u)  { (void)u; }
static void s_nvgcb_flush(void *u)   { (void)u; }
static void s_nvgcb_stroke(void *u, NVGpaint *p, NVGcompositeOperationState c,
                             NVGscissor *s, float f, float sw, const NVGpath *ps, int n)
            { (void)u;(void)p;(void)c;(void)s;(void)f;(void)sw;(void)ps;(void)n; }
static void s_nvgcb_triangles(void *u, NVGpaint *p, NVGcompositeOperationState c,
                                NVGscissor *s, const NVGvertex *v, int n, float f)
            { (void)u;(void)p;(void)c;(void)s;(void)v;(void)n;(void)f; }
static void s_nvgcb_delete(void *u)  { (void)u; }

/* Capture: with edgeAntiAlias=0, paths[i].fill[] IS the boundary polygon. */
static void s_nvgcb_fill(void *uptr, NVGpaint *paint, NVGcompositeOperationState cop,
                          NVGscissor *scissor, float fringe, const float *bounds,
                          const NVGpath *paths, int npaths)
{
    s_nvg_cap_t *cap = (s_nvg_cap_t *)uptr;
    (void)paint; (void)cop; (void)scissor; (void)fringe; (void)bounds;
    cap->count = 0;
    for (int i = 0; i < npaths; i++) {
        int32_t cn = paths[i].nfill;
        if (cn <= 0 || paths[i].fill == NULL) { continue; }
        if (cn > (int32_t)SM_NVG_MAX_PTS) { cn = (int32_t)SM_NVG_MAX_PTS; }
        for (int32_t j = 0; j < cn; j++) {
            cap->x[j] = paths[i].fill[j].x;
            cap->y[j] = paths[i].fill[j].y;
        }
        cap->count = cn;
        break;
    }
}

static NVGcontext *s_nvg_create_ctx(s_nvg_cap_t *cap)
{
    NVGparams p;
    memset(&p, 0, sizeof(p));
    p.userPtr             = cap;
    p.edgeAntiAlias       = 0;   /* no fringe — fill[] = plain boundary pts */
    p.renderCreate        = s_nvgcb_create;
    p.renderCreateTexture = s_nvgcb_create_tex;
    p.renderDeleteTexture = s_nvgcb_del_tex;
    p.renderUpdateTexture = s_nvgcb_upd_tex;
    p.renderGetTextureSize = s_nvgcb_tex_size;
    p.renderViewport      = s_nvgcb_viewport;
    p.renderCancel        = s_nvgcb_cancel;
    p.renderFlush         = s_nvgcb_flush;
    p.renderFill          = s_nvgcb_fill;
    p.renderStroke        = s_nvgcb_stroke;
    p.renderTriangles     = s_nvgcb_triangles;
    p.renderDelete        = s_nvgcb_delete;
    return nvgCreateInternal(&p);
}

/* Flatten a CLOSED n=3k+1 Bézier chain into s_nvg_cap.  Returns point count. */
static int32_t s_nvg_flatten_closed(const s_spt_t *ctrl, uint8_t n)
{
    s_nvg_cap.count = 0;
    if (s_nvg_ctx == NULL || n < 4U) { return 0; }
    nvgBeginFrame(s_nvg_ctx, 1.0f, 1.0f, 1.0f);
    nvgShapeAntiAlias(s_nvg_ctx, 0);
    nvgBeginPath(s_nvg_ctx);
    nvgMoveTo(s_nvg_ctx, (float)ctrl[0].x, (float)ctrl[0].y);
    for (uint8_t i = 1U; i + 2U < n; i += 3U) {
        nvgBezierTo(s_nvg_ctx,
                    (float)ctrl[i].x,       (float)ctrl[i].y,
                    (float)ctrl[i + 1U].x,  (float)ctrl[i + 1U].y,
                    (float)ctrl[i + 2U].x,  (float)ctrl[i + 2U].y);
    }
    nvgClosePath(s_nvg_ctx);
    nvgFill(s_nvg_ctx);
    nvgEndFrame(s_nvg_ctx);
    return s_nvg_cap.count;
}

/* Flatten a SINGLE cubic Bézier segment (open) into s_nvg_cap.  Returns point count.
 * The captured polygon includes the curve portion + a closing straight-line back to p0,
 * which is acceptable for arc-length resampling of short eye-half arcs. */
static int32_t s_nvg_flatten_single(const s_spt_t *p0, const s_spt_t *p1,
                                     const s_spt_t *p2, const s_spt_t *p3)
{
    s_nvg_cap.count = 0;
    if (s_nvg_ctx == NULL) { return 0; }
    nvgBeginFrame(s_nvg_ctx, 1.0f, 1.0f, 1.0f);
    nvgShapeAntiAlias(s_nvg_ctx, 0);
    nvgBeginPath(s_nvg_ctx);
    nvgMoveTo(s_nvg_ctx, (float)p0->x, (float)p0->y);
    nvgBezierTo(s_nvg_ctx, (float)p1->x, (float)p1->y,
                            (float)p2->x, (float)p2->y,
                            (float)p3->x, (float)p3->y);
    nvgFill(s_nvg_ctx);
    nvgEndFrame(s_nvg_ctx);
    return s_nvg_cap.count;
}

/* Arc-length resample s_nvg_cap.x/y[0..src_n-1] → dst[0..dst_n-1]. */
static bool s_nvg_resample(int32_t src_n, uint16_t dst_n, s_spt_t *dst)
{
    static float s_arclen[SM_NVG_MAX_PTS];   /* static: avoids 1 KB stack alloc */
    if (src_n < 2 || dst_n < 2) { return false; }

    s_arclen[0] = 0.0f;
    for (int32_t i = 1; i < src_n; i++) {
        float dx = s_nvg_cap.x[i] - s_nvg_cap.x[i - 1];
        float dy = s_nvg_cap.y[i] - s_nvg_cap.y[i - 1];
        s_arclen[i] = s_arclen[i - 1] + sqrtf(dx * dx + dy * dy);
    }
    float total = s_arclen[src_n - 1];
    if (total < 0.001f) { return false; }

    int32_t si = 0;
    for (uint16_t j = 0; j < dst_n; j++) {
        float target = (float)j / (float)(dst_n - 1U) * total;
        while (si < src_n - 2 && s_arclen[si + 1] < target) { si++; }
        float seg = s_arclen[si + 1] - s_arclen[si];
        float t   = (seg > 0.001f) ? (target - s_arclen[si]) / seg : 0.0f;
        dst[j].x = (int32_t)lroundf(s_nvg_cap.x[si] + t * (s_nvg_cap.x[si + 1] - s_nvg_cap.x[si]));
        dst[j].y = (int32_t)lroundf(s_nvg_cap.y[si] + t * (s_nvg_cap.y[si + 1] - s_nvg_cap.y[si]));
    }
    return true;
}

#endif /* GFX_SM_HAVE_NANOVG */

#define SM_RING_SEGS_MIN  16U
#define SM_RING_SEGS_MAX  48U
/*
 * Max control points in a SINGLE segment (not total joints across all segments).
 * Cubic-bezier closed loop with N anchors has 3N+1 control points; N=20 → 61.
 * Decoupled from GFX_SM_SCENE_MAX_JOINTS to keep per-call stack buffers small.
 */
#define SM_BEZIER_MAX_PTS       64U
/*
 * Control points use the cubic-Bezier polygon format: n = 3k+1
 * (k segments, adjacent segments share one endpoint).
 * Only pts[0], pts[3], pts[6], … lie on the curve; interior pts are handles.
 */
/* Samples per cubic Bezier segment for stroke (BEZIER_LOOP / BEZIER_STRIP). */
#define SM_BEZIER_SEGS_PER_SEG  12U
/* Samples per half-eye for BEZIER_FILL (matches gfx_face_emote default eye_segs=24). */
#define SM_BEZIER_FILL_SEGS     24U
/* Max tessellated points: worst-case (n-1)/3 segments × segs_per + 1. */
#define SM_BEZIER_MAX_TESS      ((((SM_BEZIER_MAX_PTS - 1U) / 3U) * SM_BEZIER_SEGS_PER_SEG) + 1U)

/* ------------------------------------------------------------------ */
/*  Coordinate transform — design space → screen pixels               */
/* ------------------------------------------------------------------ */

static void s_to_screen(const gfx_sm_asset_t *asset,
                        const gfx_sm_pt_t    *dp,
                        gfx_coord_t ox, gfx_coord_t oy,
                        uint16_t ow, uint16_t oh,
                        s_spt_t *out)
{
    const gfx_sm_meta_t *m = asset->meta;
    float scale = fminf((float)ow / (float)m->viewbox_w,
                        (float)oh / (float)m->viewbox_h);
    float rw    = (float)m->viewbox_w * scale;
    float rh    = (float)m->viewbox_h * scale;
    float offx  = (float)ox + floorf(((float)ow - rw) * 0.5f) - (float)m->viewbox_x * scale;
    float offy  = (float)oy + floorf(((float)oh - rh) * 0.5f) - (float)m->viewbox_y * scale;

    out->x = (int32_t)lroundf(offx + (float)dp->x * scale);
    out->y = (int32_t)lroundf(offy + (float)dp->y * scale);
}

static int32_t s_scalar_px(const gfx_sm_asset_t *asset,
                            uint16_t ow, uint16_t oh, float v)
{
    const gfx_sm_meta_t *m = asset->meta;
    float scale = fminf((float)ow / (float)m->viewbox_w,
                        (float)oh / (float)m->viewbox_h);
    int32_t px  = (int32_t)lroundf(v * scale);
    return (px < 1) ? 1 : px;
}

static uint8_t s_ring_segs(float radius)
{
    int32_t s = (int32_t)lroundf(radius * 2.0f);
    if (s < (int32_t)SM_RING_SEGS_MIN) { s = (int32_t)SM_RING_SEGS_MIN; }
    if (s > (int32_t)SM_RING_SEGS_MAX) { s = (int32_t)SM_RING_SEGS_MAX; }
    return (uint8_t)s;
}

/* ------------------------------------------------------------------ */
/*  CAPSULE primitive                                                  */
/* ------------------------------------------------------------------ */

static esp_err_t s_apply_capsule(gfx_obj_t *obj,
                                  const s_spt_t *a, const s_spt_t *b,
                                  int32_t thick)
{
    gfx_mesh_img_point_q8_t pts[4];
    float ax = (float)a->x, ay = (float)a->y;
    float bx = (float)b->x, by = (float)b->y;
    float dx = bx - ax, dy = by - ay;
    float len  = sqrtf(dx * dx + dy * dy);
    float half = (float)thick * 0.5f;
    int32_t px[4], py[4];
    int32_t min_x, max_x, min_y, max_y;

    if (len <= 0.001f) {
        px[0] = (int32_t)(ax - half); py[0] = (int32_t)(ay - half);
        px[1] = (int32_t)(ax + half); py[1] = (int32_t)(ay - half);
        px[2] = (int32_t)(ax - half); py[2] = (int32_t)(ay + half);
        px[3] = (int32_t)(ax + half); py[3] = (int32_t)(ay + half);
    } else {
        float tx = dx / len * half, ty = dy / len * half;
        float nx = -dy / len * half, ny = dx / len * half;
        px[0] = (int32_t)lroundf(ax - tx + nx); py[0] = (int32_t)lroundf(ay - ty + ny);
        px[1] = (int32_t)lroundf(bx + tx + nx); py[1] = (int32_t)lroundf(by + ty + ny);
        px[2] = (int32_t)lroundf(ax - tx - nx); py[2] = (int32_t)lroundf(ay - ty - ny);
        px[3] = (int32_t)lroundf(bx + tx - nx); py[3] = (int32_t)lroundf(by + ty - ny);
    }

    min_x = max_x = px[0]; min_y = max_y = py[0];
    for (int i = 1; i < 4; i++) {
        if (px[i] < min_x) { min_x = px[i]; } else if (px[i] > max_x) { max_x = px[i]; }
        if (py[i] < min_y) { min_y = py[i]; } else if (py[i] > max_y) { max_y = py[i]; }
    }
    if (max_x <= min_x) max_x = min_x + 1;
    if (max_y <= min_y) max_y = min_y + 1;

    for (int i = 0; i < 4; i++) {
        pts[i].x_q8 = (px[i] - min_x) << 8;
        pts[i].y_q8 = (py[i] - min_y) << 8;
    }
    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "capsule align");
    return gfx_mesh_img_set_points_q8(obj, pts, 4U);
}

/* ------------------------------------------------------------------ */
/*  RING primitive                                                     */
/* ------------------------------------------------------------------ */

static esp_err_t s_apply_ring(gfx_obj_t *obj,
                               const s_spt_t *c,
                               int32_t radius, int32_t thick, uint8_t segs)
{
    gfx_mesh_img_point_q8_t pts[(SM_RING_SEGS_MAX + 1U) * 2U];
    float outer_r = (float)radius + (float)thick * 0.5f;
    float inner_r = (float)radius - (float)thick * 0.5f;
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;

    if (inner_r < 1.0f) inner_r = 1.0f;

    for (uint8_t i = 0; i <= segs; i++) {
        float ang = (float)i * 2.0f * (float)M_PI / (float)segs;
        int32_t ox = (int32_t)lroundf((float)c->x + cosf(ang) * outer_r);
        int32_t oy = (int32_t)lroundf((float)c->y + sinf(ang) * outer_r);
        int32_t ix = (int32_t)lroundf((float)c->x + cosf(ang) * inner_r);
        int32_t iy = (int32_t)lroundf((float)c->y + sinf(ang) * inner_r);

        if (ox < min_x) { min_x = ox; } if (ix < min_x) { min_x = ix; }
        if (ox > max_x) { max_x = ox; } if (ix > max_x) { max_x = ix; }
        if (oy < min_y) { min_y = oy; } if (iy < min_y) { min_y = iy; }
        if (oy > max_y) { max_y = oy; } if (iy > max_y) { max_y = iy; }

        pts[i].x_q8               = ox << 8;
        pts[i].y_q8               = oy << 8;
        pts[(size_t)segs + 1 + i].x_q8 = ix << 8;
        pts[(size_t)segs + 1 + i].y_q8 = iy << 8;
    }
    if (max_x <= min_x) max_x = min_x + 1;
    if (max_y <= min_y) max_y = min_y + 1;

    for (size_t i = 0; i < ((size_t)segs + 1U) * 2U; i++) {
        pts[i].x_q8 -= (min_x << 8);
        pts[i].y_q8 -= (min_y << 8);
    }
    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "ring align");
    return gfx_mesh_img_set_points_q8(obj, pts, ((size_t)segs + 1U) * 2U);
}

/* ------------------------------------------------------------------ */
/*  Cubic Bezier tessellation                                         */
/*                                                                     */
/*  Control points use the standard cubic polygon format: n = 3k+1.  */
/*  pts[0], pts[3], pts[6], … are ON the curve; interior pts are      */
/*  tangent handles. Adjacent segments share one endpoint.            */
/*                                                                     */
/*  s_tess_cubic_bezier — for BEZIER_LOOP / BEZIER_STRIP:            */
/*    closed (loop): k*segs_per points, no endpoint repeat            */
/*    open  (strip): k*segs_per + 1 points                            */
/* ------------------------------------------------------------------ */

static void s_cubic_bezier(const s_spt_t *p0, const s_spt_t *p1,
                            const s_spt_t *p2, const s_spt_t *p3,
                            float t, s_spt_t *out)
{
    float u  = 1.0f - t;
    float u2 = u * u, u3 = u2 * u;
    float t2 = t * t, t3 = t2 * t;
    out->x = (int32_t)lroundf(u3*(float)p0->x + 3.0f*u2*t*(float)p1->x
                               + 3.0f*u*t2*(float)p2->x + t3*(float)p3->x);
    out->y = (int32_t)lroundf(u3*(float)p0->y + 3.0f*u2*t*(float)p1->y
                               + 3.0f*u*t2*(float)p2->y + t3*(float)p3->y);
}

/* s_tess_cubic_bezier was removed — tessellation now lives inline in
 * s_apply_bezier(), where positions are kept as floats and tangents are
 * computed from the analytic cubic derivative. That eliminates the twist
 * bowties that produced "dashed" strokes on the device. */

/* ------------------------------------------------------------------ */
/*  BEZIER_STRIP / BEZIER_LOOP primitives                             */
/*                                                                     */
/*  Control points are first tessellated via Catmull-Rom (SUBDIV      */
/*  samples per interval) for smooth curves; then the tessellated     */
/*  path is extruded ±half_thick along per-vertex normals.            */
/*                                                                     */
/*  Mesh topology (same as RING):                                      */
/*   loop=true  : grid(M, 1) + wrap_cols → (M+1)*2 pts               */
/*   loop=false : grid(M-1, 1), no wrap  →   M  *2 pts               */
/*  where M = n*SUBDIV (loop) or (n-1)*SUBDIV+1 (strip).             */
/*                                                                     */
/*  Row 0 = "outer" ring (tess + normal * half_thick)                 */
/*  Row 1 = "inner" ring (tess - normal * half_thick)                 */
/* ------------------------------------------------------------------ */

/* Analytic cubic position B(t) and tangent B'(t) in floats.
 * Using the analytic derivative (instead of central difference on rounded
 * tessellated samples) is essential for stable normal directions — otherwise
 * sharp-cornered Lottie paths (where i/o tangents are zero, producing
 * duplicate ctrl pts) and tight bends where adjacent samples round to the
 * same pixel produce twist bowties that render as dashed/broken strokes. */
static inline void s_cubic_pos_tan(const s_spt_t *p0, const s_spt_t *p1,
                                    const s_spt_t *p2, const s_spt_t *p3,
                                    float t,
                                    float *px, float *py,
                                    float *tx, float *ty)
{
    float u  = 1.0f - t;
    float u2 = u * u, t2 = t * t;
    *px = u2*u*(float)p0->x + 3.0f*u2*t*(float)p1->x
        + 3.0f*u*t2*(float)p2->x + t2*t*(float)p3->x;
    *py = u2*u*(float)p0->y + 3.0f*u2*t*(float)p1->y
        + 3.0f*u*t2*(float)p2->y + t2*t*(float)p3->y;
    *tx = 3.0f*u2*(float)(p1->x - p0->x) + 6.0f*u*t*(float)(p2->x - p1->x)
        + 3.0f*t2*(float)(p3->x - p2->x);
    *ty = 3.0f*u2*(float)(p1->y - p0->y) + 6.0f*u*t*(float)(p2->y - p1->y)
        + 3.0f*t2*(float)(p3->y - p2->y);
    /* If derivative is zero (p0==p1 at t=0 or p2==p3 at t=1 — sharp corner),
     * fall back to a sibling chord that still lies tangent to the curve. */
    if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
        *tx = (float)(p2->x - p0->x); *ty = (float)(p2->y - p0->y);
        if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
            *tx = (float)(p3->x - p0->x); *ty = (float)(p3->y - p0->y);
            if (fabsf(*tx) + fabsf(*ty) < 1e-6f) {
                *tx = 1.0f; *ty = 0.0f;
            }
        }
    }
}

static esp_err_t s_apply_bezier(gfx_obj_t *obj,
                                 const s_spt_t *ctrl, uint8_t n,
                                 int32_t thick, bool loop)
{
    gfx_mesh_img_point_q8_t pts[(SM_BEZIER_MAX_TESS + 1U) * 2U];
    int32_t ox[SM_BEZIER_MAX_TESS + 1U];
    int32_t oy[SM_BEZIER_MAX_TESS + 1U];
    int32_t ix_arr[SM_BEZIER_MAX_TESS + 1U];
    int32_t iy_arr[SM_BEZIER_MAX_TESS + 1U];
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;
    float   half  = (float)thick * 0.5f;
    uint16_t M    = 0;

    if (n < 4U) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t  k        = (uint8_t)((n - 1U) / 3U);

#if GFX_SM_HAVE_NANOVG
    /* Closed loops: NVG adaptive flattening, then resample to EXACTLY
     * k*SM_BEZIER_SEGS_PER_SEG points so the mesh grid (set at init)
     * always matches. */
    if (loop) {
        s_spt_t  tess[SM_BEZIER_MAX_TESS + 1U];
        const uint16_t target_m = (uint16_t)k * (uint16_t)SM_BEZIER_SEGS_PER_SEG;
        int32_t cnt = s_nvg_flatten_closed(ctrl, n);
        if (cnt >= 3 && target_m > 0U && s_nvg_resample(cnt, target_m, tess)) {
            /* ── Robust central-difference tangent on resampled points ──
             * NVG samples lie on the curve but carry no analytic tangent, so
             * compute tangent from tess[next] - tess[prev]; if they collide
             * (short arc / rounding), scan outward for the first non-identical
             * sample so the fallback (1,0) direction never fires. */
            for (uint16_t i = 0; i < target_m; i++) {
                uint16_t prev_i = (uint16_t)((i + target_m - 1U) % target_m);
                uint16_t next_i = (uint16_t)((i + 1U)            % target_m);
                uint16_t guard;
                guard = 0U;
                while ((tess[next_i].x == tess[i].x && tess[next_i].y == tess[i].y)
                        && guard < target_m) {
                    next_i = (uint16_t)((next_i + 1U) % target_m);
                    guard++;
                }
                guard = 0U;
                while ((tess[prev_i].x == tess[i].x && tess[prev_i].y == tess[i].y)
                        && guard < target_m) {
                    prev_i = (uint16_t)((prev_i + target_m - 1U) % target_m);
                    guard++;
                }
                float tx = (float)(tess[next_i].x - tess[prev_i].x);
                float ty = (float)(tess[next_i].y - tess[prev_i].y);
                float len = sqrtf(tx * tx + ty * ty);
                if (len < 1e-6f) { tx = 1.0f; ty = 0.0f; }
                else             { tx /= len; ty /= len; }
                float nx = -ty, ny = tx;
                ox[i]     = (int32_t)lroundf((float)tess[i].x + nx * half);
                oy[i]     = (int32_t)lroundf((float)tess[i].y + ny * half);
                ix_arr[i] = (int32_t)lroundf((float)tess[i].x - nx * half);
                iy_arr[i] = (int32_t)lroundf((float)tess[i].y - ny * half);
            }
            M = target_m;
        }
    }
#endif /* GFX_SM_HAVE_NANOVG */

    if (M == 0U) {
        /* ── Fixed-step fallback with analytic tangent (also used for STRIP).
         *
         * Keeps positions as floats until the final extrusion step so that
         * short/tight cubics do not collapse into each other after rounding.
         *
         * Sub-pixel sample COLLAPSE — anti-bowtie safeguard:
         * Bowties are caused by pixel *rounding* flipping the relative order
         * of outer/inner vertices; the root precondition is that two
         * consecutive float positions sit within one pixel of each other.
         * Snap such pairs onto the last *kept* sample (position + normal),
         * producing a zero-area degenerate quad that is invisible. The
         * threshold is a FIXED 1 px — scaling it with stroke_width would
         * wrongly fold whole shapes (e.g. a head/foot loop with many samples
         * per screen pixel of perimeter but a thick stroke) into a single
         * point.  Keep this purely pixel-level. */
        const float min_step2 = 1.0f;   /* (1 px)^2 */
        float       last_px = 0.0f, last_py = 0.0f;
        float       last_nx = 0.0f, last_ny = 0.0f;
        bool        have_last = false;

        for (uint8_t seg = 0U; seg < k; seg++) {
            const s_spt_t *p0 = &ctrl[seg * 3U];
            const s_spt_t *p1 = &ctrl[seg * 3U + 1U];
            const s_spt_t *p2 = &ctrl[seg * 3U + 2U];
            const s_spt_t *p3 = loop
                ? &ctrl[((seg + 1U) * 3U) % (n - 1U)]
                : &ctrl[(seg + 1U) * 3U];
            for (uint8_t sub = 0U; sub < SM_BEZIER_SEGS_PER_SEG; sub++) {
                float t = (float)sub / (float)SM_BEZIER_SEGS_PER_SEG;
                float px, py, tx, ty;
                s_cubic_pos_tan(p0, p1, p2, p3, t, &px, &py, &tx, &ty);
                float len = sqrtf(tx * tx + ty * ty);
                if (len < 1e-6f) { tx = 1.0f; ty = 0.0f; }
                else             { tx /= len; ty /= len; }
                float nx = -ty, ny = tx;   /* left-hand normal */
                if (have_last) {
                    float dx = px - last_px, dy = py - last_py;
                    if (dx * dx + dy * dy < min_step2) {
                        px = last_px; py = last_py;
                        nx = last_nx; ny = last_ny;
                    } else {
                        last_px = px; last_py = py;
                        last_nx = nx; last_ny = ny;
                    }
                } else {
                    last_px = px; last_py = py;
                    last_nx = nx; last_ny = ny;
                    have_last = true;
                }
                ox[M]     = (int32_t)lroundf(px + nx * half);
                oy[M]     = (int32_t)lroundf(py + ny * half);
                ix_arr[M] = (int32_t)lroundf(px - nx * half);
                iy_arr[M] = (int32_t)lroundf(py - ny * half);
                M++;
            }
        }
        if (!loop) {
            /* Strip: add the final endpoint with the last segment's t=1
             * tangent, so the tube cap aligns with the true curve direction. */
            const s_spt_t *p0 = &ctrl[(k - 1U) * 3U];
            const s_spt_t *p1 = &ctrl[(k - 1U) * 3U + 1U];
            const s_spt_t *p2 = &ctrl[(k - 1U) * 3U + 2U];
            const s_spt_t *p3 = &ctrl[k * 3U];
            float px, py, tx, ty;
            s_cubic_pos_tan(p0, p1, p2, p3, 1.0f, &px, &py, &tx, &ty);
            float len = sqrtf(tx * tx + ty * ty);
            if (len < 1e-6f) { tx = 1.0f; ty = 0.0f; }
            else             { tx /= len; ty /= len; }
            float nx = -ty, ny = tx;
            if (have_last) {
                float dx = px - last_px, dy = py - last_py;
                if (dx * dx + dy * dy < min_step2) {
                    px = last_px; py = last_py;
                    nx = last_nx; ny = last_ny;
                }
            }
            ox[M]     = (int32_t)lroundf(px + nx * half);
            oy[M]     = (int32_t)lroundf(py + ny * half);
            ix_arr[M] = (int32_t)lroundf(px - nx * half);
            iy_arr[M] = (int32_t)lroundf(py - ny * half);
            M++;
        }
    }

    uint16_t cols;
    if (loop) {
        ox[M]     = ox[0];      oy[M]     = oy[0];
        ix_arr[M] = ix_arr[0];  iy_arr[M] = iy_arr[0];
        cols = (uint16_t)(M + 1U);
    } else {
        cols = M;
    }

    /* ── Bounding box ── */
    for (uint16_t i = 0; i < cols; i++) {
        if (ox[i]     < min_x) { min_x = ox[i];     } if (ox[i]     > max_x) { max_x = ox[i];     }
        if (oy[i]     < min_y) { min_y = oy[i];     } if (oy[i]     > max_y) { max_y = oy[i];     }
        if (ix_arr[i] < min_x) { min_x = ix_arr[i]; } if (ix_arr[i] > max_x) { max_x = ix_arr[i]; }
        if (iy_arr[i] < min_y) { min_y = iy_arr[i]; } if (iy_arr[i] > max_y) { max_y = iy_arr[i]; }
    }
    if (max_x <= min_x) { max_x = min_x + 1; }
    if (max_y <= min_y) { max_y = min_y + 1; }

    for (uint16_t i = 0; i < cols; i++) {
        pts[i].x_q8        = (ox[i]     - min_x) << 8;
        pts[i].y_q8        = (oy[i]     - min_y) << 8;
        pts[cols + i].x_q8 = (ix_arr[i] - min_x) << 8;
        pts[cols + i].y_q8 = (iy_arr[i] - min_y) << 8;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "bezier align");
    return gfx_mesh_img_set_points_q8(obj, pts, (size_t)cols * 2U);
}

/* ------------------------------------------------------------------ */
/*  BEZIER_FILL primitive                                              */
/*                                                                     */
/*  Hub-and-spoke mesh topology (same grid size as BEZIER_LOOP):      */
/*   grid(n, 1) + wrap_cols → (n+1)*2 pts                            */
/*   Row 0 = n+1 copies of the centroid  ("hub")                      */
/*   Row 1 = outline control pts + first pt repeated ("rim")          */
/*                                                                     */
/*  Every quad covers one sector from center to edge — fills the      */
/*  entire closed shape with no gap at the middle.                     */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  BEZIER_FILL primitive — two-half cubic Bezier scan fill           */
/*                                                                     */
/*  Exactly mirrors gfx_face_emote_apply_bezier_fill():              */
/*  n=7 control points define two cubic Bezier halves:               */
/*    upper arc: ctrl[0..3]  →  SM_BEZIER_FILL_SEGS+1 pts left→right */
/*    lower arc: ctrl[3..6]  →  SM_BEZIER_FILL_SEGS+1 pts right→left */
/*               (reversed so both rows go left→right)                */
/*                                                                     */
/*  Mesh: grid(SM_BEZIER_FILL_SEGS, 1), no wrap.                      */
/*  Each column i connects the corresponding upper and lower pts,     */
/*  creating horizontal scan quads that fill the lens/eye shape.      */
/* ------------------------------------------------------------------ */

static esp_err_t s_apply_bezier_fill(gfx_obj_t *obj,
                                      const s_spt_t *ctrl, uint8_t n)
{
    const uint8_t  segs = SM_BEZIER_FILL_SEGS;
    const uint16_t cols = (uint16_t)segs + 1U;

    gfx_mesh_img_point_q8_t pts[(SM_BEZIER_FILL_SEGS + 1U) * 2U];
    s_spt_t  upper[SM_BEZIER_FILL_SEGS + 1U];
    s_spt_t  lower[SM_BEZIER_FILL_SEGS + 1U];
    int32_t  min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t  min_y = INT32_MAX, max_y = INT32_MIN;

    /* Supported n values:
     *   n=7  : two-half eye format (face emote): 1 cubic per half
     *          upper: ctrl[0..3]  lower: ctrl[6..3] reversed → left→right
     *   n=13 : 4-quadrant closed ellipse (rig): 2 cubics per half
     *          ctrl[0]=TOP, ctrl[3]=RIGHT, ctrl[6]=BOTTOM, ctrl[9]=LEFT, ctrl[12]=TOP
     *          upper (LEFT→TOP→RIGHT): Q3[9→12] + Q0[0→3]
     *          lower (LEFT→BOTTOM→RIGHT): Q2 rev[9→6] + Q1 rev[6→3]   */
    if (n != 7U && n != 13U) {
        return ESP_ERR_INVALID_ARG;
    }

    bool have_upper = false, have_lower = false;

#if GFX_SM_HAVE_NANOVG
    /* NanoVG path only for n=7; for n=13 the fixed-step path is used. */
    if (n == 7U) {
        int32_t cnt;
        /* Upper arc: ctrl[0..3].  With edgeAntiAlias=0 + shapeAntiAlias=0, NVG's
         * fill polygon for an open path contains exactly the curve points [p0..p3]
         * with no implicit closing segment — so plain arc-length resample is correct. */
        cnt = s_nvg_flatten_single(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3]);
        if (cnt >= 2) {
            have_upper = s_nvg_resample(cnt, cols, upper);
        }
        /* Lower arc: ctrl[6..3] forward (= ctrl[3..6] reversed → left→right). */
        cnt = s_nvg_flatten_single(&ctrl[6], &ctrl[5], &ctrl[4], &ctrl[3]);
        if (cnt >= 2) {
            have_lower = s_nvg_resample(cnt, cols, lower);
        }
    }
#endif /* GFX_SM_HAVE_NANOVG */

    /* ── Fixed-step tessellation ── */
    if (!have_upper) {
        if (n == 7U) {
            /* Single cubic bezier: ctrl[0..3], left→right through top arc */
            for (uint8_t i = 0; i <= segs; i++) {
                float t = (float)i / (float)segs;
                s_cubic_bezier(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3], t, &upper[i]);
            }
        } else {
            /* n=13: two cubic beziers forming the upper semicircle.
             * ctrl[9]=LEFT, ctrl[12]=ctrl[0]=TOP, ctrl[3]=RIGHT.
             * Q3 forward [9→12]: LEFT→TOP (first segs/2 + 1 samples).
             * Q0 forward [0→3]:  TOP→RIGHT (remaining samples, skip duplicate TOP). */
            const uint8_t h = segs / 2U;
            for (uint8_t i = 0; i <= h; i++) {
                s_cubic_bezier(&ctrl[9], &ctrl[10], &ctrl[11], &ctrl[12],
                               (float)i / (float)h, &upper[i]);
            }
            for (uint8_t i = 1; i <= segs - h; i++) {
                s_cubic_bezier(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3],
                               (float)i / (float)(segs - h), &upper[h + i]);
            }
        }
    }
    if (!have_lower) {
        if (n == 7U) {
            /* Lower cubic Bezier: ctrl[3..6], t = 1→0 (reversed → left→right) */
            for (uint8_t i = 0; i <= segs; i++) {
                float t = (float)(segs - i) / (float)segs;
                s_cubic_bezier(&ctrl[3], &ctrl[4], &ctrl[5], &ctrl[6], t, &lower[i]);
            }
        } else {
            /* n=13: two cubic beziers forming the lower semicircle.
             * Q2 reversed [9→6]: LEFT→BOTTOM (first segs/2 + 1 samples).
             * Q1 reversed [6→3]: BOTTOM→RIGHT (remaining samples, skip duplicate BOTTOM). */
            const uint8_t h = segs / 2U;
            for (uint8_t i = 0; i <= h; i++) {
                s_cubic_bezier(&ctrl[9], &ctrl[8], &ctrl[7], &ctrl[6],
                               (float)i / (float)h, &lower[i]);
            }
            for (uint8_t i = 1; i <= segs - h; i++) {
                s_cubic_bezier(&ctrl[6], &ctrl[5], &ctrl[4], &ctrl[3],
                               (float)i / (float)(segs - h), &lower[h + i]);
            }
        }
    }

    /* Bounding box */
    for (uint16_t i = 0; i < cols; i++) {
        if (upper[i].x < min_x) { min_x = upper[i].x; } if (upper[i].x > max_x) { max_x = upper[i].x; }
        if (upper[i].y < min_y) { min_y = upper[i].y; } if (upper[i].y > max_y) { max_y = upper[i].y; }
        if (lower[i].x < min_x) { min_x = lower[i].x; } if (lower[i].x > max_x) { max_x = lower[i].x; }
        if (lower[i].y < min_y) { min_y = lower[i].y; } if (lower[i].y > max_y) { max_y = lower[i].y; }
    }
    if (max_x <= min_x) { max_x = min_x + 1; }
    if (max_y <= min_y) { max_y = min_y + 1; }

    /* Row 0: upper arc, left→right */
    for (uint16_t i = 0; i < cols; i++) {
        pts[i].x_q8 = (upper[i].x - min_x) << 8;
        pts[i].y_q8 = (upper[i].y - min_y) << 8;
    }
    /* Row 1: lower arc reversed, also left→right */
    for (uint16_t i = 0; i < cols; i++) {
        pts[cols + i].x_q8 = (lower[i].x - min_x) << 8;
        pts[cols + i].y_q8 = (lower[i].y - min_y) << 8;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "bezier_fill align");
    return gfx_mesh_img_set_points_q8(obj, pts, (size_t)cols * 2U);
}

/* ------------------------------------------------------------------ */
/*  gfx_rig callbacks                                                  */
/* ------------------------------------------------------------------ */

static bool s_rig_tick(gfx_rig_t *rig, void *user_data)
{
    gfx_sm_runtime_t *rt = (gfx_sm_runtime_t *)user_data;
    bool changed;

    (void)rig;
    if (rt == NULL) {
        return false;
    }
    gfx_sm_scene_advance(&rt->scene);
    changed = gfx_sm_scene_tick(&rt->scene);
    return changed || rt->scene.dirty || rt->mesh_dirty;
}

static esp_err_t s_rig_apply(gfx_rig_t *rig, void *user_data, bool force)
{
    gfx_sm_runtime_t      *rt    = (gfx_sm_runtime_t *)user_data;
    const gfx_sm_scene_t  *scene;
    const gfx_sm_asset_t  *asset;
    const gfx_sm_layout_t *layout;
    int32_t                def_stroke_px;

    (void)rig;
    if (rt == NULL) {
        return ESP_OK;
    }
    scene = &rt->scene;
    if (scene->asset == NULL) {
        return ESP_OK;
    }
    if (!force && !scene->dirty && !rt->mesh_dirty) {
        return ESP_OK;
    }

    asset  = scene->asset;
    layout = asset->layout;

    def_stroke_px = s_scalar_px(asset, rt->canvas_w, rt->canvas_h,
                                (float)layout->stroke_width);

    for (uint8_t i = 0; i < rt->seg_obj_count && i < asset->segment_count; i++) {
        const gfx_sm_segment_t *seg = &asset->segments[i];
        gfx_obj_t              *obj = rt->seg_objs[i];

        if (obj == NULL) {
            continue;
        }

        /* Per-segment stroke override */
        int32_t stroke_px = (seg->stroke_width > 0)
            ? s_scalar_px(asset, rt->canvas_w, rt->canvas_h, (float)seg->stroke_width)
            : def_stroke_px;

        switch (seg->kind) {
        case GFX_SM_SEG_CAPSULE: {
            s_spt_t pa, pb;
            s_to_screen(asset, &scene->pose_cur[seg->joint_a],
                        rt->canvas_x, rt->canvas_y, rt->canvas_w, rt->canvas_h, &pa);
            s_to_screen(asset, &scene->pose_cur[seg->joint_b],
                        rt->canvas_x, rt->canvas_y, rt->canvas_w, rt->canvas_h, &pb);
            ESP_RETURN_ON_ERROR(s_apply_capsule(obj, &pa, &pb, stroke_px),
                                TAG, "capsule seg[%u]", i);
            break;
        }

        case GFX_SM_SEG_RING: {
            s_spt_t pc;
            s_to_screen(asset, &scene->pose_cur[seg->joint_a],
                        rt->canvas_x, rt->canvas_y, rt->canvas_w, rt->canvas_h, &pc);
            int32_t radius_px = (seg->radius_hint > 0)
                ? s_scalar_px(asset, rt->canvas_w, rt->canvas_h, (float)seg->radius_hint)
                : stroke_px * 4;
            uint8_t segs = s_ring_segs((float)radius_px);
            gfx_mesh_img_set_grid(obj, segs, 1U);
            ESP_RETURN_ON_ERROR(s_apply_ring(obj, &pc, radius_px, stroke_px, segs),
                                TAG, "ring seg[%u]", i);
            break;
        }

        case GFX_SM_SEG_BEZIER_STRIP:
        case GFX_SM_SEG_BEZIER_LOOP: {
            uint16_t n = seg->joint_count;
            if (n < 2U || n > SM_BEZIER_MAX_PTS ||
                    (uint32_t)seg->joint_a + n > asset->joint_count) {
                continue;
            }
            s_spt_t ctrl_pts[SM_BEZIER_MAX_PTS];
            for (uint16_t j = 0; j < n; j++) {
                s_to_screen(asset,
                            &scene->pose_cur[seg->joint_a + j],
                            rt->canvas_x, rt->canvas_y,
                            rt->canvas_w, rt->canvas_h,
                            &ctrl_pts[j]);
            }
            bool loop = (seg->kind == GFX_SM_SEG_BEZIER_LOOP);
            ESP_RETURN_ON_ERROR(s_apply_bezier(obj, ctrl_pts, (uint8_t)n, stroke_px, loop),
                                TAG, "bezier seg[%u]", i);
            break;
        }

        case GFX_SM_SEG_BEZIER_FILL: {
            uint16_t n = seg->joint_count;
            if (n < 3U || n > SM_BEZIER_MAX_PTS ||
                    (uint32_t)seg->joint_a + n > asset->joint_count) {
                continue;
            }
            s_spt_t ctrl_pts[SM_BEZIER_MAX_PTS];
            for (uint16_t j = 0; j < n; j++) {
                s_to_screen(asset,
                            &scene->pose_cur[seg->joint_a + j],
                            rt->canvas_x, rt->canvas_y,
                            rt->canvas_w, rt->canvas_h,
                            &ctrl_pts[j]);
            }
            ESP_RETURN_ON_ERROR(s_apply_bezier_fill(obj, ctrl_pts, (uint8_t)n),
                                TAG, "bezier_fill seg[%u]", i);
            break;
        }

        default:
            break;
        }

        gfx_obj_set_visible(obj, true);
    }

    rt->mesh_dirty             = false;
    ((gfx_sm_scene_t *)scene)->dirty = false;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t gfx_sm_runtime_init(gfx_sm_runtime_t *rt,
                               gfx_disp_t *disp,
                               const gfx_sm_asset_t *asset)
{
    gfx_img_src_t solid_src;
    gfx_rig_cfg_t rig_cfg;
    bool          swap;

    ESP_RETURN_ON_FALSE(rt    != NULL, ESP_ERR_INVALID_ARG, TAG, "rt is NULL");
    ESP_RETURN_ON_FALSE(disp  != NULL, ESP_ERR_INVALID_ARG, TAG, "disp is NULL");
    ESP_RETURN_ON_FALSE(asset != NULL, ESP_ERR_INVALID_ARG, TAG, "asset is NULL");
    ESP_RETURN_ON_FALSE(asset->segment_count <= GFX_SM_RUNTIME_MAX_SEGS,
                        ESP_ERR_INVALID_ARG, TAG, "too many segments");

    memset(rt, 0, sizeof(*rt));

    /* ── Parse / validate asset ── */
    ESP_RETURN_ON_ERROR(gfx_sm_scene_init(&rt->scene, asset),
                        TAG, "scene init failed");

    /* ── Solid-colour 1×1 pixel source ── */
    rt->stroke_color = GFX_COLOR_HEX(0x1F1F1F);
    swap = disp->flags.swap;
    rt->solid_pixel  = gfx_color_to_native_u16(rt->stroke_color, swap);
    rt->solid_img.header.magic  = 0x19;
    rt->solid_img.header.cf     = GFX_COLOR_FORMAT_RGB565;
    rt->solid_img.header.w      = 1;
    rt->solid_img.header.h      = 1;
    rt->solid_img.header.stride = 2;
    rt->solid_img.data_size     = 2;
    rt->solid_img.data          = (const uint8_t *)&rt->solid_pixel;

    solid_src.type = GFX_IMG_SRC_TYPE_IMAGE_DSC;
    solid_src.data = &rt->solid_img;

    /* ── Palette 1×1 pixel sources (colour_idx 1..N) ── */
    for (uint8_t pi = 0U; pi < GFX_SM_PALETTE_MAX; pi++) {
        rt->palette_imgs[pi]       = rt->solid_img;          /* copy header fields */
        rt->palette_pixels[pi]     = rt->solid_pixel;        /* default = stroke colour */
        rt->palette_imgs[pi].data  = (const uint8_t *)&rt->palette_pixels[pi];
    }
    if (asset->color_palette != NULL) {
        uint8_t n = (asset->color_palette_count < GFX_SM_PALETTE_MAX)
                    ? asset->color_palette_count : GFX_SM_PALETTE_MAX;
        for (uint8_t pi = 0U; pi < n; pi++) {
            gfx_color_t pc = GFX_COLOR_HEX(asset->color_palette[pi]);
            rt->palette_pixels[pi] = gfx_color_to_native_u16(pc, swap);
        }
    }

    /* ── Default canvas: full display ── */
    rt->canvas_x = 0;
    rt->canvas_y = 0;
    rt->canvas_w = (uint16_t)gfx_disp_get_hor_res(disp);
    rt->canvas_h = (uint16_t)gfx_disp_get_ver_res(disp);
    rt->mesh_dirty = true;

    /* ── Create one mesh_img per segment ── */
    for (uint8_t i = 0; i < asset->segment_count; i++) {
        const gfx_sm_segment_t *seg = &asset->segments[i];
        gfx_obj_t *obj = gfx_mesh_img_create(disp);
        ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, TAG, "mesh_img[%u] failed", i);
        gfx_obj_set_visible(obj, false);   /* hide until first valid mesh is applied */

        switch (seg->kind) {
        case GFX_SM_SEG_RING: {
            uint8_t segs = s_ring_segs((float)(seg->radius_hint > 0 ? seg->radius_hint : 20));
            gfx_mesh_img_set_grid(obj, segs, 1U);
            gfx_mesh_img_set_wrap_cols(obj, true);
            break;
        }
        case GFX_SM_SEG_CAPSULE:
            gfx_mesh_img_set_grid(obj, 1U, 1U);
            break;
        case GFX_SM_SEG_BEZIER_LOOP: {
            /* Stroke: k = (n-1)/3 segments × segs_per → k*segs_per cols, wrap. */
            uint16_t n     = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
            uint16_t k     = (n - 1U) / 3U;
            uint16_t tcols = k * (uint16_t)SM_BEZIER_SEGS_PER_SEG;
            uint8_t  gcols = (tcols > 255U) ? 255U : (uint8_t)tcols;
            gfx_mesh_img_set_grid(obj, gcols, 1U);
            gfx_mesh_img_set_wrap_cols(obj, true);
            break;
        }
        case GFX_SM_SEG_BEZIER_FILL: {
            /* Two-half scan fill: grid(SM_BEZIER_FILL_SEGS, 1), no wrap. */
            gfx_mesh_img_set_grid(obj, (uint8_t)SM_BEZIER_FILL_SEGS, 1U);
            break;
        }
        case GFX_SM_SEG_BEZIER_STRIP: {
            /* Stroke: k = (n-1)/3 segments × segs_per → k*segs_per cols, no wrap. */
            uint16_t n     = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
            uint16_t k     = (n - 1U) / 3U;
            uint16_t tcols = k * (uint16_t)SM_BEZIER_SEGS_PER_SEG;
            uint8_t  gcols = (tcols > 255U) ? 255U : (uint8_t)tcols;
            gfx_mesh_img_set_grid(obj, gcols, 1U);
            break;
        }
        default:
            gfx_mesh_img_set_grid(obj, 1U, 1U);
            break;
        }

        /* BEZIER_FILL is a solid region — no inward-edge AA (would make centre transparent) */
        if (seg->kind != GFX_SM_SEG_BEZIER_FILL) {
            gfx_mesh_img_set_aa_inward(obj, true);
        }

        /* Bind per-segment image source: texture > palette colour > solid colour */
        if (seg->resource_idx > 0U
                && asset->resources != NULL
                && (uint8_t)(seg->resource_idx - 1U) < asset->resource_count
                && asset->resources[seg->resource_idx - 1U].image != NULL) {
            gfx_img_src_t res_src = {
                .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
                .data = (const void *)asset->resources[seg->resource_idx - 1U].image,
            };
            gfx_mesh_img_set_src_desc(obj, &res_src);
        } else if (seg->color_idx > 0U && seg->color_idx <= GFX_SM_PALETTE_MAX) {
            gfx_img_src_t pal_src = {
                .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
                .data = &rt->palette_imgs[seg->color_idx - 1U],
            };
            gfx_mesh_img_set_src_desc(obj, &pal_src);
        } else {
            gfx_mesh_img_set_src_desc(obj, &solid_src);
        }

        /* TODO: apply seg->opacity once gfx_obj exposes an opacity API */

        gfx_obj_set_visible(obj, false);

        rt->seg_objs[i] = obj;
    }
    rt->seg_obj_count = asset->segment_count;

    /* ── Start rig timer — use first seg_obj as anchor (non-NULL required) ── */
    gfx_rig_cfg_init(&rig_cfg,
                     (asset->layout->timer_period_ms > 0U) ? asset->layout->timer_period_ms : 33U,
                     (asset->layout->damping_div > 0) ? (uint32_t)asset->layout->damping_div : 4U);
    ESP_RETURN_ON_ERROR(
        gfx_rig_init(&rt->rig, disp, rt->seg_objs[0], &rig_cfg,
                     s_rig_tick, s_rig_apply, rt),
        TAG, "rig init failed");

    /* Do NOT sync here: let the first rig tick (≤ one timer period)
     * trigger the initial sync after the layout pass stabilises. */

#if GFX_SM_HAVE_NANOVG
    /* Lazily create the shared NVG context on first runtime init. */
    if (s_nvg_ctx == NULL) {
        s_nvg_ctx = s_nvg_create_ctx(&s_nvg_cap);
        if (s_nvg_ctx == NULL) {
            ESP_LOGW(TAG, "NVG context creation failed; falling back to fixed-step Bézier");
        }
    }
    if (s_nvg_ctx != NULL) {
        s_nvg_refcnt++;
    }
#endif

    return ESP_OK;
}

void gfx_sm_runtime_deinit(gfx_sm_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }
    gfx_rig_deinit(&rt->rig);
    for (uint8_t i = 0; i < rt->seg_obj_count; i++) {
        if (rt->seg_objs[i] != NULL) {
            gfx_obj_delete(rt->seg_objs[i]);
            rt->seg_objs[i] = NULL;
        }
    }

#if GFX_SM_HAVE_NANOVG
    if (s_nvg_ctx != NULL && s_nvg_refcnt > 0U) {
        s_nvg_refcnt--;
        if (s_nvg_refcnt == 0U) {
            nvgDeleteInternal(s_nvg_ctx);
            s_nvg_ctx = NULL;
        }
    }
#endif

    memset(rt, 0, sizeof(*rt));
}

esp_err_t gfx_sm_runtime_set_color(gfx_sm_runtime_t *rt, gfx_color_t color)
{
    gfx_img_src_t solid_src;
    bool          swap;

    ESP_RETURN_ON_FALSE(rt != NULL && rt->seg_obj_count > 0U,
                        ESP_ERR_INVALID_STATE, TAG, "runtime not ready");
    ESP_RETURN_ON_FALSE(rt->seg_objs[0] != NULL && rt->seg_objs[0]->disp != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display not ready");

    swap = rt->seg_objs[0]->disp->flags.swap;
    rt->stroke_color = color;
    rt->solid_pixel  = gfx_color_to_native_u16(color, swap);

    solid_src.type = GFX_IMG_SRC_TYPE_IMAGE_DSC;
    solid_src.data = &rt->solid_img;

    for (uint8_t i = 0; i < rt->seg_obj_count; i++) {
        /* Skip texture-mapped and palette-coloured segments */
        if (rt->scene.asset != NULL && i < rt->scene.asset->segment_count) {
            const gfx_sm_segment_t *seg = &rt->scene.asset->segments[i];
            if (seg->resource_idx != 0U || seg->color_idx != 0U) {
                continue;
            }
        }
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(rt->seg_objs[i], &solid_src),
                            TAG, "set color seg[%u]", i);
    }
    return ESP_OK;
}

esp_err_t gfx_sm_runtime_set_canvas(gfx_sm_runtime_t *rt,
                                     gfx_coord_t x, gfx_coord_t y,
                                     uint16_t w, uint16_t h)
{
    ESP_RETURN_ON_FALSE(rt != NULL, ESP_ERR_INVALID_ARG, TAG, "rt is NULL");
    ESP_RETURN_ON_FALSE(w > 0U && h > 0U, ESP_ERR_INVALID_ARG, TAG, "size must be > 0");
    rt->canvas_x   = x;
    rt->canvas_y   = y;
    rt->canvas_w   = w;
    rt->canvas_h   = h;
    rt->mesh_dirty = true;
    return ESP_OK;
}

esp_err_t gfx_sm_runtime_set_clip(gfx_sm_runtime_t *rt, uint16_t clip_idx, bool snap)
{
    ESP_RETURN_ON_FALSE(rt != NULL, ESP_ERR_INVALID_ARG, TAG, "rt is NULL");
    return gfx_sm_scene_set_clip(&rt->scene, clip_idx, snap);
}
