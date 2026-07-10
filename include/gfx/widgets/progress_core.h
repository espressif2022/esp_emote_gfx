/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared progress-bar value math (0..1000 permille).
 * Object + arena shells keep their own draw/touch wiring.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_PROGRESS_CORE_MAX 1000U

uint16_t gfx_progress_core_clamp(uint16_t permille);

/** Horizontal: local_x in [0, length] → permille. */
uint16_t gfx_progress_core_from_local_x(int32_t local_x, int32_t length);

/**
 * Vertical (bottom=0, top=max): local_y from track top → permille.
 * Matches gfx_progress_bar vertical mapping.
 */
uint16_t gfx_progress_core_from_local_y_vertical(int32_t local_y, int32_t length);

/** Fill pixel extent with ceil-ish rounding; at least 1 when permille > 0. */
int32_t gfx_progress_core_fill_extent(int32_t inner, uint16_t permille);

#ifdef __cplusplus
}
#endif
