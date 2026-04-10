/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dragon part transform (X, Y, Rotation, Scale)
 * X/Y are in pixels, R in degrees, S in percent (100 = 1.0)
 */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t r;
    int16_t s;
} gfx_dragon_transform_t;

typedef enum {
    GFX_DRAGON_PUPIL_SHAPE_AUTO = 0,
    GFX_DRAGON_PUPIL_SHAPE_O,
    GFX_DRAGON_PUPIL_SHAPE_U,
    GFX_DRAGON_PUPIL_SHAPE_N,
    GFX_DRAGON_PUPIL_SHAPE_LINE,
} gfx_dragon_pupil_shape_t;

/**
 * @brief Holistic pose for the Dragon character
 */
typedef struct {
    const char *name;
    const char *name_cn;
    gfx_dragon_transform_t head;
    gfx_dragon_transform_t body;
    gfx_dragon_transform_t tail;
    gfx_dragon_transform_t clawL;
    gfx_dragon_transform_t clawR;
    gfx_dragon_transform_t antenna;
    gfx_dragon_transform_t eyeL;
    gfx_dragon_transform_t eyeR;
    gfx_dragon_transform_t dots;
    uint32_t hold_ticks;
} gfx_dragon_emote_pose_t;

typedef struct {
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t look_x;
    int16_t look_y;
    gfx_dragon_pupil_shape_t pupil_shape;
} gfx_dragon_emote_mix_t;

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
    gfx_dragon_pupil_shape_t pupil_shape;
    uint32_t hold_ticks;
} gfx_dragon_emote_expr_t;

typedef struct {
    const gfx_mesh_img_point_t *pts_head;
    const gfx_mesh_img_point_t *pts_body;
    const gfx_mesh_img_point_t *pts_tail;
    const gfx_mesh_img_point_t *pts_clawL;
    const gfx_mesh_img_point_t *pts_clawR;
    const gfx_mesh_img_point_t *pts_antenna;
    const gfx_mesh_img_point_t *pts_eyeL;
    const gfx_mesh_img_point_t *pts_eyeR;
    const gfx_mesh_img_point_t *pts_dots;

    size_t count_head;
    size_t count_body;
    size_t count_tail;
    size_t count_clawL;
    size_t count_clawR;
    size_t count_antenna;
    size_t count_eyeL;
    size_t count_eyeR;
    size_t count_dots;

    const gfx_dragon_emote_pose_t *sequence;
    size_t sequence_count;
    const gfx_dragon_emote_expr_t *expr_sequence;
    size_t expr_sequence_count;
} gfx_dragon_emote_assets_t;

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    uint16_t timer_period_ms;
    int16_t damping_div; // Divisor for easing (larger = slower)
} gfx_dragon_emote_cfg_t;

/**
 * @brief Create a Dragon Emote widget.
 */
gfx_obj_t *gfx_dragon_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);

/**
 * @brief Set assets (expressions).
 */
esp_err_t gfx_dragon_emote_set_assets(gfx_obj_t *obj, const gfx_dragon_emote_assets_t *assets);

/**
 * @brief Set color.
 */
esp_err_t gfx_dragon_emote_set_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Switch to a named pose.
 */
esp_err_t gfx_dragon_emote_set_pose_name(gfx_obj_t *obj, const char *name, bool snap_now);

/**
 * @brief Switch to a named expression from the current expression sequence.
 */
esp_err_t gfx_dragon_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now);

/**
 * @brief Evaluate and apply one expression mix directly.
 */
esp_err_t gfx_dragon_emote_set_mix(gfx_obj_t *obj, const gfx_dragon_emote_mix_t *mix, bool snap_now);

#ifdef __cplusplus
}
#endif
