/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "common/gfx_comm.h"
#include "core/gfx_blend_priv.h"
#include "core/gfx_core_priv.h"
#include "core/gfx_refr_priv.h"
#include "widget/gfx_img_priv.h"

static const char *TAG = "gfx_img";

/*********************
 *      DEFINES
 *********************/

/* Helper macro for type checking */
#define CHECK_OBJ_TYPE_IMAGE(obj) \
    do { \
        ESP_RETURN_ON_ERROR((obj == NULL) ? ESP_ERR_INVALID_ARG : ESP_OK, TAG, "Object is NULL"); \
        ESP_RETURN_ON_ERROR((obj->type != GFX_OBJ_TYPE_IMAGE) ? ESP_ERR_INVALID_ARG : ESP_OK, TAG, \
                           "Object is not an IMAGE type (type=%d). Cannot use image API on non-image objects.", obj->type); \
    } while(0)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void gfx_draw_img(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (obj == NULL || obj->src == NULL) {
        ESP_LOGD(TAG, "Invalid object or source");
        return;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGW(TAG, "Object is not an image type");
        return;
    }

    // Use unified decoder to get image information
    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = obj->src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get image info");
        return;
    }

    uint16_t image_width = header.w;
    uint16_t image_height = header.h;
    uint8_t color_format = header.cf;

    // Check color format - support RGB565A8 format
    if (color_format != GFX_COLOR_FORMAT_RGB565A8) {
        ESP_LOGW(TAG, "Unsupported color format: 0x%02X, only RGB565A8 (0x%02X) is supported",
                 color_format, GFX_COLOR_FORMAT_RGB565A8);
        return;
    }

    // Get image data using unified decoder
    gfx_image_decoder_dsc_t decoder_dsc = {
        .src = obj->src,
        .header = header,
        .data = NULL,
        .data_size = 0,
        .user_data = NULL
    };

    ret = gfx_image_decoder_open(&decoder_dsc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open image decoder");
        return;
    }

    const uint8_t *image_data = decoder_dsc.data;
    if (image_data == NULL) {
        ESP_LOGE(TAG, "No image data available");
        gfx_image_decoder_close(&decoder_dsc);
        return;
    }

    // Get parent container dimensions for alignment calculation
    uint32_t parent_w, parent_h;
    gfx_emote_get_screen_size(obj->parent_handle, &parent_w, &parent_h);

    gfx_coord_t *obj_x = &obj->x;
    gfx_coord_t *obj_y = &obj->y;

    gfx_obj_cal_aligned_pos(obj, parent_w, parent_h, obj_x, obj_y);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {*obj_x, *obj_y, *obj_x + image_width, *obj_y + image_height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        gfx_image_decoder_close(&decoder_dsc);
        return;
    }

    gfx_coord_t dest_stride = (x2 - x1);
    gfx_coord_t src_stride = image_width;

    // Calculate data pointers based on format
    gfx_color_t *src_pixels = (gfx_color_t *)(image_data + (clip_area.y1 - *obj_y) * src_stride * 2);
    gfx_opa_t *alpha_mask = (gfx_opa_t *)(image_data + src_stride * image_height * 2 + (clip_area.y1 - *obj_y) * src_stride);
    gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + (clip_area.y1 - y1) * dest_stride + (clip_area.x1 - x1);

    gfx_sw_blend_img_draw(
        dest_pixels,
        dest_stride,
        src_pixels,
        src_stride,
        alpha_mask,
        src_stride,
        &clip_area,
        255,
        swap
    );

    // Close decoder
    gfx_image_decoder_close(&decoder_dsc);
}

/*=====================
 * Image object creation and management
 *====================*/

gfx_obj_t *gfx_img_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "No mem for image object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_IMAGE;
    obj->parent_handle = handle;
    obj->is_visible = true;
    gfx_obj_invalidate(obj);
    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_IMAGE, obj);
    ESP_LOGD(TAG, "Created image object");
    return obj;
}

esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    if (src == NULL) {
        ESP_LOGE(TAG, "Source is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    //invalidate the old image
    gfx_obj_invalidate(obj);

    obj->src = src;

    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret == ESP_OK) {
        obj->width = header.w;
        obj->height = header.h;
    } else {
        ESP_LOGE(TAG, "Failed to get image info");
    }

    //invalidate the new image
    gfx_obj_invalidate(obj);
    ESP_LOGI(TAG, "Set image source, size: %dx%d", obj->width, obj->height);
    return ESP_OK;
}

/*=====================
 * Image object deletion
 *====================*/

esp_err_t gfx_img_delete(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    return ESP_OK;
}
