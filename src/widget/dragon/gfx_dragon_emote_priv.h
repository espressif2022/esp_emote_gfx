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
#include "widget/gfx_dragon_emote.h"
#include "widget/gfx_mesh_img.h"

typedef struct {
    gfx_dragon_transform_t cur;
    gfx_dragon_transform_t tgt;
} gfx_dragon_part_state_t;

typedef struct {
    gfx_mesh_img_point_t cur[22];
    gfx_mesh_img_point_t tgt[22];
    size_t count;
} gfx_dragon_shape_state_t;

typedef struct {
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    gfx_color_t color;

    gfx_obj_t *head_obj;
    gfx_obj_t *body_obj;
    gfx_obj_t *tail_obj;
    gfx_obj_t *tail_stripe1_obj;
    gfx_obj_t *tail_stripe2_obj;
    gfx_obj_t *clawL_obj;
    gfx_obj_t *clawR_obj;
    gfx_obj_t *antennaL_obj;
    gfx_obj_t *antennaR_obj;
    gfx_obj_t *eyeL_obj;
    gfx_obj_t *eyeR_obj;
    gfx_obj_t *eyeLineL_obj;
    gfx_obj_t *eyeLineR_obj;
    gfx_obj_t *dot1_obj;
    gfx_obj_t *dot2_obj;
    gfx_obj_t *dot3_obj;

    gfx_timer_handle_t anim_timer;
    gfx_dragon_emote_cfg_t cfg;
    const gfx_dragon_emote_assets_t *assets;

    gfx_dragon_part_state_t head;
    gfx_dragon_part_state_t body;
    gfx_dragon_part_state_t tail;
    gfx_dragon_part_state_t clawL;
    gfx_dragon_part_state_t clawR;
    gfx_dragon_part_state_t antenna;
    gfx_dragon_part_state_t eyeL;
    gfx_dragon_part_state_t eyeR;
    gfx_dragon_part_state_t pupilL;
    gfx_dragon_part_state_t pupilR;
    gfx_dragon_part_state_t dots;
    gfx_dragon_shape_state_t pupil_shape;

    size_t pose_idx;
} gfx_dragon_emote_t;

// --- Internal Logic ---

void gfx_dragon_emote_expr_to_mix(const gfx_dragon_emote_expr_t *expr, gfx_dragon_emote_mix_t *mix);
const gfx_mesh_img_point_t *gfx_dragon_emote_get_pupil_shape_points(gfx_dragon_pupil_shape_t shape,
                                                                    size_t *count_out);
esp_err_t gfx_dragon_emote_find_expr_index(const gfx_dragon_emote_assets_t *assets,
                                           const char *name,
                                           size_t *index_out);
esp_err_t gfx_dragon_emote_eval_mix(const gfx_dragon_emote_assets_t *assets,
                                    const gfx_dragon_emote_mix_t *mix,
                                    gfx_dragon_emote_pose_t *pose_out,
                                    gfx_dragon_transform_t *pupil_out,
                                    gfx_dragon_pupil_shape_t *pupil_shape_out);
esp_err_t gfx_dragon_emote_update_pose(gfx_obj_t *obj, gfx_dragon_emote_t *dragon);
