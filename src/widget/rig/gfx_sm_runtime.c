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

#define SM_RING_SEGS_MIN  16U
#define SM_RING_SEGS_MAX  48U
/* Max control points in one segment (joint_count ≤ GFX_SM_SCENE_MAX_JOINTS). */
#define SM_BEZIER_MAX_PTS       GFX_SM_SCENE_MAX_JOINTS
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

typedef struct { int32_t x; int32_t y; } s_spt_t;

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

/* Tessellate n = 3k+1 cubic Bezier control points.
 * segs_per: samples per segment (t = 0, 1/segs, …, (segs-1)/segs per seg).
 * loop=true: closed — k*segs_per output pts (no repeated endpoint).
 * loop=false: open  — k*segs_per + 1 output pts (includes final endpoint). */
static uint16_t s_tess_cubic_bezier(const s_spt_t *ctrl, uint8_t n,
                                     bool loop, uint8_t segs_per,
                                     s_spt_t *out)
{
    uint16_t count = 0;
    uint8_t  k;

    if (n < 4U) { return 0U; }
    k = (uint8_t)((n - 1U) / 3U);   /* number of cubic segments */

    for (uint8_t seg = 0; seg < k; seg++) {
        const s_spt_t *p0 = &ctrl[(uint8_t)(seg * 3U)];
        const s_spt_t *p1 = &ctrl[(uint8_t)(seg * 3U + 1U)];
        const s_spt_t *p2 = &ctrl[(uint8_t)(seg * 3U + 2U)];
        const s_spt_t *p3 = loop
            ? &ctrl[(uint8_t)(((seg + 1U) * 3U) % (n - 1U))]
            : &ctrl[(uint8_t)((seg + 1U) * 3U)];

        for (uint8_t sub = 0; sub < segs_per; sub++) {
            s_cubic_bezier(p0, p1, p2, p3,
                           (float)sub / (float)segs_per,
                           &out[count++]);
        }
    }
    if (!loop) {
        out[count++] = ctrl[n - 1U];   /* final endpoint for open strip */
    }
    return count;
}

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

static esp_err_t s_apply_bezier(gfx_obj_t *obj,
                                 const s_spt_t *ctrl, uint8_t n,
                                 int32_t thick, bool loop)
{
    s_spt_t  tess[SM_BEZIER_MAX_TESS + 1U];
    gfx_mesh_img_point_q8_t pts[(SM_BEZIER_MAX_TESS + 1U) * 2U];
    int32_t ox[SM_BEZIER_MAX_TESS + 1U];
    int32_t oy[SM_BEZIER_MAX_TESS + 1U];
    int32_t ix_arr[SM_BEZIER_MAX_TESS + 1U];
    int32_t iy_arr[SM_BEZIER_MAX_TESS + 1U];
    int32_t min_x = INT32_MAX, max_x = INT32_MIN;
    int32_t min_y = INT32_MAX, max_y = INT32_MIN;
    float   half  = (float)thick * 0.5f;

    if (n < 4U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t M = s_tess_cubic_bezier(ctrl, n, loop, SM_BEZIER_SEGS_PER_SEG, tess);

    /* ── Compute per-vertex normals on the tessellated path ── */
    for (uint16_t i = 0; i < M; i++) {
        uint16_t prev, next;
        if (loop) {
            prev = (uint16_t)((i + M - 1U) % M);
            next = (uint16_t)((i + 1U)     % M);
        } else {
            prev = (i == 0U)      ? 0U          : (uint16_t)(i - 1U);
            next = (i == M - 1U)  ? (uint16_t)(M - 1U) : (uint16_t)(i + 1U);
        }

        float tx = (float)(tess[next].x - tess[prev].x);
        float ty = (float)(tess[next].y - tess[prev].y);
        float len = sqrtf(tx * tx + ty * ty);
        if (len < 0.001f) { tx = 1.0f; ty = 0.0f; }
        else { tx /= len; ty /= len; }

        float nx = -ty, ny = tx;   /* left-hand normal */

        ox[i]     = (int32_t)lroundf((float)tess[i].x + nx * half);
        oy[i]     = (int32_t)lroundf((float)tess[i].y + ny * half);
        ix_arr[i] = (int32_t)lroundf((float)tess[i].x - nx * half);
        iy_arr[i] = (int32_t)lroundf((float)tess[i].y - ny * half);
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

    /* Need at least two cubic halves: n must be 7 (= 3*2+1). */
    if (n < 7U) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ── Sample upper cubic Bezier: ctrl[0..3], t = 0→1 ── */
    for (uint8_t i = 0; i <= segs; i++) {
        float t = (float)i / (float)segs;
        s_cubic_bezier(&ctrl[0], &ctrl[1], &ctrl[2], &ctrl[3], t, &upper[i]);
    }
    /* ── Sample lower cubic Bezier: ctrl[3..6], t = 1→0 (reversed) ── */
    for (uint8_t i = 0; i <= segs; i++) {
        float t = (float)(segs - i) / (float)segs;   /* descend t: right→left on curve */
        s_cubic_bezier(&ctrl[3], &ctrl[4], &ctrl[5], &ctrl[6], t, &lower[i]);
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
            uint8_t n = seg->joint_count;
            if (n < 2U || (uint32_t)seg->joint_a + n > asset->joint_count) {
                continue;
            }
            s_spt_t ctrl_pts[SM_BEZIER_MAX_PTS];
            for (uint8_t j = 0; j < n; j++) {
                s_to_screen(asset,
                            &scene->pose_cur[seg->joint_a + j],
                            rt->canvas_x, rt->canvas_y,
                            rt->canvas_w, rt->canvas_h,
                            &ctrl_pts[j]);
            }
            bool loop = (seg->kind == GFX_SM_SEG_BEZIER_LOOP);
            ESP_RETURN_ON_ERROR(s_apply_bezier(obj, ctrl_pts, n, stroke_px, loop),
                                TAG, "bezier seg[%u]", i);
            break;
        }

        case GFX_SM_SEG_BEZIER_FILL: {
            uint8_t n = seg->joint_count;
            if (n < 3U || (uint32_t)seg->joint_a + n > asset->joint_count) {
                continue;
            }
            s_spt_t ctrl_pts[SM_BEZIER_MAX_PTS];
            for (uint8_t j = 0; j < n; j++) {
                s_to_screen(asset,
                            &scene->pose_cur[seg->joint_a + j],
                            rt->canvas_x, rt->canvas_y,
                            rt->canvas_w, rt->canvas_h,
                            &ctrl_pts[j]);
            }
            ESP_RETURN_ON_ERROR(s_apply_bezier_fill(obj, ctrl_pts, n),
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
            uint8_t  n     = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
            uint8_t  k     = (uint8_t)((n - 1U) / 3U);
            uint16_t tcols = (uint16_t)k * SM_BEZIER_SEGS_PER_SEG;
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
            uint8_t  n     = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
            uint8_t  k     = (uint8_t)((n - 1U) / 3U);
            uint16_t tcols = (uint16_t)k * SM_BEZIER_SEGS_PER_SEG;
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

        /* Bind per-segment image source: texture if resource_idx > 0, else solid colour */
        if (seg->resource_idx > 0U
                && asset->resources != NULL
                && (uint8_t)(seg->resource_idx - 1U) < asset->resource_count
                && asset->resources[seg->resource_idx - 1U].image != NULL) {
            gfx_img_src_t res_src = {
                .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
                .data = (const void *)asset->resources[seg->resource_idx - 1U].image,
            };
            gfx_mesh_img_set_src_desc(obj, &res_src);
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
        /* Skip texture-mapped segments — colour change does not apply */
        if (rt->scene.asset != NULL && i < rt->scene.asset->segment_count) {
            if (rt->scene.asset->segments[i].resource_idx != 0U) {
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

esp_err_t gfx_sm_runtime_set_clip_name(gfx_sm_runtime_t *rt, const char *name, bool snap)
{
    ESP_RETURN_ON_FALSE(rt != NULL, ESP_ERR_INVALID_ARG, TAG, "rt is NULL");
    return gfx_sm_scene_set_clip_name(&rt->scene, name, snap);
}
