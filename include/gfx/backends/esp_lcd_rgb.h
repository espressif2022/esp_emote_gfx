/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lcd_panel_ops.h"
#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_panel_handle_t panel;      /**< ESP-IDF RGB panel handle. */
} gfx_backend_esp_lcd_rgb_config_t;

/**
 * @brief Create a GFX backend for ESP-IDF RGB LCD panels.
 *
 * The backend owns the flush/wait handshake for full-frame RGB panel use. It
 * registers an RGB panel frame-done callback and signals gfx_display_flush_ready()
 * from that callback.
 *
 * If the returned backend is passed to gfx_display_add() and display creation
 * succeeds, the display owns it and will delete it from gfx_display_delete().
 *
 * @param cfg Backend configuration.
 * @return Backend pointer on success, or NULL on failure.
 */
gfx_backend_t *gfx_backend_esp_lcd_rgb_create(const gfx_backend_esp_lcd_rgb_config_t *cfg);

/**
 * @brief Delete an ESP LCD RGB backend that is not owned by a display.
 * @param backend Backend returned by gfx_backend_esp_lcd_rgb_create().
 */
void gfx_backend_esp_lcd_rgb_delete(gfx_backend_t *backend);

#ifdef __cplusplus
}
#endif
