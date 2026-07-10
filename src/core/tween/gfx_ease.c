/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/ease.h"

#define GFX_EASE_Q 1024U

int32_t gfx_ease_out_quad_i32(int32_t from, int32_t to,
                              uint32_t elapsed_ms, uint32_t duration_ms)
{
    uint32_t t;
    uint32_t inv;
    uint32_t progress;

    if (duration_ms == 0U || elapsed_ms >= duration_ms || from == to) {
        return to;
    }
    t = (elapsed_ms * GFX_EASE_Q) / duration_ms;
    if (t > GFX_EASE_Q) {
        t = GFX_EASE_Q;
    }
    inv = GFX_EASE_Q - t;
    progress = GFX_EASE_Q - (inv * inv) / GFX_EASE_Q;
    return from + (int32_t)(((int64_t)(to - from) * (int64_t)progress) / (int64_t)GFX_EASE_Q);
}
