/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared wheel math for object + arena shells.
 * Scroll convention (object-canonical):
 *   center_scroll(index) = index * item_height - (height - item_height) / 2
 * No gfx_object_t / arena types here.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_WHEEL_CORE_TWEEN_MS         160U
#define GFX_WHEEL_CORE_DRAG_THRESHOLD   6U
#define GFX_WHEEL_CORE_DEFAULT_ITEM_H   40U
#define GFX_WHEEL_CORE_DEFAULT_ROWS     5U

int32_t gfx_wheel_core_clamp_index(uint16_t item_count, int32_t index, bool cyclic);

int32_t gfx_wheel_core_center_scroll_y(int32_t height, uint16_t item_height, int32_t index);

int32_t gfx_wheel_core_max_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count);

int32_t gfx_wheel_core_clamp_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                      bool cyclic, int32_t scroll_y);

/** Clamped index from scroll; -1 if item_height/count invalid. */
int32_t gfx_wheel_core_index_from_scroll(int32_t height, uint16_t item_height, uint16_t item_count,
                                         bool cyclic, int32_t scroll_y);

/** Unclamped visual center index (for draw window). */
int32_t gfx_wheel_core_raw_index_from_scroll(int32_t height, uint16_t item_height, int32_t scroll_y);

/** Tap: map local_y within widget to item index. */
int32_t gfx_wheel_core_index_from_local_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                          bool cyclic, int32_t scroll_y, int32_t local_y);

/** Ease-out quad lerp (matches GFX_TWEEN_EASE_OUT_QUAD). */
int32_t gfx_wheel_core_ease_out_quad_i32(int32_t from, int32_t to,
                                         uint32_t elapsed_ms, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
