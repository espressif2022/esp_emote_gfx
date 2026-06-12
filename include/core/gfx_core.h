/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "core/gfx_err.h"
#include "gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
/** Use as .task = GFX_CORE_TASK_DEFAULT_CONFIG() when initializing gfx_core_config_t */
#define GFX_CORE_TASK_DEFAULT_CONFIG()                   \
    {                                              \
        .task_priority = 4,                        \
        .task_stack = 7168,                        \
        .task_affinity = -1,                       \
        .task_stack_caps = GFX_CORE_TASK_STACK_CAP_DEFAULT, \
    }

#define GFX_CORE_TASK_STACK_CAP_DEFAULT 0x00000001U
#define GFX_CORE_TASK_STACK_CAP_INTERNAL 0x00000008U

/*********************
 *      TYPEDEFS
 *********************/
/** Passed to gfx_core_init(); add displays with gfx_display_add() after init */
typedef struct {
    uint32_t fps;                               /**< Target FPS (frames per second) */
    bool manual_tick;                           /**< True when caller drives gfx_core_tick() */
    struct {
        uint32_t task_priority;                  /**< Render task priority (1-20) */
        uint32_t task_stack;                     /**< Render task stack size (bytes) */
        int32_t task_affinity;                   /**< CPU core (-1: any, 0/1: pinned) */
        uint32_t task_stack_caps;                /**< Platform stack heap caps; 0 uses default */
    } task;
} gfx_core_config_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Initialize graphics context
 *
 * @param cfg Core configuration (gfx_core_config_t): fps, task. Add displays with gfx_display_add() and gfx_display_config_t.
 * @return gfx_handle_t Graphics handle, NULL on error
 *
 * @note gfx_core_config_t fields: fps, task (priority, stack, affinity, stack_caps).
 *       Resolution, buffers and flush callback are per-display; see gfx_display_config_t and gfx_display_add().
 */
gfx_handle_t gfx_core_init(const gfx_core_config_t *cfg);

/**
 * @brief Deinitialize graphics context
 *
 * @param handle Graphics handle
 */
void gfx_core_deinit(gfx_handle_t handle);

/**
 * @brief Lock the recursive render mutex to prevent rendering during external operations
 *
 * @param handle Graphics handle
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_core_lock(gfx_handle_t handle);

/**
 * @brief Unlock the recursive render mutex after external operations
 *
 * @param handle Graphics handle
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_core_unlock(gfx_handle_t handle);

/**
 * @brief Perform one synchronous refresh (render and flush) immediately.
 *        Holds the render mutex for the duration; safe to call from any task.
 *
 * @param handle Graphics handle
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_core_refresh_now(gfx_handle_t handle);

/**
 * @brief Perform one synchronous runtime tick.
 *
 * This executes due timers and refreshes dirty displays when needed. It is
 * useful for host simulators whose window backend must be pumped from the
 * application's main thread.
 *
 * @param handle Graphics handle
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_core_tick(gfx_handle_t handle);

#ifdef __cplusplus
}
#endif
