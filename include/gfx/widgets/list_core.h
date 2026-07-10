/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared list scroll math for object + arena shells.
 * No gfx_object_t / arena types here.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_LIST_CORE_DRAG_THRESHOLD    6
#define GFX_LIST_CORE_OVERSCROLL_PX     36
#define GFX_LIST_CORE_INERTIA_MIN_V     80
#define GFX_LIST_CORE_INERTIA_FRICTION  880
#define GFX_LIST_CORE_BOUNCE_STEP_DIV   4

int32_t gfx_list_core_max_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count);

int32_t gfx_list_core_clamp_scroll_y(int32_t height, uint16_t item_height, uint16_t item_count,
                                     int32_t scroll_y, bool allow_overscroll);

int32_t gfx_list_core_snap_scroll_y(int32_t scroll_y, uint16_t item_height);

/** Index from content-space y (= local_y + scroll_y); -1 if OOB. */
int32_t gfx_list_core_index_from_content_y(int32_t content_y, uint16_t item_height,
                                           uint16_t item_count);

int32_t gfx_list_core_index_from_point(int32_t widget_y1, int32_t y, int32_t scroll_y,
                                       uint16_t item_height, uint16_t item_count);

uint32_t gfx_list_core_clamp_dt(uint32_t dt_ms);

int32_t gfx_list_core_inertia_delta(int32_t velocity_y, uint32_t dt_ms);

/** Apply friction toward 0; returns updated velocity. */
int32_t gfx_list_core_apply_friction(int32_t velocity_y, uint32_t dt_ms);

bool gfx_list_core_inertia_should_stop(int32_t velocity_y, int32_t scroll_y, int32_t max_scroll);

/** One bounce step toward [0, max]; returns new scroll_y. */
int32_t gfx_list_core_bounce_step(int32_t scroll_y, int32_t max_scroll);

/**
 * Advance inertia + bounce + optional snap.
 * Updates scroll_y / velocity_y / last_ms / inertia in place.
 * Returns true if still animating (caller should keep ticking).
 * *changed set if scroll_y mutated.
 */
bool gfx_list_core_anim_step(int32_t height, uint16_t item_height, uint16_t item_count,
                             int32_t *scroll_y, int32_t *velocity_y, uint32_t *last_ms,
                             bool *inertia, bool pressed, bool snap_to_item,
                             uint32_t now_ms, bool *changed);

#ifdef __cplusplus
}
#endif
