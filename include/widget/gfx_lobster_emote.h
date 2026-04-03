/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t x;
    int16_t y;
    int16_t r;
    int16_t s;
} gfx_lobster_transform_t;

typedef struct {
    const char *name;
    const char *name_cn;
    gfx_lobster_transform_t body;
    gfx_lobster_transform_t tail;
    gfx_lobster_transform_t claw_l;
    gfx_lobster_transform_t claw_r;
    int16_t eye_open;
    int16_t eye_look_x;
    int16_t eye_look_y;
    uint32_t hold_ticks;
} gfx_lobster_emote_state_t;

typedef struct {
    const gfx_mesh_img_point_t *pts_body;
    const gfx_mesh_img_point_t *pts_tail;
    const gfx_mesh_img_point_t *pts_claw_l;
    const gfx_mesh_img_point_t *pts_claw_r;
    const gfx_mesh_img_point_t *pts_eye_white;
    const gfx_mesh_img_point_t *pts_eye_pupil;
    size_t count_body;
    size_t count_tail;
    size_t count_claw_l;
    size_t count_claw_r;
    size_t count_eye_white;
    size_t count_eye_pupil;
    const gfx_lobster_emote_state_t *sequence;
    size_t sequence_count;
} gfx_lobster_emote_assets_t;

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    uint16_t timer_period_ms;
    int16_t damping_div;
} gfx_lobster_emote_cfg_t;

gfx_obj_t *gfx_lobster_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);
esp_err_t gfx_lobster_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_emote_assets_t *assets);
esp_err_t gfx_lobster_emote_set_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_lobster_emote_set_state_name(gfx_obj_t *obj, const char *name, bool snap_now);
esp_err_t gfx_lobster_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);

#ifdef __cplusplus
}
#endif
