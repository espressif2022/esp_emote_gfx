/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "core/gfx_blend_internal.h"
#include "core/gfx_core_internal.h"
#include "core/gfx_refr.h"
#include "widget/gfx_comm.h"
#include "widget/gfx_img_internal.h"

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

    ESP_LOGD(TAG, "Drawing image: %dx%d, format: 0x%02X", image_width, image_height, color_format);

    // Get parent container dimensions for alignment calculation
    uint32_t parent_width, parent_height;
    gfx_emote_get_screen_size(obj->parent_handle, &parent_width, &parent_height);

    gfx_coord_t obj_x = obj->x;
    gfx_coord_t obj_y = obj->y;

    gfx_obj_cal_aligned_pos(obj, parent_width, parent_height, &obj_x, &obj_y);

    gfx_area_t clip_region;
    clip_region.x1 = MAX(x1, obj_x);
    clip_region.y1 = MAX(y1, obj_y);
    clip_region.x2 = MIN(x2, obj_x + image_width);
    clip_region.y2 = MIN(y2, obj_y + image_height);

    // Check if there's any overlap
    if (clip_region.x1 >= clip_region.x2 || clip_region.y1 >= clip_region.y2) {
        gfx_image_decoder_close(&decoder_dsc);
        return;
    }

    gfx_coord_t dest_buffer_stride = (x2 - x1);
    gfx_coord_t source_buffer_stride = image_width;

    // Calculate data pointers based on format
    gfx_color_t *source_pixels = (gfx_color_t *)(image_data + (clip_region.y1 - obj_y) * source_buffer_stride * 2);
    gfx_opa_t *alpha_mask = (gfx_opa_t *)(image_data + source_buffer_stride * image_height * 2 + (clip_region.y1 - obj_y) * source_buffer_stride);
    gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + (clip_region.y1 - y1) * dest_buffer_stride + (clip_region.x1 - x1);

    gfx_sw_blend_img_draw(
        dest_pixels,
        dest_buffer_stride,
        source_pixels,
        source_buffer_stride,
        alpha_mask,
        source_buffer_stride,
        &clip_region,
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
        ESP_LOGE(TAG, "Failed to allocate memory for image object");
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

    obj->src = src;

    if (src != NULL) {
        gfx_image_header_t header;
        gfx_image_decoder_dsc_t dsc = {
            .src = src,
        };
        esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
        if (ret == ESP_OK) {
            obj->width = header.w;
            obj->height = header.h;
        }
    }

    gfx_obj_invalidate(obj);
    ESP_LOGD(TAG, "Set image source, size: %dx%d", obj->width, obj->height);
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
