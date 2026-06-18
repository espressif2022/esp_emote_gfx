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
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/widgets/image.h"
#include "widgets/img/gfx_image_resource_priv.h"

/*********************
 *      DEFINES
 *********************/
#define CHECK_OBJ_TYPE_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE, TAG)

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_image_resource_t resource;
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
static esp_err_t gfx_image_load_impl(gfx_object_t *obj);
static void gfx_image_release_impl(gfx_object_t *obj);

static const gfx_widget_class_t s_gfx_image_widget_class = {
    .type = GFX_OBJ_TYPE_IMAGE,
    .name = "image",
    .draw = gfx_image_draw,
    .delete = gfx_image_delete_impl,
    .load = gfx_image_load_impl,
    .release = gfx_image_release_impl,
    .update = NULL,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_image_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_image_t *image;

    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGD(TAG, "draw image: object, state, or draw context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        GFX_LOGW(TAG, "draw image: object type is not image");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    image = (gfx_image_t *)obj->src;
    if (image->resource.src.data == NULL) {
        GFX_LOGD(TAG, "draw image: source descriptor has no payload");
        return ESP_OK;
    }

    uint16_t image_width = image->resource.header.w;
    uint16_t image_height = image->resource.header.h;
    gfx_color_format_t color_format = gfx_image_resource_format(&image->resource);

    if (image_width == 0U || image_height == 0U) {
        return ESP_OK;
    }

    if (!gfx_color_format_is_image_supported(color_format)) {
        GFX_LOGW(TAG, "draw image: unsupported color format %u", color_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint8_t *image_data = gfx_image_resource_pixels(&image->resource);
    if (image_data == NULL) {
        GFX_LOGE(TAG, "draw image: resource is not loaded");
        return ESP_ERR_INVALID_STATE;
    }

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area;
    gfx_area_t clip_area;

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return ESP_OK;
    }
    obj_area.x2 = (gfx_coord_t)(obj_area.x1 + image_width);
    obj_area.y2 = (gfx_coord_t)(obj_area.y1 + image_height);

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
        return ESP_OK;
    }

    uint8_t src_pixel_size = gfx_image_resource_pixel_size(&image->resource);
    gfx_coord_t src_stride = gfx_image_resource_stride_px(&image->resource);

    gfx_area_t draw_local = {
        .x1 = (gfx_coord_t)(clip_area.x1 - ctx->buf_area.x1),
        .y1 = (gfx_coord_t)(clip_area.y1 - ctx->buf_area.y1),
        .x2 = (gfx_coord_t)(clip_area.x2 - ctx->buf_area.x1),
        .y2 = (gfx_coord_t)(clip_area.y2 - ctx->buf_area.y1),
    };
    gfx_coord_t src_x = (gfx_coord_t)(clip_area.x1 - obj_area.x1);
    gfx_coord_t src_y = (gfx_coord_t)(clip_area.y1 - obj_area.y1);
    const uint8_t *src_pixels = image_data +
                                ((size_t)src_y * src_stride +
                                 (size_t)src_x) * src_pixel_size;

    gfx_opa_t *alpha_mask = NULL;
    gfx_coord_t alpha_stride = 0;
    if (gfx_color_format_has_plane_alpha(color_format)) {
        const uint8_t *alpha_base = (const uint8_t *)gfx_image_resource_alpha(&image->resource);
        alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(alpha_base,
                     src_y,
                     image_width,
                     src_x);
        alpha_stride = gfx_image_resource_alpha_stride(&image->resource);
    }

    gfx_render_image_t render_src = {
        .pixels = src_pixels,
        .stride = src_stride,
        .format = color_format,
        .alpha = alpha_mask,
        .alpha_stride = alpha_stride,
    };
    gfx_render_image_t backend_src = render_src;
    backend_src.pixels = image_data;
    if (gfx_color_format_has_plane_alpha(color_format)) {
        backend_src.alpha = (gfx_opa_t *)gfx_image_resource_alpha(&image->resource);
    }
    if (gfx_render_surface_blit_image(obj->disp, &dst_surface, &clip_area, &backend_src,
                                      src_x,
                                      src_y,
                                      0xFFU)) {
        return ESP_OK;
    }

    gfx_sw_blend_img_draw_fmt(
        ctx->buf,
        ctx->stride,
        ctx->format,
        src_pixels,
        src_stride,
        alpha_mask,
        alpha_stride,
        &draw_local,
        color_format,
        0xFFU
    );

    return ESP_OK;
}

static esp_err_t gfx_image_load_impl(gfx_object_t *obj)
{
    gfx_image_t *image;

    CHECK_OBJ_TYPE_IMAGE(obj);
    image = (gfx_image_t *)obj->src;
    ESP_RETURN_ON_FALSE(image != NULL, ESP_ERR_INVALID_STATE, TAG, "load image: state is NULL");
    if (image->resource.src.data == NULL) {
        return ESP_OK;
    }

    return gfx_image_resource_open(&image->resource);
}

static void gfx_image_release_impl(gfx_object_t *obj)
{
    gfx_image_t *image;

    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_IMAGE) {
        return;
    }

    image = (gfx_image_t *)obj->src;
    gfx_image_resource_close(&image->resource);
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
    CHECK_OBJ_TYPE_IMAGE(obj);

    image = (gfx_image_t *)obj->src;
    ESP_RETURN_ON_FALSE(image != NULL, ESP_ERR_INVALID_STATE, TAG, "set image src: state is NULL");

    ESP_RETURN_ON_ERROR(gfx_image_resource_set_source(&image->resource, src), TAG, "set image src failed");

    gfx_object_invalidate(obj);

    obj->geometry.width = image->resource.header.w;
    obj->geometry.height = image->resource.header.h;
    obj->local_geometry.width = image->resource.header.w;
    obj->local_geometry.height = image->resource.header.h;

    gfx_object_mark_resource_dirty(obj);
    gfx_object_update_layout(obj);
    gfx_object_invalidate(obj);

    return ESP_OK;
}
