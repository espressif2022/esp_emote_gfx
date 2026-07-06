/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/object.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gfx_progress_bar_value_changed_cb_t)(gfx_object_t *obj, uint16_t permille, void *user_data);

typedef enum {
    GFX_PROGRESS_BAR_DIR_HORIZONTAL = 0,
    GFX_PROGRESS_BAR_DIR_VERTICAL,
} gfx_progress_bar_dir_t;

gfx_object_t *gfx_progress_bar_create(gfx_display_t *disp);
gfx_err_t gfx_progress_bar_set_value(gfx_object_t *obj, uint16_t permille);
uint16_t gfx_progress_bar_get_value(gfx_object_t *obj);
gfx_err_t gfx_progress_bar_set_colors(gfx_object_t *obj, gfx_color_t track_color, gfx_color_t fill_color);
gfx_err_t gfx_progress_bar_set_thumb_style(gfx_object_t *obj, gfx_color_t color,
        gfx_color_t border_color, uint16_t border_width);
gfx_err_t gfx_progress_bar_set_radius(gfx_object_t *obj, uint16_t radius);
gfx_err_t gfx_progress_bar_set_fill_pad(gfx_object_t *obj, uint16_t fill_pad);
gfx_err_t gfx_progress_bar_set_direction(gfx_object_t *obj, gfx_progress_bar_dir_t direction);
gfx_err_t gfx_progress_bar_set_interactive(gfx_object_t *obj, bool interactive);
gfx_err_t gfx_progress_bar_set_value_changed_cb(gfx_object_t *obj,
        gfx_progress_bar_value_changed_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
