/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_obj_t *gfx_list_create(gfx_disp_t *disp);

esp_err_t gfx_list_set_items(gfx_obj_t *obj, const char *const *items, size_t count);
esp_err_t gfx_list_add_item(gfx_obj_t *obj, const char *item);
esp_err_t gfx_list_clear(gfx_obj_t *obj);

esp_err_t gfx_list_set_selected(gfx_obj_t *obj, int32_t index);
int32_t gfx_list_get_selected(gfx_obj_t *obj);

esp_err_t gfx_list_set_font(gfx_obj_t *obj, gfx_font_t font);
esp_err_t gfx_list_set_text_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_bg_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_selected_bg_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_border_color(gfx_obj_t *obj, gfx_color_t color);
esp_err_t gfx_list_set_border_width(gfx_obj_t *obj, uint16_t width);
esp_err_t gfx_list_set_row_height(gfx_obj_t *obj, uint16_t row_height);
esp_err_t gfx_list_set_padding(gfx_obj_t *obj, uint16_t pad_x, uint16_t pad_y);

#ifdef __cplusplus
}
#endif

