/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/widgets/pageflow_core.h"

#include <stddef.h>

int32_t gfx_pageflow_core_clamp_page(uint16_t page_count, int32_t page)
{
    if (page_count == 0U) {
        return 0;
    }
    if (page < 0) {
        return 0;
    }
    if (page >= (int32_t)page_count) {
        return (int32_t)page_count - 1;
    }
    return page;
}

int32_t gfx_pageflow_core_page_target(uint16_t page_count, int32_t page_index,
                                      int32_t drag_offset, uint16_t page_threshold)
{
    int32_t target = gfx_pageflow_core_clamp_page(page_count, page_index);

    if (page_count == 0U) {
        return 0;
    }
    if (drag_offset <= -(int32_t)page_threshold) {
        target = page_index + 1;
    } else if (drag_offset >= (int32_t)page_threshold) {
        target = page_index - 1;
    }
    return gfx_pageflow_core_clamp_page(page_count, target);
}

void gfx_pageflow_core_plan_tween(uint16_t page_count, int32_t page_index, int32_t target_page,
                                  int32_t span, int32_t drag_offset,
                                  gfx_pageflow_core_tween_plan_t *out)
{
    if (out == NULL) {
        return;
    }

    page_index = gfx_pageflow_core_clamp_page(page_count, page_index);
    target_page = gfx_pageflow_core_clamp_page(page_count, target_page);
    if (span <= 0) {
        span = GFX_PAGEFLOW_CORE_PAGE_THRESHOLD;
    }

    out->page_changed = (target_page != page_index);
    out->page_index = target_page;
    out->end_offset = 0;
    out->start_offset = drag_offset;

    if (out->page_changed) {
        out->start_offset = (target_page > page_index) ? (span + drag_offset)
                            : (drag_offset - span);
    }
}

int32_t gfx_pageflow_core_slot_offset(int32_t slot, int32_t span, int32_t drag_offset)
{
    return slot * span + drag_offset;
}
