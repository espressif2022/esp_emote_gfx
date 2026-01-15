/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_touch.h"

#include "core/gfx_disp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   OPAQUE TYPES
 *********************/
/** Touch handle: from gfx_touch_add(), pass to event_cb and other touch APIs */
typedef struct gfx_touch gfx_touch_t;

/*********************
 *   ENUMS
 *********************/
typedef enum {
    GFX_TOUCH_EVENT_PRESS = 0,
    GFX_TOUCH_EVENT_RELEASE,
    GFX_TOUCH_EVENT_MOVE,   /**< Finger moved while pressed (slide) */
} gfx_touch_event_type_t;

/*********************
 *   EVENT STRUCTS
 *********************/
/** Payload passed to gfx_touch_event_cb_t; hit_obj is set when touch is bound to a disp */
typedef struct {
    gfx_touch_event_type_t type;
    uint16_t x;
    uint16_t y;
    uint16_t strength;
    uint8_t track_id;
    uint32_t timestamp_ms;
} gfx_touch_event_t;

/*********************
 *   CALLBACK TYPES
 *********************/
typedef void (*gfx_touch_event_cb_t)(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);

/*********************
 *   CONFIG STRUCTS
 *********************/
/** Passed to gfx_touch_add(); NULL or no handle disables touch */
typedef struct {
    esp_lcd_touch_handle_t handle;           /**< LCD touch driver handle */
    gfx_touch_event_cb_t event_cb;           /**< Event callback */
    uint32_t poll_ms;                        /**< Poll interval ms (0 = default) */
    gfx_disp_t *disp;                        /**< Display handle */
    void *user_data;                         /**< User data for callback */
} gfx_touch_config_t;

/**
 * @brief Add a touch device (like gfx_disp_add; multiple touch devices supported)
 *
 * @param handle Graphics handle from gfx_emote_init
 * @param cfg Touch configuration (handle, poll_ms, event_cb, etc.); required
 * @return gfx_touch_t* Touch pointer on success, NULL on error
 */
gfx_touch_t *gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg);


/**
 * @brief Set the user data for a touch device
 *
 * @param touch Touch pointer returned from gfx_touch_add
 * @param user_data User data to set
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if touch is NULL
 */
esp_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_disp_t *disp);

/**
 * @brief Remove a touch device from the list and release resources (stops polling, disables IRQ).
 *        Does not free the gfx_touch_t; caller must free(touch) after.
 *
 * @param touch Touch pointer returned from gfx_touch_add; safe to pass NULL
 */
void gfx_touch_del(gfx_touch_t *touch);

#ifdef __cplusplus
}
#endif
