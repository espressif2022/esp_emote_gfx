/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/widgets/progress_core.h"

uint16_t gfx_progress_core_clamp(uint16_t permille)
{
    return permille > GFX_PROGRESS_CORE_MAX ? GFX_PROGRESS_CORE_MAX : permille;
}

uint16_t gfx_progress_core_from_local_x(int32_t local_x, int32_t length)
{
    if (length <= 0) {
        return 0;
    }
    if (local_x <= 0) {
        return 0;
    }
    if (local_x >= length) {
        return GFX_PROGRESS_CORE_MAX;
    }
    return (uint16_t)(((uint32_t)local_x * GFX_PROGRESS_CORE_MAX) / (uint32_t)length);
}

uint16_t gfx_progress_core_from_local_y_vertical(int32_t local_y, int32_t length)
{
    if (length <= 0) {
        return 0;
    }
    if (local_y <= 0) {
        return GFX_PROGRESS_CORE_MAX;
    }
    if (local_y >= length) {
        return 0;
    }
    return (uint16_t)(((uint32_t)(length - local_y) * GFX_PROGRESS_CORE_MAX) / (uint32_t)length);
}

int32_t gfx_progress_core_fill_extent(int32_t inner, uint16_t permille)
{
    int32_t fill;

    if (inner <= 0) {
        return 0;
    }
    permille = gfx_progress_core_clamp(permille);
    fill = (int32_t)(((uint32_t)inner * (uint32_t)permille + (GFX_PROGRESS_CORE_MAX - 1U)) /
                     GFX_PROGRESS_CORE_MAX);
    if (permille > 0U && fill == 0) {
        fill = 1;
    }
    if (fill > inner) {
        fill = inner;
    }
    return fill;
}
