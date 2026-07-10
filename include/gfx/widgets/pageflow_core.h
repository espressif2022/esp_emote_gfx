/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared pageflow math for object + arena shells.
 * Unlike coverflow, page index commits when tween starts (gfx_pageflow).
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_PAGEFLOW_CORE_TWEEN_MS       180U
#define GFX_PAGEFLOW_CORE_DRAG_THRESHOLD 6U
#define GFX_PAGEFLOW_CORE_PAGE_THRESHOLD 48U

typedef struct {
    int32_t page_index;     /* committed page after plan */
    int32_t start_offset;   /* tween from */
    int32_t end_offset;     /* always 0 */
    bool    page_changed;
} gfx_pageflow_core_tween_plan_t;

int32_t gfx_pageflow_core_clamp_page(uint16_t page_count, int32_t page);

int32_t gfx_pageflow_core_page_target(uint16_t page_count, int32_t page_index,
                                      int32_t drag_offset, uint16_t page_threshold);

/**
 * Plan release tween. If target != current, page commits immediately and
 * start_offset is shifted by ±span (widget width/height).
 */
void gfx_pageflow_core_plan_tween(uint16_t page_count, int32_t page_index, int32_t target_page,
                                  int32_t span, int32_t drag_offset,
                                  gfx_pageflow_core_tween_plan_t *out);

/** Slot page area origin offset along main axis. */
int32_t gfx_pageflow_core_slot_offset(int32_t slot, int32_t span, int32_t drag_offset);

#ifdef __cplusplus
}
#endif
