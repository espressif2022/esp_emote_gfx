/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_err_t gfx_object_set_pos(gfx_object_t *obj, gfx_coord_t x, gfx_coord_t y);
gfx_err_t gfx_object_set_size(gfx_object_t *obj, uint16_t w, uint16_t h);
gfx_err_t gfx_object_align(gfx_object_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);
gfx_err_t gfx_object_align_to(gfx_object_t *obj, gfx_object_t *base, uint8_t align,
                              gfx_coord_t x_ofs, gfx_coord_t y_ofs);
gfx_err_t gfx_object_set_visible(gfx_object_t *obj, bool visible);
bool gfx_object_get_visible(gfx_object_t *obj);
gfx_err_t gfx_object_get_pos(gfx_object_t *obj, gfx_coord_t *x, gfx_coord_t *y);
gfx_err_t gfx_object_get_size(gfx_object_t *obj, uint16_t *w, uint16_t *h);
gfx_err_t gfx_object_set_touch_cb(gfx_object_t *obj, gfx_object_touch_cb_t cb, void *user_data);
uint32_t gfx_object_get_trace_id(gfx_object_t *obj);
const char *gfx_object_get_class_name(gfx_object_t *obj);
const char *gfx_object_get_trace_tag(gfx_object_t *obj);
gfx_err_t gfx_object_delete(gfx_object_t *obj);

#ifdef __cplusplus
}
#endif
