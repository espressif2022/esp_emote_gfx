/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>

#include "esp_check.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "widget/gfx_mesh_img.h"
#include "widget/face/gfx_face_emote_priv.h"

static const char *TAG = "face_emote";

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

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT,
                                      gfx_face_emote_q8_floor_to_int(min_x_q8),
                                      gfx_face_emote_q8_floor_to_int(min_y_q8)),
                        TAG, "stroke align failed");
    return gfx_mesh_img_set_points_q8(obj, mesh_pts, (total_segs + 1) * 2);
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
