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
#include "widget/gfx_parametric_emote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_STICKMAN_EXPORT_SCHEMA_VERSION 3

#ifndef GFX_STICKMAN_EXPORT_TYPES_DEFINED
#define GFX_STICKMAN_EXPORT_TYPES_DEFINED

typedef struct {
    int16_t x;
    int16_t y;
} gfx_stickman_point_t;

typedef struct {
    gfx_stickman_point_t head_center;
    gfx_stickman_point_t neck;
    gfx_stickman_point_t shoulder;
    gfx_stickman_point_t hip;
    gfx_stickman_point_t elbow_l;
    gfx_stickman_point_t hand_l;
    gfx_stickman_point_t elbow_r;
    gfx_stickman_point_t hand_r;
    gfx_stickman_point_t knee_l;
    gfx_stickman_point_t foot_l;
    gfx_stickman_point_t knee_r;
    gfx_stickman_point_t foot_r;
} gfx_stickman_pose_t;

typedef struct {
    const char *name;
    const char *name_cn;
    const char *group_name;
    uint16_t pose_index;
    uint8_t step_index;
    uint8_t step_count;
    int8_t facing;
    uint16_t transition_ticks;
} gfx_stickman_action_t;

typedef struct {
    int16_t head_radius;
    int16_t stroke_width;
    int16_t mirror_x;
    int16_t ground_y;
    uint16_t timer_period_ms;
    int16_t damping_div;
} gfx_stickman_layout_t;

typedef struct {
    const gfx_parametric_export_meta_t *meta;
    const gfx_stickman_layout_t *layout;
    const gfx_stickman_pose_t *poses;
    size_t pose_count;
    const gfx_stickman_action_t *actions;
    size_t action_count;
    const uint16_t *sequence;
    size_t sequence_count;
} gfx_stickman_export_t;

#endif

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    uint16_t timer_period_ms;
    int16_t damping_div;
} gfx_stickman_emote_cfg_t;

gfx_obj_t *gfx_stickman_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);
esp_err_t gfx_stickman_emote_set_export(gfx_obj_t *obj, const gfx_stickman_export_t *export_data);
esp_err_t gfx_stickman_emote_set_action_name(gfx_obj_t *obj, const char *name, bool snap_now);
esp_err_t gfx_stickman_emote_set_action_index(gfx_obj_t *obj, size_t action_index, bool snap_now);
esp_err_t gfx_stickman_emote_set_stroke_color(gfx_obj_t *obj, gfx_color_t color);

#ifdef __cplusplus
}
#endif
