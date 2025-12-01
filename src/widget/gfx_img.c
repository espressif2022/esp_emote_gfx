/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "common/gfx_comm.h"
#include "core/gfx_blend_priv.h"
#include "core/gfx_refr_priv.h"
#include "widget/gfx_img.h"
#include "decoder/gfx_img_dec_priv.h"

/*********************
 *      DEFINES
 *********************/
/* Use generic type checking macro from gfx_obj_priv.h */
#define CHECK_OBJ_TYPE_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "gfx_img";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void gfx_draw_img(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);
static esp_err_t gfx_img_delete(gfx_obj_t *obj);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief Virtual draw function for image widget
 */
static void gfx_draw_img(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap)
{
    if (obj == NULL || obj->src == NULL) {
        ESP_LOGD(TAG, "Invalid object or source");
        return;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGW(TAG, "Object is not an image type");
        return;
    }

    /* Use unified decoder to get image information */
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

    /* Check color format - support RGB565 and RGB565A8 formats */
    if (color_format != GFX_COLOR_FORMAT_RGB565 && color_format != GFX_COLOR_FORMAT_RGB565A8) {
        ESP_LOGW(TAG, "Unsupported color format");
        return;
    }

    /* Get image data using unified decoder */
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

    /* Get parent dimensions and calculate aligned position */
    gfx_obj_calc_pos_in_parent(obj);

    /* Calculate clipping area */
    gfx_area_t render_area = {x1, y1, x2, y2};
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + image_width, obj->geometry.y + image_height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        gfx_image_decoder_close(&decoder_dsc);
        return;
    }

    gfx_coord_t dest_stride = (x2 - x1);
    gfx_coord_t src_stride = image_width;

    /* Calculate source and destination buffer pointers with offset */
    gfx_color_t *src_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(image_data,
                              clip_area.y1 - obj->geometry.y,
                              src_stride,
                              clip_area.x1 - obj->geometry.x);
    gfx_color_t *dest_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(dest_buf,
                               clip_area.y1 - y1,
                               dest_stride,
                               clip_area.x1 - x1);

    /* Alpha mask is only present in RGB565A8 format */
    gfx_opa_t *alpha_mask = NULL;
    if (color_format == GFX_COLOR_FORMAT_RGB565A8) {
        /* Alpha mask starts after RGB565 data */
        const uint8_t *alpha_base = image_data + src_stride * image_height * GFX_PIXEL_SIZE_16BPP;
        alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(alpha_base,
                     clip_area.y1 - obj->geometry.y,
                     src_stride,
                     clip_area.x1 - obj->geometry.x);
    }

    /* Blend image to destination buffer */
    gfx_sw_blend_img_draw(
        dest_pixels,
        dest_stride,
        src_pixels,
        src_stride,
        alpha_mask,
        alpha_mask ? src_stride : 0,
        &clip_area,
        255,
        swap
    );

    /* Close decoder */
    gfx_image_decoder_close(&decoder_dsc);
}

/**
 * @brief Virtual delete function for image widget
 */
static esp_err_t gfx_img_delete(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    /* No dynamic resources to free for basic image */
    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

/**
 * @brief Create an image object
 */
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
    obj->state.is_visible = true;
    obj->vfunc.draw = gfx_draw_img;
    obj->vfunc.delete = gfx_img_delete;
    gfx_obj_invalidate(obj);
    gfx_emote_add_child(handle, GFX_OBJ_TYPE_IMAGE, obj);

    ESP_LOGD(TAG, "Created image object");
    return obj;
}

/**
 * @brief Set image source
 */
esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    if (src == NULL) {
        ESP_LOGE(TAG, "Source is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Invalidate the old image */
    gfx_obj_invalidate(obj);

    obj->src = src;

    /* Get image dimensions */
    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret == ESP_OK) {
        obj->geometry.width = header.w;
        obj->geometry.height = header.h;
    } else {
        ESP_LOGE(TAG, "Failed to get image info");
    }

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    return ESP_OK;
}
