/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/gfx_img.h"
#include "decoder/image/gfx_img_dec_priv.h"

/*********************
 *      DEFINES
 *********************/
#define CHECK_OBJ_TYPE_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "img";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static esp_err_t gfx_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_img_delete_impl(gfx_obj_t *obj);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGD(TAG, "Invalid object or source");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        GFX_LOGW(TAG, "Object is not an image type");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = obj->src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "Failed to get image info");
        return ret;
    }

    uint16_t image_width = header.w;
    uint16_t image_height = header.h;
    uint8_t color_format = header.cf;

    if (color_format != GFX_COLOR_FORMAT_RGB565 && color_format != GFX_COLOR_FORMAT_RGB565A8) {
        GFX_LOGW(TAG, "Unsupported color format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    gfx_image_decoder_dsc_t decoder_dsc = {
        .src = obj->src,
        .header = header,
        .data = NULL,
        .data_size = 0,
        .user_data = NULL
    };

    ret = gfx_image_decoder_open(&decoder_dsc);
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "Failed to open image decoder");
        return ret;
    }

    const uint8_t *image_data = decoder_dsc.data;
    if (image_data == NULL) {
        GFX_LOGE(TAG, "No image data available");
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_ERR_INVALID_STATE;
    }

    gfx_obj_calc_pos_in_parent(obj);

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + image_width, obj->geometry.y + image_height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect(&clip_area, &render_area, &obj_area)) {
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_OK;
    }

    gfx_coord_t src_stride = image_width;

    gfx_color_t *dest_pixels = GFX_DRAW_CTX_DEST_PTR(ctx, clip_area.x1, clip_area.y1);
    gfx_color_t *src_pixels = (gfx_color_t *)GFX_BUFFER_OFFSET_16BPP(image_data,
                              clip_area.y1 - obj->geometry.y,
                              src_stride,
                              clip_area.x1 - obj->geometry.x);

    gfx_opa_t *alpha_mask = NULL;
    if (color_format == GFX_COLOR_FORMAT_RGB565A8) {
        const uint8_t *alpha_base = image_data + src_stride * image_height * GFX_PIXEL_SIZE_16BPP;
        alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(alpha_base,
                     clip_area.y1 - obj->geometry.y,
                     src_stride,
                     clip_area.x1 - obj->geometry.x);
    }

    gfx_sw_blend_img_draw(
        dest_pixels,
        ctx->stride,
        src_pixels,
        src_stride,
        alpha_mask,
        alpha_mask ? src_stride : 0,
        &clip_area,
        ctx->swap
    );

    gfx_image_decoder_close(&decoder_dsc);
    return ESP_OK;
}

static esp_err_t gfx_img_delete_impl(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_img_create(gfx_disp_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "disp must be from gfx_emote_add_disp");
        return NULL;
    }

    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        GFX_LOGE(TAG, "No mem for image object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_IMAGE;
    obj->disp = disp;
    obj->state.is_visible = true;
    obj->vfunc.draw = gfx_img_draw;
    obj->vfunc.delete = gfx_img_delete_impl;
    gfx_obj_invalidate(obj);

    if (gfx_disp_add_child(disp, obj) != ESP_OK) {
        free(obj);
        return NULL;
    }

    GFX_LOGD(TAG, "Created image object");
    return obj;
}

esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src)
{
    CHECK_OBJ_TYPE_IMAGE(obj);

    if (src == NULL) {
        GFX_LOGE(TAG, "Source is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_obj_invalidate(obj);

    obj->src = src;

    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret == ESP_OK) {
        obj->geometry.width = header.w;
        obj->geometry.height = header.h;
    } else {
        GFX_LOGE(TAG, "Failed to get image info");
    }

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    return ESP_OK;
}
