/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "core/gfx_obj.h"
#include "core/gfx_timer.h"
#include "widget/gfx_img.h"
#include "widget/gfx_lobster_face_emote.h"

#define GFX_LOBSTER_FACE_EMOTE_REF_COUNT 6U
#define GFX_LOBSTER_FACE_EMOTE_SHAPE_NUM_PTS 14
#define GFX_LOBSTER_FACE_EMOTE_BLEND_WEIGHT_DIV 100
#define GFX_LOBSTER_FACE_EMOTE_EASE_DIV 4

typedef struct {
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    gfx_color_t color;
    gfx_obj_t *tail_obj;
    gfx_obj_t *body_obj;
    gfx_obj_t *head_obj;
    gfx_obj_t *claw_l_obj;
    gfx_obj_t *claw_r_obj;
    gfx_obj_t *eye_l_obj;
    gfx_obj_t *eye_r_obj;
    gfx_obj_t *pupil_l_obj;
    gfx_obj_t *pupil_r_obj;
    gfx_timer_handle_t anim_timer;
    gfx_lobster_face_emote_cfg_t cfg;
    const gfx_lobster_face_emote_assets_t *assets;
    gfx_lobster_face_emote_eye_shape_t eye_current;
    gfx_lobster_face_emote_eye_shape_t eye_target;
    gfx_lobster_face_emote_claw_shape_t claw_current;
    gfx_lobster_face_emote_claw_shape_t claw_target;
    int16_t look_x_current;
    int16_t look_x_target;
    int16_t look_y_current;
    int16_t look_y_target;
    bool manual_look_enabled;
    size_t expr_idx;
} gfx_lobster_face_emote_t;

bool gfx_lobster_face_emote_assets_valid(const gfx_lobster_face_emote_assets_t *assets);
void gfx_lobster_face_emote_expr_to_mix(const gfx_lobster_face_emote_expr_t *expr, gfx_lobster_face_emote_mix_t *mix);
esp_err_t gfx_lobster_face_emote_find_expr_index(const gfx_lobster_face_emote_assets_t *assets, const char *name, size_t *index_out);
esp_err_t gfx_lobster_face_emote_eval_mix(const gfx_lobster_face_emote_assets_t *assets,
                                          const gfx_lobster_face_emote_mix_t *mix,
                                          gfx_lobster_face_emote_eye_shape_t *eye_next,
                                          gfx_lobster_face_emote_claw_shape_t *claw_next,
                                          int16_t *look_x_next,
                                          int16_t *look_y_next);
esp_err_t gfx_lobster_face_emote_set_target_pose(gfx_obj_t *obj,
                                                 gfx_lobster_face_emote_t *lobster,
                                                 const gfx_lobster_face_emote_eye_shape_t *eye_next,
                                                 const gfx_lobster_face_emote_claw_shape_t *claw_next,
                                                 int16_t look_x_next,
                                                 int16_t look_y_next,
                                                 bool snap_now);
esp_err_t gfx_lobster_face_emote_update_pose(gfx_obj_t *obj, gfx_lobster_face_emote_t *lobster);
