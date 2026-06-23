/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t h_res;          /**< Simulated display width in pixels */
    uint32_t v_res;          /**< Simulated display height in pixels */
    uint32_t scale;          /**< Window scale factor; 0 means 1 */
    const char *title;       /**< Window title; NULL uses a default title */
} gfx_backend_sdl_config_t;

/**
 * @brief Create an SDL display backend for host-side simulators.
 *
 * The implementation is intentionally kept out of the embedded ESP-IDF
 * component build. Host builds can compile src/backend/sdl/gfx_backend_sdl.c with an
 * SDL2 or SDL3 dependency.
 *
 * If the returned backend is passed to gfx_display_add() and display creation
 * succeeds, the display owns it and will delete it from gfx_display_delete().
 *
 * @param cfg SDL backend configuration.
 * @return Backend pointer on success, or NULL on failure.
 */
gfx_backend_t *gfx_backend_sdl_create(const gfx_backend_sdl_config_t *cfg);

/**
 * @brief Delete an SDL backend that is not owned by a display.
 * @param backend Backend returned by gfx_backend_sdl_create().
 */
void gfx_backend_sdl_delete(gfx_backend_t *backend);

/**
 * @brief Poll SDL window events, dispatch mouse input, and tick the display.
 *
 * Host simulator loops should call this regularly from the SDL owner thread.
 * When display is not NULL, left mouse button events are converted to GFX touch
 * events using the backend scale configured in gfx_backend_sdl_create().
 *
 * @param display Display that should receive simulated touch events, or NULL to
 *                only process window/keyboard events.
 * @return true when the user requested quit, false otherwise.
 */
bool gfx_backend_sdl_poll(gfx_display_t *display);

/**
 * @brief Poll SDL window events and dispatch mouse input without ticking GFX.
 *
 * Host runners that own the frame loop should call this and then advance
 * gfx_core_tick() themselves. gfx_backend_sdl_poll() remains as the legacy
 * convenience helper that also ticks manual-tick displays.
 *
 * @param display Display that should receive simulated touch events, or NULL to
 *                only process window/keyboard events.
 * @return true when the user requested quit, false otherwise.
 */
bool gfx_backend_sdl_poll_events(gfx_display_t *display);

/**
 * @brief Poll SDL window events without a display.
 * @return true when the user requested quit, false otherwise.
 */
bool gfx_backend_sdl_poll_quit(void);

#ifdef __cplusplus
}
#endif
