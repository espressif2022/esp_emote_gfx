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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t pts[14];
} gfx_lobster_face_emote_eye_shape_t;

typedef gfx_lobster_face_emote_eye_shape_t gfx_lobster_face_emote_claw_shape_t;

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
} gfx_lobster_face_emote_expr_t;

typedef struct {
    int16_t w_smile;
    int16_t w_happy;
    int16_t w_sad;
    int16_t w_surprise;
    int16_t w_angry;
    int16_t look_x;
    int16_t look_y;
} gfx_lobster_face_emote_mix_t;

typedef struct {
    const gfx_lobster_face_emote_eye_shape_t *ref_eye;
    size_t ref_eye_count;
    const gfx_lobster_face_emote_claw_shape_t *ref_claw;
    size_t ref_claw_count;
    const gfx_lobster_face_emote_expr_t *sequence;
    size_t sequence_count;
} gfx_lobster_face_emote_assets_t;

typedef struct {
    uint16_t display_w;
    uint16_t display_h;
    int16_t eye_x_half_gap;
    int16_t eye_y_ofs;
    int16_t claw_x_half_gap;
    int16_t claw_y_ofs;
    int16_t head_y_ofs;
    int16_t tail_x_ofs;
    int16_t tail_y_ofs;
    uint16_t timer_period_ms;
    uint8_t eye_segs;
    uint8_t claw_segs;
    uint8_t shell_segs;
    int16_t eye_scale_percent;
    int16_t claw_scale_percent;
    int16_t shell_scale_percent;
} gfx_lobster_face_emote_cfg_t;

void gfx_lobster_face_emote_cfg_init(gfx_lobster_face_emote_cfg_t *cfg, uint16_t display_w, uint16_t display_h);
gfx_obj_t *gfx_lobster_face_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h);
esp_err_t gfx_lobster_face_emote_set_config(gfx_obj_t *obj, const gfx_lobster_face_emote_cfg_t *cfg);
esp_err_t gfx_lobster_face_emote_set_assets(gfx_obj_t *obj, const gfx_lobster_face_emote_assets_t *assets);
esp_err_t gfx_lobster_face_emote_set_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_lobster_face_emote_set_expression_name(gfx_obj_t *obj, const char *name, bool snap_now);
esp_err_t gfx_lobster_face_emote_set_mix(gfx_obj_t *obj, const gfx_lobster_face_emote_mix_t *mix, bool snap_now);
esp_err_t gfx_lobster_face_emote_set_manual_look(gfx_obj_t *obj, bool enabled, int16_t look_x, int16_t look_y);

#ifdef __cplusplus
}
#endif
