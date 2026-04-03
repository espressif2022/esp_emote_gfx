/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/object/gfx_obj_priv.h"
#include "widget/dragon/gfx_dragon_emote_priv.h"

static const char *TAG = "dragon_pose";

#define PI 3.14159265f
#define DEG_TO_RAD(d) ((d) * PI / 180.0f)
#define DRAGON_VIEWBOX_X   120.0f
#define DRAGON_VIEWBOX_Y   150.0f
#define DRAGON_VIEWBOX_W  1000.0f
#define DRAGON_VIEWBOX_H   980.0f
#define DRAGON_VIEWBOX_CX (DRAGON_VIEWBOX_X + DRAGON_VIEWBOX_W * 0.5f)
#define DRAGON_VIEWBOX_CY (DRAGON_VIEWBOX_Y + DRAGON_VIEWBOX_H * 0.5f)

static const gfx_mesh_img_point_t s_dragon_tail_stripe1_pts[] = {
    { 60, -9 }, { 62, -8 }, { 63, -7 }, { 64, -5 }, { 64, 5 }, { 63, 7 }, { 62, 8 }, { 60, 9 },
    { -60, 9 }, { -62, 8 }, { -63, 7 }, { -64, 5 }, { -64, -5 }, { -63, -7 }, { -62, -8 }, { -60, -9 },
    { 60, -9 }, { 60, -9 }
};
static const gfx_mesh_img_point_t s_dragon_tail_stripe2_pts[] = {
    { 62, -9 }, { 64, -8 }, { 65, -7 }, { 66, -5 }, { 66, 5 }, { 65, 7 }, { 64, 8 }, { 62, 9 },
    { -62, 9 }, { -64, 8 }, { -65, 7 }, { -66, 5 }, { -66, -5 }, { -65, -7 }, { -64, -8 }, { -62, -9 },
    { 62, -9 }, { 62, -9 }
};
static const gfx_mesh_img_point_t s_dragon_eye_line_pts[] = {
    { -21, 18 }, { -20, 12 }, { -15, 2 }, { -10, -3 }, { -4, -5 }, { 3, -5 }, { 9, -3 }, { 15, 3 },
    { 20, 12 }, { 21, 18 }, { 41, 12 }, { 38, 4 }, { 31, -9 }, { 21, -19 }, { 7, -25 }, { -6, -25 },
    { -20, -21 }, { -31, -10 }, { -38, 4 }, { -41, 12 }, { -21, 18 }, { -21, 18 }
};
static const gfx_mesh_img_point_t s_dragon_antenna_l_pts[] = {
    { 8, 97 }, { -3, 75 }, { -14, 34 }, { -13, 1 }, { -5, -27 }, { 7, -50 }, { 22, -67 }, { 35, -79 },
    { 43, -85 }, { 44, -85 }, { 27, -110 }, { 25, -109 }, { 16, -102 }, { 0, -88 }, { -17, -66 }, { -33, -38 },
    { -43, -3 }, { -44, 39 }, { -31, 85 }, { -18, 110 }, { 8, 97 }, { 8, 97 }
};
static const gfx_mesh_img_point_t s_dragon_antenna_r_pts[] = {
    { 42, -111 }, { 29, -111 }, { 18, -105 }, { 3, -92 }, { -15, -72 }, { -31, -44 }, { -41, -8 }, { -42, 35 },
    { -30, 83 }, { -18, 111 }, { 9, 99 }, { -1, 75 }, { -12, 31 }, { -11, -4 }, { -3, -33 }, { 10, -55 },
    { 24, -71 }, { 36, -80 }, { 41, -83 }, { 30, -83 }, { 42, -111 }, { 42, -111 }
};
static const gfx_mesh_img_point_t s_dragon_dot_circle_pts[] = {
    { 27, 0 }, { 25, 10 }, { 19, 19 }, { 10, 25 }, { 0, 27 }, { -10, 25 }, { -19, 19 }, { -25, 10 },
    { -27, 0 }, { -25, -10 }, { -19, -19 }, { -10, -25 }, { 0, -27 }, { 10, -25 }, { 19, -19 }, { 25, -10 },
    { 27, 0 }, { 27, 0 }
};

static inline int16_t gfx_dragon_emote_ease(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    if (diff == 0) return cur;
    int32_t step = diff / div;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    return (int16_t)((int32_t)cur + step);
}

static esp_err_t apply_scaled_transform(gfx_obj_t *part_obj,
                                        gfx_obj_t *root_obj,
                                        const gfx_mesh_img_point_t *base_pts,
                                        size_t pt_count,
                                        int32_t ax, int32_t ay,
                                        float sc, int32_t ox, int32_t oy,
                                        const gfx_dragon_transform_t *t)
{
    if (part_obj == NULL || root_obj == NULL || base_pts == NULL || pt_count == 0 || t == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_mesh_img_point_t transformed[64];
    float abs_x[64];
    float abs_y[64];
    float min_x;
    float min_y;
    if (pt_count > 64) pt_count = 64;

    float angle = DEG_TO_RAD((float)t->r);
    float s = ((float)t->s / 100.0f) * sc;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    // Convert from dragon_animator.html viewBox coordinates into widget-local coordinates.
    float sax = ((float)ax - DRAGON_VIEWBOX_CX) * sc + (float)ox;
    float say = ((float)ay - DRAGON_VIEWBOX_CY) * sc + (float)oy;

    for (size_t i = 0; i < pt_count; i++) {
        float px = (float)base_pts[i].x;
        float py = (float)base_pts[i].y;

        // Rotate
        float rx = px * cos_a - py * sin_a;
        float ry = px * sin_a + py * cos_a;

        abs_x[i] = rx * s + (float)t->x * sc + sax;
        abs_y[i] = ry * s + (float)t->y * sc + say;
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
                        TAG, "align part failed");

    for (size_t i = 0; i < pt_count; i++) {
        transformed[i].x = (int16_t)(abs_x[i] - min_x);
        transformed[i].y = (int16_t)(abs_y[i] - min_y);
    }

    return gfx_mesh_img_set_points(part_obj, transformed, pt_count);
}

static void gfx_dragon_emote_update_part(gfx_dragon_part_state_t *ps, int16_t div)
{
    ps->cur.x = gfx_dragon_emote_ease(ps->cur.x, ps->tgt.x, div);
    ps->cur.y = gfx_dragon_emote_ease(ps->cur.y, ps->tgt.y, div);
    ps->cur.r = gfx_dragon_emote_ease(ps->cur.r, ps->tgt.r, div);
    ps->cur.s = gfx_dragon_emote_ease(ps->cur.s, ps->tgt.s, div);
}

esp_err_t gfx_dragon_emote_update_pose(gfx_obj_t *obj, gfx_dragon_emote_t *dragon)
{
    if (dragon == NULL || dragon->assets == NULL) return ESP_OK;

    int16_t div = dragon->cfg.damping_div;
    if (div < 1) div = 1;

    gfx_dragon_emote_update_part(&dragon->head, div);
    gfx_dragon_emote_update_part(&dragon->body, div);
    gfx_dragon_emote_update_part(&dragon->tail, div);
    gfx_dragon_emote_update_part(&dragon->clawL, div);
    gfx_dragon_emote_update_part(&dragon->clawR, div);
    gfx_dragon_emote_update_part(&dragon->antenna, div);
    gfx_dragon_emote_update_part(&dragon->eyeL, div);
    gfx_dragon_emote_update_part(&dragon->eyeR, div);
    gfx_dragon_emote_update_part(&dragon->dots, div);

    // Get widget-local resolution
    gfx_obj_calc_pos_in_parent(obj);
    int32_t ww = dragon->cfg.display_w;
    int32_t wh = dragon->cfg.display_h;
    const gfx_dragon_emote_assets_t *ass = dragon->assets;

    // Match dragon_animator.html viewBox: 1000x980 content inside 120,150 origin.
    float sc = (float)ww / DRAGON_VIEWBOX_W;
    if ((float)wh / DRAGON_VIEWBOX_H < sc) sc = (float)wh / DRAGON_VIEWBOX_H;

    // Widget-local center, then each part aligns to the dragon root object.
    int32_t ox = (int32_t)(ww / 2);
    int32_t oy = (int32_t)(wh / 2);

    apply_scaled_transform(dragon->clawL_obj, obj,        ass->pts_clawL,          ass->count_clawL,   283, 731, sc, ox, oy, &dragon->clawL.cur);
    apply_scaled_transform(dragon->body_obj, obj,         ass->pts_body,           ass->count_body,    860, 424, sc, ox, oy, &dragon->body.cur);
    apply_scaled_transform(dragon->tail_obj, obj,         ass->pts_tail,           ass->count_tail,    980, 312, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->tail_stripe1_obj, obj, s_dragon_tail_stripe1_pts, sizeof(s_dragon_tail_stripe1_pts) / sizeof(s_dragon_tail_stripe1_pts[0]),
                           955, 230, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->tail_stripe2_obj, obj, s_dragon_tail_stripe2_pts, sizeof(s_dragon_tail_stripe2_pts) / sizeof(s_dragon_tail_stripe2_pts[0]),
                           1024, 296, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->antennaL_obj, obj,     s_dragon_antenna_l_pts,  sizeof(s_dragon_antenna_l_pts) / sizeof(s_dragon_antenna_l_pts[0]),
                           526, 353, sc, ox, oy, &dragon->antenna.cur);
    apply_scaled_transform(dragon->antennaR_obj, obj,     s_dragon_antenna_r_pts,  sizeof(s_dragon_antenna_r_pts) / sizeof(s_dragon_antenna_r_pts[0]),
                           673, 355, sc, ox, oy, &dragon->antenna.cur);
    apply_scaled_transform(dragon->head_obj, obj,         ass->pts_head,           ass->count_head,    633, 633, sc, ox, oy, &dragon->head.cur);
    apply_scaled_transform(dragon->eyeL_obj, obj,         ass->pts_eyeL,           ass->count_eyeL,    478, 652, sc, ox, oy, &dragon->eyeL.cur);
    apply_scaled_transform(dragon->eyeR_obj, obj,         ass->pts_eyeR,           ass->count_eyeR,    633, 652, sc, ox, oy, &dragon->eyeR.cur);
    apply_scaled_transform(dragon->eyeLineL_obj, obj,     s_dragon_eye_line_pts,   sizeof(s_dragon_eye_line_pts) / sizeof(s_dragon_eye_line_pts[0]),
                           477, 650, sc, ox, oy, &dragon->eyeL.cur);
    apply_scaled_transform(dragon->eyeLineR_obj, obj,     s_dragon_eye_line_pts,   sizeof(s_dragon_eye_line_pts) / sizeof(s_dragon_eye_line_pts[0]),
                           633, 650, sc, ox, oy, &dragon->eyeR.cur);
    apply_scaled_transform(dragon->clawR_obj, obj,        ass->pts_clawR,          ass->count_clawR,   703, 974, sc, ox, oy, &dragon->clawR.cur);
    apply_scaled_transform(dragon->dot1_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           872, 846, sc, ox, oy, &dragon->dots.cur);
    apply_scaled_transform(dragon->dot2_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           915, 784, sc, ox, oy, &dragon->dots.cur);
    apply_scaled_transform(dragon->dot3_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           940, 714, sc, ox, oy, &dragon->dots.cur);

    return ESP_OK;
}
