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
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"
#include "core/runtime/gfx_core_priv.h"
#include "gfx/object.h"

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "obj";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_object_notify_aligned_dependents(gfx_object_t *obj, uint8_t depth);
static void gfx_object_detach_aligned_dependents(gfx_object_t *obj);
static void gfx_object_calc_pos_in_parent_internal(gfx_object_t *obj, uint8_t depth);
static gfx_err_t gfx_object_detach_from_parent(gfx_object_t *obj);
static gfx_err_t gfx_object_delete_children(gfx_object_t *obj);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_object_notify_aligned_dependents(gfx_object_t *obj, uint8_t depth)
{
    if (obj == NULL || obj->disp == NULL || depth > 8) {
        return;
    }

    for (gfx_object_child_t *child = obj->disp->child_list; child != NULL; child = child->next) {
        gfx_object_t *child_obj = (gfx_object_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.enabled && (child_obj->align.target == obj ||
                                         (child_obj->align.target == NULL && child_obj->parent == obj))) {
            child_obj->state.layout_dirty = true;
            gfx_object_invalidate_tree(child_obj);
            gfx_object_notify_aligned_dependents(child_obj, depth + 1);
        }
    }

    for (gfx_object_child_t *child = obj->child_list; child != NULL; child = child->next) {
        gfx_object_t *child_obj = (gfx_object_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.enabled && (child_obj->align.target == obj ||
                                         (child_obj->align.target == NULL && child_obj->parent == obj))) {
            child_obj->state.layout_dirty = true;
            gfx_object_invalidate_tree(child_obj);
            gfx_object_notify_aligned_dependents(child_obj, depth + 1);
        }
    }
}

void gfx_object_invalidate_abs_area_cache_tree(gfx_object_t *obj)
{
    if (obj == NULL) {
        return;
    }

    obj->state.abs_area_valid = false;
    for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_object_invalidate_abs_area_cache_tree((gfx_object_t *)child_node->src);
    }
}

static void gfx_object_detach_aligned_dependents(gfx_object_t *obj)
{
    if (obj == NULL || obj->disp == NULL) {
        return;
    }

    for (gfx_object_child_t *child = obj->disp->child_list; child != NULL; child = child->next) {
        gfx_object_t *child_obj = (gfx_object_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.target == obj) {
            child_obj->align.target = NULL;
            child_obj->state.layout_dirty = true;
            gfx_object_invalidate_tree(child_obj);
            gfx_object_notify_aligned_dependents(child_obj, 1);
        }
    }

    for (gfx_object_child_t *child = obj->child_list; child != NULL; child = child->next) {
        gfx_object_t *child_obj = (gfx_object_t *)child->src;

        if (child_obj == NULL || child_obj == obj) {
            continue;
        }

        if (child_obj->align.target == obj) {
            child_obj->align.target = NULL;
            child_obj->state.layout_dirty = true;
            gfx_object_invalidate_tree(child_obj);
            gfx_object_notify_aligned_dependents(child_obj, 1);
        }
    }
}

static gfx_err_t gfx_object_detach_from_parent(gfx_object_t *obj)
{
    if (obj == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->parent != NULL) {
        gfx_object_t *parent = obj->parent;
        obj->parent = NULL;
        return gfx_object_child_list_remove(&parent->child_list, obj);
    }

    if (obj->disp != NULL) {
        esp_err_t ret = gfx_display_remove_child(obj->disp, obj);
        if (ret == ESP_ERR_NOT_FOUND) {
            return ESP_OK;
        }
        return ret;
    }

    return ESP_OK;
}

static gfx_err_t gfx_object_delete_children(gfx_object_t *obj)
{
    if (obj == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (obj->child_list != NULL) {
        gfx_object_t *child_obj = (gfx_object_t *)obj->child_list->src;
        if (child_obj == NULL) {
            gfx_object_child_t *node = obj->child_list;
            obj->child_list = node->next;
            free(node);
            continue;
        }

        esp_err_t ret = gfx_object_delete(child_obj);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static void gfx_object_calc_pos_in_parent_internal(gfx_object_t *obj, uint8_t depth)
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

    if (!obj->align.enabled) {
        return;
    }

    parent_w = gfx_display_get_h_res(obj->disp);
    parent_h = gfx_display_get_v_res(obj->disp);

    if (obj->align.target != NULL && obj->align.target != obj) {
        gfx_object_t *target = obj->align.target;

        if (target->disp == obj->disp) {
            gfx_object_calc_pos_in_parent_internal(target, depth + 1);
            origin_x = target->geometry.x;
            origin_y = target->geometry.y;
            parent_w = target->geometry.width;
            parent_h = target->geometry.height;
        }
    } else if (obj->parent != NULL && obj->parent != obj && obj->parent->disp == obj->disp) {
        gfx_object_t *parent = obj->parent;

        gfx_object_calc_pos_in_parent_internal(parent, depth + 1);
        origin_x = parent->geometry.x;
        origin_y = parent->geometry.y;
        parent_w = parent->geometry.width;
        parent_h = parent->geometry.height;
    }

    gfx_object_cal_aligned_pos(obj, parent_w, parent_h, &obj->geometry.x, &obj->geometry.y);
    obj->geometry.x += origin_x;
    obj->geometry.y += origin_y;
    obj->state.abs_area_valid = false;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

/* Generic object setters */

gfx_err_t gfx_object_set_pos(gfx_object_t *obj, gfx_coord_t x, gfx_coord_t y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    //invalidate the old position
    gfx_object_invalidate_tree(obj);

    obj->geometry.x = x;
    obj->geometry.y = y;
    obj->local_geometry.x = x;
    obj->local_geometry.y = y;
    obj->align.enabled = false;
    obj->align.target = NULL;
    gfx_object_invalidate_abs_area_cache_tree(obj);
    //invalidate the new position
    gfx_object_invalidate_tree(obj);
    gfx_object_notify_aligned_dependents(obj, 0);
    GFX_LOGD(TAG, "Set object position: (%d, %d)", x, y);
    return ESP_OK;
}

gfx_err_t gfx_object_set_size(gfx_object_t *obj, uint16_t w, uint16_t h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (obj->type == GFX_OBJ_TYPE_ANIMATION ||
            obj->type == GFX_OBJ_TYPE_IMAGE ||
            obj->type == GFX_OBJ_TYPE_MESH_IMAGE ||
            obj->type == GFX_OBJ_TYPE_QRCODE) {
        GFX_LOGD(TAG, "Set size is not useful for type: %d", obj->type);
    } else {
        //invalidate the old size
        gfx_object_invalidate_tree(obj);

        obj->geometry.width = w;
        obj->geometry.height = h;
        obj->local_geometry.width = w;
        obj->local_geometry.height = h;
        gfx_object_invalidate_abs_area_cache_tree(obj);

        gfx_object_update_layout(obj);
        gfx_object_invalidate_tree(obj);
        gfx_object_notify_aligned_dependents(obj, 0);
    }

    GFX_LOGD(TAG, "Set object size: %dx%d", w, h);
    return ESP_OK;
}

gfx_err_t gfx_object_align(gfx_object_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj->disp, ESP_ERR_INVALID_STATE);

    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        GFX_LOGW(TAG, "Unknown alignment type: %d", align);
        return ESP_ERR_INVALID_ARG;
    }
    // Invalidate old position first
    gfx_object_invalidate_tree(obj);

    // Update alignment parameters and enable alignment
    obj->align.type = align;
    obj->align.x_ofs = x_ofs;
    obj->align.y_ofs = y_ofs;
    obj->align.target = NULL;
    obj->align.enabled = true;
    gfx_object_invalidate_abs_area_cache_tree(obj);

    gfx_object_update_layout(obj);

    GFX_LOGD(TAG, "Set object alignment: type=%d, offset=(%d, %d)", align, x_ofs, y_ofs);
    return ESP_OK;
}

gfx_err_t gfx_object_align_to(gfx_object_t *obj, gfx_object_t *base, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
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

    gfx_object_invalidate_tree(obj);
    obj->align.type = align;
    obj->align.x_ofs = x_ofs;
    obj->align.y_ofs = y_ofs;
    obj->align.target = base;
    obj->align.enabled = true;
    gfx_object_invalidate_abs_area_cache_tree(obj);

    gfx_object_update_layout(obj);
    gfx_object_invalidate_tree(obj);

    GFX_LOGD(TAG, "Set object align_to: base=%p, type=%d, offset=(%d, %d)", base, align, x_ofs, y_ofs);
    return ESP_OK;
}

gfx_err_t gfx_object_set_visible(gfx_object_t *obj, bool visible)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (obj->state.is_visible == visible) {
        return ESP_OK;
    }

    gfx_object_invalidate_tree(obj);
    obj->state.is_visible = visible;
    gfx_object_invalidate_abs_area_cache_tree(obj);
    gfx_object_invalidate_tree(obj);

    GFX_LOGD(TAG, "Set object visibility: %s", visible ? "visible" : "hidden");
    return ESP_OK;
}

bool gfx_object_get_visible(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, false);

    return obj->state.is_visible;
}

void gfx_object_update_layout(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    if (obj->align.enabled) {
        obj->state.layout_dirty = true;
    }
}

/* Internal alignment helpers */

void gfx_object_cal_aligned_pos(gfx_object_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y)
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

void gfx_object_calc_pos_in_parent(gfx_object_t *obj)
{
    gfx_object_calc_pos_in_parent_internal(obj, 0);
}

/* Generic getters */

gfx_err_t gfx_object_get_pos(gfx_object_t *obj, gfx_coord_t *x, gfx_coord_t *y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(x, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(y, ESP_ERR_INVALID_ARG);

    *x = obj->geometry.x;
    *y = obj->geometry.y;
    return ESP_OK;
}

gfx_err_t gfx_object_get_size(gfx_object_t *obj, uint16_t *w, uint16_t *h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(w, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(h, ESP_ERR_INVALID_ARG);

    *w = obj->geometry.width;
    *h = obj->geometry.height;
    return ESP_OK;
}

gfx_err_t gfx_object_child_list_add(gfx_object_child_t **list, gfx_object_t *obj)
{
    gfx_object_child_t *new_child;

    GFX_RETURN_IF_NULL(list, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    for (gfx_object_child_t *current = *list; current != NULL; current = current->next) {
        if (current->src == obj) {
            return ESP_OK;
        }
    }

    new_child = (gfx_object_child_t *)malloc(sizeof(gfx_object_child_t));
    if (new_child == NULL) {
        return ESP_ERR_NO_MEM;
    }
    new_child->src = obj;
    new_child->next = NULL;

    if (*list == NULL) {
        *list = new_child;
    } else {
        gfx_object_child_t *current = *list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_child;
    }

    return ESP_OK;
}

gfx_err_t gfx_object_child_list_remove(gfx_object_child_t **list, gfx_object_t *obj)
{
    gfx_object_child_t *current;
    gfx_object_child_t *prev = NULL;

    GFX_RETURN_IF_NULL(list, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    current = *list;
    while (current != NULL) {
        if (current->src == obj) {
            if (prev == NULL) {
                *list = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}

bool gfx_object_child_list_contains(gfx_object_child_t *list, gfx_object_t *obj, uint32_t create_seq)
{
    if (obj == NULL) {
        return false;
    }

    for (gfx_object_child_t *current = list; current != NULL; current = current->next) {
        gfx_object_t *child_obj = (gfx_object_t *)current->src;
        if (child_obj == NULL) {
            continue;
        }

        if (child_obj == obj) {
            return child_obj->state.is_visible &&
                   (create_seq == 0U || child_obj->trace.create_seq == create_seq);
        }

        if (gfx_object_child_list_contains(child_obj->child_list, obj, create_seq)) {
            return true;
        }
    }

    return false;
}

void gfx_object_child_list_free_nodes(gfx_object_child_t **list)
{
    gfx_object_child_t *current;

    if (list == NULL) {
        return;
    }

    current = *list;
    while (current != NULL) {
        gfx_object_child_t *next = current->next;
        free(current);
        current = next;
    }
    *list = NULL;
}

gfx_err_t gfx_object_add_child(gfx_object_t *parent, gfx_object_t *child)
{
    esp_err_t ret;

    GFX_RETURN_IF_NULL(parent, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(child, ESP_ERR_INVALID_ARG);

    if (parent == child) {
        return ESP_ERR_INVALID_ARG;
    }

    if (parent->disp == NULL || child->disp == NULL || parent->disp != child->disp) {
        GFX_LOGW(TAG, "add child: parent and child must belong to same display");
        return ESP_ERR_INVALID_ARG;
    }

    for (gfx_object_t *ancestor = parent; ancestor != NULL; ancestor = ancestor->parent) {
        if (ancestor == child) {
            GFX_LOGW(TAG, "add child: cycle is not allowed");
            return ESP_ERR_INVALID_ARG;
        }
    }

    gfx_object_invalidate_tree(parent);
    gfx_object_invalidate_tree(child);

    if (child->parent != parent) {
        ret = gfx_object_detach_from_parent(child);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = gfx_object_child_list_add(&parent->child_list, child);
    if (ret != ESP_OK) {
        return ret;
    }

    child->parent = parent;
    child->disp = parent->disp;
    gfx_object_invalidate_abs_area_cache_tree(child);

    gfx_object_invalidate_tree(parent);
    gfx_object_invalidate_tree(child);
    return ESP_OK;
}

gfx_err_t gfx_object_remove_child(gfx_object_t *parent, gfx_object_t *child)
{
    esp_err_t ret;

    GFX_RETURN_IF_NULL(parent, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(child, ESP_ERR_INVALID_ARG);

    if (child->parent != parent) {
        return ESP_ERR_NOT_FOUND;
    }

    gfx_object_invalidate_tree(parent);
    gfx_object_invalidate_tree(child);

    ret = gfx_object_child_list_remove(&parent->child_list, child);
    if (ret != ESP_OK) {
        return ret;
    }

    child->parent = NULL;
    gfx_object_invalidate_abs_area_cache_tree(child);
    ret = gfx_display_add_child(child->disp, child);
    if (ret != ESP_OK) {
        return ret;
    }

    gfx_object_invalidate_tree(parent);
    gfx_object_invalidate_tree(child);
    return ESP_OK;
}

gfx_object_t *gfx_object_get_parent(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, NULL);
    return obj->parent;
}

/*=====================
 * Touch callback (application listener)
 *====================*/

gfx_err_t gfx_object_set_touch_cb(gfx_object_t *obj, gfx_object_touch_cb_t cb, void *user_data)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    obj->user_touch_cb = cb;
    obj->user_touch_data = user_data;
    return ESP_OK;
}

uint32_t gfx_object_get_trace_id(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, 0U);
    return obj->trace.create_seq;
}

const char *gfx_object_get_class_name(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, NULL);
    return obj->trace.class_name;
}

const char *gfx_object_get_trace_tag(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, NULL);
    return obj->trace.create_tag;
}

gfx_err_t gfx_object_load_resource(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (obj->state.resource_loaded && !obj->state.resource_dirty) {
        return ESP_OK;
    }

    if (obj->state.resource_loaded && obj->state.resource_dirty) {
        gfx_object_release_resource(obj);
    }

    if (obj->vfunc.load == NULL) {
        obj->state.resource_loaded = true;
        obj->state.resource_dirty = false;
        return ESP_OK;
    }

    esp_err_t ret = obj->vfunc.load(obj);
    if (ret != ESP_OK) {
        return ret;
    }

    obj->state.resource_loaded = true;
    obj->state.resource_dirty = false;
    return ESP_OK;
}

void gfx_object_release_resource(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    if (obj->state.resource_loaded && obj->vfunc.release != NULL) {
        obj->vfunc.release(obj);
    }

    obj->state.resource_loaded = false;
}

void gfx_object_mark_resource_dirty(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    if (obj->state.resource_loaded) {
        gfx_object_release_resource(obj);
    }
    obj->state.resource_dirty = true;
    gfx_object_invalidate_tree(obj);
}

void gfx_object_set_manual_child_draw(gfx_object_t *obj, bool enable)
{
    GFX_RETURN_IF_NULL_VOID(obj);
    obj->state.manual_child_draw = enable;
}

void gfx_object_set_input_passthrough(gfx_object_t *obj, bool enable)
{
    GFX_RETURN_IF_NULL_VOID(obj);
    obj->state.input_passthrough = enable;
}

void gfx_object_set_input_passthrough_tree(gfx_object_t *obj, bool enable)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    gfx_object_set_input_passthrough(obj, enable);
    for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_object_set_input_passthrough_tree((gfx_object_t *)child_node->src, enable);
    }
}

void gfx_object_set_clip_children(gfx_object_t *obj, bool enable)
{
    GFX_RETURN_IF_NULL_VOID(obj);

    if (obj->state.clip_children == enable) {
        return;
    }

    obj->state.clip_children = enable;
    gfx_object_invalidate_abs_area_cache_tree(obj);
}

/*=====================
 * Other functions
 *====================*/

gfx_err_t gfx_object_delete(gfx_object_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    gfx_object_invalidate_tree(obj);
    (void)gfx_object_delete_children(obj);
    (void)gfx_object_detach_from_parent(obj);

    gfx_object_detach_aligned_dependents(obj);
    gfx_object_invalidate_tree(obj);
    gfx_object_notify_aligned_dependents(obj, 0);

    gfx_object_release_resource(obj);

    /* Call object's delete function if available */
    if (obj->vfunc.delete) {
        obj->vfunc.delete(obj);
    }

    free(obj);
    return ESP_OK;
}
