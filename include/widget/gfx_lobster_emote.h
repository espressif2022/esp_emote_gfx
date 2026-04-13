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

typedef enum {
    GFX_LOBSTER_PUPIL_SHAPE_AUTO = 0,
    GFX_LOBSTER_PUPIL_SHAPE_O,
    GFX_LOBSTER_PUPIL_SHAPE_U,
    GFX_LOBSTER_PUPIL_SHAPE_N,
    GFX_LOBSTER_PUPIL_SHAPE_LINE,
} gfx_lobster_pupil_shape_t;

typedef struct {
    const char *name;
    const char *name_cn;
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t w_look_x;
    int16_t w_look_y;
    gfx_lobster_pupil_shape_t pupil_shape;
    uint32_t hold_ticks;
} gfx_lobster_emote_state_t;

#define GFX_LOBSTER_EMOTE_EXPORT_VERSION 2
#define GFX_LOBSTER_EMOTE_EXPORT_VERSION_LEGACY 1

typedef struct {
    uint32_t version;
    int32_t design_viewbox_x;
    int32_t design_viewbox_y;
    int32_t design_viewbox_w;
    int32_t design_viewbox_h;
    int32_t export_width;
    int32_t export_height;
    float export_scale;
    int32_t export_offset_x;
    int32_t export_offset_y;
} gfx_lobster_emote_export_meta_t;

typedef struct {
    float eye_open;
    float eye_focus;
    float eye_soft;
    float pupil_x;
    float pupil_y;
    float pupil_scale;
    float droop;
    float alert;
    float antenna_lift;
    float antenna_open;
    float antenna_curl;
    float look_bias_x;
    float look_bias_y;
} gfx_lobster_emote_axis_t;

typedef struct {
    float look_scale_x;
    float look_scale_y;
    float eye_x_from_look;
    float eye_y_from_alert;
    float eye_y_from_droop;
    float eye_scale_base;
    float eye_scale_from_eye_open;
    float eye_scale_from_droop;
    float eye_rot_from_focus;
    float eye_rot_from_soft;
    float pupil_x_from_look;
    float pupil_y_from_look;
    float mouth_x_from_look;
    float mouth_y_from_look;
    float antenna_x_from_look;
    float antenna_y_from_lift;
    float antenna_y_from_droop;
    float antenna_rot_from_open;
    float antenna_rot_from_curl;
    float antenna_rot_from_droop;
    float antenna_scale_base;
    float antenna_scale_from_alert;
    float antenna_scale_from_lift;
    float look_x_min;
    float look_x_max;
    float look_y_min;
    float look_y_max;
    float pupil_x_min;
    float pupil_x_max;
    float pupil_y_min;
    float pupil_y_max;
    float eye_scale_multiplier;
    float antenna_thickness_base;
    uint16_t timer_period_ms;
    int16_t damping_div;
    uint8_t eye_segs;
    uint8_t pupil_segs;
    uint8_t antenna_segs;
    uint8_t reserved;
    gfx_lobster_emote_axis_t smile;
    gfx_lobster_emote_axis_t happy;
    gfx_lobster_emote_axis_t sad;
    gfx_lobster_emote_axis_t surprise;
    gfx_lobster_emote_axis_t angry;
} gfx_lobster_emote_semantics_t;

typedef struct {
    int32_t eye_left_cx;
    int32_t eye_left_cy;
    int32_t eye_right_cx;
    int32_t eye_right_cy;
    int32_t pupil_left_cx;
    int32_t pupil_left_cy;
    int32_t pupil_right_cx;
    int32_t pupil_right_cy;
    int32_t mouth_cx;
    int32_t mouth_cy;
    int32_t antenna_left_cx;
    int32_t antenna_left_cy;
    int32_t antenna_right_cx;
    int32_t antenna_right_cy;
} gfx_lobster_emote_layout_t;

typedef struct {
    const gfx_mesh_img_point_t *pts_head;
    const gfx_mesh_img_point_t *pts_body;
    const gfx_mesh_img_point_t *pts_tail;
    const gfx_mesh_img_point_t *pts_claw_l;
    const gfx_mesh_img_point_t *pts_claw_r;
    const gfx_mesh_img_point_t *pts_eye_l;
    const gfx_mesh_img_point_t *pts_eye_r;
    const gfx_mesh_img_point_t *pts_dots;
    size_t count_head;
    size_t count_body;
    size_t count_tail;
    size_t count_claw_l;
    size_t count_claw_r;
    size_t count_eye_l;
    size_t count_eye_r;
    size_t count_dots;
    const int16_t (*eye_white_base)[14];
    const int16_t (*pupil_base)[14];
    const int16_t (*mouth_base)[14];
    const int16_t (*antenna_left_base)[8];
    const int16_t (*antenna_right_base)[8];
    const gfx_lobster_emote_export_meta_t *export_meta;
    const gfx_lobster_emote_layout_t *layout;
    const gfx_lobster_emote_semantics_t *semantics;
    const gfx_lobster_emote_state_t *sequence;
    size_t sequence_count;
} gfx_lobster_emote_assets_t;

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    uint16_t timer_period_ms;
    int16_t damping_div;
} gfx_lobster_emote_cfg_t;

#define GFX_LOBSTER_EMOTE_LAYER_ANTENNA (1U << 0)
#define GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE (1U << 1)
#define GFX_LOBSTER_EMOTE_LAYER_PUPIL (1U << 2)
#define GFX_LOBSTER_EMOTE_LAYER_MOUTH (1U << 3)
#define GFX_LOBSTER_EMOTE_LAYER_ALL (GFX_LOBSTER_EMOTE_LAYER_ANTENNA | GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE | GFX_LOBSTER_EMOTE_LAYER_PUPIL | GFX_LOBSTER_EMOTE_LAYER_MOUTH)

gfx_obj_t *gfx_lobster_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);
esp_err_t gfx_lobster_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_emote_assets_t *assets);
esp_err_t gfx_lobster_emote_set_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_lobster_emote_set_state_name(gfx_obj_t *obj, const char *name, bool snap_now);
esp_err_t gfx_lobster_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);
esp_err_t gfx_lobster_emote_set_layer_mask(gfx_obj_t *obj, uint32_t mask);

#ifdef __cplusplus
}
#endif
