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

typedef struct {
    int16_t pts[14];
} gfx_face_emote_shape14_t;

typedef struct {
    int16_t pts[8];
} gfx_face_emote_shape8_t;

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
    uint32_t hold_ticks;
} gfx_face_emote_expr_t;

typedef struct {
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t look_x;
    int16_t look_y;
} gfx_face_emote_mix_t;

typedef struct {
    const gfx_face_emote_shape14_t *ref_eye;
    size_t ref_eye_count;
    const gfx_face_emote_shape8_t *ref_brow;
    size_t ref_brow_count;
    const gfx_face_emote_shape14_t *ref_mouth;
    size_t ref_mouth_count;
    const gfx_face_emote_expr_t *sequence;
    size_t sequence_count;
} gfx_face_emote_assets_t;

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    int16_t mouth_x_ofs;
    int16_t mouth_y_ofs;
    int16_t eye_x_half_gap;
    int16_t eye_y_ofs;
    int16_t brow_y_ofs_extra;
    uint16_t timer_period_ms;
    uint8_t eye_segs;
    uint8_t brow_segs;
    uint8_t mouth_segs;
    int16_t eye_scale_percent;
    int16_t brow_scale_percent;
    int16_t mouth_scale_percent;
    int16_t brow_thickness;
    int16_t mouth_thickness;
} gfx_face_emote_cfg_t;

/**
 * @brief Fill configuration from the widget (or screen) pixel size.
 *
 * @param cfg                 Output configuration
 * @param display_w           Width in pixels (0 is clamped to 1)
 * @param display_h           Height in pixels (0 is clamped to 1)
 * @param layout_ref_x  Horizontal layout reference (e.g. BSP_LCD_H_RES × ratio). Must be > 0.
 * @param layout_ref_y  Vertical layout reference (e.g. BSP_LCD_V_RES × ratio). Must be > 0.
 */
void gfx_face_emote_cfg_init(gfx_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h,
                             uint16_t layout_ref_x, uint16_t layout_ref_y);

/**
 * @param layout_ref_x  Same as gfx_face_emote_cfg_init()
 * @param layout_ref_y  Same as gfx_face_emote_cfg_init()
 */
gfx_obj_t *gfx_face_emote_create(gfx_disp_t *disp, uint16_t layout_ref_x, uint16_t layout_ref_y);
esp_err_t gfx_face_emote_set_config(gfx_obj_t *obj, const gfx_face_emote_cfg_t *cfg);
esp_err_t gfx_face_emote_set_assets(gfx_obj_t *obj, const gfx_face_emote_assets_t *assets);
esp_err_t gfx_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now);
esp_err_t gfx_face_emote_set_mix(gfx_obj_t *obj, const gfx_face_emote_mix_t *mix, bool snap_now);
esp_err_t gfx_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);

#ifdef __cplusplus
}
#endif
