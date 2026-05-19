/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gfx_list_focus_cb_t)(gfx_obj_t *obj, int32_t focused_index, void *user_data);

gfx_obj_t *gfx_list_create(gfx_disp_t *disp);

esp_err_t gfx_list_clear(gfx_obj_t *obj);
esp_err_t gfx_list_add_item(gfx_obj_t *obj, const char *text);
esp_err_t gfx_list_set_items(gfx_obj_t *obj, const char *const *items, uint16_t item_count);

esp_err_t gfx_list_set_focus(gfx_obj_t *obj, int32_t index);
int32_t gfx_list_get_focus(gfx_obj_t *obj);
esp_err_t gfx_list_set_top_index(gfx_obj_t *obj, uint16_t index);
uint16_t gfx_list_get_top_index(gfx_obj_t *obj);
uint16_t gfx_list_get_item_count(gfx_obj_t *obj);
const char *gfx_list_get_item_text(gfx_obj_t *obj, uint16_t index);

esp_err_t gfx_list_set_font(gfx_obj_t *obj, gfx_font_t font);
esp_err_t gfx_list_set_item_height(gfx_obj_t *obj, uint16_t height);
esp_err_t gfx_list_set_text_pad(gfx_obj_t *obj, uint16_t pad_x, uint16_t pad_y);
esp_err_t gfx_list_set_focus_cb(gfx_obj_t *obj, gfx_list_focus_cb_t cb, void *user_data);

esp_err_t gfx_list_set_bg_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_focus_bg_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_text_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_focus_text_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_border_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_border_width(gfx_obj_t *obj, uint16_t width);

#ifdef __cplusplus
}
#endif
