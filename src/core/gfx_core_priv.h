/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

#include "core/gfx_core.h"
#include "core/gfx_disp_priv.h"
#include "core/gfx_timer_priv.h"
#include "core/gfx_obj_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   DEFINES
 *********************/
/* Event bits (sync.lifecycle_events) */
#define NEED_DELETE         BIT0
#define DELETE_DONE         BIT1
#define WAIT_FLUSH_DONE     BIT2

/* Animation: no timer ready yet */
#define ANIM_NO_TIMER_READY 0xFFFFFFFF

/*********************
 *   CONTEXT STRUCT
 *********************/
typedef struct gfx_core_context {
    struct {
        SemaphoreHandle_t render_mutex;      /**< Recursive mutex for render/touch */
        EventGroupHandle_t lifecycle_events; /**< NEED_DELETE / DELETE_DONE / WAIT_FLUSH_DONE */
    } sync;

    gfx_timer_mgr_t timer_mgr;         /**< Timer manager (see gfx_timer_priv.h) */
    gfx_disp_t *disp;                  /**< Display list (one per screen, malloc'd) */
    gfx_touch_t *touch;                /**< Touch list (multiple touch devices, malloc'd) */
} gfx_core_context_t;

#ifdef __cplusplus
}
#endif
