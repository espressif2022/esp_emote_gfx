/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "core/gfx_core.h"
#include "core/gfx_touch.h"
#include "core/display/gfx_disp_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/runtime/gfx_timer_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   DEFINES
 *********************/
#define NEED_DELETE         BIT0
#define DELETE_DONE         BIT1
#define WAIT_FLUSH_DONE     BIT2

#define GFX_EVENT_INVALIDATE    BIT0
#define GFX_EVENT_ALL           0xFF

#define ANIM_NO_TIMER_READY 0xFFFFFFFF

#define GFX_RENDER_TASK_IDLE_SLEEP_MS  100

/*********************
 *      TYPEDEFS
 *********************/
typedef struct gfx_core_context {
    struct {
        SemaphoreHandle_t render_mutex;      /**< Recursive mutex for render/touch */
        EventGroupHandle_t lifecycle_events; /**< NEED_DELETE / DELETE_DONE / WAIT_FLUSH_DONE */
        EventGroupHandle_t render_events;    /**< GFX_EVENT_INVALIDATE etc. - wake render task */
    } sync;

    gfx_timer_mgr_t timer_mgr;             /**< Timer manager (see gfx_timer_priv.h) */
    gfx_disp_t *disp;                      /**< Display list (one per screen, malloc'd) */
    gfx_touch_t *touch;                    /**< Touch list (multiple touch devices, malloc'd) */
} gfx_core_context_t;

/*********************
 *   INTERNAL API
 *********************/

#ifdef __cplusplus
}
#endif
