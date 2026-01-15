/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_core_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle rendering of all objects in the scene
 * @param ctx Player context
 * @return true if rendering was performed, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx);

/**
 * @brief Render all dirty areas
 * @param ctx Graphics context
 * @return Total number of flush operations
 */
uint32_t gfx_render_dirty_areas(gfx_core_context_t *ctx);

/**
 * @brief Render a single dirty area with dynamic height-based blocking
 * @param ctx Graphics context
 * @param area Area to render
 * @param area_idx Area index for logging
 * @param start_block_count Starting block count for logging
 * @return Number of flush operations performed
 */
uint32_t gfx_render_part_area(gfx_core_context_t *ctx, gfx_area_t *area,
                              uint8_t area_idx, uint32_t start_block_count);

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags
 * @param ctx Graphics context
 */
void gfx_render_cleanup(gfx_core_context_t *ctx);

/**
 * @brief Print summary of dirty areas
 * @param ctx Graphics context
 * @return Total dirty pixels
 */
uint32_t gfx_render_area_summary(gfx_core_context_t *ctx);

/**
 * @brief Draw child objects in the specified area
 * @param ctx Graphics context
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
 * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 */
void gfx_render_draw_child_objects(gfx_core_context_t *ctx, int x1, int y1, int x2, int y2, const void *dest_buf);


/**
 * @brief Update child objects
 * @param ctx Graphics context
 */
void gfx_render_update_child_objects(gfx_core_context_t *ctx);

#ifdef __cplusplus
}
#endif
