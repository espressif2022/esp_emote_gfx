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

typedef enum {
    GFX_PAGEFLOW_DIR_HORIZONTAL = 0,
    GFX_PAGEFLOW_DIR_VERTICAL,
} gfx_pageflow_dir_t;

typedef void (*gfx_pageflow_changed_cb_t)(gfx_object_t *obj, int32_t page_index, void *user_data);

gfx_object_t *gfx_pageflow_create(gfx_display_t *disp);

gfx_err_t gfx_pageflow_clear(gfx_object_t *obj);
gfx_err_t gfx_pageflow_add_page(gfx_object_t *obj, const char *text);
gfx_err_t gfx_pageflow_set_pages(gfx_object_t *obj, const char *const *pages, uint16_t page_count);
gfx_err_t gfx_pageflow_set_image_pages(gfx_object_t *obj, const gfx_image_dsc_t *const *images,
                                       uint16_t page_count);

gfx_err_t gfx_pageflow_set_page(gfx_object_t *obj, int32_t page_index);
int32_t gfx_pageflow_get_page(gfx_object_t *obj);
uint16_t gfx_pageflow_get_page_count(gfx_object_t *obj);

gfx_err_t gfx_pageflow_set_font(gfx_object_t *obj, gfx_font_t font);
gfx_err_t gfx_pageflow_set_direction(gfx_object_t *obj, gfx_pageflow_dir_t dir);
gfx_err_t gfx_pageflow_set_drag_threshold(gfx_object_t *obj, uint16_t threshold);
gfx_err_t gfx_pageflow_set_page_threshold(gfx_object_t *obj, uint16_t threshold);
gfx_err_t gfx_pageflow_set_changed_cb(gfx_object_t *obj, gfx_pageflow_changed_cb_t cb, void *user_data);

gfx_err_t gfx_pageflow_set_bg_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_pageflow_set_page_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_pageflow_set_text_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_pageflow_set_border_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_pageflow_set_border_width(gfx_object_t *obj, uint16_t width);

#ifdef __cplusplus
}
#endif
