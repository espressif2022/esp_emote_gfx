/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Widget-internal label lifecycle hooks and shared private label state. */
#include "widgets/label/gfx_label_types_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_err_t gfx_label_delete_impl(gfx_object_t *obj);
gfx_err_t gfx_label_load_impl(gfx_object_t *obj);
void gfx_label_release_impl(gfx_object_t *obj);
gfx_err_t gfx_label_load_state(gfx_object_t *owner, gfx_label_t *label);
void gfx_label_release_state(gfx_label_t *label);
void gfx_label_delete_state(gfx_object_t *owner, gfx_label_t *label);
gfx_err_t gfx_label_update_impl(gfx_object_t *obj);
gfx_err_t gfx_label_set_font_source(gfx_object_t *obj, gfx_label_t *label, gfx_font_t font);

#ifdef __cplusplus
}
#endif
