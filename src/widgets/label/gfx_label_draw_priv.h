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
esp_err_t gfx_label_text_box_update(gfx_object_t *owner, gfx_label_t *label, const gfx_area_t *area);
esp_err_t gfx_label_text_box_draw(gfx_object_t *owner, gfx_label_t *label, const gfx_draw_ctx_t *ctx,
                                  const gfx_area_t *area, const gfx_area_t *clip);

#ifdef __cplusplus
}
#endif
