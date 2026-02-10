/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   DEFAULT CONFIG MACRO
 *********************/
/** Use as .task = GFX_EMOTE_INIT_CONFIG() when initializing gfx_core_config_t */
#define GFX_EMOTE_INIT_CONFIG()                   \
    {                                              \
        .task_priority = 4,                        \
        .task_stack = 7168,                        \
        .task_affinity = -1,                       \
        .task_stack_caps = MALLOC_CAP_DEFAULT,     \
    }

/*********************
 *   CONFIG STRUCTS
 *********************/
/** Passed to gfx_emote_init(); add displays with gfx_disp_add() after init */
typedef struct {
    uint32_t fps;                               /**< Target FPS (frames per second) */
    struct {
        int task_priority;                       /**< Render task priority (1–20) */
        int task_stack;                         /**< Render task stack size (bytes) */
        int task_affinity;                       /**< CPU core (-1: any, 0/1: pinned) */
        unsigned task_stack_caps;                /**< Stack heap caps (see esp_heap_caps.h) */
    } task;
} gfx_core_config_t;

/**********************
 *   GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Core initialization
 *====================*/

/**
 * @brief Initialize graphics context
 *
 * @param cfg Core configuration (gfx_core_config_t): fps, task. Add displays with gfx_disp_add() and gfx_disp_config_t.
 * @return gfx_handle_t Graphics handle, NULL on error
 *
 * @note gfx_core_config_t fields: fps, task (priority, stack, affinity, stack_caps).
 *       Resolution, buffers and flush callback are per-display; see gfx_disp_config_t and gfx_disp_add().
 */
gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg);

/**
 * @brief Deinitialize graphics context
 *
 * @param handle Graphics handle
 */
void gfx_emote_deinit(gfx_handle_t handle);

/**
 * @brief Lock the recursive render mutex to prevent rendering during external operations
 *
 * @param handle Graphics handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t gfx_emote_lock(gfx_handle_t handle);

/**
 * @brief Unlock the recursive render mutex after external operations
 *
 * @param handle Graphics handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t gfx_emote_unlock(gfx_handle_t handle);


#ifdef __cplusplus
}
#endif
