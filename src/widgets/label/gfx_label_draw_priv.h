/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/object/gfx_object_priv.h"
#include "widgets/label/gfx_label_types_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *   INTERNAL API
 **********************/

void gfx_label_clear_glyph_cache(gfx_label_t *label);
void gfx_label_scroll_timer_callback(void *arg);
void gfx_label_snap_timer_callback(void *arg);
esp_err_t gfx_label_prepare_glyphs(gfx_object_t *obj);
esp_err_t gfx_label_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
