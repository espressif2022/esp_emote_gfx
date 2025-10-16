/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_core_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Invalidate an area globally (mark it for redraw)
 * @param handle Graphics handle
 * @param area Pointer to the area to invalidate, or NULL to clear all invalid areas
 *
 * This function adds an area to the global dirty area list.
 * - If area is NULL, clears all invalid areas
 * - Areas are automatically clipped to screen bounds
 * - Overlapping/adjacent areas are merged
 * - If buffer is full, marks entire screen as dirty
 */
void gfx_invalidate_area(gfx_handle_t handle, const gfx_area_t *area);

/**
 * @brief Invalidate an object's area (convenience function)
 * @param obj Pointer to the object to invalidate
 *
 * Marks the entire object bounds as dirty in the global invalidation list.
 */
void gfx_obj_invalidate(gfx_obj_t *obj);

/**
 * @brief Merge overlapping/adjacent dirty areas to minimize redraw regions
 * @param ctx Graphics context containing dirty areas
 */
void gfx_refr_merge_areas(gfx_core_context_t *ctx);

/**
 * @brief Invalidate full screen to trigger initial refresh
 * @param ctx Graphics context
 */
void gfx_invalidate_all(gfx_core_context_t *ctx);

#ifdef __cplusplus
}
#endif
