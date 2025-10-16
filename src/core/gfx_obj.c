/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "esp_log.h"
#include "core/gfx_obj.h"
#include "core/gfx_core_internal.h"
#include "core/gfx_refr.h"
#include "widget/gfx_img_internal.h"
#include "widget/gfx_label_internal.h"
#include "widget/gfx_anim_internal.h"

static const char *TAG = "gfx_obj";

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*=====================
 * Generic object setter functions
 *====================*/

void gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    obj->x = x;
    obj->y = y;
    obj->use_align = false;
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set object position: (%d, %d)", x, y);
}

void gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->type == GFX_OBJ_TYPE_ANIMATION || obj->type == GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGW(TAG, "Set size is not useful");
    } else {
        obj->width = w;
        obj->height = h;
        gfx_obj_invalidate(obj);
    }

    ESP_LOGD(TAG, "Set object size: %dx%d", w, h);
}

void gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->parent_handle == NULL) {
        ESP_LOGE(TAG, "Object has no parent handle");
        return;
    }

    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        ESP_LOGW(TAG, "Unknown alignment type: %d", align);
        return;
    }
    obj->align_type = align;
    obj->align_x_ofs = x_ofs;
    obj->align_y_ofs = y_ofs;
    obj->use_align = true;
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set object alignment: type=%d, offset=(%d, %d)", align, x_ofs, y_ofs);
}

void gfx_obj_set_visible(gfx_obj_t *obj, bool visible)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    obj->is_visible = visible;
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set object visibility: %s", visible ? "visible" : "hidden");
}

bool gfx_obj_get_visible(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return false;
    }

    return obj->is_visible;
}

/*=====================
 * Static helper functions
 *====================*/

void gfx_obj_cal_aligned_pos(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y)
{
    if (obj == NULL || x == NULL || y == NULL) {
        return;
    }

    if (!obj->use_align) {
        *x = obj->x;
        *y = obj->y;
        return;
    }

    gfx_coord_t calculated_x = 0;
    gfx_coord_t calculated_y = 0;
    switch (obj->align_type) {
    case GFX_ALIGN_TOP_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_LEFT_MID:
        calculated_x = obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_CENTER:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_TOP:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_MID:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_BOTTOM:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_TOP:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_BOTTOM:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    default:
        ESP_LOGW(TAG, "Unknown alignment type: %d", obj->align_type);
        calculated_x = obj->x;
        calculated_y = obj->y;
        break;
    }

    *x = calculated_x;
    *y = calculated_y;
}

/*=====================
 * Getter functions
 *====================*/

void gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y)
{
    if (obj == NULL || x == NULL || y == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return;
    }

    *x = obj->x;
    *y = obj->y;
}

void gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h)
{
    if (obj == NULL || w == NULL || h == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return;
    }

    *w = obj->width;
    *h = obj->height;
}

/*=====================
 * Other functions
 *====================*/

void gfx_obj_delete(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->parent_handle != NULL) {
        gfx_emote_remove_child(obj->parent_handle, obj);
    }

    switch (obj->type) {
    case GFX_OBJ_TYPE_LABEL:
        gfx_label_delete(obj);
        break;
    case GFX_OBJ_TYPE_ANIMATION:
        gfx_anim_delete(obj);
        break;
    case GFX_OBJ_TYPE_IMAGE:
        gfx_img_delete(obj);
        break;
    default:
        ESP_LOGW(TAG, "Unknown object type: %d", obj->type);
        break;
    }

    free(obj);
}
