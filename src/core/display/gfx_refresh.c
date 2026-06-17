/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <inttypes.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_REFRESH
#include "common/gfx_log_priv.h"

#include "core/display/gfx_refresh_priv.h"
#include "core/runtime/gfx_core_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "refresh";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static gfx_object_t *gfx_display_hit_test_top_down(gfx_object_child_t *node, uint16_t x, uint16_t y);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

static bool gfx_display_object_contains(gfx_object_t *obj, uint16_t x, uint16_t y)
{
    gfx_area_t area;

    if (obj == NULL || !obj->state.is_visible) {
        return false;
    }

    if (!gfx_object_get_abs_area(obj, &area)) {
        return false;
    }

    return (int32_t)x >= area.x1 && (int32_t)x <= area.x2 &&
           (int32_t)y >= area.y1 && (int32_t)y <= area.y2;
}

static gfx_object_t *gfx_display_hit_test_object_top_down(gfx_object_t *obj, uint16_t x, uint16_t y)
{
    gfx_object_t *hit;

    if (obj == NULL || !obj->state.is_visible) {
        return NULL;
    }

    if (obj->state.clip_children && !gfx_display_object_contains(obj, x, y)) {
        return NULL;
    }

    hit = gfx_display_hit_test_top_down(obj->child_list, x, y);
    if (hit != NULL) {
        return hit;
    }

    if (obj->state.input_passthrough) {
        return NULL;
    }

    return gfx_display_object_contains(obj, x, y) ? obj : NULL;
}

static gfx_object_t *gfx_display_hit_test_top_down(gfx_object_child_t *node, uint16_t x, uint16_t y)
{
    if (node == NULL) {
        return NULL;
    }

    gfx_object_t *hit = gfx_display_hit_test_top_down(node->next, x, y);
    if (hit != NULL) {
        return hit;
    }

    gfx_object_t *obj = (gfx_object_t *)node->src;
    return gfx_display_hit_test_object_top_down(obj, x, y);
}

gfx_object_t *gfx_display_hit_test(gfx_display_t *disp, uint16_t x, uint16_t y)
{
    if (disp == NULL) {
        return NULL;
    }

    return gfx_display_hit_test_top_down(disp->child_list, x, y);
}

/* Area helpers */
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

bool gfx_area_intersect_exclusive(gfx_area_t *result, const gfx_area_t *a1, const gfx_area_t *a2)
{
    gfx_coord_t x1 = (a1->x1 > a2->x1) ? a1->x1 : a2->x1;
    gfx_coord_t y1 = (a1->y1 > a2->y1) ? a1->y1 : a2->y1;
    gfx_coord_t x2 = (a1->x2 < a2->x2) ? a1->x2 : a2->x2;
    gfx_coord_t y2 = (a1->y2 < a2->y2) ? a1->y2 : a2->y2;

    if (x1 < x2 && y1 < y2) {
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

void gfx_refresh_merge_areas(gfx_display_t *disp)
{
    uint32_t src_idx;
    uint32_t dst_idx;
    gfx_area_t merged_area;

    if (disp == NULL) {
        return;
    }

    memset(disp->dirty.merged, 0, sizeof(disp->dirty.merged));

    for (dst_idx = 0; dst_idx < disp->dirty.count; dst_idx++) {
        if (disp->dirty.merged[dst_idx] != 0) {
            continue;
        }

        for (src_idx = 0; src_idx < disp->dirty.count; src_idx++) {
            if (disp->dirty.merged[src_idx] != 0 || dst_idx == src_idx) {
                continue;
            }

            if (!gfx_area_is_on(&disp->dirty.areas[dst_idx], &disp->dirty.areas[src_idx])) {
                continue;
            }

            gfx_area_join(&merged_area, &disp->dirty.areas[dst_idx], &disp->dirty.areas[src_idx]);

            uint32_t merged_size = gfx_area_get_size(&merged_area);
            uint32_t separate_size = gfx_area_get_size(&disp->dirty.areas[dst_idx]) +
                                     gfx_area_get_size(&disp->dirty.areas[src_idx]);

            if (merged_size < separate_size) {
                gfx_area_copy(&disp->dirty.areas[dst_idx], &merged_area);
                disp->dirty.merged[src_idx] = 1;

                GFX_LOGD(TAG, "merge dirty areas: [%" PRIu32 "] into [%" PRIu32 "], saved %" PRIu32 " pixels",
                         src_idx, dst_idx, separate_size - merged_size);
            }
        }
    }
}

void gfx_invalidate_area_disp(gfx_display_t *disp, const gfx_area_t *area_p)
{
    if (disp == NULL) {
        return;
    }

    if (area_p == NULL) {
        disp->dirty.count = 0;
        memset(disp->dirty.merged, 0, sizeof(disp->dirty.merged));
        GFX_LOGD(TAG, "invalidate area: cleared all dirty areas");
        return;
    }

    gfx_area_t screen_area;
    screen_area.x1 = 0;
    screen_area.y1 = 0;
    screen_area.x2 = disp->res.h_res - 1;
    screen_area.y2 = disp->res.v_res - 1;

    gfx_area_t clipped_area;
    bool success = gfx_area_intersect(&clipped_area, area_p, &screen_area);
    if (!success) {
        GFX_LOGD(TAG, "invalidate area: area is out of screen bounds");
        return;
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (gfx_area_is_in(&clipped_area, &disp->dirty.areas[i])) {
            GFX_LOGD(TAG, "invalidate area: area is already covered by dirty area %d", i);
            return;
        }
    }

    if (disp->dirty.count < GFX_DISP_INV_BUF_SIZE) {
        gfx_area_copy(&disp->dirty.areas[disp->dirty.count], &clipped_area);
        disp->dirty.count++;
        GFX_LOGD(TAG, "invalidate area: added [%d,%d,%d,%d], total=%d",
                 clipped_area.x1, clipped_area.y1, clipped_area.x2, clipped_area.y2, disp->dirty.count);
    } else {
        GFX_LOGW(TAG, "invalidate area: dirty buffer is full[%d], marking full screen", disp->dirty.count);
        disp->dirty.count = 1;
        gfx_area_copy(&disp->dirty.areas[0], &screen_area);
    }

    /* Wake render task so it refreshes without waiting for the next timer tick */
    gfx_core_context_t *ctx = (gfx_core_context_t *)disp->ctx;
    if (ctx != NULL && ctx->sync.render_events != NULL) {
        gfx_platform_event_set(ctx->sync.render_events, GFX_EVENT_INVALIDATE);
    }
}

void gfx_invalidate_area(gfx_handle_t handle, const gfx_area_t *area_p)
{
    if (handle == NULL) {
        GFX_LOGE(TAG, "invalidate area: handle is NULL");
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;

    if (area_p == NULL) {
        for (gfx_display_t *d = ctx->disp; d != NULL; d = d->next) {
            gfx_invalidate_area_disp(d, NULL);
        }
        return;
    }

    /* Invalidate first display (backward compat) */
    if (ctx->disp != NULL) {
        gfx_invalidate_area_disp(ctx->disp, area_p);
    }
}

bool gfx_object_get_abs_area(gfx_object_t *obj, gfx_area_t *area)
{
    gfx_area_t result;

    if (obj == NULL || area == NULL || obj->disp == NULL ||
            obj->geometry.width == 0 || obj->geometry.height == 0) {
        return false;
    }

    if (obj->align.enabled || obj->state.layout_dirty) {
        gfx_object_calc_pos_in_parent(obj);
    }

    if (obj->state.abs_area_valid) {
        *area = obj->resolved.abs_area;
        return true;
    }

    result.x1 = obj->geometry.x;
    result.y1 = obj->geometry.y;
    result.x2 = obj->geometry.x + obj->geometry.width - 1;
    result.y2 = obj->geometry.y + obj->geometry.height - 1;

    for (gfx_object_t *parent = obj->parent; parent != NULL; parent = parent->parent) {
        gfx_area_t parent_area;
        if (!parent->state.clip_children) {
            continue;
        }
        if (!gfx_object_get_abs_area(parent, &parent_area) ||
                !gfx_area_intersect(&result, &result, &parent_area)) {
            obj->state.abs_area_valid = false;
            return false;
        }
    }

    obj->resolved.abs_area = result;
    obj->state.abs_area_valid = true;
    *area = result;
    return true;
}

bool gfx_object_get_abs_area_exclusive(gfx_object_t *obj, gfx_area_t *area)
{
    gfx_area_t inclusive;

    if (!gfx_object_get_abs_area(obj, &inclusive)) {
        return false;
    }

    area->x1 = inclusive.x1;
    area->y1 = inclusive.y1;
    area->x2 = (gfx_coord_t)(inclusive.x2 + 1);
    area->y2 = (gfx_coord_t)(inclusive.y2 + 1);
    return true;
}

void gfx_object_invalidate(gfx_object_t *obj)
{
    gfx_area_t obj_area;

    if (obj == NULL) {
        GFX_LOGE(TAG, "invalidate object: object is NULL");
        return;
    }

    obj->state.dirty = true;

    if (!gfx_object_get_abs_area(obj, &obj_area)) {
        if (obj->disp == NULL) {
            GFX_LOGE(TAG, "invalidate object: object has no display");
        }
        return;
    }

    gfx_invalidate_area_disp(obj->disp, &obj_area);
}

void gfx_object_invalidate_tree(gfx_object_t *obj)
{
    if (obj == NULL) {
        return;
    }

    gfx_object_invalidate(obj);

    for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_object_invalidate_tree((gfx_object_t *)child_node->src);
    }
}

static void gfx_refresh_update_layout_dirty_object(gfx_object_t *obj)
{
    if (obj == NULL) {
        return;
    }

    if (obj->state.layout_dirty && obj->align.enabled) {
        gfx_coord_t old_x = obj->geometry.x;
        gfx_coord_t old_y = obj->geometry.y;

        gfx_object_invalidate_tree(obj);
        gfx_object_calc_pos_in_parent(obj);

        gfx_object_invalidate_tree(obj);

        GFX_LOGD(TAG,
                 "layout update: obj=%p (%d,%d) -> (%d,%d)",
                 obj,
                 old_x,
                 old_y,
                 obj->geometry.x,
                 obj->geometry.y);

        obj->state.layout_dirty = false;
    }

    for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_refresh_update_layout_dirty_object((gfx_object_t *)child_node->src);
    }
}

void gfx_refresh_update_layout_dirty(gfx_display_t *disp)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    for (gfx_object_child_t *child_node = disp->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_refresh_update_layout_dirty_object((gfx_object_t *)child_node->src);
    }
}
