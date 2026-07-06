/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/core.h"
#include "gfx/input.h"
#include "core/display/gfx_display_priv.h"
#include "core/object/gfx_object_priv.h"
#include "core/runtime/gfx_timer_priv.h"
#include "platform/gfx_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   DEFINES
 *********************/
#define NEED_DELETE         (1U << 0)
#define DELETE_DONE         (1U << 1)
#define WAIT_FLUSH_DONE     (1U << 2)

#define GFX_EVENT_INVALIDATE    (1U << 0)
#define GFX_EVENT_ALL           0xFF

#define ANIM_NO_TIMER_READY 0xFFFFFFFF

#define GFX_RENDER_TASK_IDLE_SLEEP_MS  100

/*********************
 *      TYPEDEFS
 *********************/
typedef struct gfx_core_context {
    struct {
        gfx_platform_mutex_t render_mutex;   /**< Recursive mutex for render/touch */
        gfx_platform_event_t lifecycle_events; /**< NEED_DELETE / DELETE_DONE / WAIT_FLUSH_DONE */
        gfx_platform_event_t render_events;  /**< GFX_EVENT_INVALIDATE etc. - wake render task */
    } sync;

    gfx_timer_mgr_t timer_mgr;             /**< Timer manager (see gfx_timer_priv.h) */
    gfx_display_t *disp;                      /**< Display list (one per screen, malloc'd) */
    gfx_touch_t *touch;                    /**< Touch list (multiple touch devices, malloc'd) */
    bool manual_tick;                      /**< Caller drives timers/render via gfx_core_tick() */
} gfx_core_context_t;

/*********************
 *   INTERNAL API
 *********************/

#ifdef __cplusplus
}
#endif
