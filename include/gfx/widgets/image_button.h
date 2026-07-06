/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/widgets/image.h"
#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_object_t *gfx_image_button_create(gfx_display_t *disp);
gfx_err_t gfx_image_button_set_src_released(gfx_object_t *obj, const gfx_image_src_t *src);
gfx_err_t gfx_image_button_set_src_pressed(gfx_object_t *obj, const gfx_image_src_t *src);
gfx_err_t gfx_image_button_set_text(gfx_object_t *obj, const char *text);
gfx_err_t gfx_image_button_set_font(gfx_object_t *obj, gfx_font_t font);
gfx_err_t gfx_image_button_set_text_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_image_button_set_text_align(gfx_object_t *obj, gfx_text_align_t align);
gfx_err_t gfx_image_button_set_text_padding(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y);
gfx_err_t gfx_image_button_set_touch_feedback_enabled(gfx_object_t *obj, bool enabled);
gfx_err_t gfx_image_button_set_pressed(gfx_object_t *obj, bool pressed);

#ifdef __cplusplus
}
#endif
