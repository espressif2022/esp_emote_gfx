/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "core/gfx_refr_priv.h"

static const char *TAG = "gfx_refr";

/* Area utility functions (merged from gfx_area.c) */
void gfx_area_copy(gfx_area_t *dest, const gfx_area_t *src)
{
    dest->x1 = src->x1;
    dest->y1 = src->y1;
    dest->x2 = src->x2;
    dest->y2 = src->y2;
}

bool gfx_area_is_in(const gfx_area_t *area_in, const gfx_area_t *area_parent)
{
    if (area_in->x1 >= area_parent->x1 &&
            area_in->y1 >= area_parent->y1 &&
            area_in->x2 <= area_parent->x2 &&
            area_in->y2 <= area_parent->y2) {
        return true;
    }
    return false;
}

bool gfx_area_intersect(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    gfx_coord_t x1 = (a1->x1 > a2->x1) ? a1->x1 : a2->x1;
    gfx_coord_t y1 = (a1->y1 > a2->y1) ? a1->y1 : a2->y1;
    gfx_coord_t x2 = (a1->x2 < a2->x2) ? a1->x2 : a2->x2;
    gfx_coord_t y2 = (a1->y2 < a2->y2) ? a1->y2 : a2->y2;

    if (x1 <= x2 && y1 <= y2) {
        result->x1 = x1;
        result->y1 = y1;
        result->x2 = x2;
        result->y2 = y2;
        return true;
    }
    return false;
}

uint32_t gfx_area_get_size(const gfx_area_t *area)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    return width * height;
}

bool gfx_area_is_on(const gfx_area_t *a1, const gfx_area_t *a2)
{
    /* Check if areas are completely separate */
    if ((a1->x1 > a2->x2) ||
            (a2->x1 > a1->x2) ||
            (a1->y1 > a2->y2) ||
            (a2->y1 > a1->y2)) {
        return false;
    }
    return true;
}

void gfx_area_join(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    result->x1 = (a1->x1 < a2->x1) ? a1->x1 : a2->x1;
    result->y1 = (a1->y1 < a2->y1) ? a1->y1 : a2->y1;
    result->x2 = (a1->x2 > a2->x2) ? a1->x2 : a2->x2;
    result->y2 = (a1->y2 > a2->y2) ? a1->y2 : a2->y2;
}

void gfx_refr_merge_areas(gfx_disp_t *disp)
{
    uint32_t src_idx;
    uint32_t dst_idx;
    gfx_area_t merged_area;

    if (disp == NULL) {
        return;
    }

    memset(disp->area_merged, 0, sizeof(disp->area_merged));

    for (dst_idx = 0; dst_idx < disp->dirty_count; dst_idx++) {
        if (disp->area_merged[dst_idx] != 0) {
            continue;
        }

        for (src_idx = 0; src_idx < disp->dirty_count; src_idx++) {
            if (disp->area_merged[src_idx] != 0 || dst_idx == src_idx) {
                continue;
            }

            if (!gfx_area_is_on(&disp->dirty_areas[dst_idx], &disp->dirty_areas[src_idx])) {
                continue;
            }

            gfx_area_join(&merged_area, &disp->dirty_areas[dst_idx], &disp->dirty_areas[src_idx]);

            uint32_t merged_size = gfx_area_get_size(&merged_area);
            uint32_t separate_size = gfx_area_get_size(&disp->dirty_areas[dst_idx]) +
                                     gfx_area_get_size(&disp->dirty_areas[src_idx]);

            if (merged_size < separate_size) {
                gfx_area_copy(&disp->dirty_areas[dst_idx], &merged_area);
                disp->area_merged[src_idx] = 1;

                ESP_LOGD(TAG, "Merged area [%" PRIu32 "] into [%" PRIu32 "], saved %" PRIu32 " pixels",
                         src_idx, dst_idx, separate_size - merged_size);
            }
        }
    }
}

void gfx_invalidate_area_disp(gfx_disp_t *disp, const gfx_area_t *area_p)
{
    if (disp == NULL) {
        return;
    }

    if (area_p == NULL) {
        disp->dirty_count = 0;
        memset(disp->area_merged, 0, sizeof(disp->area_merged));
        ESP_LOGD(TAG, "Cleared all dirty areas");
        return;
    }

    gfx_area_t screen_area;
    screen_area.x1 = 0;
    screen_area.y1 = 0;
    screen_area.x2 = disp->h_res - 1;
    screen_area.y2 = disp->v_res - 1;

    gfx_area_t clipped_area;
    bool success = gfx_area_intersect(&clipped_area, area_p, &screen_area);
    if (!success) {
        ESP_LOGD(TAG, "Area out of screen bounds");
        return;
    }

    for (uint8_t i = 0; i < disp->dirty_count; i++) {
        if (gfx_area_is_in(&clipped_area, &disp->dirty_areas[i])) {
            ESP_LOGD(TAG, "Area already covered by existing dirty area %d", i);
            return;
        }
    }

    if (disp->dirty_count < GFX_DISP_INV_BUF_SIZE) {
        gfx_area_copy(&disp->dirty_areas[disp->dirty_count], &clipped_area);
        disp->dirty_count++;
        ESP_LOGD(TAG, "Added dirty area [%d,%d,%d,%d], total: %d",
                 clipped_area.x1, clipped_area.y1, clipped_area.x2, clipped_area.y2, disp->dirty_count);
    } else {
        disp->dirty_count = 1;
        gfx_area_copy(&disp->dirty_areas[0], &screen_area);
        ESP_LOGW(TAG, "Dirty area buffer full, marking entire screen as dirty");
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
        for (gfx_disp_t *d = ctx->disp; d != NULL; d = d->next) {
            gfx_invalidate_area_disp(d, NULL);
        }
        return;
    }

    /* Invalidate first display (backward compat) */
    if (ctx->disp != NULL) {
        gfx_invalidate_area_disp(ctx->disp, area_p);
    }
}

void gfx_obj_invalidate(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->disp == NULL) {
        ESP_LOGE(TAG, "Object has no display");
        return;
    }

    gfx_area_t obj_area;
    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width - 1;
    obj_area.y2 = obj->geometry.y + obj->geometry.height - 1;

    obj->state.dirty = true;

    gfx_invalidate_area_disp(obj->disp, &obj_area);
}

void gfx_refr_update_layout_dirty(gfx_disp_t *disp)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    uint32_t parent_w = disp->h_res;
    uint32_t parent_h = disp->v_res;

    gfx_obj_child_t *child_node = disp->child_list;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

        if (obj != NULL && obj->state.layout_dirty && obj->align.enabled) {
            gfx_obj_invalidate(obj);

            gfx_coord_t new_x = obj->geometry.x;
            gfx_coord_t new_y = obj->geometry.y;
            gfx_obj_cal_aligned_pos(obj, parent_w, parent_h, &new_x, &new_y);
            obj->geometry.x = new_x;
            obj->geometry.y = new_y;

            gfx_obj_invalidate(obj);

            obj->state.layout_dirty = false;
        }

        child_node = child_node->next;
    }
}
