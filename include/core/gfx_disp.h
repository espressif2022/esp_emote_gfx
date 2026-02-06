/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "gfx_types.h"
#include "core/gfx_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   OPAQUE TYPES
 *********************/
/** Display handle: one per screen; from gfx_disp_add(), use with all gfx_disp_* APIs */
typedef struct gfx_disp gfx_disp_t;

/*********************
 *   ENUMS
 *********************/
typedef enum {
    GFX_PLAYER_EVENT_IDLE = 0,
    GFX_PLAYER_EVENT_ONE_FRAME_DONE,
    GFX_PLAYER_EVENT_ALL_FRAME_DONE,
} gfx_player_event_t;

/*********************
 *   CALLBACK TYPES
 *********************/
typedef void (*gfx_player_flush_cb_t)(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data);
typedef void (*gfx_player_update_cb_t)(gfx_disp_t *disp, gfx_player_event_t event, const void *obj);

/*********************
 *   CONFIG STRUCTS
 *********************/
/** Passed to gfx_disp_add() for multi-screen setup */
typedef struct {
    uint32_t h_res;                          /**< Screen width in pixels */
    uint32_t v_res;                          /**< Screen height in pixels */
    gfx_player_flush_cb_t flush_cb;          /**< Flush callback for this display */
    gfx_player_update_cb_t update_cb;       /**< Update callback (frame/playback events) */
    void *user_data;                         /**< User data for this display */
    struct {
        unsigned char swap : 1;              /**< Color swap flag */
    } flags;
    struct {
        void *buf1;                          /**< Frame buffer 1 (NULL = internal alloc) */
        void *buf2;                          /**< Frame buffer 2 (NULL = internal alloc) */
        size_t buf_pixels;                   /**< Size per buffer in pixels (0 = auto) */
    } buffers;
} gfx_disp_config_t;

/**********************
 * PUBLIC API
 **********************/

/**
 * @brief Add a display (multi-screen support)
 *
 * @param handle Graphics handle from gfx_emote_init
 * @param cfg Display configuration (resolution, flush callback, buffers)
 * @return gfx_disp_t* New display pointer on success, NULL on error
 */
gfx_disp_t *gfx_disp_add(gfx_handle_t handle, const gfx_disp_config_t *cfg);

/**
 * @brief Invalidate full screen of a display to trigger refresh
 *
 * @param disp Display from gfx_disp_add
 */
void gfx_disp_refresh_all(gfx_disp_t *disp);

/**
 * @brief Notify that flush is done (e.g. from panel IO callback)
 *
 * @param disp Display from gfx_disp_add
 * @param swap_act_buf Whether to swap the active buffer
 * @return bool True on success
 */
bool gfx_disp_flush_ready(gfx_disp_t *disp, bool swap_act_buf);

/**
 * @brief Get user data for a display
 *
 * @param disp Display from gfx_disp_add
 * @return void* User data, or NULL
 */
void *gfx_disp_get_user_data(gfx_disp_t *disp);

/**
 * @brief Set default background color for a display
 *
 * @param disp Display from gfx_disp_add
 * @param color Background color (e.g. RGB565)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gfx_disp_set_bg_color(gfx_disp_t *disp, gfx_color_t color);

#ifdef __cplusplus
}
#endif
