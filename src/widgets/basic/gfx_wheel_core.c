/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/widgets/wheel_core.h"

#include "gfx/ease.h"

int32_t gfx_wheel_core_clamp_index(uint16_t item_count, int32_t index, bool cyclic)
{
    if (item_count == 0U) {
        return 0;
    }

    if (cyclic) {
        int32_t count = (int32_t)item_count;
        index %= count;
        return index < 0 ? index + count : index;
    }

    if (index < 0) {
        return 0;
    }
    if (index >= (int32_t)item_count) {
        return (int32_t)item_count - 1;
    }
    return index;
}

int32_t gfx_wheel_core_center_scroll_y(int32_t height, uint16_t item_height, int32_t index)
{
    if (item_height == 0U) {
        return 0;
    }
    return index * (int32_t)item_height - (height - (int32_t)item_height) / 2;
}

int32_t gfx_wheel_core_max_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count)
{
    int32_t max;

    if (item_count == 0U || item_height == 0U) {
        return 0;
    }
    max = gfx_wheel_core_center_scroll_y(height, item_height, (int32_t)item_count - 1);
    return max > 0 ? max : 0;
}

int32_t gfx_wheel_core_clamp_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                      bool cyclic, int32_t scroll_y)
{
    int32_t max_scroll_y;

    if (cyclic) {
        return scroll_y;
    }
    if (scroll_y < 0) {
        return 0;
    }
    max_scroll_y = gfx_wheel_core_max_scroll_y(height, item_height, item_count);
    if (scroll_y > max_scroll_y) {
        return max_scroll_y;
    }
    return scroll_y;
}

int32_t gfx_wheel_core_index_from_scroll(int32_t height, uint16_t item_height, uint16_t item_count,
                                         bool cyclic, int32_t scroll_y)
{
    int32_t numerator;
    int32_t index;

    if (item_height == 0U || item_count == 0U) {
        return -1;
    }

    numerator = scroll_y + (height - (int32_t)item_height) / 2;
    if (numerator >= 0) {
        index = (numerator + (int32_t)item_height / 2) / (int32_t)item_height;
    } else {
        index = (numerator - (int32_t)item_height / 2) / (int32_t)item_height;
    }
    return gfx_wheel_core_clamp_index(item_count, index, cyclic);
}

int32_t gfx_wheel_core_raw_index_from_scroll(int32_t height, uint16_t item_height, int32_t scroll_y)
{
    int32_t numerator;

    if (item_height == 0U) {
        return 0;
    }

    numerator = scroll_y + (height - (int32_t)item_height) / 2;
    if (numerator >= 0) {
        return (numerator + (int32_t)item_height / 2) / (int32_t)item_height;
    }
    return (numerator - (int32_t)item_height / 2) / (int32_t)item_height;
}

int32_t gfx_wheel_core_index_from_local_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                          bool cyclic, int32_t scroll_y, int32_t local_y)
{
    int32_t numerator;
    int32_t index;

    if (item_height == 0U || item_count == 0U || local_y < 0 || local_y >= height) {
        return -1;
    }

    /* Row i occupies [i*h - scroll, (i+1)*h - scroll); use floor, not round. */
    numerator = scroll_y + local_y;
    if (numerator >= 0) {
        index = numerator / (int32_t)item_height;
    } else {
        index = -((-numerator + (int32_t)item_height - 1) / (int32_t)item_height);
    }
    return gfx_wheel_core_clamp_index(item_count, index, cyclic);
}

int32_t gfx_wheel_core_ease_out_quad_i32(int32_t from, int32_t to,
                                         uint32_t elapsed_ms, uint32_t duration_ms)
{
    return gfx_ease_out_quad_i32(from, to, elapsed_ms, duration_ms);
}
