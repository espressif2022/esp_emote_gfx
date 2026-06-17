/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/widgets/pageflow.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gfx_coverflow_changed_cb_t)(gfx_object_t *obj, int32_t index, void *user_data);

typedef struct {
    gfx_object_t *card;        /**< Card root object, usually a container */
    gfx_object_t *image_slot;  /**< Optional image-like child resized to the visual image area */
    gfx_object_t *title_slot;  /**< Optional title/text child resized to the visual title area */
} gfx_coverflow_card_dsc_t;

gfx_object_t *gfx_coverflow_create(gfx_display_t *disp);

gfx_err_t gfx_coverflow_clear(gfx_object_t *obj);
gfx_err_t gfx_coverflow_add_item(gfx_object_t *obj, const char *text);
gfx_err_t gfx_coverflow_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count);
gfx_err_t gfx_coverflow_set_image_items(gfx_object_t *obj, const gfx_image_dsc_t *const *images,
                                        uint16_t item_count);
gfx_err_t gfx_coverflow_set_card_descriptors(gfx_object_t *obj, const gfx_coverflow_card_dsc_t *cards,
                                             uint16_t item_count);
gfx_err_t gfx_coverflow_set_card_items(gfx_object_t *obj, gfx_object_t *const *cards,
                                       uint16_t item_count);

gfx_err_t gfx_coverflow_set_selected(gfx_object_t *obj, int32_t index);
int32_t gfx_coverflow_get_selected(gfx_object_t *obj);
uint16_t gfx_coverflow_get_item_count(gfx_object_t *obj);

gfx_err_t gfx_coverflow_set_font(gfx_object_t *obj, gfx_font_t font);
gfx_err_t gfx_coverflow_set_drag_threshold(gfx_object_t *obj, uint16_t threshold);
gfx_err_t gfx_coverflow_set_page_threshold(gfx_object_t *obj, uint16_t threshold);
gfx_err_t gfx_coverflow_set_changed_cb(gfx_object_t *obj, gfx_coverflow_changed_cb_t cb, void *user_data);

gfx_err_t gfx_coverflow_set_bg_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_coverflow_set_center_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_coverflow_set_side_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_coverflow_set_text_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_coverflow_set_border_color(gfx_object_t *obj, gfx_color_t color);
gfx_err_t gfx_coverflow_set_border_width(gfx_object_t *obj, uint16_t width);
gfx_err_t gfx_coverflow_set_zoom(gfx_object_t *obj, uint16_t center_zoom, uint16_t side_zoom);
gfx_err_t gfx_coverflow_set_spacing(gfx_object_t *obj, uint16_t spacing_pct);
gfx_err_t gfx_coverflow_set_side_dim(gfx_object_t *obj, gfx_opa_t opa);

#ifdef __cplusplus
}
#endif
