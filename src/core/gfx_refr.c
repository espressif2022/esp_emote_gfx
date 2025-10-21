/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "core/gfx_refr.h"
#include "core/gfx_area.h"

static const char *TAG = "gfx_refr";

void gfx_refr_merge_areas(gfx_core_context_t *ctx)
{
    uint32_t src_idx;
    uint32_t dst_idx;
    gfx_area_t merged_area;

    memset(ctx->disp.area_merged, 0, sizeof(ctx->disp.area_merged));

    for (dst_idx = 0; dst_idx < ctx->disp.dirty_count; dst_idx++) {
        if (ctx->disp.area_merged[dst_idx] != 0) {
            continue;
        }

        /* Check all areas to merge them into 'dst_idx' */
        for (src_idx = 0; src_idx < ctx->disp.dirty_count; src_idx++) {
            /* Handle only unmerged areas and ignore itself */
            if (ctx->disp.area_merged[src_idx] != 0 || dst_idx == src_idx) {
                continue;
            }

            /* Check if the areas are on each other (overlap or adjacent) */
            if (!gfx_area_is_on(&ctx->disp.dirty_areas[dst_idx], &ctx->disp.dirty_areas[src_idx])) {
                continue;
            }

            /* Create merged area */
            gfx_area_join(&merged_area, &ctx->disp.dirty_areas[dst_idx], &ctx->disp.dirty_areas[src_idx]);

            /* Merge two areas only if the merged area size is smaller than the sum
             * This prevents unnecessary merging of areas that would waste rendering */
            uint32_t merged_size = gfx_area_get_size(&merged_area);
            uint32_t separate_size = gfx_area_get_size(&ctx->disp.dirty_areas[dst_idx]) +
                                     gfx_area_get_size(&ctx->disp.dirty_areas[src_idx]);

            if (merged_size < separate_size) {
                gfx_area_copy(&ctx->disp.dirty_areas[dst_idx], &merged_area);

                /* Mark 'src_idx' as merged into 'dst_idx' */
                ctx->disp.area_merged[src_idx] = 1;

                // ESP_LOGI(TAG, "Merged area [%d] into [%d], saved %lu pixels",
                //          src_idx, dst_idx, separate_size - merged_size);
            }
        }
    }
}

void gfx_invalidate_area(gfx_handle_t handle, const gfx_area_t *area_p)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is NULL");
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;

    if (area_p == NULL) {
        ctx->disp.dirty_count = 0;
        memset(ctx->disp.area_merged, 0, sizeof(ctx->disp.area_merged));
        ESP_LOGD(TAG, "Cleared all dirty areas");
        return;
    }

    gfx_area_t screen_area;
    screen_area.x1 = 0;
    screen_area.y1 = 0;
    screen_area.x2 = ctx->display.h_res - 1;
    screen_area.y2 = ctx->display.v_res - 1;

    /* Intersect with screen bounds */
    gfx_area_t clipped_area;
    bool success = gfx_area_intersect(&clipped_area, area_p, &screen_area);
    if (!success) {
        ESP_LOGD(TAG, "Area out of screen bounds");
        return;  /* Out of the screen */
    }

    /* Check if this area is already covered by existing dirty areas */
    for (uint8_t i = 0; i < ctx->disp.dirty_count; i++) {
        if (gfx_area_is_in(&clipped_area, &ctx->disp.dirty_areas[i])) {
            ESP_LOGD(TAG, "Area already covered by existing dirty area %d", i);
            return;
        }
    }

    if (ctx->disp.dirty_count < GFX_INV_BUF_SIZE) {
        gfx_area_copy(&ctx->disp.dirty_areas[ctx->disp.dirty_count], &clipped_area);
        ctx->disp.dirty_count++;
        // ESP_LOGW(TAG, "Added dirty area [%d,%d,%d,%d], total: %d",
        //          clipped_area.x1, clipped_area.y1, clipped_area.x2, clipped_area.y2, ctx->disp.dirty_count);
    } else {
        /* No space left, mark entire screen as dirty */
        ctx->disp.dirty_count = 1;
        gfx_area_copy(&ctx->disp.dirty_areas[0], &screen_area);
        ESP_LOGW(TAG, "Dirty area buffer full, marking entire screen as dirty");
    }
}

#include "esp_log.h"
#include "esp_debug_helpers.h"

void gfx_obj_invalidate(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->parent_handle == NULL) {
        ESP_LOGE(TAG, "Object has no parent handle");
        return;
    }

    gfx_area_t obj_area;
    obj_area.x1 = obj->x;
    obj_area.y1 = obj->y;
    obj_area.x2 = obj->x + obj->width - 1;
    obj_area.y2 = obj->y + obj->height - 1;

    // ESP_LOGI(TAG, "type:%d, Invalidated object area [%d,%d,%d,%d]",
            //  obj->type, obj_area.x1, obj_area.y1, obj_area.x2, obj_area.y2);

    // esp_backtrace_print(2);

    gfx_invalidate_area(obj->parent_handle, &obj_area);
}

void gfx_invalidate_all(gfx_core_context_t *ctx)
{
    gfx_area_t full_screen;
    full_screen.x1 = 0;
    full_screen.y1 = 0;
    full_screen.x2 = ctx->display.h_res - 1;
    full_screen.y2 = ctx->display.v_res - 1;
    gfx_invalidate_area(ctx, &full_screen);
}
