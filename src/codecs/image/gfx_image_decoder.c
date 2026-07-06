/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMAGE_DECODER
#include "common/gfx_log_priv.h"
#include "gfx/fs.h"
#include "common/gfx_types_priv.h"
#include "platform/gfx_platform.h"
#include "platform/gfx_platform_jpeg_priv.h"

#include "codecs/image/gfx_image_decoder_priv.h"

/*********************
 *      DEFINES
 *********************/

#define GFX_IMAGE_DECODER_MAX_COUNT 8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static gfx_err_t gfx_image_decoder_c_array_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
static gfx_err_t gfx_image_decoder_c_array_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static void gfx_image_decoder_c_array_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static gfx_err_t gfx_image_decoder_jpeg_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
static gfx_err_t gfx_image_decoder_jpeg_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static void gfx_image_decoder_jpeg_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static gfx_err_t gfx_image_decoder_jpeg_prepare_file(gfx_image_decoder_dsc_t *dsc);
static gfx_err_t gfx_image_decoder_jpeg_probe_payload(const uint8_t *payload, size_t payload_size,
        gfx_image_header_t *header);
static const void *gfx_image_decoder_get_payload(const gfx_image_decoder_dsc_t *dsc);
static size_t gfx_image_decoder_get_payload_size(const gfx_image_decoder_dsc_t *dsc);
static gfx_err_t gfx_image_decoder_fs_err(gfx_err_t err);
static bool gfx_image_decoder_is_legacy_plane_alpha_dsc(const gfx_image_dsc_t *image_desc);
static void gfx_image_decoder_canonicalize_dsc_header(const gfx_image_dsc_t *image_desc,
        gfx_image_header_t *header);

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "image_decoder";
static gfx_image_decoder_t *s_registered_decoders[GFX_IMAGE_DECODER_MAX_COUNT] = {NULL};
static uint8_t s_decoder_count = 0;

static gfx_image_decoder_t s_gfx_image_decoder_c_array = {
    .name = "c_array",
    .info_cb = gfx_image_decoder_c_array_info_cb,
    .open_cb = gfx_image_decoder_c_array_open_cb,
    .close_cb = gfx_image_decoder_c_array_close_cb,
};

static gfx_image_decoder_t s_gfx_image_decoder_jpeg = {
    .name = "jpeg",
    .info_cb = gfx_image_decoder_jpeg_info_cb,
    .open_cb = gfx_image_decoder_jpeg_open_cb,
    .close_cb = gfx_image_decoder_jpeg_close_cb,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static const void *gfx_image_decoder_get_payload(const gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return NULL;
    }

    switch (dsc->src.type) {
    case GFX_IMAGE_SRC_TYPE_IMAGE_DSC:
    case GFX_IMAGE_SRC_TYPE_MEMORY:
        return dsc->src.data;
    case GFX_IMAGE_SRC_TYPE_FILE:
        return dsc->src.data;
    default:
        return NULL;
    }
}

static const char *gfx_image_decoder_src_type_name(gfx_image_src_type_t type)
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

static size_t gfx_image_decoder_get_payload_size(const gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return 0U;
    }

    switch (dsc->src.type) {
    case GFX_IMAGE_SRC_TYPE_MEMORY:
        return dsc->src.data_len;
    default:
        return 0U;
    }
}

static gfx_err_t gfx_image_decoder_fs_err(gfx_err_t err)
{
    switch (err) {
    case GFX_OK:                return GFX_OK;
    case GFX_ERR_NO_MEM:        return GFX_ERR_NO_MEM;
    case GFX_ERR_INVALID_ARG:   return GFX_ERR_INVALID_ARG;
    case GFX_ERR_INVALID_STATE: return GFX_ERR_INVALID_STATE;
    case GFX_ERR_INVALID_SIZE:  return GFX_ERR_INVALID_SIZE;
    case GFX_ERR_NOT_FOUND:     return GFX_ERR_NOT_FOUND;
    case GFX_ERR_NOT_SUPPORTED: return GFX_ERR_NOT_SUPPORTED;
    default:                    return GFX_FAIL;
    }
}

static bool gfx_image_decoder_is_legacy_plane_alpha_dsc(const gfx_image_dsc_t *image_desc)
{
    if (image_desc == NULL) {
        return false;
    }

    gfx_color_format_t cf = (gfx_color_format_t)image_desc->header.cf;
    if (!gfx_color_format_has_plane_alpha(cf)) {
        return false;
    }

    uint8_t pixel_size = gfx_color_format_get_size(cf);
    if (pixel_size == 0U || image_desc->header.w == 0U || image_desc->header.h == 0U) {
        return false;
    }

    uint32_t legacy_stride = (uint32_t)image_desc->header.w * ((uint32_t)pixel_size + 1U);
    if (image_desc->header.stride != legacy_stride) {
        return false;
    }

    return image_desc->data_size >= ((size_t)image_desc->header.w * image_desc->header.h *
                                     ((size_t)pixel_size + 1U));
}

static void gfx_image_decoder_canonicalize_dsc_header(const gfx_image_dsc_t *image_desc,
        gfx_image_header_t *header)
{
    if (image_desc == NULL || header == NULL) {
        return;
    }

    memcpy(header, &image_desc->header, sizeof(*header));
    if (gfx_image_decoder_is_legacy_plane_alpha_dsc(image_desc)) {
        header->stride = (uint16_t)((uint32_t)image_desc->header.w *
                                    gfx_color_format_get_size((gfx_color_format_t)image_desc->header.cf));
    }
}

gfx_err_t gfx_image_validate_dsc(const gfx_image_dsc_t *image_desc)
{
    GFX_RETURN_ON_FALSE(image_desc != NULL, GFX_ERR_INVALID_ARG, TAG, "validate image: descriptor is NULL");
    GFX_RETURN_ON_FALSE(image_desc->data != NULL, GFX_ERR_INVALID_ARG, TAG, "validate image: data is NULL");
    GFX_RETURN_ON_FALSE(image_desc->header.magic == GFX_IMAGE_HEADER_MAGIC,
                        GFX_ERR_INVALID_ARG, TAG, "validate image: bad magic");
    GFX_RETURN_ON_FALSE(image_desc->header.flags == 0,
                        GFX_ERR_NOT_SUPPORTED, TAG, "validate image: flags are not supported");
    GFX_RETURN_ON_FALSE(image_desc->header.w > 0 && image_desc->header.h > 0,
                        GFX_ERR_INVALID_ARG, TAG, "validate image: invalid dimensions");

    gfx_color_format_t cf = (gfx_color_format_t)image_desc->header.cf;
    GFX_RETURN_ON_FALSE(gfx_color_format_is_image_supported(cf),
                        GFX_ERR_NOT_SUPPORTED, TAG, "validate image: unsupported color format %u", cf);

    uint8_t pixel_size = gfx_color_format_get_size(cf);
    GFX_RETURN_ON_FALSE(pixel_size > 0, GFX_ERR_NOT_SUPPORTED, TAG, "validate image: invalid pixel size");

    uint32_t min_stride = (uint32_t)image_desc->header.w * pixel_size;
    uint32_t stride = image_desc->header.stride != 0U ? image_desc->header.stride : min_stride;

    if (gfx_image_decoder_is_legacy_plane_alpha_dsc(image_desc)) {
        return GFX_OK;
    }

    GFX_RETURN_ON_FALSE(stride >= min_stride,
                        GFX_ERR_INVALID_ARG, TAG, "validate image: stride is smaller than row payload");
    GFX_RETURN_ON_FALSE((stride % pixel_size) == 0,
                        GFX_ERR_INVALID_ARG, TAG, "validate image: stride is not pixel aligned");

    size_t color_bytes = (size_t)stride * image_desc->header.h;
    size_t alpha_bytes = gfx_color_format_has_plane_alpha(cf)
                         ? (size_t)image_desc->header.w * image_desc->header.h
                         : 0U;
    size_t required_size = color_bytes + alpha_bytes;

    GFX_RETURN_ON_FALSE(image_desc->data_size >= required_size,
                        GFX_ERR_INVALID_SIZE, TAG,
                        "validate image: data_size too small (%" PRIu32 " < %zu)",
                        image_desc->data_size,
                        required_size);

    return GFX_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_image_format_t gfx_image_detect_format(const void *src)
{
    if (src == NULL) {
        return GFX_IMAGE_FORMAT_UNKNOWN;
    }

    uint8_t *byte_ptr = (uint8_t *)src;

    if (byte_ptr[0] == GFX_IMAGE_HEADER_MAGIC) {
        return GFX_IMAGE_FORMAT_C_ARRAY;
    }
    if (byte_ptr[0] == 0xFFU && byte_ptr[1] == 0xD8U) {
        return GFX_IMAGE_FORMAT_JPEG;
    }

    return GFX_IMAGE_FORMAT_UNKNOWN;
}

gfx_err_t gfx_image_decoder_register(gfx_image_decoder_t *decoder)
{
    if (decoder == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    if (s_decoder_count >= GFX_IMAGE_DECODER_MAX_COUNT) {
        GFX_LOGE(TAG, "register image decoder: decoder registry is full");
        return GFX_ERR_NO_MEM;
    }

    s_registered_decoders[s_decoder_count] = decoder;
    s_decoder_count++;

    GFX_LOGD(TAG, "register image decoder: %s", decoder->name);
    return GFX_OK;
}

gfx_err_t gfx_image_decoder_info(gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header)
{
    if (dsc == NULL || header == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_decoder_count; i++) {
        gfx_image_decoder_t *decoder = s_registered_decoders[i];
        if (decoder && decoder->info_cb) {
            gfx_err_t ret = decoder->info_cb(decoder, dsc, header);
            if (ret == GFX_OK) {
                GFX_LOGD(TAG, "probe image decoder: %s matched source", decoder->name);
                return GFX_OK;
            }
        }
    }

    GFX_LOGW(TAG, "probe image decoder: no decoder matched source");
    return GFX_ERR_INVALID_ARG;
}

gfx_err_t gfx_image_decoder_open(gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_decoder_count; i++) {
        gfx_image_decoder_t *decoder = s_registered_decoders[i];
        if (decoder && decoder->open_cb) {
            gfx_err_t ret = decoder->open_cb(decoder, dsc);
            if (ret == GFX_OK) {
                dsc->decoder = decoder;
                GFX_LOGD(TAG, "open image decoder: %s opened source", decoder->name);
                return GFX_OK;
            }
        }
    }

    GFX_LOGW(TAG, "open image decoder: no decoder could open source");
    return GFX_ERR_INVALID_ARG;
}

void gfx_image_decoder_close(gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return;
    }

    gfx_image_decoder_t *decoder = (gfx_image_decoder_t *)dsc->decoder;
    if (decoder != NULL && decoder->close_cb != NULL) {
        decoder->close_cb(decoder, dsc);
    }
    dsc->decoder = NULL;
}

static gfx_err_t gfx_image_decoder_c_array_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header)
{
    (void)decoder;

    if (dsc == NULL || dsc->src.type != GFX_IMAGE_SRC_TYPE_IMAGE_DSC) {
        return GFX_ERR_INVALID_ARG;
    }

    const void *payload = gfx_image_decoder_get_payload(dsc);

    if (payload == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_image_format_t format = gfx_image_detect_format(payload);
    if (format != GFX_IMAGE_FORMAT_C_ARRAY) {
        return GFX_ERR_INVALID_ARG;
    }

    const gfx_image_dsc_t *image_desc = (const gfx_image_dsc_t *)payload;
    GFX_RETURN_ON_ERROR(gfx_image_validate_dsc(image_desc), TAG, "c array info: invalid image descriptor");
    gfx_image_decoder_canonicalize_dsc_header(image_desc, header);

    return GFX_OK;
}

static gfx_err_t gfx_image_decoder_c_array_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    if (dsc == NULL || dsc->src.type != GFX_IMAGE_SRC_TYPE_IMAGE_DSC) {
        return GFX_ERR_INVALID_ARG;
    }

    const void *payload = gfx_image_decoder_get_payload(dsc);

    if (payload == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_image_format_t format = gfx_image_detect_format(payload);
    if (format != GFX_IMAGE_FORMAT_C_ARRAY) {
        return GFX_ERR_INVALID_ARG;
    }

    const gfx_image_dsc_t *image_desc = (const gfx_image_dsc_t *)payload;
    GFX_RETURN_ON_ERROR(gfx_image_validate_dsc(image_desc), TAG, "c array open: invalid image descriptor");
    if (gfx_image_decoder_is_legacy_plane_alpha_dsc(image_desc)) {
        gfx_color_format_t cf = (gfx_color_format_t)image_desc->header.cf;
        uint8_t pixel_size = gfx_color_format_get_size(cf);
        uint32_t color_stride = (uint32_t)image_desc->header.w * pixel_size;
        uint32_t alpha_stride = (uint32_t)image_desc->header.w;
        size_t color_size = (size_t)color_stride * image_desc->header.h;
        size_t alpha_size = (size_t)alpha_stride * image_desc->header.h;
        size_t out_size = color_size + alpha_size;
        uint8_t *out_data = gfx_platform_aligned_alloc(16, out_size, GFX_PLATFORM_HEAP_DEFAULT);
        GFX_RETURN_ON_FALSE(out_data != NULL, GFX_ERR_NO_MEM, TAG,
                            "c array open: alloc row-alpha image failed");

        const uint8_t *src = image_desc->data;
        const uint8_t *src_alpha = src + color_size;
        uint8_t *dst_alpha = out_data + color_size;
        memcpy(out_data, src, color_size);
        memcpy(dst_alpha, src_alpha, alpha_size);

        gfx_image_decoder_canonicalize_dsc_header(image_desc, &dsc->header);
        dsc->data = out_data;
        dsc->data_size = (uint32_t)out_size;
        dsc->user_data = out_data;
        GFX_LOGD(TAG, "c array opened legacy plane-alpha image: %ux%u cf=%u bytes=%zu",
                 (unsigned)dsc->header.w, (unsigned)dsc->header.h,
                 (unsigned)dsc->header.cf, out_size);
        return GFX_OK;
    }

    gfx_image_decoder_canonicalize_dsc_header(image_desc, &dsc->header);
    dsc->data = image_desc->data;
    dsc->data_size = image_desc->data_size;

    return GFX_OK;
}

static void gfx_image_decoder_c_array_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    if (dsc == NULL || dsc->user_data == NULL) {
        return;
    }
    GFX_LOGD(TAG, "c array close: pixels=%p bytes=%zu", dsc->user_data, dsc->data_size);
    gfx_platform_free(dsc->user_data);
    dsc->user_data = NULL;
}

static gfx_err_t gfx_image_decoder_jpeg_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc,
        gfx_image_header_t *header)
{
    const uint8_t *payload = NULL;
    size_t payload_size = 0;
    gfx_err_t ret;
    (void)decoder;

    GFX_RETURN_ON_FALSE(dsc != NULL && (dsc->src.type == GFX_IMAGE_SRC_TYPE_MEMORY ||
                                        dsc->src.type == GFX_IMAGE_SRC_TYPE_FILE),
                        GFX_ERR_INVALID_ARG, TAG, "jpeg info: unsupported source type");
    if (dsc->src.type == GFX_IMAGE_SRC_TYPE_FILE) {
        ret = gfx_image_decoder_jpeg_prepare_file(dsc);
        GFX_RETURN_ON_ERROR(ret, TAG, "jpeg info: open file source failed");
        payload = dsc->src_blob.data;
        payload_size = dsc->src_blob.size;
    } else {
        payload = (const uint8_t *)gfx_image_decoder_get_payload(dsc);
        payload_size = gfx_image_decoder_get_payload_size(dsc);
    }

    ret = gfx_image_decoder_jpeg_probe_payload(payload, payload_size, header);
    if (!dsc->retain_src_blob) {
        gfx_fs_unload(&dsc->src_blob);
    }
    return ret;
}

static gfx_err_t gfx_image_decoder_jpeg_prepare_file(gfx_image_decoder_dsc_t *dsc)
{
    gfx_err_t fs_err;

    if (dsc == NULL || dsc->src.type != GFX_IMAGE_SRC_TYPE_FILE || dsc->src.data == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (dsc->src_blob.data != NULL) {
        return GFX_OK;
    }

    fs_err = gfx_fs_load((const char *)dsc->src.data, &dsc->src_blob);
    return gfx_image_decoder_fs_err(fs_err);
}

static gfx_err_t gfx_image_decoder_jpeg_probe_payload(const uint8_t *payload, size_t payload_size,
        gfx_image_header_t *header)
{
    uint32_t w = 0;
    uint32_t h = 0;
    gfx_err_t ret = GFX_OK;

    GFX_RETURN_ON_FALSE(header != NULL, GFX_ERR_INVALID_ARG, TAG, "jpeg probe: header is NULL");

    GFX_GOTO_ON_FALSE(payload != NULL && payload_size >= 2U,
                      GFX_ERR_INVALID_ARG, cleanup, TAG, "jpeg probe: payload is invalid");
    GFX_GOTO_ON_FALSE(gfx_image_detect_format(payload) == GFX_IMAGE_FORMAT_JPEG,
                      GFX_ERR_INVALID_ARG, cleanup, TAG, "jpeg probe: not jpeg");
    GFX_GOTO_ON_FALSE(gfx_platform_jpeg_is_available(),
                      GFX_ERR_NOT_SUPPORTED, cleanup, TAG, "jpeg probe: platform jpeg unavailable");
    GFX_GOTO_ON_ERROR(gfx_platform_jpeg_get_info(payload, payload_size, &w, &h),
                      cleanup, TAG, "jpeg probe: get info failed");
    GFX_GOTO_ON_FALSE(w > 0U && h > 0U && w <= UINT16_MAX && h <= UINT16_MAX &&
                      w * 3U <= UINT16_MAX,
                      GFX_ERR_INVALID_SIZE, cleanup,
                      TAG, "jpeg probe: unsupported size %" PRIu32 "x%" PRIu32, w, h);

    memset(header, 0, sizeof(*header));
    header->magic = GFX_IMAGE_HEADER_MAGIC;
    header->cf = GFX_COLOR_FORMAT_RGB888;
    header->w = (uint16_t)w;
    header->h = (uint16_t)h;
    header->stride = (uint16_t)(w * 3U);

cleanup:
    return ret;
}

static gfx_err_t gfx_image_decoder_jpeg_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    gfx_image_header_t header;
    const uint8_t *payload = NULL;
    gfx_fs_blob_t file_blob = {0};
    size_t payload_size = 0;
    uint8_t *out_data;
    size_t out_size;
    gfx_err_t ret;
    gfx_err_t fs_err;
    (void)decoder;

    GFX_RETURN_ON_FALSE(dsc != NULL && (dsc->src.type == GFX_IMAGE_SRC_TYPE_MEMORY ||
                                        dsc->src.type == GFX_IMAGE_SRC_TYPE_FILE),
                        GFX_ERR_INVALID_ARG, TAG, "jpeg open: unsupported source type");
    if (dsc->src.type == GFX_IMAGE_SRC_TYPE_FILE) {
        fs_err = gfx_image_decoder_jpeg_prepare_file(dsc);
        GFX_RETURN_ON_ERROR(fs_err, TAG, "jpeg open: open file source failed");
        file_blob = dsc->src_blob;
        memset(&dsc->src_blob, 0, sizeof(dsc->src_blob));
        payload = file_blob.data;
        payload_size = file_blob.size;
    } else {
        payload = (const uint8_t *)gfx_image_decoder_get_payload(dsc);
        payload_size = gfx_image_decoder_get_payload_size(dsc);
    }

    ret = gfx_image_decoder_jpeg_probe_payload(payload, payload_size, &header);
    if (ret != GFX_OK) {
        gfx_fs_unload(&file_blob);
        return ret;
    }

    out_size = (size_t)header.stride * (size_t)header.h;
    out_data = gfx_platform_aligned_alloc(16, out_size, GFX_PLATFORM_HEAP_DEFAULT);
    if (out_data == NULL) {
        gfx_fs_unload(&file_blob);
        return GFX_ERR_NO_MEM;
    }

    GFX_LOGD(TAG, "jpeg open: src=%s payload=%p encoded=%zu out=%ux%u bytes=%zu",
             gfx_image_decoder_src_type_name(dsc->src.type), dsc->src.data, payload_size,
             (unsigned)header.w, (unsigned)header.h, out_size);
    ret = gfx_platform_jpeg_decode_rgb888(payload, payload_size, out_data, &out_size);
    if (ret != GFX_OK) {
        gfx_fs_unload(&file_blob);
        gfx_platform_free(out_data);
        return ret;
    }

    gfx_fs_unload(&file_blob);
    dsc->header = header;
    dsc->data = out_data;
    dsc->data_size = out_size;
    dsc->user_data = out_data;
    GFX_LOGD(TAG, "jpeg opened: pixels=%p bytes=%zu", out_data, out_size);
    return GFX_OK;
}

static void gfx_image_decoder_jpeg_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    if (dsc == NULL) {
        return;
    }
    gfx_fs_unload(&dsc->src_blob);
    if (dsc->user_data == NULL) {
        return;
    }
    GFX_LOGD(TAG, "jpeg close: pixels=%p bytes=%zu", dsc->user_data, dsc->data_size);
    gfx_platform_free(dsc->user_data);
    dsc->user_data = NULL;
    dsc->data = NULL;
    dsc->data_size = 0;
}

gfx_err_t gfx_image_decoder_init(void)
{
    gfx_err_t ret = gfx_image_decoder_register(&s_gfx_image_decoder_c_array);
    if (ret != GFX_OK) {
        return ret;
    }

    ret = gfx_image_decoder_register(&s_gfx_image_decoder_jpeg);
    if (ret != GFX_OK) {
        return ret;
    }

    GFX_LOGD(TAG, "init image decoder: %d decoders registered", s_decoder_count);
    return GFX_OK;
}

gfx_err_t gfx_image_decoder_deinit(void)
{
    for (int i = 0; i < s_decoder_count; i++) {
        s_registered_decoders[i] = NULL;
    }

    s_decoder_count = 0;

    GFX_LOGD(TAG, "deinit image decoder: registry cleared");
    return GFX_OK;
}
