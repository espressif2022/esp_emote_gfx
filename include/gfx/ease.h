/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Lightweight easing helpers shared by widget cores / arena tick.
 * Independent of gfx_object_t / gfx_tween.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ease-out quad lerp matching GFX_TWEEN_EASE_OUT_QUAD. */
int32_t gfx_ease_out_quad_i32(int32_t from, int32_t to,
                              uint32_t elapsed_ms, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
