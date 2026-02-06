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
#include "core/gfx_touch.h"
#include "core/gfx_touch_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/* Event bits for synchronization */
#define NEED_DELETE         BIT0
#define DELETE_DONE         BIT1
#define WAIT_FLUSH_DONE     BIT2

/* Animation timer constants */
#define ANIM_NO_TIMER_READY 0xFFFFFFFF

/**********************
 *      TYPEDEFS
 **********************/


/* Core context structure */
typedef struct gfx_core_context {
    /* Timer management */
    struct {
        gfx_timer_mgr_t timer_mgr; /**< Timer manager */
    } timer;                           /**< Timer management */

    /* Synchronization primitives */
    struct {
        SemaphoreHandle_t lock_mutex;  /**< Render mutex for thread safety */
        EventGroupHandle_t event_group; /**< Event group for synchronization */
    } sync;                            /**< Synchronization primitives */
    
    /**< Display list (one per screen, malloc'd) */
    gfx_disp_t *disp;

    /**< Touch state (see gfx_touch_priv.h) */
    gfx_touch_t touch;
} gfx_core_context_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#ifdef __cplusplus
}
#endif
