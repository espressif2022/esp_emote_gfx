/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/object/gfx_obj_priv.h"
#include "widget/label/gfx_label_types_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *   INTERNAL API
 **********************/

void gfx_label_clear_glyph_cache(gfx_label_t *label);
void gfx_label_scroll_timer_callback(void *arg);
void gfx_label_snap_timer_callback(void *arg);
esp_err_t gfx_get_glphy_dsc(gfx_obj_t *obj);
esp_err_t gfx_draw_label(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
