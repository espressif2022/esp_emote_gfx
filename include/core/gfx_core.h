/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
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
        .task_stack_caps = MALLOC_CAP_DEFAULT,     \
    }

/*********************
 *      TYPEDEFS
 *********************/
/** Passed to gfx_core_init(); add displays with gfx_disp_add() after init */
typedef struct {
    uint32_t fps;                               /**< Target FPS (frames per second) */
    struct {
        uint32_t task_priority;                  /**< Render task priority (1-20) */
        uint32_t task_stack;                     /**< Render task stack size (bytes) */
        int32_t task_affinity;                   /**< CPU core (-1: any, 0/1: pinned) */
        uint32_t task_stack_caps;                /**< Stack heap caps (see esp_heap_caps.h) */
    } task;
} gfx_core_config_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Initialize graphics context
 *
 * @param cfg Core configuration (gfx_core_config_t): fps, task. Add displays with gfx_disp_add() and gfx_disp_config_t.
 * @return gfx_handle_t Graphics handle, NULL on error
 *
 * @note gfx_core_config_t fields: fps, task (priority, stack, affinity, stack_caps).
 *       Resolution, buffers and flush callback are per-display; see gfx_disp_config_t and gfx_disp_add().
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
 * @return ESP_OK on success, or an ESP_ERR_* code on failure.
 */
esp_err_t gfx_core_lock(gfx_handle_t handle);

/**
 * @brief Unlock the recursive render mutex after external operations
 *
 * @param handle Graphics handle
 * @return ESP_OK on success, or an ESP_ERR_* code on failure.
 */
esp_err_t gfx_core_unlock(gfx_handle_t handle);

/**
 * @brief Perform one synchronous refresh (render and flush) immediately.
 *        Holds the render mutex for the duration; safe to call from any task.
 *
 * @param handle Graphics handle
 * @return ESP_OK on success, or an ESP_ERR_* code on failure.
 */
esp_err_t gfx_core_refresh_now(gfx_handle_t handle);

#ifdef __cplusplus
}
#endif
