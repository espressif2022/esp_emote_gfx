/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/widgets/list_core.h"

#include <stddef.h>

static int32_t abs_i32(int32_t v)
{
    return v < 0 ? -v : v;
}

int32_t gfx_list_core_max_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count)
{
    int32_t content_h;

    if (item_height == 0U || item_count == 0U) {
        return 0;
    }
    content_h = (int32_t)item_count * (int32_t)item_height;
    if (content_h <= height) {
        return 0;
    }
    return content_h - height;
}

int32_t gfx_list_core_clamp_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                     int32_t scroll_y, bool allow_overscroll)
{
    int32_t max_scroll = gfx_list_core_max_scroll_y(height, item_height, item_count);

    if (allow_overscroll) {
        int32_t over = GFX_LIST_CORE_OVERSCROLL_PX;
        int32_t h3 = height / 3;
        if (h3 > 0 && h3 < over) {
            over = h3;
        }
        if (over < 0) {
            over = 0;
        }
        if (scroll_y < -over) {
            return -over;
        }
        if (scroll_y > max_scroll + over) {
            return max_scroll + over;
        }
        return scroll_y;
    }

    if (scroll_y < 0) {
        return 0;
    }
    if (scroll_y > max_scroll) {
        return max_scroll;
    }
    return scroll_y;
}

int32_t gfx_list_core_snap_scroll_y(int32_t scroll_y, uint16_t item_height)
{
    if (item_height == 0U) {
        return scroll_y;
    }
    return ((scroll_y + (int32_t)item_height / 2) / (int32_t)item_height) * (int32_t)item_height;
}

int32_t gfx_list_core_index_from_content_y(int32_t content_y, uint16_t item_height,
                                           uint16_t item_count)
{
    int32_t index;

    if (item_height == 0U || item_count == 0U || content_y < 0) {
        return -1;
    }
    index = content_y / (int32_t)item_height;
    return (index >= 0 && index < (int32_t)item_count) ? index : -1;
}

int32_t gfx_list_core_index_from_point(int32_t widget_y1, int32_t y, int32_t scroll_y,
                                       uint16_t item_height, uint16_t item_count)
{
    return gfx_list_core_index_from_content_y(y - widget_y1 + scroll_y, item_height, item_count);
}

uint32_t gfx_list_core_clamp_dt(uint32_t dt_ms)
{
    if (dt_ms == 0U) {
        return 16U;
    }
    if (dt_ms > 48U) {
        return 48U;
    }
    return dt_ms;
}

int32_t gfx_list_core_inertia_delta(int32_t velocity_y, uint32_t dt_ms)
{
    return (velocity_y * (int32_t)dt_ms) / 1000;
}

int32_t gfx_list_core_apply_friction(int32_t velocity_y, uint32_t dt_ms)
{
    int32_t friction = (GFX_LIST_CORE_INERTIA_FRICTION * (int32_t)dt_ms) / 1000;

    if (velocity_y > 0) {
        velocity_y -= friction;
        if (velocity_y < 0) {
            velocity_y = 0;
        }
    } else {
        velocity_y += friction;
        if (velocity_y > 0) {
            velocity_y = 0;
        }
    }
    return velocity_y;
}

bool gfx_list_core_inertia_should_stop(int32_t velocity_y, int32_t scroll_y, int32_t max_scroll)
{
    return abs_i32(velocity_y) < GFX_LIST_CORE_INERTIA_MIN_V ||
           scroll_y < 0 || scroll_y > max_scroll;
}

int32_t gfx_list_core_bounce_step(int32_t scroll_y, int32_t max_scroll)
{
    int32_t target = scroll_y < 0 ? 0 : max_scroll;
    int32_t diff = target - scroll_y;
    int32_t step = diff / GFX_LIST_CORE_BOUNCE_STEP_DIV;

    if (step == 0) {
        step = diff > 0 ? 1 : -1;
    }
    if (abs_i32(diff) <= 1) {
        return target;
    }
    return scroll_y + step;
}

bool gfx_list_core_anim_step(int32_t height, uint16_t item_height, uint16_t item_count,
                             int32_t *scroll_y, int32_t *velocity_y, uint32_t *last_ms,
                             bool *inertia, bool pressed, bool snap_to_item,
                             uint32_t now_ms, bool *changed)
{
    const int32_t min_scroll = 0;
    int32_t max_scroll;
    uint32_t dt;
    bool did_change = false;

    if (scroll_y == NULL || velocity_y == NULL || last_ms == NULL || inertia == NULL) {
        return false;
    }

    max_scroll = gfx_list_core_max_scroll_y(height, item_height, item_count);
    if (*last_ms == 0U) {
        *last_ms = now_ms;
    }
    dt = gfx_list_core_clamp_dt(now_ms - *last_ms);
    *last_ms = now_ms;

    if (*inertia) {
        int32_t delta = gfx_list_core_inertia_delta(*velocity_y, dt);
        if (delta != 0) {
            int32_t next = gfx_list_core_clamp_scroll_y(height, item_height, item_count,
                           *scroll_y + delta, true);
            if (next != *scroll_y) {
                *scroll_y = next;
                did_change = true;
            }
        }
        *velocity_y = gfx_list_core_apply_friction(*velocity_y, dt);
        if (gfx_list_core_inertia_should_stop(*velocity_y, *scroll_y, max_scroll)) {
            *inertia = false;
        }
    }

    if (!pressed && (*scroll_y < min_scroll || *scroll_y > max_scroll)) {
        int32_t target = *scroll_y < min_scroll ? min_scroll : max_scroll;
        int32_t next = gfx_list_core_bounce_step(*scroll_y, max_scroll);
        if (abs_i32(target - *scroll_y) <= 1) {
            next = gfx_list_core_clamp_scroll_y(height, item_height, item_count, target, false);
            if (snap_to_item) {
                next = gfx_list_core_snap_scroll_y(next, item_height);
                next = gfx_list_core_clamp_scroll_y(height, item_height, item_count, next, false);
            }
        } else {
            next = gfx_list_core_clamp_scroll_y(height, item_height, item_count, next, true);
        }
        if (next != *scroll_y) {
            *scroll_y = next;
            did_change = true;
        }
    } else if (!(*inertia) && !pressed && snap_to_item) {
        int32_t next = gfx_list_core_snap_scroll_y(*scroll_y, item_height);
        next = gfx_list_core_clamp_scroll_y(height, item_height, item_count, next, false);
        if (next != *scroll_y) {
            *scroll_y = next;
            did_change = true;
        }
    }

    if (changed != NULL) {
        *changed = did_change;
    }
    return *inertia ||
           (!pressed && (*scroll_y < min_scroll || *scroll_y > max_scroll));
}
