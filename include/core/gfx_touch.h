/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "core/gfx_err.h"
#include "core/gfx_types.h"
#include "core/gfx_disp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/
typedef struct gfx_touch gfx_touch_t;

typedef enum {
    GFX_TOUCH_EVENT_PRESS = 0,
    GFX_TOUCH_EVENT_RELEASE,
    GFX_TOUCH_EVENT_MOVE,   /**< Finger moved while pressed (slide) */
} gfx_touch_event_type_t;

/** Payload passed to gfx_touch_event_cb_t; hit_obj is set when touch is bound to a disp */
typedef struct gfx_touch_event {
    gfx_touch_event_type_t type;
    uint16_t x;
    uint16_t y;
    uint16_t strength;
    uint8_t track_id;
    uint32_t timestamp_ms;
} gfx_touch_event_t;

typedef void (*gfx_touch_event_cb_t)(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);

/** Passed to gfx_touch_add(); NULL or no driver_handle disables touch */
typedef struct {
    void *driver_handle;                     /**< Port-specific touch driver handle */
    gfx_touch_event_cb_t event_cb;           /**< Event callback */
    uint32_t poll_ms;                        /**< Poll interval ms (0 = default) */
    gfx_display_t *disp;                        /**< Display handle */
    void *user_data;                         /**< User data for callback */
} gfx_touch_config_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Add a touch device (like gfx_display_add; multiple touch devices supported)
 *
 * @param handle Graphics handle from gfx_core_init
 * @param cfg Touch configuration (handle, poll_ms, event_cb, etc.); required
 * @return Touch pointer on success, or NULL on failure
 */
gfx_touch_t *gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg);

/**
 * @brief Bind a display to a touch device
 *
 * @param touch Touch pointer returned from gfx_touch_add
 * @param disp Display to receive touch hit-testing and dispatch
 * @return GFX_OK on success, GFX_ERR_INVALID_ARG if touch is NULL
 */
gfx_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_display_t *disp);

/**
 * @brief Remove a touch device and release its resources.
 *
 * @param touch Touch pointer returned from gfx_touch_add; safe to pass NULL
 */
void gfx_touch_delete(gfx_touch_t *touch);

/**
 * @brief Inject a touch event into a display.
 *
 * This is intended for host simulators, test backends, or input devices that
 * already provide normalized screen coordinates and do not need the ESP LCD
 * touch polling adapter.
 *
 * @param disp Display that receives hit-testing and widget dispatch.
 * @param event Touch event payload in display coordinates.
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_touch_inject(gfx_display_t *disp, const gfx_touch_event_t *event);

#ifdef __cplusplus
}
#endif
