/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/object/gfx_obj_priv.h"
#include "widget/lobster_face/gfx_lobster_face_emote_priv.h"

static const char *TAG = "lobster_face";

extern esp_err_t gfx_face_emote_apply_bezier_stroke(gfx_obj_t *obj, const int16_t *pts, bool closed,
                                                    int32_t cx, int32_t cy, int32_t scale_percent,
                                                    int32_t thickness, bool flip_x, int32_t segs);
extern esp_err_t gfx_face_emote_apply_bezier_fill(gfx_obj_t *obj, const int16_t *pts,
                                                  int32_t cx, int32_t cy, int32_t scale_percent,
                                                  bool flip_x, int32_t segs);

static const int16_t s_shell_body[14] = { -32, 0, -24, -32, 24, -32, 32, 0, 24, 32, -24, 32, -32, 0 };
static const int16_t s_shell_head[14] = { -24, 0, -18, -22, 18, -22, 24, 0, 18, 22, -18, 22, -24, 0 };
static const int16_t s_shell_tail[14] = { -18, 0, -14, -18, 14, -18, 18, 0, 14, 18, -14, 18, -18, 0 };
static const int16_t s_pupil[14] = { -6, 0, -4, -6, 4, -6, 6, 0, 4, 6, -4, 6, -6, 0 };

static inline int16_t gfx_lobster_face_emote_ease(int16_t cur, int16_t tgt)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    step = diff / GFX_LOBSTER_FACE_EMOTE_EASE_DIV;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(cur + step);
}

bool gfx_lobster_face_emote_assets_valid(const gfx_lobster_face_emote_assets_t *assets)
{
    return assets != NULL &&
           assets->ref_eye != NULL &&
           assets->ref_claw != NULL &&
           assets->ref_eye_count >= GFX_LOBSTER_FACE_EMOTE_REF_COUNT &&
           assets->ref_claw_count >= GFX_LOBSTER_FACE_EMOTE_REF_COUNT;
}

void gfx_lobster_face_emote_expr_to_mix(const gfx_lobster_face_emote_expr_t *expr, gfx_lobster_face_emote_mix_t *mix)
{
    if (mix == NULL) {
        return;
    }
    memset(mix, 0, sizeof(*mix));
    if (expr == NULL) {
        return;
    }
    mix->w_smile = expr->w_smile;
    mix->w_happy = expr->w_happy;
    mix->w_sad = expr->w_sad;
    mix->w_surprise = expr->w_surprise;
    mix->w_angry = expr->w_angry;
    mix->look_x = expr->w_look_x;
    mix->look_y = expr->w_look_y;
}

esp_err_t gfx_lobster_face_emote_find_expr_index(const gfx_lobster_face_emote_assets_t *assets, const char *name, size_t *index_out)
{
    ESP_RETURN_ON_FALSE(assets != NULL && assets->sequence != NULL, ESP_ERR_INVALID_STATE, TAG, "sequence assets not ready");
    ESP_RETURN_ON_FALSE(name != NULL && index_out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    for (size_t i = 0; i < assets->sequence_count; i++) {
        if (assets->sequence[i].name != NULL && strcmp(assets->sequence[i].name, name) == 0) {
            *index_out = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_lobster_face_emote_eval_mix(const gfx_lobster_face_emote_assets_t *assets,
                                          const gfx_lobster_face_emote_mix_t *mix,
                                          gfx_lobster_face_emote_eye_shape_t *eye_next,
                                          gfx_lobster_face_emote_claw_shape_t *claw_next,
                                          int16_t *look_x_next,
                                          int16_t *look_y_next)
{
    ESP_RETURN_ON_FALSE(gfx_lobster_face_emote_assets_valid(assets), ESP_ERR_INVALID_STATE, TAG, "assets invalid");
    ESP_RETURN_ON_FALSE(mix != NULL && eye_next != NULL && claw_next != NULL, ESP_ERR_INVALID_ARG, TAG, "mix out invalid");

    for (int i = 0; i < GFX_LOBSTER_FACE_EMOTE_SHAPE_NUM_PTS; i++) {
        eye_next->pts[i] = assets->ref_eye[0].pts[i]
                         + (assets->ref_eye[1].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_smile / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_eye[2].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_happy / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_eye[3].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_sad / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_eye[4].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_surprise / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                         + (assets->ref_eye[5].pts[i] - assets->ref_eye[0].pts[i]) * mix->w_angry / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV;

        claw_next->pts[i] = assets->ref_claw[0].pts[i]
                          + (assets->ref_claw[1].pts[i] - assets->ref_claw[0].pts[i]) * mix->w_smile / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_claw[2].pts[i] - assets->ref_claw[0].pts[i]) * mix->w_happy / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_claw[3].pts[i] - assets->ref_claw[0].pts[i]) * mix->w_sad / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_claw[4].pts[i] - assets->ref_claw[0].pts[i]) * mix->w_surprise / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV
                          + (assets->ref_claw[5].pts[i] - assets->ref_claw[0].pts[i]) * mix->w_angry / GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV;
    }

    if (look_x_next != NULL) {
        *look_x_next = mix->look_x;
    }
    if (look_y_next != NULL) {
        *look_y_next = mix->look_y;
    }
    return ESP_OK;
}

static void gfx_lobster_face_emote_get_anchors(gfx_obj_t *obj, const gfx_lobster_face_emote_t *lobster,
                                               int32_t *body_cx, int32_t *body_cy,
                                               int32_t *head_cx, int32_t *head_cy,
                                               int32_t *tail_cx, int32_t *tail_cy,
                                               int32_t *eye_l_cx, int32_t *eye_l_cy,
                                               int32_t *eye_r_cx, int32_t *eye_r_cy,
                                               int32_t *claw_l_cx, int32_t *claw_l_cy,
                                               int32_t *claw_r_cx, int32_t *claw_r_cy)
{
    int32_t root_x;
    int32_t root_y;
    int32_t cx;
    int32_t cy;

    gfx_obj_calc_pos_in_parent(obj);
    root_x = obj->geometry.x;
    root_y = obj->geometry.y;
    cx = root_x + (lobster->cfg.display_w / 2);
    cy = root_y + (lobster->cfg.display_h / 2);

    *body_cx = cx;
    *body_cy = cy + lobster->cfg.claw_y_ofs / 2;
    *head_cx = cx;
    *head_cy = cy + lobster->cfg.head_y_ofs;
    *tail_cx = cx + lobster->cfg.tail_x_ofs;
    *tail_cy = cy + lobster->cfg.tail_y_ofs;
    *eye_l_cx = cx - lobster->cfg.eye_x_half_gap;
    *eye_l_cy = cy + lobster->cfg.eye_y_ofs;
    *eye_r_cx = cx + lobster->cfg.eye_x_half_gap;
    *eye_r_cy = *eye_l_cy;
    *claw_l_cx = cx - lobster->cfg.claw_x_half_gap;
    *claw_l_cy = cy + lobster->cfg.claw_y_ofs;
    *claw_r_cx = cx + lobster->cfg.claw_x_half_gap;
    *claw_r_cy = *claw_l_cy;
}

esp_err_t gfx_lobster_face_emote_update_pose(gfx_obj_t *obj, gfx_lobster_face_emote_t *lobster)
{
    int32_t body_cx, body_cy, head_cx, head_cy, tail_cx, tail_cy;
    int32_t eye_l_cx, eye_l_cy, eye_r_cx, eye_r_cy, claw_l_cx, claw_l_cy, claw_r_cx, claw_r_cy;
    int32_t lx, ly;

    if (lobster == NULL || lobster->assets == NULL) {
        return ESP_OK;
    }

    for (int i = 0; i < GFX_LOBSTER_FACE_EMOTE_SHAPE_NUM_PTS; i++) {
        lobster->eye_current.pts[i] = gfx_lobster_face_emote_ease(lobster->eye_current.pts[i], lobster->eye_target.pts[i]);
        lobster->claw_current.pts[i] = gfx_lobster_face_emote_ease(lobster->claw_current.pts[i], lobster->claw_target.pts[i]);
    }
    lobster->look_x_current = gfx_lobster_face_emote_ease(lobster->look_x_current, lobster->look_x_target);
    lobster->look_y_current = gfx_lobster_face_emote_ease(lobster->look_y_current, lobster->look_y_target);
    lx = lobster->look_x_current;
    ly = lobster->look_y_current;

    gfx_lobster_face_emote_get_anchors(obj, lobster, &body_cx, &body_cy, &head_cx, &head_cy, &tail_cx, &tail_cy,
                                       &eye_l_cx, &eye_l_cy, &eye_r_cx, &eye_r_cy, &claw_l_cx, &claw_l_cy, &claw_r_cx, &claw_r_cy);

    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->tail_obj, s_shell_tail,
                                                         tail_cx, tail_cy, lobster->cfg.shell_scale_percent, false, lobster->cfg.shell_segs),
                        TAG, "apply tail failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->body_obj, s_shell_body,
                                                         body_cx, body_cy, lobster->cfg.shell_scale_percent + 10, false, lobster->cfg.shell_segs),
                        TAG, "apply body failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->head_obj, s_shell_head,
                                                         head_cx, head_cy, lobster->cfg.shell_scale_percent, false, lobster->cfg.shell_segs),
                        TAG, "apply head failed");

    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->eye_l_obj, lobster->eye_current.pts,
                                                         eye_l_cx + lx, eye_l_cy + ly, lobster->cfg.eye_scale_percent, false, lobster->cfg.eye_segs),
                        TAG, "apply left eye failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->eye_r_obj, lobster->eye_current.pts,
                                                         eye_r_cx + lx, eye_r_cy + ly, lobster->cfg.eye_scale_percent, true, lobster->cfg.eye_segs),
                        TAG, "apply right eye failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->pupil_l_obj, s_pupil,
                                                         eye_l_cx + lx, eye_l_cy + ly + (lobster->cfg.eye_scale_percent / 10),
                                                         lobster->cfg.eye_scale_percent, false, 10),
                        TAG, "apply left pupil failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->pupil_r_obj, s_pupil,
                                                         eye_r_cx + lx, eye_r_cy + ly + (lobster->cfg.eye_scale_percent / 10),
                                                         lobster->cfg.eye_scale_percent, true, 10),
                        TAG, "apply right pupil failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->claw_l_obj, lobster->claw_current.pts,
                                                         claw_l_cx, claw_l_cy, lobster->cfg.claw_scale_percent, false, lobster->cfg.claw_segs),
                        TAG, "apply left claw failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->claw_r_obj, lobster->claw_current.pts,
                                                         claw_r_cx, claw_r_cy, lobster->cfg.claw_scale_percent, true, lobster->cfg.claw_segs),
                        TAG, "apply right claw failed");
    return ESP_OK;
}

esp_err_t gfx_lobster_face_emote_set_target_pose(gfx_obj_t *obj,
                                                 gfx_lobster_face_emote_t *lobster,
                                                 const gfx_lobster_face_emote_eye_shape_t *eye_next,
                                                 const gfx_lobster_face_emote_claw_shape_t *claw_next,
                                                 int16_t look_x_next,
                                                 int16_t look_y_next,
                                                 bool snap_now)
{
    ESP_RETURN_ON_FALSE(lobster != NULL && eye_next != NULL && claw_next != NULL, ESP_ERR_INVALID_ARG, TAG, "target pose invalid");

    if (snap_now) {
        lobster->eye_current = *eye_next;
        lobster->claw_current = *claw_next;
        lobster->look_x_current = look_x_next;
        lobster->look_y_current = look_y_next;
    }
    lobster->eye_target = *eye_next;
    lobster->claw_target = *claw_next;
    if (!lobster->manual_look_enabled) {
        lobster->look_x_target = look_x_next;
        lobster->look_y_target = look_y_next;
    }
    return gfx_lobster_face_emote_update_pose(obj, lobster);
}
