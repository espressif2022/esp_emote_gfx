/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gfx_wheel_value_cb_t)(gfx_object_t *obj, int32_t selected_index, void *user_data);
typedef void (*gfx_wheel_confirm_cb_t)(gfx_object_t *obj, int32_t selected_index, void *user_data);

gfx_object_t *gfx_wheel_create(gfx_display_t *disp);

gfx_err_t gfx_wheel_clear(gfx_object_t *obj);
gfx_err_t gfx_wheel_add_item(gfx_object_t *obj, const char *text);
gfx_err_t gfx_wheel_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count);

gfx_err_t gfx_wheel_set_selected(gfx_object_t *obj, int32_t index);
gfx_err_t gfx_wheel_confirm(gfx_object_t *obj);
int32_t gfx_wheel_get_selected(gfx_object_t *obj);
uint16_t gfx_wheel_get_item_count(gfx_object_t *obj);
const char *gfx_wheel_get_item_text(gfx_object_t *obj, uint16_t index);

gfx_err_t gfx_wheel_set_font(gfx_object_t *obj, gfx_font_t font);
gfx_err_t gfx_wheel_set_item_height(gfx_object_t *obj, uint16_t height);
gfx_err_t gfx_wheel_set_visible_rows(gfx_object_t *obj, uint8_t rows);
gfx_err_t gfx_wheel_set_cyclic(gfx_object_t *obj, bool cyclic);
gfx_err_t gfx_wheel_set_drag_threshold(gfx_object_t *obj, uint16_t threshold);
gfx_err_t gfx_wheel_set_value_cb(gfx_object_t *obj, gfx_wheel_value_cb_t cb, void *user_data);
gfx_err_t gfx_wheel_set_confirm_cb(gfx_object_t *obj, gfx_wheel_confirm_cb_t cb, void *user_data);

gfx_err_t gfx_wheel_set_bg_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_wheel_set_text_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_wheel_set_center_bg_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_wheel_set_center_text_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_wheel_set_border_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_wheel_set_border_width(gfx_object_t *obj, uint16_t width);

#ifdef __cplusplus
}
#endif
