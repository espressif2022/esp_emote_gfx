/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"
#include "common/gfx_types_priv.h"
#include "widgets/img/gfx_image_resource_priv.h"

static const char *const TAG = "img_res";

static const char *gfx_image_resource_src_type_name(gfx_image_src_type_t type)
{
    switch (type) {
    case GFX_IMAGE_SRC_TYPE_IMAGE_DSC:
        return "dsc";
    case GFX_IMAGE_SRC_TYPE_MEMORY:
        return "mem";
    case GFX_IMAGE_SRC_TYPE_FILE:
        return "file";
    default:
        return "unknown";
    }
}

static gfx_err_t gfx_image_resource_validate_src(const gfx_image_src_t *src)
{
    GFX_RETURN_ON_FALSE(src != NULL, GFX_ERR_INVALID_ARG, TAG, "image source is NULL");
    GFX_RETURN_ON_FALSE(src->data != NULL, GFX_ERR_INVALID_ARG, TAG, "image source payload is NULL");

    switch (src->type) {
    case GFX_IMAGE_SRC_TYPE_IMAGE_DSC:
        return GFX_OK;
    case GFX_IMAGE_SRC_TYPE_MEMORY:
        GFX_RETURN_ON_FALSE(src->data_len > 0U, GFX_ERR_INVALID_ARG,
                            TAG, "image source payload length is zero");
        return GFX_OK;
    case GFX_IMAGE_SRC_TYPE_FILE:
        return GFX_OK;
    default:
        return GFX_ERR_NOT_SUPPORTED;
    }
}

gfx_err_t gfx_image_resource_set_source(gfx_image_resource_t *resource, const gfx_image_src_t *src)
{
    gfx_image_decoder_dsc_t dsc = {0};
    gfx_image_header_t header;

    GFX_RETURN_ON_FALSE(resource != NULL, GFX_ERR_INVALID_ARG, TAG, "image resource is NULL");
    GFX_RETURN_ON_ERROR(gfx_image_resource_validate_src(src), TAG, "set source: invalid descriptor");

    dsc.src = *src;
    dsc.retain_src_blob = true;
    gfx_err_t ret = gfx_image_decoder_info(&dsc, &header);
    if (ret != GFX_OK) {
        gfx_fs_unload(&dsc.src_blob);
        GFX_LOGE(TAG, "set source: query header failed");
        return ret;
    }
    if (!gfx_color_format_is_image_supported((gfx_color_format_t)header.cf)) {
        gfx_fs_unload(&dsc.src_blob);
        return GFX_ERR_NOT_SUPPORTED;
    }

    gfx_image_resource_close(resource);
    resource->src = *src;
    resource->header = header;
    resource->decoder.src_blob = dsc.src_blob;
    memset(&dsc.src_blob, 0, sizeof(dsc.src_blob));
    return GFX_OK;
}

gfx_err_t gfx_image_resource_open(gfx_image_resource_t *resource)
{
    GFX_RETURN_ON_FALSE(resource != NULL, GFX_ERR_INVALID_ARG, TAG, "image resource is NULL");
    if (resource->src.data == NULL) {
        return GFX_OK;
    }
    if (resource->decoder.data != NULL) {
        return GFX_OK;
    }

    gfx_fs_blob_t src_blob = resource->decoder.src_blob;
    memset(&resource->decoder, 0, sizeof(resource->decoder));
    resource->decoder = (gfx_image_decoder_dsc_t) {
        .src = resource->src,
        .header = resource->header,
        .src_blob = src_blob,
        .retain_src_blob = true,
    };
    GFX_LOGD(TAG, "open image resource: src=%s payload=%p size=%zu header=%ux%u cf=%u",
             gfx_image_resource_src_type_name(resource->src.type), resource->src.data,
             resource->src.data_len, (unsigned)resource->header.w, (unsigned)resource->header.h,
             (unsigned)resource->header.cf);
    gfx_err_t ret = gfx_image_decoder_open(&resource->decoder);
    if (ret != GFX_OK) {
        gfx_image_decoder_close(&resource->decoder);
        memset(&resource->decoder, 0, sizeof(resource->decoder));
        GFX_LOGE(TAG, "open decoder failed");
        return ret;
    }
    if (resource->decoder.data == NULL) {
        gfx_image_decoder_close(&resource->decoder);
        memset(&resource->decoder, 0, sizeof(resource->decoder));
        return GFX_ERR_INVALID_STATE;
    }
    GFX_LOGD(TAG, "opened image resource: pixels=%p bytes=%zu",
             resource->decoder.data, resource->decoder.data_size);
    return GFX_OK;
}

void gfx_image_resource_close(gfx_image_resource_t *resource)
{
    if (resource == NULL) {
        return;
    }

    if (resource->decoder.data != NULL || resource->decoder.user_data != NULL ||
            resource->decoder.src_blob.data != NULL) {
        GFX_LOGD(TAG, "close image resource: src=%s payload=%p pixels=%p bytes=%zu",
                 gfx_image_resource_src_type_name(resource->src.type), resource->src.data,
                 resource->decoder.data, resource->decoder.data_size);
        gfx_image_decoder_close(&resource->decoder);
    }
    gfx_fs_unload(&resource->decoder.src_blob);
    memset(&resource->decoder, 0, sizeof(resource->decoder));
}

bool gfx_image_resource_is_open(const gfx_image_resource_t *resource)
{
    return resource != NULL && resource->decoder.data != NULL;
}

uint8_t gfx_image_resource_pixel_size(const gfx_image_resource_t *resource)
{
    return resource != NULL
           ? gfx_color_format_get_size((gfx_color_format_t)resource->header.cf)
           : 0U;
}

gfx_coord_t gfx_image_resource_stride_px(const gfx_image_resource_t *resource)
{
    uint8_t pixel_size = gfx_image_resource_pixel_size(resource);

    if (resource == NULL || pixel_size == 0U) {
        return 0;
    }
    return (resource->header.stride > 0U)
           ? (gfx_coord_t)(resource->header.stride / pixel_size)
           : (gfx_coord_t)resource->header.w;
}

gfx_coord_t gfx_image_resource_height(const gfx_image_resource_t *resource)
{
    return resource != NULL ? (gfx_coord_t)resource->header.h : 0;
}

gfx_color_format_t gfx_image_resource_format(const gfx_image_resource_t *resource)
{
    return resource != NULL ? (gfx_color_format_t)resource->header.cf : GFX_COLOR_FORMAT_UNKNOWN;
}

const uint8_t *gfx_image_resource_pixels(const gfx_image_resource_t *resource)
{
    return gfx_image_resource_is_open(resource) ? resource->decoder.data : NULL;
}

const gfx_opa_t *gfx_image_resource_alpha(const gfx_image_resource_t *resource)
{
    uint8_t pixel_size;
    gfx_coord_t stride;

    if (resource == NULL ||
            !gfx_color_format_has_plane_alpha((gfx_color_format_t)resource->header.cf) ||
            resource->decoder.data == NULL) {
        return NULL;
    }

    pixel_size = gfx_image_resource_pixel_size(resource);
    stride = gfx_image_resource_stride_px(resource);
    if (pixel_size == 0U || stride <= 0) {
        return NULL;
    }

    return (const gfx_opa_t *)(resource->decoder.data +
                               (size_t)stride * resource->header.h * pixel_size);
}

gfx_coord_t gfx_image_resource_alpha_stride(const gfx_image_resource_t *resource)
{
    if (resource == NULL ||
            !gfx_color_format_has_plane_alpha((gfx_color_format_t)resource->header.cf)) {
        return 0;
    }
    return (gfx_coord_t)resource->header.w;
}
