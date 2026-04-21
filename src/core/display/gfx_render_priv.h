/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/runtime/gfx_core_priv.h"

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
void gfx_render_dirty_areas(gfx_disp_t *disp);

/**
 * @brief Render a single dirty area with dynamic height-based blocking
 * @param is_last_area true if this is the last dirty area in the list (flushing_last = last chunk of this area AND is_last_area)
 */
void gfx_render_part_area(gfx_disp_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area);

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
 * @brief Draw child objects for one display using draw context (buf_area + clip_area)
 * @param ctx Draw context: buf, buf_area, clip_area, stride, swap
 *            buf_area and clip_area use half-open bounds [x1, x2) x [y1, y2)
 */
void gfx_render_draw_child_objects(gfx_disp_t *disp, const gfx_draw_ctx_t *ctx);

/**
 * @brief Update child objects for one display
 */
void gfx_render_update_child_objects(gfx_disp_t *disp);

#ifdef __cplusplus
}
#endif
