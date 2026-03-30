/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "widget/gfx_mesh_img.h"
#include "widget/face/gfx_face_emote_priv.h"

static const char *TAG = "face_emote";

#ifndef GFX_HAVE_NANOVG
#define GFX_HAVE_NANOVG 0
#endif

#if GFX_HAVE_NANOVG
#include "nanovg.h"
#endif

#define GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS 512

static int32_t gfx_face_emote_isqrt_i64(uint64_t value)
{
    uint64_t op, res, one;

    if (value <= 1U) {
        return (int32_t)value;
    }

    op = value;
    res = 0U;
    one = 1ULL << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0U) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return (int32_t)res;
}

static inline int32_t gfx_face_emote_q8_floor_to_int(int32_t value_q8)
{
    if (value_q8 >= 0) {
        return value_q8 / GFX_FACE_EMOTE_Q8_ONE;
    }
    return -(((-value_q8) + (GFX_FACE_EMOTE_Q8_ONE - 1)) / GFX_FACE_EMOTE_Q8_ONE);
}

static inline int32_t gfx_face_emote_transform_x(int16_t value,
                                                 int32_t base,
                                                 int32_t scale_percent,
                                                 bool flip_x)
{
    int32_t sign = flip_x ? -1 : 1;
    return base + (sign * (int32_t)value * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
}

static inline int32_t gfx_face_emote_transform_y(int16_t value,
                                                 int32_t base,
                                                 int32_t scale_percent)
{
    return base + ((int32_t)value * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
}

/*
 * Keep cubic sampling centralized so nanovg path flattening can later replace
 * this layer without changing the stroke/fill call sites.
 */
static void gfx_face_emote_eval_cubic_q8(int32_t p0x, int32_t p0y,
                                         int32_t p1x, int32_t p1y,
                                         int32_t p2x, int32_t p2y,
                                         int32_t p3x, int32_t p3y,
                                         int32_t t,
                                         int32_t *x_q8,
                                         int32_t *y_q8)
{
    int32_t mt = GFX_FACE_EMOTE_BEZIER_T_DEN - t;
    int64_t t64 = t;
    int64_t mt64 = mt;
    int64_t bw0 = mt64 * mt64 * mt64;
    int64_t bw1 = 3 * t64 * mt64 * mt64;
    int64_t bw2 = 3 * t64 * t64 * mt64;
    int64_t bw3 = t64 * t64 * t64;
    int64_t den3 = (int64_t)GFX_FACE_EMOTE_BEZIER_T_DEN *
                   GFX_FACE_EMOTE_BEZIER_T_DEN *
                   GFX_FACE_EMOTE_BEZIER_T_DEN;

    *x_q8 = (int32_t)((p0x * bw0 + p1x * bw1 + p2x * bw2 + p3x * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
    *y_q8 = (int32_t)((p0y * bw0 + p1y * bw1 + p2y * bw2 + p3y * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
}

static void gfx_face_emote_sample_cubic_q8(int32_t p0x, int32_t p0y,
                                           int32_t p1x, int32_t p1y,
                                           int32_t p2x, int32_t p2y,
                                           int32_t p3x, int32_t p3y,
                                           int32_t segs,
                                           bool reverse_t,
                                           int32_t *px_q8,
                                           int32_t *py_q8)
{
    for (int32_t i = 0; i <= segs; i++) {
        int32_t t = reverse_t ? ((segs - i) * GFX_FACE_EMOTE_BEZIER_T_DEN) / segs
                              : (i * GFX_FACE_EMOTE_BEZIER_T_DEN) / segs;
        gfx_face_emote_eval_cubic_q8(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, t,
                                     &px_q8[i], &py_q8[i]);
    }
}

static void gfx_face_emote_reverse_points_q8(int32_t *px_q8, int32_t *py_q8, int32_t count)
{
    for (int32_t i = 0; i < count / 2; i++) {
        int32_t j = count - 1 - i;
        int32_t tmp_x = px_q8[i];
        int32_t tmp_y = py_q8[i];
        px_q8[i] = px_q8[j];
        py_q8[i] = py_q8[j];
        px_q8[j] = tmp_x;
        py_q8[j] = tmp_y;
    }
}

static bool gfx_face_emote_resample_polyline_q8(const int32_t *src_px_q8,
                                                const int32_t *src_py_q8,
                                                int32_t src_count,
                                                int32_t dst_count,
                                                int32_t *dst_px_q8,
                                                int32_t *dst_py_q8)
{
    uint64_t acc_len[GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS];
    int32_t seg = 1;

    if (src_px_q8 == NULL || src_py_q8 == NULL || dst_px_q8 == NULL || dst_py_q8 == NULL ||
            src_count <= 0 || src_count > GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS || dst_count <= 0) {
        return false;
    }

    if (src_count == 1 || dst_count == 1) {
        for (int32_t i = 0; i < dst_count; i++) {
            dst_px_q8[i] = src_px_q8[0];
            dst_py_q8[i] = src_py_q8[0];
        }
        return true;
    }

    acc_len[0] = 0;
    for (int32_t i = 1; i < src_count; i++) {
        int64_t dx = (int64_t)src_px_q8[i] - src_px_q8[i - 1];
        int64_t dy = (int64_t)src_py_q8[i] - src_py_q8[i - 1];
        acc_len[i] = acc_len[i - 1] + (uint64_t)gfx_face_emote_isqrt_i64((uint64_t)(dx * dx + dy * dy));
    }

    if (acc_len[src_count - 1] == 0U) {
        for (int32_t i = 0; i < dst_count; i++) {
            dst_px_q8[i] = src_px_q8[0];
            dst_py_q8[i] = src_py_q8[0];
        }
        return true;
    }

    dst_px_q8[0] = src_px_q8[0];
    dst_py_q8[0] = src_py_q8[0];
    dst_px_q8[dst_count - 1] = src_px_q8[src_count - 1];
    dst_py_q8[dst_count - 1] = src_py_q8[src_count - 1];

    for (int32_t i = 1; i < dst_count - 1; i++) {
        uint64_t target = (acc_len[src_count - 1] * (uint64_t)i) / (uint64_t)(dst_count - 1);

        while (seg < src_count && acc_len[seg] < target) {
            seg++;
        }

        if (seg >= src_count) {
            dst_px_q8[i] = src_px_q8[src_count - 1];
            dst_py_q8[i] = src_py_q8[src_count - 1];
            continue;
        }

        {
            uint64_t base = acc_len[seg - 1];
            uint64_t span = acc_len[seg] - base;

            if (span == 0U) {
                dst_px_q8[i] = src_px_q8[seg];
                dst_py_q8[i] = src_py_q8[seg];
            } else {
                uint64_t w1 = target - base;
                uint64_t w0 = span - w1;
                dst_px_q8[i] = (int32_t)(((int64_t)src_px_q8[seg - 1] * (int64_t)w0 +
                                          (int64_t)src_px_q8[seg] * (int64_t)w1 + (int64_t)(span / 2U)) / (int64_t)span);
                dst_py_q8[i] = (int32_t)(((int64_t)src_py_q8[seg - 1] * (int64_t)w0 +
                                          (int64_t)src_py_q8[seg] * (int64_t)w1 + (int64_t)(span / 2U)) / (int64_t)span);
            }
        }
    }

    return true;
}

#if GFX_HAVE_NANOVG
typedef struct {
    int next_image_id;
    int tex_w[8];
    int tex_h[8];
    int32_t point_count;
    int32_t px_q8[GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS];
    int32_t py_q8[GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS];
} gfx_face_emote_nanovg_capture_t;

static int gfx_face_emote_nanovg_render_create(void *uptr)
{
    NVG_NOTUSED(uptr);
    return 1;
}

static int gfx_face_emote_nanovg_render_create_texture(void *uptr, int type, int w, int h, int image_flags, const unsigned char *data)
{
    gfx_face_emote_nanovg_capture_t *cap = (gfx_face_emote_nanovg_capture_t *)uptr;
    int image_id;

    NVG_NOTUSED(type);
    NVG_NOTUSED(image_flags);
    NVG_NOTUSED(data);

    if (cap == NULL) {
        return 0;
    }

    image_id = ++cap->next_image_id;
    if (image_id > 0 && image_id <= (int)(sizeof(cap->tex_w) / sizeof(cap->tex_w[0]))) {
        cap->tex_w[image_id - 1] = w;
        cap->tex_h[image_id - 1] = h;
    }
    return image_id;
}

static int gfx_face_emote_nanovg_render_delete_texture(void *uptr, int image)
{
    NVG_NOTUSED(uptr);
    NVG_NOTUSED(image);
    return 1;
}

static int gfx_face_emote_nanovg_render_update_texture(void *uptr, int image, int x, int y, int w, int h, const unsigned char *data)
{
    NVG_NOTUSED(uptr);
    NVG_NOTUSED(image);
    NVG_NOTUSED(x);
    NVG_NOTUSED(y);
    NVG_NOTUSED(w);
    NVG_NOTUSED(h);
    NVG_NOTUSED(data);
    return 1;
}

static int gfx_face_emote_nanovg_render_get_texture_size(void *uptr, int image, int *w, int *h)
{
    gfx_face_emote_nanovg_capture_t *cap = (gfx_face_emote_nanovg_capture_t *)uptr;
    int idx = image - 1;

    if (cap == NULL || idx < 0 || idx >= (int)(sizeof(cap->tex_w) / sizeof(cap->tex_w[0])) || w == NULL || h == NULL) {
        return 0;
    }

    *w = cap->tex_w[idx];
    *h = cap->tex_h[idx];
    return 1;
}

static void gfx_face_emote_nanovg_render_viewport(void *uptr, float width, float height, float device_pixel_ratio)
{
    NVG_NOTUSED(uptr);
    NVG_NOTUSED(width);
    NVG_NOTUSED(height);
    NVG_NOTUSED(device_pixel_ratio);
}

static void gfx_face_emote_nanovg_render_cancel(void *uptr)
{
    NVG_NOTUSED(uptr);
}

static void gfx_face_emote_nanovg_render_flush(void *uptr)
{
    NVG_NOTUSED(uptr);
}

static void gfx_face_emote_nanovg_render_fill(void *uptr, NVGpaint *paint, NVGcompositeOperationState composite_operation,
                                              NVGscissor *scissor, float fringe, const float *bounds,
                                              const NVGpath *paths, int npaths)
{
    gfx_face_emote_nanovg_capture_t *cap = (gfx_face_emote_nanovg_capture_t *)uptr;

    NVG_NOTUSED(paint);
    NVG_NOTUSED(composite_operation);
    NVG_NOTUSED(scissor);
    NVG_NOTUSED(fringe);
    NVG_NOTUSED(bounds);

    if (cap == NULL || paths == NULL) {
        return;
    }

    cap->point_count = 0;
    for (int i = 0; i < npaths; i++) {
        int32_t copy_n = paths[i].nfill;
        if (copy_n <= 0 || paths[i].fill == NULL) {
            continue;
        }
        if (copy_n > GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS) {
            copy_n = GFX_FACE_EMOTE_NANOVG_MAX_FLATTEN_POINTS;
        }
        for (int32_t j = 0; j < copy_n; j++) {
            cap->px_q8[j] = (int32_t)(paths[i].fill[j].x * (float)GFX_FACE_EMOTE_Q8_ONE + (paths[i].fill[j].x >= 0.0f ? 0.5f : -0.5f));
            cap->py_q8[j] = (int32_t)(paths[i].fill[j].y * (float)GFX_FACE_EMOTE_Q8_ONE + (paths[i].fill[j].y >= 0.0f ? 0.5f : -0.5f));
        }
        cap->point_count = copy_n;
        break;
    }
}

static void gfx_face_emote_nanovg_render_stroke(void *uptr, NVGpaint *paint, NVGcompositeOperationState composite_operation,
                                                NVGscissor *scissor, float fringe, float stroke_width,
                                                const NVGpath *paths, int npaths)
{
    NVG_NOTUSED(uptr);
    NVG_NOTUSED(paint);
    NVG_NOTUSED(composite_operation);
    NVG_NOTUSED(scissor);
    NVG_NOTUSED(fringe);
    NVG_NOTUSED(stroke_width);
    NVG_NOTUSED(paths);
    NVG_NOTUSED(npaths);
}

static void gfx_face_emote_nanovg_render_triangles(void *uptr, NVGpaint *paint, NVGcompositeOperationState composite_operation,
                                                   NVGscissor *scissor, const NVGvertex *verts, int nverts, float fringe)
{
    NVG_NOTUSED(uptr);
    NVG_NOTUSED(paint);
    NVG_NOTUSED(composite_operation);
    NVG_NOTUSED(scissor);
    NVG_NOTUSED(verts);
    NVG_NOTUSED(nverts);
    NVG_NOTUSED(fringe);
}

static void gfx_face_emote_nanovg_render_delete(void *uptr)
{
    NVG_NOTUSED(uptr);
}

static NVGcontext *gfx_face_emote_nanovg_create_context(gfx_face_emote_nanovg_capture_t *cap)
{
    NVGparams params;

    if (cap == NULL) {
        return NULL;
    }

    memset(cap, 0, sizeof(*cap));
    memset(&params, 0, sizeof(params));
    params.userPtr = cap;
    params.edgeAntiAlias = 0;
    params.renderCreate = gfx_face_emote_nanovg_render_create;
    params.renderCreateTexture = gfx_face_emote_nanovg_render_create_texture;
    params.renderDeleteTexture = gfx_face_emote_nanovg_render_delete_texture;
    params.renderUpdateTexture = gfx_face_emote_nanovg_render_update_texture;
    params.renderGetTextureSize = gfx_face_emote_nanovg_render_get_texture_size;
    params.renderViewport = gfx_face_emote_nanovg_render_viewport;
    params.renderCancel = gfx_face_emote_nanovg_render_cancel;
    params.renderFlush = gfx_face_emote_nanovg_render_flush;
    params.renderFill = gfx_face_emote_nanovg_render_fill;
    params.renderStroke = gfx_face_emote_nanovg_render_stroke;
    params.renderTriangles = gfx_face_emote_nanovg_render_triangles;
    params.renderDelete = gfx_face_emote_nanovg_render_delete;
    return nvgCreateInternal(&params);
}

static bool gfx_face_emote_nanovg_sample_cubic_q8(NVGcontext *vg,
                                                  gfx_face_emote_nanovg_capture_t *cap,
                                                  int32_t p0x, int32_t p0y,
                                                  int32_t p1x, int32_t p1y,
                                                  int32_t p2x, int32_t p2y,
                                                  int32_t p3x, int32_t p3y,
                                                  int32_t segs,
                                                  bool reverse_t,
                                                  int32_t *px_q8,
                                                  int32_t *py_q8)
{
    int64_t d_start0, d_start1;
    int64_t d_end0, d_end1;

    if (vg == NULL || cap == NULL || px_q8 == NULL || py_q8 == NULL || segs <= 0) {
        return false;
    }

    cap->point_count = 0;
    nvgBeginFrame(vg, 1.0f, 1.0f, 1.0f);
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgMoveTo(vg, (float)p0x, (float)p0y);
    nvgBezierTo(vg, (float)p1x, (float)p1y, (float)p2x, (float)p2y, (float)p3x, (float)p3y);
    nvgFill(vg);
    nvgEndFrame(vg);

    if (cap->point_count < 2 ||
            !gfx_face_emote_resample_polyline_q8(cap->px_q8, cap->py_q8, cap->point_count, segs + 1, px_q8, py_q8)) {
        return false;
    }

    d_start0 = (int64_t)(px_q8[0] - (p0x * GFX_FACE_EMOTE_Q8_ONE)) * (px_q8[0] - (p0x * GFX_FACE_EMOTE_Q8_ONE))
             + (int64_t)(py_q8[0] - (p0y * GFX_FACE_EMOTE_Q8_ONE)) * (py_q8[0] - (p0y * GFX_FACE_EMOTE_Q8_ONE));
    d_start1 = (int64_t)(px_q8[0] - (p3x * GFX_FACE_EMOTE_Q8_ONE)) * (px_q8[0] - (p3x * GFX_FACE_EMOTE_Q8_ONE))
             + (int64_t)(py_q8[0] - (p3y * GFX_FACE_EMOTE_Q8_ONE)) * (py_q8[0] - (p3y * GFX_FACE_EMOTE_Q8_ONE));
    d_end0 = (int64_t)(px_q8[segs] - (p0x * GFX_FACE_EMOTE_Q8_ONE)) * (px_q8[segs] - (p0x * GFX_FACE_EMOTE_Q8_ONE))
           + (int64_t)(py_q8[segs] - (p0y * GFX_FACE_EMOTE_Q8_ONE)) * (py_q8[segs] - (p0y * GFX_FACE_EMOTE_Q8_ONE));
    d_end1 = (int64_t)(px_q8[segs] - (p3x * GFX_FACE_EMOTE_Q8_ONE)) * (px_q8[segs] - (p3x * GFX_FACE_EMOTE_Q8_ONE))
           + (int64_t)(py_q8[segs] - (p3y * GFX_FACE_EMOTE_Q8_ONE)) * (py_q8[segs] - (p3y * GFX_FACE_EMOTE_Q8_ONE));

    if (d_start1 + d_end0 < d_start0 + d_end1) {
        gfx_face_emote_reverse_points_q8(px_q8, py_q8, segs + 1);
    }
    if (reverse_t) {
        gfx_face_emote_reverse_points_q8(px_q8, py_q8, segs + 1);
    }

    return true;
}
#endif

static void gfx_face_emote_sample_cubic_best_q8(
#if GFX_HAVE_NANOVG
                                           NVGcontext *vg,
                                           gfx_face_emote_nanovg_capture_t *cap,
#endif
                                           int32_t p0x, int32_t p0y,
                                           int32_t p1x, int32_t p1y,
                                           int32_t p2x, int32_t p2y,
                                           int32_t p3x, int32_t p3y,
                                           int32_t segs,
                                           bool reverse_t,
                                           int32_t *px_q8,
                                           int32_t *py_q8)
{
#if GFX_HAVE_NANOVG
    if (gfx_face_emote_nanovg_sample_cubic_q8(vg, cap, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                                              segs, reverse_t, px_q8, py_q8)) {
        return;
    }
#endif
    gfx_face_emote_sample_cubic_q8(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, segs, reverse_t, px_q8, py_q8);
}

esp_err_t gfx_face_emote_apply_bezier_stroke(gfx_obj_t *obj,
                                             const int16_t *pts,
                                             bool closed,
                                             int32_t cx,
                                             int32_t cy,
                                             int32_t scale_percent,
                                             int32_t thickness,
                                             bool flip_x,
                                             int32_t segs)
{
    int32_t total_segs = closed ? (segs * 2) : segs;
    int32_t px_q8[GFX_FACE_EMOTE_MAX_STROKE_POINTS];
    int32_t py_q8[GFX_FACE_EMOTE_MAX_STROKE_POINTS];
    gfx_mesh_img_point_q8_t mesh_pts[GFX_FACE_EMOTE_MAX_STROKE_MESH_Q8];
    int32_t min_x_q8 = INT32_MAX;
    int32_t min_y_q8 = INT32_MAX;
    int32_t thick_q8 = (int32_t)thickness * GFX_FACE_EMOTE_Q8_ONE;
    esp_err_t ret;
    int c, i;
#if GFX_HAVE_NANOVG
    gfx_face_emote_nanovg_capture_t nanovg_cap;
    NVGcontext *vg = NULL;
#endif

    if (pts == NULL || segs <= 0 || thickness <= 0 || total_segs > GFX_FACE_EMOTE_MAX_STROKE_SEGS) {
        return ESP_ERR_INVALID_ARG;
    }

#if GFX_HAVE_NANOVG
    vg = gfx_face_emote_nanovg_create_context(&nanovg_cap);
#endif

    for (c = 0; c < (closed ? 2 : 1); c++) {
        int32_t p0x = gfx_face_emote_transform_x(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 0], cx, scale_percent, flip_x);
        int32_t p0y = gfx_face_emote_transform_y(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 1], cy, scale_percent);
        int32_t p1x = gfx_face_emote_transform_x(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 2], cx, scale_percent, flip_x);
        int32_t p1y = gfx_face_emote_transform_y(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 3], cy, scale_percent);
        int32_t p2x = gfx_face_emote_transform_x(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 4], cx, scale_percent, flip_x);
        int32_t p2y = gfx_face_emote_transform_y(pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 5], cy, scale_percent);
        int32_t p3x = (c == 1) ? gfx_face_emote_transform_x(pts[0], cx, scale_percent, flip_x)
                               : gfx_face_emote_transform_x(pts[GFX_FACE_EMOTE_BEZIER_STRIDE], cx, scale_percent, flip_x);
        int32_t p3y = (c == 1) ? gfx_face_emote_transform_y(pts[1], cy, scale_percent)
                               : gfx_face_emote_transform_y(pts[GFX_FACE_EMOTE_BEZIER_STRIDE + 1], cy, scale_percent);
        int32_t *dst_px = &px_q8[c * segs];
        int32_t *dst_py = &py_q8[c * segs];

        gfx_face_emote_sample_cubic_best_q8(
#if GFX_HAVE_NANOVG
                                           vg, &nanovg_cap,
#endif
                                           p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                                           segs, false, dst_px, dst_py);

        if (c == 1) {
            for (i = 0; i < segs; i++) {
                px_q8[segs + i] = dst_px[i + 1];
                py_q8[segs + i] = dst_py[i + 1];
            }
        }
    }

    for (i = 0; i <= total_segs; i++) {
        int32_t nx_q8 = 0;
        int32_t ny_q8 = thick_q8;
        int prev = (i == 0) ? (closed ? total_segs - 1 : 0) : i - 1;
        int next = (i == total_segs) ? (closed ? 1 : total_segs) : i + 1;
        int32_t out_x_q8;
        int32_t out_y_q8;
        int32_t in_x_q8;
        int32_t in_y_q8;

        {
            int32_t dx_in  = px_q8[i] - px_q8[prev];
            int32_t dy_in  = py_q8[i] - py_q8[prev];
            int32_t dx_out = px_q8[next] - px_q8[i];
            int32_t dy_out = py_q8[next] - py_q8[i];
            int32_t len_in  = gfx_face_emote_isqrt_i64((uint64_t)((int64_t)dx_in * dx_in + (int64_t)dy_in * dy_in));
            int32_t len_out = gfx_face_emote_isqrt_i64((uint64_t)((int64_t)dx_out * dx_out + (int64_t)dy_out * dy_out));

            if (len_in > 0 && len_out > 0) {
                int64_t bis_x = -(int64_t)dy_in * len_out - (int64_t)dy_out * len_in;
                int64_t bis_y =  (int64_t)dx_in * len_out + (int64_t)dx_out * len_in;
                int64_t dot_t =  (int64_t)dx_in * dx_out  + (int64_t)dy_in * dy_out;
                int64_t denom =  (int64_t)len_in * len_out + dot_t;
                int64_t denom_min = (int64_t)len_in * len_out / 8;
                if (denom < denom_min) { denom = denom_min; }
                if (denom == 0) { denom = 1; }
                nx_q8 = (int32_t)(bis_x * thick_q8 / denom);
                ny_q8 = (int32_t)(bis_y * thick_q8 / denom);

                {
                    int64_t n_sq = (int64_t)nx_q8 * nx_q8 + (int64_t)ny_q8 * ny_q8;
                    int64_t thick_sq = (int64_t)thick_q8 * thick_q8;
                    if (n_sq < thick_sq) {
                        if (n_sq > 0) {
                            int32_t n_len = gfx_face_emote_isqrt_i64((uint64_t)n_sq);
                            if (n_len > 0) {
                                nx_q8 = (int32_t)((int64_t)nx_q8 * thick_q8 / n_len);
                                ny_q8 = (int32_t)((int64_t)ny_q8 * thick_q8 / n_len);
                            } else {
                                nx_q8 = (int32_t)(-(int64_t)dy_in * thick_q8 / len_in);
                                ny_q8 = (int32_t)((int64_t)dx_in * thick_q8 / len_in);
                            }
                        } else {
                            nx_q8 = (int32_t)(-(int64_t)dy_in * thick_q8 / len_in);
                            ny_q8 = (int32_t)((int64_t)dx_in * thick_q8 / len_in);
                        }
                    }
                }
            } else if (len_in > 0) {
                nx_q8 = (int32_t)(-(int64_t)dy_in * thick_q8 / len_in);
                ny_q8 = (int32_t)((int64_t)dx_in * thick_q8 / len_in);
            } else if (len_out > 0) {
                nx_q8 = (int32_t)(-(int64_t)dy_out * thick_q8 / len_out);
                ny_q8 = (int32_t)((int64_t)dx_out * thick_q8 / len_out);
            }
        }

        out_x_q8 = px_q8[i] + nx_q8;
        out_y_q8 = py_q8[i] + ny_q8;
        in_x_q8 = px_q8[i] - nx_q8;
        in_y_q8 = py_q8[i] - ny_q8;
        mesh_pts[i].x_q8 = out_x_q8;
        mesh_pts[i].y_q8 = out_y_q8;
        mesh_pts[total_segs + 1 + i].x_q8 = in_x_q8;
        mesh_pts[total_segs + 1 + i].y_q8 = in_y_q8;

        if (out_x_q8 < min_x_q8) { min_x_q8 = out_x_q8; }
        if (out_y_q8 < min_y_q8) { min_y_q8 = out_y_q8; }
        if (in_x_q8 < min_x_q8) { min_x_q8 = in_x_q8; }
        if (in_y_q8 < min_y_q8) { min_y_q8 = in_y_q8; }
    }

    for (i = 0; i < (total_segs + 1) * 2; i++) {
        mesh_pts[i].x_q8 -= min_x_q8;
        mesh_pts[i].y_q8 -= min_y_q8;
    }

    ret = gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                        gfx_face_emote_q8_floor_to_int(min_x_q8),
                        gfx_face_emote_q8_floor_to_int(min_y_q8));
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "stroke align failed");
        goto stroke_cleanup;
    }

    ret = gfx_mesh_img_set_points_q8(obj, mesh_pts, (total_segs + 1) * 2);

stroke_cleanup:
#if GFX_HAVE_NANOVG
    if (vg != NULL) {
        nvgDeleteInternal(vg);
    }
#endif
    return ret;
}

esp_err_t gfx_face_emote_apply_bezier_fill(gfx_obj_t *obj,
                                           const int16_t *pts,
                                           int32_t cx,
                                           int32_t cy,
                                           int32_t scale_percent,
                                           bool flip_x,
                                           int32_t segs)
{
    int32_t px0_q8[GFX_FACE_EMOTE_MAX_FILL_POINTS];
    int32_t py0_q8[GFX_FACE_EMOTE_MAX_FILL_POINTS];
    int32_t px1_q8[GFX_FACE_EMOTE_MAX_FILL_POINTS];
    int32_t py1_q8[GFX_FACE_EMOTE_MAX_FILL_POINTS];
    gfx_mesh_img_point_q8_t mesh_pts[GFX_FACE_EMOTE_MAX_FILL_MESH_Q8];
    int32_t min_x_q8;
    int32_t min_y_q8;
    esp_err_t ret;
    int i;
#if GFX_HAVE_NANOVG
    gfx_face_emote_nanovg_capture_t nanovg_cap;
    NVGcontext *vg = NULL;
#endif

    if (pts == NULL || segs <= 0 || segs > GFX_FACE_EMOTE_MAX_FILL_SEGS) {
        return ESP_ERR_INVALID_ARG;
    }

#if GFX_HAVE_NANOVG
    vg = gfx_face_emote_nanovg_create_context(&nanovg_cap);
#endif

    {
        int32_t p0x = gfx_face_emote_transform_x(pts[0], cx, scale_percent, flip_x);
        int32_t p0y = gfx_face_emote_transform_y(pts[1], cy, scale_percent);
        int32_t p1x = gfx_face_emote_transform_x(pts[2], cx, scale_percent, flip_x);
        int32_t p1y = gfx_face_emote_transform_y(pts[3], cy, scale_percent);
        int32_t p2x = gfx_face_emote_transform_x(pts[4], cx, scale_percent, flip_x);
        int32_t p2y = gfx_face_emote_transform_y(pts[5], cy, scale_percent);
        int32_t p3x = gfx_face_emote_transform_x(pts[6], cx, scale_percent, flip_x);
        int32_t p3y = gfx_face_emote_transform_y(pts[7], cy, scale_percent);

        gfx_face_emote_sample_cubic_best_q8(
#if GFX_HAVE_NANOVG
                                           vg, &nanovg_cap,
#endif
                                           p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                                           segs, false, px0_q8, py0_q8);
    }

    {
        int32_t p0x = gfx_face_emote_transform_x(pts[6], cx, scale_percent, flip_x);
        int32_t p0y = gfx_face_emote_transform_y(pts[7], cy, scale_percent);
        int32_t p1x = gfx_face_emote_transform_x(pts[8], cx, scale_percent, flip_x);
        int32_t p1y = gfx_face_emote_transform_y(pts[9], cy, scale_percent);
        int32_t p2x = gfx_face_emote_transform_x(pts[10], cx, scale_percent, flip_x);
        int32_t p2y = gfx_face_emote_transform_y(pts[11], cy, scale_percent);
        int32_t p3x = gfx_face_emote_transform_x(pts[12], cx, scale_percent, flip_x);
        int32_t p3y = gfx_face_emote_transform_y(pts[13], cy, scale_percent);

        gfx_face_emote_sample_cubic_best_q8(
#if GFX_HAVE_NANOVG
                                           vg, &nanovg_cap,
#endif
                                           p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y,
                                           segs, true, px1_q8, py1_q8);
    }

    min_x_q8 = px0_q8[0];
    min_y_q8 = py0_q8[0];
    for (i = 0; i <= segs; i++) {
        if (px0_q8[i] < min_x_q8) { min_x_q8 = px0_q8[i]; }
        if (py0_q8[i] < min_y_q8) { min_y_q8 = py0_q8[i]; }
        if (px1_q8[i] < min_x_q8) { min_x_q8 = px1_q8[i]; }
        if (py1_q8[i] < min_y_q8) { min_y_q8 = py1_q8[i]; }
        mesh_pts[i].x_q8 = px0_q8[i];
        mesh_pts[i].y_q8 = py0_q8[i];
        mesh_pts[segs + 1 + i].x_q8 = px1_q8[i];
        mesh_pts[segs + 1 + i].y_q8 = py1_q8[i];
    }

    for (i = 0; i < (segs + 1) * 2; i++) {
        mesh_pts[i].x_q8 -= min_x_q8;
        mesh_pts[i].y_q8 -= min_y_q8;
    }

    ret = gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                        gfx_face_emote_q8_floor_to_int(min_x_q8),
                        gfx_face_emote_q8_floor_to_int(min_y_q8));
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "fill align failed");
        goto fill_cleanup;
    }

    ret = gfx_mesh_img_set_points_q8(obj, mesh_pts, (segs + 1) * 2);

fill_cleanup:
#if GFX_HAVE_NANOVG
    if (vg != NULL) {
        nvgDeleteInternal(vg);
    }
#endif
    return ret;
}
