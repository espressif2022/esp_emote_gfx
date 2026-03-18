/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_obj_t *gfx_button_create(gfx_disp_t *disp);

esp_err_t gfx_button_set_text(gfx_obj_t *obj, const char *text);
esp_err_t gfx_button_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...);
esp_err_t gfx_button_set_font(gfx_obj_t *obj, gfx_font_t font);
esp_err_t gfx_button_set_text_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_button_set_bg_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_button_set_bg_color_pressed(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_button_set_border_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_button_set_border_width(gfx_obj_t *obj, uint16_t width);
esp_err_t gfx_button_set_text_align(gfx_obj_t *obj, gfx_text_align_t align);

#ifdef __cplusplus
}
#endif
