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
 * @brief Handle rendering of all objects in the scene (iterates over all displays)
 * @param ctx Player context
 * @return true if rendering was performed, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx);

/**
 * @brief Render all dirty areas for one display
 */
uint32_t gfx_render_dirty_areas(gfx_disp_t *disp);

/**
 * @brief Render a single dirty area with dynamic height-based blocking
 */
uint32_t gfx_render_part_area(gfx_disp_t *disp, gfx_area_t *area,
                              uint8_t area_idx, uint32_t start_block_count);

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags for one display
 */
void gfx_render_cleanup(gfx_disp_t *disp);

/**
 * @brief Print summary of dirty areas for one display
 * @return Total dirty pixels
 */
uint32_t gfx_render_area_summary(gfx_disp_t *disp);

/**
 * @brief Draw child objects in the specified area for one display
 */
void gfx_render_draw_child_objects(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *dest_buf);

/**
 * @brief Update child objects for one display
 */
void gfx_render_update_child_objects(gfx_disp_t *disp);

#ifdef __cplusplus
}
#endif
