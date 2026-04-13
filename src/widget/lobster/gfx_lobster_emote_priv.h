/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_err.h"

#include "core/gfx_timer.h"
#include "core/gfx_obj.h"
#include "widget/gfx_lobster_emote.h"
#include "widget/gfx_mesh_img.h"

#define GFX_LOBSTER_DEFAULT_LOOK_SCALE_X 46.0f
#define GFX_LOBSTER_DEFAULT_LOOK_SCALE_Y 34.0f
#define GFX_LOBSTER_DEFAULT_EYE_X_FROM_LOOK 0.20f
#define GFX_LOBSTER_DEFAULT_EYE_Y_FROM_ALERT -2.0f
#define GFX_LOBSTER_DEFAULT_EYE_Y_FROM_DROOP 3.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_BASE 1.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_OPEN 0.16f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_DROOP -0.05f
#define GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_FOCUS 2.0f
#define GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_SOFT -1.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_FROM_LOOK 0.35f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_FROM_LOOK 0.35f
#define GFX_LOBSTER_DEFAULT_MOUTH_X_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_MOUTH_Y_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_ANTENNA_X_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_LIFT -8.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_DROOP 2.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_OPEN 12.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_CURL 18.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_DROOP -6.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_BASE 1.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_ALERT 0.05f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_LIFT 0.04f
#define GFX_LOBSTER_DEFAULT_LOOK_X_MIN -22.0f
#define GFX_LOBSTER_DEFAULT_LOOK_X_MAX 22.0f
#define GFX_LOBSTER_DEFAULT_LOOK_Y_MIN -16.0f
#define GFX_LOBSTER_DEFAULT_LOOK_Y_MAX 16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_MIN -16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_MAX 16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_MIN -18.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_MAX 18.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_MULTIPLIER 1.12f
#define GFX_LOBSTER_DEFAULT_ANTENNA_THICKNESS_BASE 15.0f
#define GFX_LOBSTER_DEFAULT_TIMER_PERIOD_MS 33U
#define GFX_LOBSTER_DEFAULT_DAMPING_DIV 4
#define GFX_LOBSTER_DEFAULT_EYE_SEGS 18U
#define GFX_LOBSTER_DEFAULT_PUPIL_SEGS 14U
#define GFX_LOBSTER_DEFAULT_ANTENNA_SEGS 18U

typedef struct {
    gfx_lobster_transform_t cur;
    gfx_lobster_transform_t tgt;
} gfx_lobster_part_state_t;

typedef struct {
    gfx_mesh_img_point_t cur[22];
    gfx_mesh_img_point_t tgt[22];
    size_t count;
} gfx_lobster_shape_state_t;

typedef struct {
    int16_t pts[14];
} gfx_lobster_curve14_t;

typedef struct {
    int16_t pts[8];
} gfx_lobster_curve8_t;

typedef struct {
    uint16_t accent_pixel;
    uint16_t eye_white_pixel;
    uint16_t pupil_pixel;
    gfx_image_dsc_t accent_img;
    gfx_image_dsc_t eye_white_img;
    gfx_image_dsc_t pupil_img;
    gfx_color_t color;

    gfx_obj_t *head_obj;
    gfx_obj_t *body_obj;
    gfx_obj_t *tail_obj;
    gfx_obj_t *tail_stripe1_obj;
    gfx_obj_t *tail_stripe2_obj;
    gfx_obj_t *claw_l_obj;
    gfx_obj_t *claw_r_obj;
    gfx_obj_t *antenna_l_obj;
    gfx_obj_t *antenna_r_obj;
    gfx_obj_t *eye_l_obj;
    gfx_obj_t *eye_r_obj;
    gfx_obj_t *pupil_l_obj;
    gfx_obj_t *pupil_r_obj;
    gfx_obj_t *mouth_obj;
    gfx_obj_t *dot1_obj;
    gfx_obj_t *dot2_obj;
    gfx_obj_t *dot3_obj;

    gfx_timer_handle_t anim_timer;
    gfx_lobster_emote_cfg_t cfg;
    const gfx_lobster_emote_assets_t *assets;

    gfx_lobster_part_state_t head;
    gfx_lobster_part_state_t body;
    gfx_lobster_part_state_t tail;
    gfx_lobster_part_state_t claw_l;
    gfx_lobster_part_state_t claw_r;
    gfx_lobster_part_state_t antenna;
    gfx_lobster_part_state_t eye_l;
    gfx_lobster_part_state_t eye_r;
    gfx_lobster_part_state_t pupil_l;
    gfx_lobster_part_state_t pupil_r;
    gfx_lobster_part_state_t mouth;
    gfx_lobster_part_state_t dots;

    gfx_lobster_curve14_t eye_white_cur;
    gfx_lobster_curve14_t eye_white_tgt;
    gfx_lobster_curve14_t pupil_cur;
    gfx_lobster_curve14_t pupil_tgt;
    gfx_lobster_curve14_t mouth_cur;
    gfx_lobster_curve14_t mouth_tgt;
    gfx_lobster_curve8_t antenna_curve_l_cur;
    gfx_lobster_curve8_t antenna_curve_l_tgt;
    gfx_lobster_curve8_t antenna_curve_r_cur;
    gfx_lobster_curve8_t antenna_curve_r_tgt;
    int16_t look_x_cur;
    int16_t look_x_tgt;
    int16_t look_y_cur;
    int16_t look_y_tgt;
    gfx_lobster_pupil_shape_t pupil_shape;
    uint32_t layer_mask;
    bool manual_look_enabled;
    size_t state_idx;
} gfx_lobster_emote_t;

void gfx_lobster_emote_state_to_mix(const gfx_lobster_emote_state_t *state, gfx_lobster_emote_state_t *mix_out);
const gfx_mesh_img_point_t *gfx_lobster_emote_get_pupil_shape_points(gfx_lobster_pupil_shape_t shape, size_t *count_out);
const gfx_lobster_emote_semantics_t *gfx_lobster_emote_get_semantics(const gfx_lobster_emote_assets_t *assets);
esp_err_t gfx_lobster_emote_find_state_index(const gfx_lobster_emote_assets_t *assets, const char *name, size_t *index_out);
esp_err_t gfx_lobster_emote_validate_assets(const gfx_lobster_emote_assets_t *assets);
esp_err_t gfx_lobster_emote_eval_state(const gfx_lobster_emote_assets_t *assets,
                                       const gfx_lobster_emote_state_t *state,
                                       gfx_lobster_transform_t *head_out,
                                       gfx_lobster_transform_t *body_out,
                                       gfx_lobster_transform_t *tail_out,
                                       gfx_lobster_transform_t *claw_l_out,
                                       gfx_lobster_transform_t *claw_r_out,
                                       gfx_lobster_transform_t *antenna_out,
                                       gfx_lobster_transform_t *eye_out,
                                       gfx_lobster_transform_t *pupil_out,
                                       gfx_lobster_transform_t *mouth_out,
                                       gfx_lobster_transform_t *dots_out,
                                       gfx_lobster_curve14_t *eye_curve_out,
                                       gfx_lobster_curve14_t *pupil_curve_out,
                                       gfx_lobster_curve14_t *mouth_curve_out,
                                       gfx_lobster_curve8_t *antenna_curve_l_out,
                                       gfx_lobster_curve8_t *antenna_curve_r_out,
                                       gfx_lobster_pupil_shape_t *pupil_shape_out,
                                       int16_t *look_x_out,
                                       int16_t *look_y_out);
esp_err_t gfx_lobster_emote_update_pose(gfx_obj_t *obj, gfx_lobster_emote_t *lobster);
