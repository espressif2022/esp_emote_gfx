/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/object/gfx_obj_priv.h"
#include "widget/face/gfx_face_emote_priv.h"

static const char *TAG = "face_emote";

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

bool gfx_face_emote_assets_valid(const gfx_face_emote_assets_t *assets)
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
                                     gfx_face_emote_eye_shape_t *eye_out,
                                     gfx_face_emote_brow_shape_t *brow_out,
                                     gfx_face_emote_mouth_shape_t *mouth_out,
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

void gfx_face_emote_expr_to_mix(const gfx_face_emote_expr_t *expr, gfx_face_emote_mix_t *mix)
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

esp_err_t gfx_face_emote_find_expr_index(const gfx_face_emote_assets_t *assets,
                                         const char *name,
                                         size_t *index_out)
{
    size_t index;

    ESP_RETURN_ON_FALSE(assets != NULL && assets->sequence != NULL, ESP_ERR_INVALID_STATE, TAG, "face sequence assets are not ready");
    ESP_RETURN_ON_FALSE(name != NULL, ESP_ERR_INVALID_ARG, TAG, "name is NULL");
    ESP_RETURN_ON_FALSE(index_out != NULL, ESP_ERR_INVALID_ARG, TAG, "index out is NULL");

    for (index = 0; index < assets->sequence_count; index++) {
        if (assets->sequence[index].name != NULL &&
                strcmp(assets->sequence[index].name, name) == 0) {
            *index_out = index;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_face_emote_eval_mix(const gfx_face_emote_assets_t *assets,
                                  const gfx_face_emote_mix_t *mix,
                                  gfx_face_emote_eye_shape_t *eye_next,
                                  gfx_face_emote_brow_shape_t *brow_next,
                                  gfx_face_emote_mouth_shape_t *mouth_next,
                                  int16_t *look_x_next,
                                  int16_t *look_y_next)
{
    ESP_RETURN_ON_FALSE(assets != NULL, ESP_ERR_INVALID_STATE, TAG, "face assets are not ready");
    ESP_RETURN_ON_FALSE(mix != NULL, ESP_ERR_INVALID_ARG, TAG, "mix is NULL");
    ESP_RETURN_ON_FALSE(eye_next != NULL && brow_next != NULL && mouth_next != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "pose out is NULL");

    gfx_face_emote_blend_mix(assets, mix, eye_next, brow_next, mouth_next, look_x_next, look_y_next);
    return ESP_OK;
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

esp_err_t gfx_face_emote_update_pose(gfx_obj_t *obj, gfx_face_emote_t *face)
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
        return ESP_OK;
    }

    for (i = 0; i < GFX_FACE_EMOTE_SHAPE14_NUM_PTS; i++) {
        face->eye_current.pts[i] = gfx_face_emote_ease_spring(face->eye_current.pts[i], face->eye_target.pts[i]);
        face->mouth_current.pts[i] = gfx_face_emote_ease_spring(face->mouth_current.pts[i], face->mouth_target.pts[i]);
    }
    for (i = 0; i < GFX_FACE_EMOTE_SHAPE8_NUM_PTS; i++) {
        face->brow_current.pts[i] = gfx_face_emote_ease_spring(face->brow_current.pts[i], face->brow_target.pts[i]);
    }

    face->look_x_current = gfx_face_emote_ease_spring(face->look_x_current, face->look_x_target);
    face->look_y_current = gfx_face_emote_ease_spring(face->look_y_current, face->look_y_target);
    lx = face->look_x_current;
    ly = face->look_y_current;

    gfx_face_emote_get_anchors(obj, face,
                               &mouth_cx, &mouth_cy,
                               &left_eye_cx, &left_eye_cy,
                               &right_eye_cx, &right_eye_cy,
                               &left_brow_cx, &left_brow_cy,
                               &right_brow_cx, &right_brow_cy);

    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(face->left_eye_obj, face->eye_current.pts,
                                                         left_eye_cx + lx, left_eye_cy + ly,
                                                         face->cfg.eye_scale_percent, false, face->cfg.eye_segs),
                        TAG, "apply left eye pose failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(face->right_eye_obj, face->eye_current.pts,
                                                         right_eye_cx + lx, right_eye_cy + ly,
                                                         face->cfg.eye_scale_percent, true, face->cfg.eye_segs),
                        TAG, "apply right eye pose failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_stroke(face->left_brow_obj, face->brow_current.pts, false,
                                                           left_brow_cx + lx, left_brow_cy + ly,
                                                           face->cfg.brow_scale_percent, face->cfg.brow_thickness, false, face->cfg.brow_segs),
                        TAG, "apply left brow pose failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_stroke(face->right_brow_obj, face->brow_current.pts, false,
                                                           right_brow_cx + lx, right_brow_cy + ly,
                                                           face->cfg.brow_scale_percent, face->cfg.brow_thickness, true, face->cfg.brow_segs),
                        TAG, "apply right brow pose failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_stroke(face->mouth_obj, face->mouth_current.pts, true,
                                                           mouth_cx + (lx * GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM / GFX_FACE_EMOTE_MOUTH_LOOK_DEN),
                                                           mouth_cy + (ly * GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM / GFX_FACE_EMOTE_MOUTH_LOOK_DEN),
                                                           face->cfg.mouth_scale_percent, face->cfg.mouth_thickness, false, face->cfg.mouth_segs),
                        TAG, "apply mouth pose failed");
    return ESP_OK;
}

esp_err_t gfx_face_emote_set_target_pose(gfx_obj_t *obj,
                                         gfx_face_emote_t *face,
                                         const gfx_face_emote_eye_shape_t *eye_next,
                                         const gfx_face_emote_brow_shape_t *brow_next,
                                         const gfx_face_emote_mouth_shape_t *mouth_next,
                                         int16_t look_x_next,
                                         int16_t look_y_next,
                                         bool snap_now)
{
    ESP_RETURN_ON_FALSE(face != NULL, ESP_ERR_INVALID_ARG, TAG, "face state is NULL");
    ESP_RETURN_ON_FALSE(eye_next != NULL && brow_next != NULL && mouth_next != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "target pose is NULL");

    if (snap_now) {
        face->eye_current = *eye_next;
        face->brow_current = *brow_next;
        face->mouth_current = *mouth_next;
        face->look_x_current = look_x_next;
        face->look_y_current = look_y_next;
    }

    face->eye_target = *eye_next;
    face->brow_target = *brow_next;
    face->mouth_target = *mouth_next;
    if (!face->manual_look_enabled) {
        face->look_x_target = look_x_next;
        face->look_y_target = look_y_next;
    }

    return gfx_face_emote_update_pose(obj, face);
}
