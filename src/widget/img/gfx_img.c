/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
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
#include "widget/img/gfx_img_dec_priv.h"

/*********************
 *      DEFINES
 *********************/
#define CHECK_OBJ_TYPE_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_img_src_t src;
} gfx_img_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "img";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static esp_err_t gfx_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_img_delete_impl(gfx_obj_t *obj);
static esp_err_t gfx_img_resolve_src_payload(const gfx_img_src_t *src, const void **out_payload);

static const gfx_widget_class_t s_gfx_img_widget_class = {
    .type = GFX_OBJ_TYPE_IMAGE,
    .name = "image",
    .draw = gfx_img_draw,
    .delete = gfx_img_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_img_t *image;
    gfx_img_src_t src_desc;

    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGD(TAG, "draw image: object, state, or draw context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        GFX_LOGW(TAG, "draw image: object type is not image");
        return ESP_ERR_INVALID_ARG;
    }

    image = (gfx_img_t *)obj->src;
    if (image->src.data == NULL) {
        GFX_LOGD(TAG, "draw image: source descriptor has no payload");
        return ESP_ERR_INVALID_STATE;
    }

    src_desc = image->src;

    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = src_desc,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "draw image: query header failed");
        return ret;
    }

    uint16_t image_width = header.w;
    uint16_t image_height = header.h;
    uint8_t color_format = header.cf;

    if (color_format != GFX_COLOR_FORMAT_RGB565 && color_format != GFX_COLOR_FORMAT_RGB565A8) {
        GFX_LOGW(TAG, "draw image: unsupported color format %u", color_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    gfx_image_decoder_dsc_t decoder_dsc = {
        .src = src_desc,
        .header = header,
        .data = NULL,
        .data_size = 0,
        .user_data = NULL
    };

    ret = gfx_image_decoder_open(&decoder_dsc);
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "draw image: open decoder failed");
        return ret;
    }

    const uint8_t *image_data = decoder_dsc.data;
    if (image_data == NULL) {
        GFX_LOGE(TAG, "draw image: decoder returned no data");
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_ERR_INVALID_STATE;
    }

    gfx_obj_calc_pos_in_parent(obj);

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + image_width, obj->geometry.y + image_height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
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

static esp_err_t gfx_img_resolve_src_payload(const gfx_img_src_t *src, const void **out_payload)
{
    ESP_RETURN_ON_FALSE(src != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: descriptor is NULL");
    ESP_RETURN_ON_FALSE(out_payload != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: output is NULL");
    ESP_RETURN_ON_FALSE(src->data != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: payload is NULL");

    switch (src->type) {
    case GFX_IMG_SRC_TYPE_IMAGE_DSC:
        *out_payload = src->data;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t gfx_img_delete_impl(gfx_obj_t *obj)
{
    gfx_img_t *image;

    CHECK_OBJ_TYPE_IMAGE(obj);
    image = (gfx_img_t *)obj->src;
    if (image != NULL) {
        free(image);
        obj->src = NULL;
    }

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_img_create(gfx_disp_t *disp)
{
    gfx_obj_t *obj = NULL;
    gfx_img_t *image = NULL;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create image: display is NULL");
        return NULL;
    }

    image = calloc(1, sizeof(*image));
    if (image == NULL) {
        GFX_LOGE(TAG, "create image: no mem for state");
        return NULL;
    }

    if (gfx_obj_create_class_instance(disp, &s_gfx_img_widget_class,
                                      image, 0, 0, "gfx_img_create", &obj) != ESP_OK) {
        free(image);
        GFX_LOGE(TAG, "create image: no mem for object");
        return NULL;
    }

    GFX_LOGD(TAG, "create image: object created");
    return obj;
}

esp_err_t gfx_img_set_src_desc(gfx_obj_t *obj, const gfx_img_src_t *src)
{
    gfx_img_t *image;
    const void *payload = NULL;
    CHECK_OBJ_TYPE_IMAGE(obj);
    ESP_RETURN_ON_ERROR(gfx_img_resolve_src_payload(src, &payload), TAG, "set image src: resolve descriptor failed");
    (void)payload;

    image = (gfx_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(image != NULL, ESP_ERR_INVALID_STATE, TAG, "set image src: state is NULL");

    gfx_obj_invalidate(obj);

    image->src = *src;

    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc = {
        .src = image->src,
    };
    esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret == ESP_OK) {
        obj->geometry.width = header.w;
        obj->geometry.height = header.h;
    } else {
        GFX_LOGE(TAG, "set image src: query header failed");
    }

    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    return ESP_OK;
}

esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src)
{
    const gfx_img_src_t compat_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = src,
    };

    return gfx_img_set_src_desc(obj, &compat_src);
}
