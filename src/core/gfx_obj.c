/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "esp_log.h"
#include "common/gfx_comm.h"
#include "core/gfx_obj.h"
#include "core/gfx_core_priv.h"
#include "core/gfx_refr_priv.h"
#include "widget/gfx_img_priv.h"
#include "widget/gfx_label_priv.h"
#include "widget/gfx_anim_priv.h"

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

esp_err_t gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    //invalidate the old position
    gfx_obj_invalidate(obj);

    obj->x = x;
    obj->y = y;
    obj->use_align = false;
    //invalidate the new position
    gfx_obj_invalidate(obj);
    ESP_LOGD(TAG, "Set object position: (%d, %d)", x, y);
    return ESP_OK;
}

esp_err_t gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (obj->type == GFX_OBJ_TYPE_ANIMATION || obj->type == GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGW(TAG, "Set size is not useful");
    } else {
        //invalidate the old size
        gfx_obj_invalidate(obj);

        obj->width = w;
        obj->height = h;
        //invalidate the new size
        gfx_obj_invalidate(obj);
    }

    ESP_LOGD(TAG, "Set object size: %dx%d", w, h);
    return ESP_OK;
}

esp_err_t gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(obj->parent_handle, ESP_ERR_INVALID_STATE);

    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        ESP_LOGW(TAG, "Unknown alignment type: %d", align);
        return ESP_ERR_INVALID_ARG;
    }
    // Invalidate old position first
    gfx_obj_invalidate(obj);

    // Update alignment parameters and enable alignment
    obj->align_type = align;
    obj->align_x_ofs = x_ofs;
    obj->align_y_ofs = y_ofs;
    obj->use_align = true;

    uint32_t parent_w = 0, parent_h = 0;
    gfx_emote_get_screen_size(obj->parent_handle, &parent_w, &parent_h);
    gfx_coord_t new_x = obj->x;
    gfx_coord_t new_y = obj->y;
    gfx_obj_cal_aligned_pos(obj, parent_w, parent_h, &new_x, &new_y);
    obj->x = new_x;
    obj->y = new_y;

    // Invalidate new position
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set object alignment: type=%d, offset=(%d, %d)", align, x_ofs, y_ofs);
    return ESP_OK;
}

esp_err_t gfx_obj_set_visible(gfx_obj_t *obj, bool visible)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    obj->is_visible = visible;
    gfx_obj_invalidate(obj);

    ESP_LOGD(TAG, "Set object visibility: %s", visible ? "visible" : "hidden");
    return ESP_OK;
}

bool gfx_obj_get_visible(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, false);

    return obj->is_visible;
}

/*=====================
 * Static helper functions
 *====================*/

void gfx_obj_cal_aligned_pos(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y)
{
    GFX_RETURN_IF_NULL_VOID(obj);
    GFX_RETURN_IF_NULL_VOID(x);
    GFX_RETURN_IF_NULL_VOID(y);

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

esp_err_t gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(x, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(y, ESP_ERR_INVALID_ARG);

    *x = obj->x;
    *y = obj->y;
    return ESP_OK;
}

esp_err_t gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(w, ESP_ERR_INVALID_ARG);
    GFX_RETURN_IF_NULL(h, ESP_ERR_INVALID_ARG);

    *w = obj->width;
    *h = obj->height;
    return ESP_OK;
}

/*=====================
 * Other functions
 *====================*/

esp_err_t gfx_obj_delete(gfx_obj_t *obj)
{
    GFX_RETURN_IF_NULL(obj, ESP_ERR_INVALID_ARG);

    if (GFX_NOT_NULL(obj->parent_handle)) {
        gfx_emote_remove_child(obj->parent_handle, obj);
    }

    gfx_obj_invalidate(obj);
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
    return ESP_OK;
}
