/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/* Timer callback function type */
typedef void (*gfx_timer_cb_t)(void *);

/* Timer handle type for external use */
typedef void *gfx_timer_handle_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Timer functions
 *====================*/

/**
 * @brief Create a new timer
 * @param handle Player handle
 * @param timer_cb Timer callback function
 * @param period Timer period in milliseconds
 * @param user_data User data passed to callback
 * @return Timer handle, NULL on error
 */
gfx_timer_handle_t gfx_timer_create(void *handle, gfx_timer_cb_t timer_cb, uint32_t period, void *user_data);

/**
 * @brief Delete a timer
 * @param handle Player handle
 * @param timer Timer handle to delete
 */
void gfx_timer_delete(void *handle, gfx_timer_handle_t timer);

/**
 * @brief Pause a timer
 * @param timer Timer handle to pause
 */
void gfx_timer_pause(gfx_timer_handle_t timer);

/**
 * @brief Resume a timer
 * @param timer Timer handle to resume
 */
void gfx_timer_resume(gfx_timer_handle_t timer);


/**
 * @brief Check if a timer is running
 * @param timer_handle Timer handle to check
 * @return true if timer is running, false otherwise
 */
bool gfx_timer_is_running(gfx_timer_handle_t timer_handle);

/**
 * @brief Set timer repeat count
 * @param timer Timer handle to modify
 * @param repeat_count Number of times to repeat (-1 for infinite)
 */
void gfx_timer_set_repeat_count(gfx_timer_handle_t timer, int32_t repeat_count);

/**
 * @brief Set timer period
 * @param timer Timer handle to modify
 * @param period New period in milliseconds
 */
void gfx_timer_set_period(gfx_timer_handle_t timer, uint32_t period);

/**
 * @brief Reset a timer
 * @param timer Timer handle to reset
 */
void gfx_timer_reset(gfx_timer_handle_t timer);

/**
 * @brief Get current system tick
 * @return Current tick value in milliseconds
 */
uint32_t gfx_timer_tick_get(void);

/**
 * @brief Get actual FPS from timer manager
 * @param handle Player handle
 * @return Actual FPS value, 0 if handle is invalid
 */
uint32_t gfx_timer_get_actual_fps(void *handle);

#ifdef __cplusplus
}
#endif
