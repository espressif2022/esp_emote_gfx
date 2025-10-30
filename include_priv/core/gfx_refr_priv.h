/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_core_priv.h"

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

/* Area utility functions (merged from gfx_area.h) */
/**
 * @brief Copy area from src to dest
 * @param dest Destination area
 * @param src Source area
 */
void gfx_area_copy(gfx_area_t *dest, const gfx_area_t *src);

/**
 * @brief Check if area_in is fully contained within area_parent
 * @param area_in Area to check
 * @param area_parent Parent area
 * @return true if area_in is completely inside area_parent
 */
bool gfx_area_is_in(const gfx_area_t *area_in, const gfx_area_t *area_parent);

/**
 * @brief Get intersection of two areas
 * @param result Result area (intersection)
 * @param a1 First area
 * @param a2 Second area
 * @return true if areas intersect, false otherwise
 */
bool gfx_area_intersect(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2);

/**
 * @brief Get the size (area) of a rectangular region
 * @param area Area to calculate size for
 * @return Size in pixels (width * height)
 */
uint32_t gfx_area_get_size(const gfx_area_t *area);

/**
 * @brief Check if two areas are on each other (overlap or touch)
 * @param a1 First area
 * @param a2 Second area
 * @return true if areas overlap or are adjacent (touch)
 */
bool gfx_area_is_on(const gfx_area_t *a1, const gfx_area_t *a2);

/**
 * @brief Join two areas into a larger area (bounding box)
 * @param result Result area (bounding box of a1 and a2)
 * @param a1 First area
 * @param a2 Second area
 */
void gfx_area_join(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2);

#ifdef __cplusplus
}
#endif


