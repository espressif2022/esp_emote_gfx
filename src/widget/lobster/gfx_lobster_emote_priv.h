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

#include "core/gfx_obj.h"
#include "core/gfx_timer.h"
#include "widget/gfx_lobster_emote.h"
#include "widget/gfx_mesh_img.h"

typedef struct {
    gfx_lobster_transform_t cur;
    gfx_lobster_transform_t tgt;
} gfx_lobster_part_state_t;

typedef struct {
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    gfx_color_t color;
    gfx_obj_t *tail_obj;
    gfx_obj_t *body_obj;
    gfx_obj_t *claw_l_obj;
    gfx_obj_t *claw_r_obj;
    gfx_obj_t *eye_white_l_obj;
    gfx_obj_t *eye_white_r_obj;
    gfx_obj_t *eye_pupil_l_obj;
    gfx_obj_t *eye_pupil_r_obj;
    gfx_timer_handle_t anim_timer;
    gfx_lobster_emote_cfg_t cfg;
    const gfx_lobster_emote_assets_t *assets;
    gfx_lobster_part_state_t body;
    gfx_lobster_part_state_t tail;
    gfx_lobster_part_state_t claw_l;
    gfx_lobster_part_state_t claw_r;
    int16_t eye_open_cur;
    int16_t eye_open_tgt;
    int16_t eye_look_x_cur;
    int16_t eye_look_x_tgt;
    int16_t eye_look_y_cur;
    int16_t eye_look_y_tgt;
    bool manual_look_enabled;
    uint32_t anim_tick;
    size_t state_idx;
} gfx_lobster_emote_t;

esp_err_t gfx_lobster_emote_update_pose(gfx_obj_t *obj, gfx_lobster_emote_t *lobster);
