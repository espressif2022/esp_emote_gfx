/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_OBJ
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/gfx_obj.h"
#include "core/display/gfx_refr_priv.h"
#include "core/runtime/gfx_core_priv.h"

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "obj";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_obj_notify_aligned_dependents(gfx_obj_t *obj, uint8_t depth);
static void gfx_obj_detach_aligned_dependents(gfx_obj_t *obj);
static void gfx_obj_calc_pos_in_parent_internal(gfx_obj_t *obj, uint8_t depth);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_obj_notify_aligned_dependents(gfx_obj_t *obj, uint8_t depth)
{
    if (obj == NULL || obj->disp == NULL || depth > 8) {
        return;
    }

    for (gfx_obj_child_t *child = obj->disp->child_list; child != NULL; child = child->next) {
        gfx_obj_t *child_obj = (gfx_obj_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.enabled && child_obj->align.target == obj) {
            child_obj->state.layout_dirty = true;
            gfx_obj_invalidate(child_obj);
            gfx_obj_notify_aligned_dependents(child_obj, depth + 1);
        }
    }
}

static void gfx_obj_detach_aligned_dependents(gfx_obj_t *obj)
{
    if (obj == NULL || obj->disp == NULL) {
        return;
    }

    for (gfx_obj_child_t *child = obj->disp->child_list; child != NULL; child = child->next) {
        gfx_obj_t *child_obj = (gfx_obj_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.target == obj) {
            child_obj->align.target = NULL;
            child_obj->state.layout_dirty = true;
            gfx_obj_invalidate(child_obj);
            gfx_obj_notify_aligned_dependents(child_obj, 1);
        }
    }
}

static void gfx_obj_calc_pos_in_parent_internal(gfx_obj_t *obj, uint8_t depth)
{
    gfx_coord_t origin_x = 0;
    gfx_coord_t origin_y = 0;
    uint32_t parent_w;
    uint32_t parent_h;

    GFX_RETURN_IF_NULL_VOID(obj);

    if (depth > 8) {
        GFX_LOGW(TAG, "align depth too large, stop resolving");
        return;
    }

    parent_w = gfx_disp_get_hor_res(obj->disp);
    parent_h = gfx_disp_get_ver_res(obj->disp);

    if (obj->align.target != NULL && obj->align.target != obj) {
        gfx_obj_t *target = obj->align.target;

        if (target->disp == obj->disp) {
            gfx_obj_calc_pos_in_parent_internal(target, depth + 1);
            origin_x = target->geometry.x;
            origin_y = target->geometry.y;
            parent_w = target->geometry.width;
            parent_h = target->geometry.height;
        }
    }

    gfx_obj_cal_aligned_pos(obj, parent_w, parent_h, &obj->geometry.x, &obj->geometry.y);
    obj->geometry.x += origin_x;
    obj->geometry.y += origin_y;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

/* Generic object setters */

esp_err_t gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    //invalidate the old position
    gfx_obj_invalidate(obj);

    obj->geometry.x = x;
    obj->geometry.y = y;
    obj->align.enabled = false;
    obj->align.target = NULL;
    //invalidate the new position
    gfx_obj_invalidate(obj);
    gfx_obj_notify_aligned_dependents(obj, 0);
    GFX_LOGD(TAG, "Set object position: (%d, %d)", x, y);
    return ESP_OK;
}

esp_err_t gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (obj->type == GFX_OBJ_TYPE_ANIMATION ||
            obj->type == GFX_OBJ_TYPE_IMAGE ||
            obj->type == GFX_OBJ_TYPE_MESH_IMAGE ||
            obj->type == GFX_OBJ_TYPE_QRCODE) {
        GFX_LOGD(TAG, "Set size is not useful for type: %d", obj->type);
    } else {
        //invalidate the old size
        gfx_obj_invalidate(obj);

        obj->geometry.width = w;
        obj->geometry.height = h;

        gfx_obj_update_layout(obj);
        gfx_obj_invalidate(obj);
        gfx_obj_notify_aligned_dependents(obj, 0);
    }

    GFX_LOGD(TAG, "Set object size: %dx%d", w, h);
    return ESP_OK;
}

esp_err_t gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj->disp, ESP_ERR_INVALID_STATE);

    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        GFX_LOGW(TAG, "Unknown alignment type: %d", align);
        return ESP_ERR_INVALID_ARG;
    }
    // Invalidate old position first
    gfx_obj_invalidate(obj);

    // Update alignment parameters and enable alignment
    obj->align.type = align;
    obj->align.x_ofs = x_ofs;
    obj->align.y_ofs = y_ofs;
    obj->align.target = NULL;
    obj->align.enabled = true;

    gfx_obj_update_layout(obj);

    GFX_LOGD(TAG, "Set object alignment: type=%d, offset=(%d, %d)", align, x_ofs, y_ofs);
    return ESP_OK;
}

esp_err_t gfx_obj_align_to(gfx_obj_t *obj, gfx_obj_t *base, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj->disp, ESP_ERR_INVALID_STATE);

    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        GFX_LOGW(TAG, "Unknown alignment type: %d", align);
        return ESP_ERR_INVALID_ARG;
    }

    if (base != NULL && base->disp != obj->disp) {
        GFX_LOGW(TAG, "align_to base must be on same display");
        return ESP_ERR_INVALID_ARG;
    }

    if (base == obj) {
        GFX_LOGW(TAG, "align_to base cannot be self");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_obj_invalidate(obj);
    obj->align.type = align;
    obj->align.x_ofs = x_ofs;
    obj->align.y_ofs = y_ofs;
    obj->align.target = base;
    obj->align.enabled = true;

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "Set object align_to: base=%p, type=%d, offset=(%d, %d)", base, align, x_ofs, y_ofs);
    return ESP_OK;
}

esp_err_t gfx_obj_set_visible(gfx_obj_t *obj, bool visible)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    obj->state.is_visible = visible;
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "Set object visibility: %s", visible ? "visible" : "hidden");
    return ESP_OK;
}

bool gfx_obj_get_visible(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, false);

    return obj->state.is_visible;
}

void gfx_obj_update_layout(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    if (obj->align.enabled) {
        obj->state.layout_dirty = true;
    }
}

/* Internal alignment helpers */

void gfx_obj_cal_aligned_pos(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y)
{
    GFX_RETURN_IF_NULL_VOID(obj);
    GFX_RETURN_IF_NULL_VOID(x);
    GFX_RETURN_IF_NULL_VOID(y);

    if (!obj->align.enabled) {
        *x = obj->geometry.x;
        *y = obj->geometry.y;
        return;
    }

    gfx_coord_t calculated_x = 0;
    gfx_coord_t calculated_y = 0;
    switch (obj->align.type) {
    case GFX_ALIGN_TOP_LEFT:
        calculated_x = obj->align.x_ofs;
        calculated_y = obj->align.y_ofs;
        break;
    case GFX_ALIGN_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->geometry.width) / 2 + obj->align.x_ofs;
        calculated_y = obj->align.y_ofs;
        break;
    case GFX_ALIGN_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->geometry.width + obj->align.x_ofs;
        calculated_y = obj->align.y_ofs;
        break;
    case GFX_ALIGN_LEFT_MID:
        calculated_x = obj->align.x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->geometry.height) / 2 + obj->align.y_ofs;
        break;
    case GFX_ALIGN_CENTER:
        calculated_x = ((gfx_coord_t)parent_width - obj->geometry.width) / 2 + obj->align.x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->geometry.height) / 2 + obj->align.y_ofs;
        break;
    case GFX_ALIGN_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width - obj->geometry.width + obj->align.x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->geometry.height) / 2 + obj->align.y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_LEFT:
        calculated_x = obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->geometry.width) / 2 + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->geometry.width + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_LEFT:
        calculated_x = obj->align.x_ofs;
        calculated_y = -obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->geometry.width) / 2 + obj->align.x_ofs;
        calculated_y = -obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align.x_ofs;
        calculated_y = -obj->geometry.height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_TOP:
        calculated_x = -obj->geometry.width + obj->align.x_ofs;
        calculated_y = obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_MID:
        calculated_x = -obj->geometry.width + obj->align.x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->geometry.height) / 2 + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_BOTTOM:
        calculated_x = -obj->geometry.width + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_TOP:
        calculated_x = (gfx_coord_t)parent_width + obj->align.x_ofs;
        calculated_y = obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width + obj->align.x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->geometry.height) / 2 + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_BOTTOM:
        calculated_x = (gfx_coord_t)parent_width + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_LEFT:
        calculated_x = obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->geometry.width) / 2 + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align.y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align.x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align.y_ofs;
        break;
    default:
        GFX_LOGW(TAG, "Unknown alignment type: %d", obj->align.type);
        calculated_x = obj->geometry.x;
        calculated_y = obj->geometry.y;
        break;
    }

    *x = calculated_x;
    *y = calculated_y;
}

void gfx_obj_calc_pos_in_parent(gfx_obj_t *obj)
{
    gfx_obj_calc_pos_in_parent_internal(obj, 0);
}

/* Generic getters */

esp_err_t gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(x, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(y, ESP_ERR_INVALID_ARG);

    *x = obj->geometry.x;
    *y = obj->geometry.y;
    return ESP_OK;
}

esp_err_t gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(w, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(h, ESP_ERR_INVALID_ARG);

    *w = obj->geometry.width;
    *h = obj->geometry.height;
    return ESP_OK;
}

/*=====================
 * Touch callback (application listener)
 *====================*/

esp_err_t gfx_obj_set_touch_cb(gfx_obj_t *obj, gfx_obj_touch_cb_t cb, void *user_data)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    obj->user_touch_cb = cb;
    obj->user_touch_data = user_data;
    return ESP_OK;
}

uint32_t gfx_obj_get_trace_id(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, 0U);
    return obj->trace.create_seq;
}

const char *gfx_obj_get_class_name(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, NULL);
    return obj->trace.class_name;
}

const char *gfx_obj_get_trace_tag(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, NULL);
    return obj->trace.create_tag;
}

/*=====================
 * Other functions
 *====================*/

esp_err_t gfx_obj_delete(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (GFX_NOT_NULL(obj->disp)) {
        gfx_disp_remove_child(obj->disp, obj);
    }

    gfx_obj_detach_aligned_dependents(obj);
    gfx_obj_invalidate(obj);
    gfx_obj_notify_aligned_dependents(obj, 0);

    /* Call object's delete function if available */
    if (obj->vfunc.delete) {
        obj->vfunc.delete(obj);
    }

    free(obj);
    return ESP_OK;
}
