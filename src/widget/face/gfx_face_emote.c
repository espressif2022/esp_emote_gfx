/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "common/gfx_mesh_frac.h"
#include "core/display/gfx_disp_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/gfx_timer.h"
#include "widget/gfx_face_emote.h"
#include "widget/gfx_img.h"

#define CHECK_OBJ_TYPE_FACE_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_FACE_EMOTE, TAG)
#define GFX_FACE_EMOTE_REF_COUNT 6U

/** Point counts (must match gfx_face_emote_shape*_t). */
#define GFX_FACE_EMOTE_SHAPE14_NUM_PTS 14
#define GFX_FACE_EMOTE_SHAPE8_NUM_PTS  8

/** Blend: mix weights are 0..100 (%). */
#define GFX_FACE_EMOTE_BLEND_WEIGHT_DIV 100

/** scale_percent field: 100 == 1.0× design units to pixels. */
#define GFX_FACE_EMOTE_SCALE_PERCENT_DIV 100

/** One pixel in mesh subpixel units (Kconfig: Q4 or Q8). */
#define GFX_FACE_EMOTE_Q8_ONE GFX_MESH_FRAC_ONE

/** Bezier parameter t in [0, 1000] (milli). */
#define GFX_FACE_EMOTE_BEZIER_T_DEN 1000

/** Cubic segment control block stride in pts[] (two cubics share endpoint on closed path). */
#define GFX_FACE_EMOTE_BEZIER_STRIDE 6

/** Layout numerators vs layout_ref_* (face_expressions_vivid proportions). */
#define GFX_FACE_EMOTE_LAYOUT_MOUTH_Y_NUM        30
#define GFX_FACE_EMOTE_LAYOUT_EYE_HALF_GAP_NUM   40
#define GFX_FACE_EMOTE_LAYOUT_EYE_Y_NUM          70
#define GFX_FACE_EMOTE_LAYOUT_BROW_Y_EXTRA_NUM   16
#define GFX_FACE_EMOTE_LAYOUT_SCALE_NUM          160
#define GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM     3
#define GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM    4

#define GFX_FACE_EMOTE_DEFAULT_TIMER_PERIOD_MS 33U
#define GFX_FACE_EMOTE_DEFAULT_EYE_SEGS          32U
#define GFX_FACE_EMOTE_DEFAULT_BROW_SEGS         48U
#define GFX_FACE_EMOTE_DEFAULT_MOUTH_SEGS        48U

/** Mouth anchor follows gaze (HTML: 0.6×). */
#define GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM 6
#define GFX_FACE_EMOTE_MOUTH_LOOK_DEN   10

/** Spring interpolation step: delta / N. */
#define GFX_FACE_EMOTE_EASE_SPRING_DIV 4

/** gfx_face_emote_create: fallback disp size when resolution is 0. */
#define GFX_FACE_EMOTE_CREATE_FALLBACK_RES 360U

/** Bezier mesh upper bounds (stroke: two cubics; fill: two cubics on closed shape). */
#define GFX_FACE_EMOTE_MAX_STROKE_SEGS     128
#define GFX_FACE_EMOTE_MAX_STROKE_POINTS   (GFX_FACE_EMOTE_MAX_STROKE_SEGS + 1)
#define GFX_FACE_EMOTE_MAX_STROKE_MESH_Q8  (GFX_FACE_EMOTE_MAX_STROKE_POINTS * 2)

#define GFX_FACE_EMOTE_MAX_FILL_SEGS     64
#define GFX_FACE_EMOTE_MAX_FILL_POINTS   (GFX_FACE_EMOTE_MAX_FILL_SEGS + 1)
#define GFX_FACE_EMOTE_MAX_FILL_MESH_Q8  (GFX_FACE_EMOTE_MAX_FILL_POINTS * 2)


typedef struct {
    void *white_src;
    gfx_obj_t *mouth_obj;
    gfx_obj_t *left_eye_obj;
    gfx_obj_t *right_eye_obj;
    gfx_obj_t *left_brow_obj;
    gfx_obj_t *right_brow_obj;
    gfx_timer_handle_t anim_timer;
    gfx_face_emote_cfg_t cfg;
    const gfx_face_emote_assets_t *assets;
    gfx_face_emote_shape14_t eye_cur;
    gfx_face_emote_shape14_t eye_tgt;
    gfx_face_emote_shape8_t brow_cur;
    gfx_face_emote_shape8_t brow_tgt;
    gfx_face_emote_shape14_t mouth_cur;
    gfx_face_emote_shape14_t mouth_tgt;
    int16_t look_x_cur;
    int16_t look_x_tgt;
    int16_t look_y_cur;
    int16_t look_y_tgt;
    bool manual_look_enabled;
    uint32_t anim_tick;
    size_t expr_idx;
} gfx_face_emote_t;

static const char *TAG = "face_emote";

static const uint16_t s_white_pixel = 0xFFFF;
static const gfx_image_dsc_t s_white_img = {
    .header = {
        .magic = 0x19,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = 2,
    .data = (const uint8_t *)&s_white_pixel,
};

static esp_err_t gfx_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_face_emote_delete_impl(gfx_obj_t *obj);
static esp_err_t gfx_face_emote_update_impl(gfx_obj_t *obj);
static void gfx_face_emote_anim_cb(void *user_data);

static const gfx_widget_class_t s_gfx_face_emote_widget_class = {
    .type = GFX_OBJ_TYPE_FACE_EMOTE,
    .name = "face_emote",
    .draw = gfx_face_emote_draw,
    .delete = gfx_face_emote_delete_impl,
    .update = gfx_face_emote_update_impl,
    .touch_event = NULL,
};

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

static inline int16_t gfx_face_emote_ease_spring(int16_t cur, int16_t tgt)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    step = diff / GFX_FACE_EMOTE_EASE_SPRING_DIV;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)((int32_t)cur + step);
}

static bool gfx_face_emote_assets_valid(const gfx_face_emote_assets_t *assets)
{
    if (assets == NULL) {
        return false;
    }
    if (assets->ref_eye == NULL || assets->ref_brow == NULL || assets->ref_mouth == NULL) {
        return false;
    }
    if (assets->ref_eye_count < GFX_FACE_EMOTE_REF_COUNT ||
            assets->ref_brow_count < GFX_FACE_EMOTE_REF_COUNT ||
            assets->ref_mouth_count < GFX_FACE_EMOTE_REF_COUNT) {
        return false;
    }
    return true;
}

static void gfx_face_emote_blend_mix(const gfx_face_emote_assets_t *assets,
                                         const gfx_face_emote_mix_t *mix,
                                         gfx_face_emote_shape14_t *eye_out,
                                         gfx_face_emote_shape8_t *brow_out,
                                         gfx_face_emote_shape14_t *mouth_out,
                                         int16_t *look_x_out,
                                         int16_t *look_y_out)
{
    int i;

    if (look_x_out != NULL) {
        *look_x_out = mix->look_x;
    }
    if (look_y_out != NULL) {
        *look_y_out = mix->look_y;
    }

    for (i = 0; i < GFX_FACE_EMOTE_SHAPE14_NUM_PTS; i++) {
        eye_out->pts[i] = assets->ref_eye[0].pts[i]
                        + (assets->ref_eye[1].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_smile / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                        + (assets->ref_eye[2].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_happy / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                        + (assets->ref_eye[3].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_sad / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                        + (assets->ref_eye[4].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_surprise / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                        + (assets->ref_eye[5].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_angry / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV;

        mouth_out->pts[i] = assets->ref_mouth[0].pts[i]
                          + (assets->ref_mouth[1].pts[i] - assets->ref_mouth[0].pts[i]) * mix->w_smile / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_mouth[2].pts[i] - assets->ref_mouth[0].pts[i]) * mix->w_happy / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_mouth[3].pts[i] - assets->ref_mouth[0].pts[i]) * mix->w_sad / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_mouth[4].pts[i] - assets->ref_mouth[0].pts[i]) * mix->w_surprise / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_mouth[5].pts[i] - assets->ref_mouth[0].pts[i]) * mix->w_angry / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV;
    }

    for (i = 0; i < GFX_FACE_EMOTE_SHAPE8_NUM_PTS; i++) {
        brow_out->pts[i] = assets->ref_brow[0].pts[i]
                         + (assets->ref_brow[1].pts[i] - assets->ref_brow[0].pts[i]) * mix->w_smile / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_brow[2].pts[i] - assets->ref_brow[0].pts[i]) * mix->w_happy / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_brow[3].pts[i] - assets->ref_brow[0].pts[i]) * mix->w_sad / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_brow[4].pts[i] - assets->ref_brow[0].pts[i]) * mix->w_surprise / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_brow[5].pts[i] - assets->ref_brow[0].pts[i]) * mix->w_angry / GFX_FACE_EMOTE_BLEND_WEIGHT_DIV;
    }
}

static void gfx_face_emote_get_anchors(gfx_obj_t *obj, const gfx_face_emote_t *face,
                                       int32_t *mouth_cx, int32_t *mouth_cy,
                                       int32_t *left_eye_cx, int32_t *left_eye_cy,
                                       int32_t *right_eye_cx, int32_t *right_eye_cy,
                                       int32_t *left_brow_cx, int32_t *left_brow_cy,
                                       int32_t *right_brow_cx, int32_t *right_brow_cy)
{
    int32_t root_x;
    int32_t root_y;
    int32_t display_cx;
    int32_t display_cy;

    gfx_obj_calc_pos_in_parent(obj);
    root_x = obj->geometry.x;
    root_y = obj->geometry.y;
    display_cx = root_x + (face->cfg.display_w / 2);
    display_cy = root_y + (face->cfg.display_h / 2);

    *mouth_cx = display_cx + face->cfg.mouth_x_ofs;
    *mouth_cy = display_cy + face->cfg.mouth_y_ofs;
    *left_eye_cx = *mouth_cx - face->cfg.eye_x_half_gap;
    *left_eye_cy = *mouth_cy + face->cfg.eye_y_ofs;
    *right_eye_cx = *mouth_cx + face->cfg.eye_x_half_gap;
    *right_eye_cy = *left_eye_cy;
    *left_brow_cx = *left_eye_cx;
    *left_brow_cy = *left_eye_cy + face->cfg.brow_y_ofs_extra;
    *right_brow_cx = *right_eye_cx;
    *right_brow_cy = *left_brow_cy;
}

/**
 * Build a thick ribbon along one or two cubic Béziers (mouth: closed; brow: open).
 * @param thickness Full stroke width in pixels (cfg); inset applied for blend AA, see macro.
 */
static esp_err_t gfx_face_emote_apply_bezier_stroke(gfx_obj_t *obj,
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
    int c, i;

    if (total_segs > GFX_FACE_EMOTE_MAX_STROKE_SEGS) {
        return ESP_ERR_INVALID_ARG;
    }

    for (c = 0; c < (closed ? 2 : 1); c++) {
        int32_t p0x = cx + ((flip_x ? -1 : 1) * pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 0] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p0y = cy + (pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 1] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1x = cx + ((flip_x ? -1 : 1) * pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 2] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1y = cy + (pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 3] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2x = cx + ((flip_x ? -1 : 1) * pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 4] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2y = cy + (pts[c * GFX_FACE_EMOTE_BEZIER_STRIDE + 5] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p3x = (c == 1) ? (cx + ((flip_x ? -1 : 1) * pts[0] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV)
                               : (cx + ((flip_x ? -1 : 1) * pts[GFX_FACE_EMOTE_BEZIER_STRIDE] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV);
        int32_t p3y = (c == 1) ? (cy + (pts[1] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV)
                               : (cy + (pts[GFX_FACE_EMOTE_BEZIER_STRIDE + 1] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV);

        for (i = 0; i <= segs; i++) {
            int idx;
            int32_t t, mt;

            if (c == 1 && i == 0) {
                continue;
            }

            idx = c * segs + i;
            t = (i * GFX_FACE_EMOTE_BEZIER_T_DEN) / segs;
            mt = GFX_FACE_EMOTE_BEZIER_T_DEN - t;
            {
                int64_t t64 = t, mt64 = mt;
                int64_t bw0 = mt64 * mt64 * mt64;
                int64_t bw1 = 3 * t64 * mt64 * mt64;
                int64_t bw2 = 3 * t64 * t64 * mt64;
                int64_t bw3 = t64 * t64 * t64;
                int64_t den3 = (int64_t)GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN;
                px_q8[idx] = (int32_t)((p0x * bw0 + p1x * bw1 + p2x * bw2 + p3x * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
                py_q8[idx] = (int32_t)((p0y * bw0 + p1y * bw1 + p2y * bw2 + p3y * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
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
            } else if (len_in > 0) {
                nx_q8 = (int32_t)(-(int64_t)dy_in * thick_q8 / len_in);
                ny_q8 = (int32_t)( (int64_t)dx_in * thick_q8 / len_in);
            } else if (len_out > 0) {
                nx_q8 = (int32_t)(-(int64_t)dy_out * thick_q8 / len_out);
                ny_q8 = (int32_t)( (int64_t)dx_out * thick_q8 / len_out);
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

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      gfx_face_emote_q8_floor_to_int(min_x_q8),
                                      gfx_face_emote_q8_floor_to_int(min_y_q8)),
                        TAG, "stroke align failed");
    return gfx_mesh_img_set_points_q8(obj, mesh_pts, (total_segs + 1) * 2);
}

/**
 * @brief Tessellate a closed eye/mouth-like region from two cubic Bézier curves and upload a quad mesh.
 *
 * `pts` is 14 int16 values: first cubic uses pts[0..7] (four (x,y) control points), second uses pts[6..13],
 * sharing the edge at pts[6], pts[7]. Design coordinates are scaled by scale_percent/100 about (cx, cy).
 *
 * @param obj            Target mesh image object (white fill).
 * @param pts            14 control values in design space (two C+C cubics closing the shape).
 * @param cx             Anchor X in parent/display pixels.
 * @param cy             Anchor Y in parent/display pixels.
 * @param scale_percent  Percent scale from design units to pixels (100 = 1:1).
 * @param flip_x         If true, negate local X (mirror for the right eye).
 * @param segs           Samples per cubic edge (must be <= GFX_FACE_EMOTE_MAX_FILL_SEGS).
 * @return ESP_OK on success, or an error from mesh/align APIs.
 */
static esp_err_t gfx_face_emote_apply_bezier_fill(gfx_obj_t *obj,
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
    int i;

    if (segs > GFX_FACE_EMOTE_MAX_FILL_SEGS) {
        return ESP_ERR_INVALID_ARG;
    }

    {
        int32_t p0x = cx + ((flip_x ? -1 : 1) * pts[0] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p0y = cy + (pts[1] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1x = cx + ((flip_x ? -1 : 1) * pts[2] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1y = cy + (pts[3] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2x = cx + ((flip_x ? -1 : 1) * pts[4] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2y = cy + (pts[5] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p3x = cx + ((flip_x ? -1 : 1) * pts[6] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p3y = cy + (pts[7] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;

        for (i = 0; i <= segs; i++) {
            int32_t t = (i * GFX_FACE_EMOTE_BEZIER_T_DEN) / segs;
            int32_t mt = GFX_FACE_EMOTE_BEZIER_T_DEN - t;
            int64_t t64 = t, mt64 = mt;
            int64_t bw0 = mt64 * mt64 * mt64;
            int64_t bw1 = 3 * t64 * mt64 * mt64;
            int64_t bw2 = 3 * t64 * t64 * mt64;
            int64_t bw3 = t64 * t64 * t64;
            int64_t den3 = (int64_t)GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN;
            px0_q8[i] = (int32_t)((p0x * bw0 + p1x * bw1 + p2x * bw2 + p3x * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
            py0_q8[i] = (int32_t)((p0y * bw0 + p1y * bw1 + p2y * bw2 + p3y * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
        }
    }

    {
        int32_t p0x = cx + ((flip_x ? -1 : 1) * pts[6] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p0y = cy + (pts[7] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1x = cx + ((flip_x ? -1 : 1) * pts[8] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p1y = cy + (pts[9] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2x = cx + ((flip_x ? -1 : 1) * pts[10] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p2y = cy + (pts[11] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p3x = cx + ((flip_x ? -1 : 1) * pts[12] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;
        int32_t p3y = cy + (pts[13] * scale_percent) / GFX_FACE_EMOTE_SCALE_PERCENT_DIV;

        for (i = 0; i <= segs; i++) {
            int32_t t = ((segs - i) * GFX_FACE_EMOTE_BEZIER_T_DEN) / segs;
            int32_t mt = GFX_FACE_EMOTE_BEZIER_T_DEN - t;
            int64_t t64 = t, mt64 = mt;
            int64_t bw0 = mt64 * mt64 * mt64;
            int64_t bw1 = 3 * t64 * mt64 * mt64;
            int64_t bw2 = 3 * t64 * t64 * mt64;
            int64_t bw3 = t64 * t64 * t64;
            int64_t den3 = (int64_t)GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN * GFX_FACE_EMOTE_BEZIER_T_DEN;
            px1_q8[i] = (int32_t)((p0x * bw0 + p1x * bw1 + p2x * bw2 + p3x * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
            py1_q8[i] = (int32_t)((p0y * bw0 + p1y * bw1 + p2y * bw2 + p3y * bw3) * GFX_FACE_EMOTE_Q8_ONE / den3);
        }
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

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      gfx_face_emote_q8_floor_to_int(min_x_q8),
                                      gfx_face_emote_q8_floor_to_int(min_y_q8)),
                        TAG, "fill align failed");
    return gfx_mesh_img_set_points_q8(obj, mesh_pts, (segs + 1) * 2);
}

static void gfx_face_emote_apply_pose(gfx_obj_t *obj, gfx_face_emote_t *face)
{
    int32_t mouth_cx;
    int32_t mouth_cy;
    int32_t left_eye_cx;
    int32_t left_eye_cy;
    int32_t right_eye_cx;
    int32_t right_eye_cy;
    int32_t left_brow_cx;
    int32_t left_brow_cy;
    int32_t right_brow_cx;
    int32_t right_brow_cy;
    int32_t lx;
    int32_t ly;
    int i;

    if (face == NULL || face->assets == NULL) {
        return;
    }

    for (i = 0; i < GFX_FACE_EMOTE_SHAPE14_NUM_PTS; i++) {
        face->eye_cur.pts[i] = gfx_face_emote_ease_spring(face->eye_cur.pts[i], face->eye_tgt.pts[i]);
        face->mouth_cur.pts[i] = gfx_face_emote_ease_spring(face->mouth_cur.pts[i], face->mouth_tgt.pts[i]);
    }
    for (i = 0; i < GFX_FACE_EMOTE_SHAPE8_NUM_PTS; i++) {
        face->brow_cur.pts[i] = gfx_face_emote_ease_spring(face->brow_cur.pts[i], face->brow_tgt.pts[i]);
    }

    face->look_x_cur = gfx_face_emote_ease_spring(face->look_x_cur, face->look_x_tgt);
    face->look_y_cur = gfx_face_emote_ease_spring(face->look_y_cur, face->look_y_tgt);
    lx = face->look_x_cur;
    ly = face->look_y_cur;

    gfx_face_emote_get_anchors(obj, face,
                               &mouth_cx, &mouth_cy,
                               &left_eye_cx, &left_eye_cy,
                               &right_eye_cx, &right_eye_cy,
                               &left_brow_cx, &left_brow_cy,
                               &right_brow_cx, &right_brow_cy);

    gfx_face_emote_apply_bezier_fill(face->left_eye_obj, face->eye_cur.pts,
                                     left_eye_cx + lx, left_eye_cy + ly,
                                     face->cfg.eye_scale_percent, false, face->cfg.eye_segs);
    gfx_face_emote_apply_bezier_fill(face->right_eye_obj, face->eye_cur.pts,
                                     right_eye_cx + lx, right_eye_cy + ly,
                                     face->cfg.eye_scale_percent, true, face->cfg.eye_segs);
    gfx_face_emote_apply_bezier_stroke(face->left_brow_obj, face->brow_cur.pts, false,
                                       left_brow_cx + lx, left_brow_cy + ly,
                                       face->cfg.brow_scale_percent, face->cfg.brow_thickness, false, face->cfg.brow_segs);
    gfx_face_emote_apply_bezier_stroke(face->right_brow_obj, face->brow_cur.pts, false,
                                       right_brow_cx + lx, right_brow_cy + ly,
                                       face->cfg.brow_scale_percent, face->cfg.brow_thickness, true, face->cfg.brow_segs);
    gfx_face_emote_apply_bezier_stroke(face->mouth_obj, face->mouth_cur.pts, true,
                                       mouth_cx + (lx * GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM / GFX_FACE_EMOTE_MOUTH_LOOK_DEN),
                                       mouth_cy + (ly * GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM / GFX_FACE_EMOTE_MOUTH_LOOK_DEN),
                                       face->cfg.mouth_scale_percent, face->cfg.mouth_thickness, false, face->cfg.mouth_segs);
}

static void gfx_face_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_face_emote_t *face;

    if (obj == NULL || obj->src == NULL) {
        return;
    }

    face = (gfx_face_emote_t *)obj->src;
    if (face->assets == NULL) {
        return;
    }

    face->anim_tick++;
    gfx_face_emote_apply_pose(obj, face);
}

static esp_err_t gfx_face_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    (void)obj;
    (void)ctx;
    return ESP_OK;
}

static esp_err_t gfx_face_emote_update_impl(gfx_obj_t *obj)
{
    (void)obj;
    return ESP_OK;
}

static esp_err_t gfx_face_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    if (face != NULL) {
        if (face->anim_timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, face->anim_timer);
        }
        if (face->right_brow_obj != NULL) {
            gfx_obj_delete(face->right_brow_obj);
        }
        if (face->left_brow_obj != NULL) {
            gfx_obj_delete(face->left_brow_obj);
        }
        if (face->right_eye_obj != NULL) {
            gfx_obj_delete(face->right_eye_obj);
        }
        if (face->left_eye_obj != NULL) {
            gfx_obj_delete(face->left_eye_obj);
        }
        if (face->mouth_obj != NULL) {
            gfx_obj_delete(face->mouth_obj);
        }
        free(face);
        obj->src = NULL;
    }
    return ESP_OK;
}

void gfx_face_emote_cfg_init(gfx_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h,
                             uint16_t layout_ref_x, uint16_t layout_ref_y)
{
    int32_t rw;
    int32_t rh;
    int32_t refx;
    int32_t refy;
    int32_t sc;
    int32_t tb;
    int32_t tm;

    if (cfg == NULL) {
        return;
    }

    if (display_w == 0U) {
        display_w = 1U;
    }
    if (display_h == 0U) {
        display_h = 1U;
    }
    if (layout_ref_x == 0U || layout_ref_y == 0U) {
        memset(cfg, 0, sizeof(*cfg));
        cfg->display_w = display_w;
        cfg->display_h = display_h;
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->display_w = display_w;
    cfg->display_h = display_h;

    rw = (int32_t)display_w;
    rh = (int32_t)display_h;
    refx = (int32_t)layout_ref_x;
    refy = (int32_t)layout_ref_y;

    /* Horizontal spacing vs ref_x; vertical vs ref_y (see face_expressions_vivid layout). */
    cfg->mouth_x_ofs = 0;
    cfg->mouth_y_ofs = (int16_t)((GFX_FACE_EMOTE_LAYOUT_MOUTH_Y_NUM * rh) / refy);
    cfg->eye_x_half_gap = (int16_t)((GFX_FACE_EMOTE_LAYOUT_EYE_HALF_GAP_NUM * rw) / refx);
    cfg->eye_y_ofs = (int16_t)((-(GFX_FACE_EMOTE_LAYOUT_EYE_Y_NUM) * rh) / refy);
    cfg->brow_y_ofs_extra = (int16_t)((-(GFX_FACE_EMOTE_LAYOUT_BROW_Y_EXTRA_NUM) * rh) / refy);

    sc = ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_SCALE_NUM * rh) / refy);
    if (sc < 1) {
        sc = 1;
    }
    cfg->eye_scale_percent = (int16_t)sc;
    cfg->brow_scale_percent = cfg->eye_scale_percent;
    cfg->mouth_scale_percent = cfg->eye_scale_percent;

    tb = ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM * rh) / refy);
    cfg->brow_thickness = (int16_t)((tb < 1) ? 1 : tb);
    tm = ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rw) / refx < (GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rh) / refy)
             ? ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rw) / refx)
             : ((GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM * rh) / refy);
    cfg->mouth_thickness = (int16_t)((tm < 1) ? 1 : tm);

    cfg->timer_period_ms = GFX_FACE_EMOTE_DEFAULT_TIMER_PERIOD_MS;
    cfg->eye_segs = GFX_FACE_EMOTE_DEFAULT_EYE_SEGS;
    cfg->brow_segs = GFX_FACE_EMOTE_DEFAULT_BROW_SEGS;
    cfg->mouth_segs = GFX_FACE_EMOTE_DEFAULT_MOUTH_SEGS;
}

gfx_obj_t *gfx_face_emote_create(gfx_disp_t *disp, uint16_t layout_ref_x, uint16_t layout_ref_y)
{
    gfx_obj_t *obj = NULL;
    gfx_face_emote_t *face = NULL;

    if (disp == NULL || layout_ref_x == 0U || layout_ref_y == 0U) {
        return NULL;
    }

    face = calloc(1, sizeof(*face));
    if (face == NULL) {
        return NULL;
    }

    {
        uint32_t dw = gfx_disp_get_hor_res(disp);
        uint32_t dh = gfx_disp_get_ver_res(disp);

        if (dw == 0U) {
            dw = GFX_FACE_EMOTE_CREATE_FALLBACK_RES;
        }
        if (dh == 0U) {
            dh = GFX_FACE_EMOTE_CREATE_FALLBACK_RES;
        }
        if (dw > 65535U) {
            dw = 65535U;
        }
        if (dh > 65535U) {
            dh = 65535U;
        }
        gfx_face_emote_cfg_init(&face->cfg, (uint16_t)dw, (uint16_t)dh, layout_ref_x, layout_ref_y);
    }
    face->white_src = (void *)&s_white_img;

    if (gfx_obj_create_class_instance(disp, &s_gfx_face_emote_widget_class,
                                      face, face->cfg.display_w, face->cfg.display_h,
                                      "gfx_face_emote_create", &obj) != ESP_OK) {
        free(face);
        return NULL;
    }

    face->mouth_obj = gfx_mesh_img_create(disp);
    face->left_eye_obj = gfx_mesh_img_create(disp);
    face->right_eye_obj = gfx_mesh_img_create(disp);
    face->left_brow_obj = gfx_mesh_img_create(disp);
    face->right_brow_obj = gfx_mesh_img_create(disp);
    if (face->mouth_obj == NULL || face->left_eye_obj == NULL || face->right_eye_obj == NULL ||
            face->left_brow_obj == NULL || face->right_brow_obj == NULL) {
        gfx_obj_delete(obj);
        return NULL;
    }

    gfx_mesh_img_set_src(face->mouth_obj, face->white_src);
    gfx_mesh_img_set_src(face->left_eye_obj, face->white_src);
    gfx_mesh_img_set_src(face->right_eye_obj, face->white_src);
    gfx_mesh_img_set_src(face->left_brow_obj, face->white_src);
    gfx_mesh_img_set_src(face->right_brow_obj, face->white_src);
    gfx_mesh_img_set_grid(face->mouth_obj, face->cfg.mouth_segs * 2U, 1U);
    gfx_mesh_img_set_grid(face->left_eye_obj, face->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(face->right_eye_obj, face->cfg.eye_segs, 1U);
    gfx_mesh_img_set_grid(face->left_brow_obj, face->cfg.brow_segs, 1U);
    gfx_mesh_img_set_grid(face->right_brow_obj, face->cfg.brow_segs, 1U);
    gfx_mesh_img_set_aa_inward(face->mouth_obj, true);
    gfx_mesh_img_set_aa_inward(face->left_brow_obj, true);
    gfx_mesh_img_set_aa_inward(face->right_brow_obj, true);
    gfx_mesh_img_set_wrap_cols(face->mouth_obj, true);

    {
        gfx_color_t white;
        white.full = 0xFFFF;
        gfx_mesh_img_set_scanline_fill(face->mouth_obj, true, white);
        gfx_mesh_img_set_scanline_fill(face->left_brow_obj, true, white);
        gfx_mesh_img_set_scanline_fill(face->right_brow_obj, true, white);
    }

    gfx_face_emote_set_config(obj, &face->cfg);
    face->anim_timer = gfx_timer_create(disp->ctx, gfx_face_emote_anim_cb, face->cfg.timer_period_ms, obj);
    return obj;
}

esp_err_t gfx_face_emote_set_config(gfx_obj_t *obj, const gfx_face_emote_cfg_t *cfg)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_STATE, TAG, "face state is NULL");

    face->cfg = *cfg;
    obj->geometry.width = cfg->display_w;
    obj->geometry.height = cfg->display_h;
    gfx_mesh_img_set_grid(face->mouth_obj, cfg->mouth_segs * 2U, 1U);
    gfx_mesh_img_set_grid(face->left_eye_obj, cfg->eye_segs, 1U);
    gfx_mesh_img_set_grid(face->right_eye_obj, cfg->eye_segs, 1U);
    gfx_mesh_img_set_grid(face->left_brow_obj, cfg->brow_segs, 1U);
    gfx_mesh_img_set_grid(face->right_brow_obj, cfg->brow_segs, 1U);
    if (face->anim_timer != NULL) {
        gfx_timer_set_period(face->anim_timer, cfg->timer_period_ms);
    }
    gfx_face_emote_apply_pose(obj, face);
    return ESP_OK;
}

esp_err_t gfx_face_emote_set_assets(gfx_obj_t *obj, const gfx_face_emote_assets_t *assets)
{
    gfx_face_emote_t *face;
    gfx_face_emote_mix_t mix = {0};

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(gfx_face_emote_assets_valid(assets), ESP_ERR_INVALID_ARG, TAG, "assets are invalid");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_STATE, TAG, "face state is NULL");

    face->assets = assets;
    face->expr_idx = 0U;
    if (assets->sequence != NULL && assets->sequence_count > 0U) {
        mix.w_smile = assets->sequence[0].w_smile;
        mix.w_happy = assets->sequence[0].w_happy;
        mix.w_sad = assets->sequence[0].w_sad;
        mix.w_surprise = assets->sequence[0].w_surprise;
        mix.w_angry = assets->sequence[0].w_angry;
        mix.look_x = assets->sequence[0].w_look_x;
        mix.look_y = assets->sequence[0].w_look_y;
    }
    gfx_face_emote_blend_mix(assets, &mix, &face->eye_cur, &face->brow_cur, &face->mouth_cur,
                             &face->look_x_cur, &face->look_y_cur);
    face->eye_tgt = face->eye_cur;
    face->brow_tgt = face->brow_cur;
    face->mouth_tgt = face->mouth_cur;
    face->look_x_tgt = face->look_x_cur;
    face->look_y_tgt = face->look_y_cur;
    face->manual_look_enabled = false;
    gfx_face_emote_apply_pose(obj, face);
    return ESP_OK;
}

esp_err_t gfx_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_face_emote_t *face;
    size_t index;
    gfx_face_emote_mix_t mix;
    gfx_face_emote_shape14_t eye_next;
    gfx_face_emote_shape8_t brow_next;
    gfx_face_emote_shape14_t mouth_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL && face->assets->sequence != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "face sequence assets are not ready");
    ESP_RETURN_ON_FALSE(name != NULL, ESP_ERR_INVALID_ARG, TAG, "name is NULL");

    for (index = 0; index < face->assets->sequence_count; index++) {
        if (face->assets->sequence[index].name != NULL &&
                strcmp(face->assets->sequence[index].name, name) == 0) {
            break;
        }
    }
    ESP_RETURN_ON_FALSE(index < face->assets->sequence_count, ESP_ERR_NOT_FOUND, TAG, "expression name not found");

    mix.w_smile = face->assets->sequence[index].w_smile;
    mix.w_happy = face->assets->sequence[index].w_happy;
    mix.w_sad = face->assets->sequence[index].w_sad;
    mix.w_surprise = face->assets->sequence[index].w_surprise;
    mix.w_angry = face->assets->sequence[index].w_angry;
    mix.look_x = face->assets->sequence[index].w_look_x;
    mix.look_y = face->assets->sequence[index].w_look_y;
    gfx_face_emote_blend_mix(face->assets, &mix, &eye_next, &brow_next, &mouth_next, &look_x_next, &look_y_next);
    face->expr_idx = index;

    if (snap_now) {
        face->eye_cur = eye_next;
        face->brow_cur = brow_next;
        face->mouth_cur = mouth_next;
        face->look_x_cur = look_x_next;
        face->look_y_cur = look_y_next;
    }

    face->eye_tgt = eye_next;
    face->brow_tgt = brow_next;
    face->mouth_tgt = mouth_next;
    if (!face->manual_look_enabled) {
        face->look_x_tgt = look_x_next;
        face->look_y_tgt = look_y_next;
    }
    gfx_face_emote_apply_pose(obj, face);
    return ESP_OK;
}

esp_err_t gfx_face_emote_set_mix(gfx_obj_t *obj, const gfx_face_emote_mix_t *mix, bool snap_now)
{
    gfx_face_emote_t *face;
    gfx_face_emote_shape14_t eye_next;
    gfx_face_emote_shape8_t brow_next;
    gfx_face_emote_shape14_t mouth_next;
    int16_t look_x_next;
    int16_t look_y_next;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    ESP_RETURN_ON_FALSE(mix != NULL, ESP_ERR_INVALID_ARG, TAG, "mix is NULL");

    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL, ESP_ERR_INVALID_STATE, TAG, "face assets are not ready");

    gfx_face_emote_blend_mix(face->assets, mix, &eye_next, &brow_next, &mouth_next, &look_x_next, &look_y_next);
    face->expr_idx = SIZE_MAX;

    if (snap_now) {
        face->eye_cur = eye_next;
        face->brow_cur = brow_next;
        face->mouth_cur = mouth_next;
        face->look_x_cur = look_x_next;
        face->look_y_cur = look_y_next;
    }

    face->eye_tgt = eye_next;
    face->brow_tgt = brow_next;
    face->mouth_tgt = mouth_next;
    if (!face->manual_look_enabled) {
        face->look_x_tgt = look_x_next;
        face->look_y_tgt = look_y_next;
    }
    gfx_face_emote_apply_pose(obj, face);
    return ESP_OK;
}

esp_err_t gfx_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y)
{
    gfx_face_emote_t *face;

    CHECK_OBJ_TYPE_FACE_EMOTE(obj);
    face = (gfx_face_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(face != NULL && face->assets != NULL, ESP_ERR_INVALID_STATE, TAG, "face assets are not ready");

    face->manual_look_enabled = enabled;
    if (enabled) {
        face->look_x_tgt = look_x;
        face->look_y_tgt = look_y;
    }
    return ESP_OK;
}
