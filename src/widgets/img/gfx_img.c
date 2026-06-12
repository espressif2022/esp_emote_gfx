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
#include "core/display/gfx_refresh_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/widgets/image.h"
#include "codecs/image/gfx_image_decoder_priv.h"

/*********************
 *      DEFINES
 *********************/
#define CHECK_OBJ_TYPE_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_image_src_t src;
} gfx_image_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *const TAG = "img";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static esp_err_t gfx_image_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_image_delete_impl(gfx_object_t *obj);
static esp_err_t gfx_image_resolve_src_payload(const gfx_image_src_t *src, const void **out_payload);

static const gfx_widget_class_t s_gfx_image_widget_class = {
    .type = GFX_OBJ_TYPE_IMAGE,
    .name = "image",
    .draw = gfx_image_draw,
    .delete = gfx_image_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_image_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_image_t *image;
    gfx_image_src_t src_desc;

    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGD(TAG, "draw image: object, state, or draw context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        GFX_LOGW(TAG, "draw image: object type is not image");
        return ESP_ERR_INVALID_ARG;
    }

    image = (gfx_image_t *)obj->src;
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
    gfx_color_format_t color_format = (gfx_color_format_t)header.cf;

    if (!gfx_color_format_is_image_supported(color_format)) {
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

    gfx_object_calc_pos_in_parent(obj);

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area = {obj->geometry.x, obj->geometry.y, obj->geometry.x + image_width, obj->geometry.y + image_height};
    gfx_area_t clip_area;

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_OK;
    }

    uint8_t src_pixel_size = gfx_color_format_get_size(color_format);
    gfx_coord_t src_stride = (header.stride > 0U && src_pixel_size > 0U)
                             ? (gfx_coord_t)(header.stride / src_pixel_size)
                             : (gfx_coord_t)image_width;

    gfx_color_t *dest_pixels = GFX_DRAW_CTX_DEST_PTR(ctx, clip_area.x1, clip_area.y1);
    const uint8_t *src_pixels = image_data +
                                ((size_t)(clip_area.y1 - obj->geometry.y) * src_stride +
                                 (size_t)(clip_area.x1 - obj->geometry.x)) * src_pixel_size;

    gfx_opa_t *alpha_mask = NULL;
    gfx_coord_t alpha_stride = 0;
    if (gfx_color_format_has_alpha(color_format)) {
        const uint8_t *alpha_base = image_data + (size_t)src_stride * image_height * src_pixel_size;
        alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(alpha_base,
                     clip_area.y1 - obj->geometry.y,
                     image_width,
                     clip_area.x1 - obj->geometry.x);
        alpha_stride = (gfx_coord_t)image_width;
    }

    gfx_sw_blend_img_draw(
        dest_pixels,
        ctx->stride,
        src_pixels,
        src_stride,
        alpha_mask,
        alpha_stride,
        &clip_area,
        color_format,
        ctx->swap
    );

    gfx_image_decoder_close(&decoder_dsc);
    return ESP_OK;
}

static esp_err_t gfx_image_resolve_src_payload(const gfx_image_src_t *src, const void **out_payload)
{
    ESP_RETURN_ON_FALSE(src != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: descriptor is NULL");
    ESP_RETURN_ON_FALSE(out_payload != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: output is NULL");
    ESP_RETURN_ON_FALSE(src->data != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve image src: payload is NULL");

    switch (src->type) {
    case GFX_IMAGE_SRC_TYPE_IMAGE_DSC:
        *out_payload = src->data;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t gfx_image_delete_impl(gfx_object_t *obj)
{
    gfx_image_t *image;

    CHECK_OBJ_TYPE_IMAGE(obj);
    image = (gfx_image_t *)obj->src;
    if (image != NULL) {
        free(image);
        obj->src = NULL;
    }

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_object_t *gfx_image_create(gfx_display_t *disp)
{
    gfx_object_t *obj = NULL;
    gfx_image_t *image = NULL;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create image: display is NULL");
        return NULL;
    }

    image = calloc(1, sizeof(*image));
    if (image == NULL) {
        GFX_LOGE(TAG, "create image: no mem for state");
        return NULL;
    }

    if (gfx_object_create_class_instance(disp, &s_gfx_image_widget_class,
                                         image, 0, 0, "gfx_image_create", &obj) != ESP_OK) {
        free(image);
        GFX_LOGE(TAG, "create image: no mem for object");
        return NULL;
    }

    GFX_LOGD(TAG, "create image: object created");
    return obj;
}

esp_err_t gfx_image_set_source_desc(gfx_object_t *obj, const gfx_image_src_t *src)
{
    gfx_image_t *image;
    const void *payload = NULL;
    gfx_image_header_t header;
    gfx_image_decoder_dsc_t dsc;
    CHECK_OBJ_TYPE_IMAGE(obj);
    ESP_RETURN_ON_ERROR(gfx_image_resolve_src_payload(src, &payload), TAG, "set image src: resolve descriptor failed");
    (void)payload;

    image = (gfx_image_t *)obj->src;
    ESP_RETURN_ON_FALSE(image != NULL, ESP_ERR_INVALID_STATE, TAG, "set image src: state is NULL");

    dsc = (gfx_image_decoder_dsc_t) {
        .src = *src,
    };
    ESP_RETURN_ON_ERROR(gfx_image_decoder_info(&dsc, &header), TAG, "set image src: query header failed");

    gfx_object_invalidate(obj);

    image->src = *src;
    obj->geometry.width = header.w;
    obj->geometry.height = header.h;

    gfx_object_update_layout(obj);
    gfx_object_invalidate(obj);

    return ESP_OK;
}
