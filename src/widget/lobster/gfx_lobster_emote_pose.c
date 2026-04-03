/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/object/gfx_obj_priv.h"
#include "widget/lobster/gfx_lobster_emote_priv.h"

static const char *TAG = "lobster_pose";

#define GFX_LOBSTER_PI 3.14159265f
#define GFX_LOBSTER_DEG_TO_RAD(d) ((d) * GFX_LOBSTER_PI / 180.0f)
#define GFX_LOBSTER_ART_MIN_X 186.0f
#define GFX_LOBSTER_ART_MAX_X 814.0f
#define GFX_LOBSTER_ART_MIN_Y 388.0f
#define GFX_LOBSTER_ART_MAX_Y 922.0f
#define GFX_LOBSTER_ART_CX ((GFX_LOBSTER_ART_MIN_X + GFX_LOBSTER_ART_MAX_X) * 0.5f)
#define GFX_LOBSTER_ART_CY ((GFX_LOBSTER_ART_MIN_Y + GFX_LOBSTER_ART_MAX_Y) * 0.5f)
#define GFX_LOBSTER_ART_W (GFX_LOBSTER_ART_MAX_X - GFX_LOBSTER_ART_MIN_X)
#define GFX_LOBSTER_ART_H (GFX_LOBSTER_ART_MAX_Y - GFX_LOBSTER_ART_MIN_Y)
#define GFX_LOBSTER_FIT_PADDING 0.88f

static inline int16_t gfx_lobster_emote_ease(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    if (div < 1) {
        div = 1;
    }

    step = diff / div;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(cur + step);
}

static esp_err_t gfx_lobster_emote_apply_scaled_transform(gfx_obj_t *part_obj,
                                                          gfx_obj_t *root_obj,
                                                          const gfx_mesh_img_point_t *base_pts,
                                                          size_t pt_count,
                                                          int32_t anchor_x,
                                                          int32_t anchor_y,
                                                          float sc,
                                                          int32_t ox,
                                                          int32_t oy,
                                                          const gfx_lobster_transform_t *t,
                                                          float sx,
                                                          float sy)
{
    gfx_mesh_img_point_t transformed[64];
    float abs_x[64];
    float abs_y[64];
    float angle;
    float scale;
    float cos_a;
    float sin_a;
    float sax;
    float say;
    float min_x;
    float min_y;

    ESP_RETURN_ON_FALSE(part_obj != NULL && root_obj != NULL && base_pts != NULL && t != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid transform inputs");

    if (pt_count > 64U) {
        pt_count = 64U;
    }
    if (pt_count == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    angle = GFX_LOBSTER_DEG_TO_RAD((float)t->r);
    scale = ((float)t->s / 100.0f) * sc;
    cos_a = cosf(angle);
    sin_a = sinf(angle);
    sax = ((float)anchor_x - GFX_LOBSTER_ART_CX) * sc + (float)ox;
    say = ((float)anchor_y - GFX_LOBSTER_ART_CY) * sc + (float)oy;

    for (size_t i = 0; i < pt_count; i++) {
        float px = (float)base_pts[i].x * sx;
        float py = (float)base_pts[i].y * sy;
        float rx = px * cos_a - py * sin_a;
        float ry = px * sin_a + py * cos_a;

        abs_x[i] = rx * scale + (float)t->x * sc + sax;
        abs_y[i] = ry * scale + (float)t->y * sc + say;
    }

    min_x = abs_x[0];
    min_y = abs_y[0];
    for (size_t i = 1; i < pt_count; i++) {
        if (abs_x[i] < min_x) {
            min_x = abs_x[i];
        }
        if (abs_y[i] < min_y) {
            min_y = abs_y[i];
        }
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align_to(part_obj, root_obj, GFX_ALIGN_TOP_LEFT,
                                         (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "align mesh part failed");

    for (size_t i = 0; i < pt_count; i++) {
        transformed[i].x = (int16_t)(abs_x[i] - min_x);
        transformed[i].y = (int16_t)(abs_y[i] - min_y);
    }

    return gfx_mesh_img_set_points(part_obj, transformed, pt_count);
}

static void gfx_lobster_emote_update_part(gfx_lobster_part_state_t *part, int16_t div)
{
    part->cur.x = gfx_lobster_emote_ease(part->cur.x, part->tgt.x, div);
    part->cur.y = gfx_lobster_emote_ease(part->cur.y, part->tgt.y, div);
    part->cur.r = gfx_lobster_emote_ease(part->cur.r, part->tgt.r, div);
    part->cur.s = gfx_lobster_emote_ease(part->cur.s, part->tgt.s, div);
}

esp_err_t gfx_lobster_emote_update_pose(gfx_obj_t *obj, gfx_lobster_emote_t *lobster)
{
    const gfx_lobster_emote_assets_t *assets;
    int16_t div;
    int16_t blink_drop = 0;
    int16_t eye_open_render;
    int16_t look_x_render;
    int16_t look_y_render;
    uint32_t blink_phase;
    int32_t ww;
    int32_t wh;
    int32_t ox;
    int32_t oy;
    float sc;
    float eye_open_scale_y;

    if (lobster == NULL || lobster->assets == NULL) {
        return ESP_OK;
    }

    assets = lobster->assets;
    div = lobster->cfg.damping_div;
    if (div < 1) {
        div = 1;
    }

    gfx_lobster_emote_update_part(&lobster->body, div);
    gfx_lobster_emote_update_part(&lobster->tail, div);
    gfx_lobster_emote_update_part(&lobster->claw_l, div);
    gfx_lobster_emote_update_part(&lobster->claw_r, div);
    lobster->eye_open_cur = gfx_lobster_emote_ease(lobster->eye_open_cur, lobster->eye_open_tgt, div);
    lobster->eye_look_x_cur = gfx_lobster_emote_ease(lobster->eye_look_x_cur, lobster->eye_look_x_tgt, div);
    lobster->eye_look_y_cur = gfx_lobster_emote_ease(lobster->eye_look_y_cur, lobster->eye_look_y_tgt, div);

    blink_phase = lobster->anim_tick % 90U;
    if (blink_phase < 3U) {
        blink_drop = (int16_t)(blink_phase * 22U);
    } else if (blink_phase < 6U) {
        blink_drop = (int16_t)((6U - blink_phase) * 22U);
    }

    eye_open_render = lobster->eye_open_cur - blink_drop;
    if (eye_open_render < 28) {
        eye_open_render = 28;
    }
    look_x_render = lobster->eye_look_x_cur;
    look_y_render = lobster->eye_look_y_cur;
    eye_open_scale_y = (float)eye_open_render / 100.0f;

    gfx_obj_calc_pos_in_parent(obj);
    ww = lobster->cfg.display_w;
    wh = lobster->cfg.display_h;
    sc = ((float)ww / GFX_LOBSTER_ART_W);
    if (((float)wh / GFX_LOBSTER_ART_H) < sc) {
        sc = ((float)wh / GFX_LOBSTER_ART_H);
    }
    sc *= GFX_LOBSTER_FIT_PADDING;
    ox = ww / 2;
    oy = wh / 2;

    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->tail_obj, obj,
                                                                 assets->pts_tail,
                                                                 assets->count_tail,
                                                                 500, 760, sc, ox, oy,
                                                                 &lobster->tail.cur, 1.0f, 1.0f),
                        TAG, "apply tail pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->body_obj, obj,
                                                                 assets->pts_body,
                                                                 assets->count_body,
                                                                 500, 560, sc, ox, oy,
                                                                 &lobster->body.cur, 1.0f, 1.0f),
                        TAG, "apply body pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->claw_l_obj, obj,
                                                                 assets->pts_claw_l,
                                                                 assets->count_claw_l,
                                                                 340, 620, sc, ox, oy,
                                                                 &lobster->claw_l.cur, 1.0f, 1.0f),
                        TAG, "apply left claw pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->claw_r_obj, obj,
                                                                 assets->pts_claw_r,
                                                                 assets->count_claw_r,
                                                                 660, 620, sc, ox, oy,
                                                                 &lobster->claw_r.cur, 1.0f, 1.0f),
                        TAG, "apply right claw pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->eye_white_l_obj, obj,
                                                                 assets->pts_eye_white,
                                                                 assets->count_eye_white,
                                                                 435, 430, sc, ox, oy,
                                                                 &lobster->body.cur, 1.0f, eye_open_scale_y),
                        TAG, "apply left eye pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->eye_white_r_obj, obj,
                                                                 assets->pts_eye_white,
                                                                 assets->count_eye_white,
                                                                 565, 430, sc, ox, oy,
                                                                 &lobster->body.cur, 1.0f, eye_open_scale_y),
                        TAG, "apply right eye pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->eye_pupil_l_obj, obj,
                                                                 assets->pts_eye_pupil,
                                                                 assets->count_eye_pupil,
                                                                 435 + look_x_render, 438 + look_y_render,
                                                                 sc, ox, oy, &lobster->body.cur,
                                                                 1.0f, eye_open_scale_y),
                        TAG, "apply left pupil pose failed");
    ESP_RETURN_ON_ERROR(gfx_lobster_emote_apply_scaled_transform(lobster->eye_pupil_r_obj, obj,
                                                                 assets->pts_eye_pupil,
                                                                 assets->count_eye_pupil,
                                                                 565 + look_x_render, 438 + look_y_render,
                                                                 sc, ox, oy, &lobster->body.cur,
                                                                 1.0f, eye_open_scale_y),
                        TAG, "apply right pupil pose failed");
    return ESP_OK;
}
