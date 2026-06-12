/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMAGE_DECODER
#include "common/gfx_log_priv.h"
#include "core/gfx_types_priv.h"

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

static esp_err_t gfx_image_decoder_c_array_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
static esp_err_t gfx_image_decoder_c_array_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static void gfx_image_decoder_c_array_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
static const void *gfx_image_decoder_get_payload(const gfx_image_decoder_dsc_t *dsc);

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
        return dsc->src.data;
    default:
        return NULL;
    }
}

esp_err_t gfx_image_validate_dsc(const gfx_image_dsc_t *image_desc)
{
    ESP_RETURN_ON_FALSE(image_desc != NULL, ESP_ERR_INVALID_ARG, TAG, "validate image: descriptor is NULL");
    ESP_RETURN_ON_FALSE(image_desc->data != NULL, ESP_ERR_INVALID_ARG, TAG, "validate image: data is NULL");
    ESP_RETURN_ON_FALSE(image_desc->header.magic == GFX_IMAGE_HEADER_MAGIC,
                        ESP_ERR_INVALID_ARG, TAG, "validate image: bad magic");
    ESP_RETURN_ON_FALSE(image_desc->header.flags == 0,
                        ESP_ERR_NOT_SUPPORTED, TAG, "validate image: flags are not supported");
    ESP_RETURN_ON_FALSE(image_desc->header.w > 0 && image_desc->header.h > 0,
                        ESP_ERR_INVALID_ARG, TAG, "validate image: invalid dimensions");

    gfx_color_format_t cf = (gfx_color_format_t)image_desc->header.cf;
    ESP_RETURN_ON_FALSE(gfx_color_format_is_image_supported(cf),
                        ESP_ERR_NOT_SUPPORTED, TAG, "validate image: unsupported color format %u", cf);

    uint8_t pixel_size = gfx_color_format_get_size(cf);
    ESP_RETURN_ON_FALSE(pixel_size > 0, ESP_ERR_NOT_SUPPORTED, TAG, "validate image: invalid pixel size");
    ESP_RETURN_ON_FALSE(image_desc->header.stride >= (uint32_t)image_desc->header.w * pixel_size,
                        ESP_ERR_INVALID_ARG, TAG, "validate image: stride is smaller than row payload");
    ESP_RETURN_ON_FALSE((image_desc->header.stride % pixel_size) == 0,
                        ESP_ERR_INVALID_ARG, TAG, "validate image: stride is not pixel aligned");

    size_t color_bytes = (size_t)image_desc->header.stride * image_desc->header.h;
    size_t alpha_bytes = gfx_color_format_has_alpha(cf)
                         ? (size_t)image_desc->header.w * image_desc->header.h
                         : 0U;
    size_t required_size = color_bytes + alpha_bytes;

    ESP_RETURN_ON_FALSE(image_desc->data_size >= required_size,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "validate image: data_size too small (%" PRIu32 " < %u)",
                        image_desc->data_size,
                        (unsigned)required_size);

    return ESP_OK;
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

    return GFX_IMAGE_FORMAT_UNKNOWN;
}

esp_err_t gfx_image_decoder_register(gfx_image_decoder_t *decoder)
{
    if (decoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_decoder_count >= GFX_IMAGE_DECODER_MAX_COUNT) {
        GFX_LOGE(TAG, "register image decoder: decoder registry is full");
        return ESP_ERR_NO_MEM;
    }

    s_registered_decoders[s_decoder_count] = decoder;
    s_decoder_count++;

    GFX_LOGD(TAG, "register image decoder: %s", decoder->name);
    return ESP_OK;
}

esp_err_t gfx_image_decoder_info(gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header)
{
    if (dsc == NULL || header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_decoder_count; i++) {
        gfx_image_decoder_t *decoder = s_registered_decoders[i];
        if (decoder && decoder->info_cb) {
            esp_err_t ret = decoder->info_cb(decoder, dsc, header);
            if (ret == ESP_OK) {
                GFX_LOGD(TAG, "probe image decoder: %s matched source", decoder->name);
                return ESP_OK;
            }
        }
    }

    GFX_LOGW(TAG, "probe image decoder: no decoder matched source");
    return ESP_ERR_INVALID_ARG;
}

esp_err_t gfx_image_decoder_open(gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_decoder_count; i++) {
        gfx_image_decoder_t *decoder = s_registered_decoders[i];
        if (decoder && decoder->open_cb) {
            esp_err_t ret = decoder->open_cb(decoder, dsc);
            if (ret == ESP_OK) {
                GFX_LOGD(TAG, "open image decoder: %s opened source", decoder->name);
                return ESP_OK;
            }
        }
    }

    GFX_LOGW(TAG, "open image decoder: no decoder could open source");
    return ESP_ERR_INVALID_ARG;
}

void gfx_image_decoder_close(gfx_image_decoder_dsc_t *dsc)
{
    if (dsc == NULL) {
        return;
    }

    for (int i = 0; i < s_decoder_count; i++) {
        gfx_image_decoder_t *decoder = s_registered_decoders[i];
        if (decoder && decoder->close_cb) {
            decoder->close_cb(decoder, dsc);
        }
    }
}

static esp_err_t gfx_image_decoder_c_array_info_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header)
{
    (void)decoder;

    const void *payload = gfx_image_decoder_get_payload(dsc);

    if (payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_image_format_t format = gfx_image_detect_format(payload);
    if (format != GFX_IMAGE_FORMAT_C_ARRAY) {
        return ESP_ERR_INVALID_ARG;
    }

    const gfx_image_dsc_t *image_desc = (const gfx_image_dsc_t *)payload;
    ESP_RETURN_ON_ERROR(gfx_image_validate_dsc(image_desc), TAG, "c array info: invalid image descriptor");
    memcpy(header, &image_desc->header, sizeof(gfx_image_header_t));

    return ESP_OK;
}

static esp_err_t gfx_image_decoder_c_array_open_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;

    const void *payload = gfx_image_decoder_get_payload(dsc);

    if (payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_image_format_t format = gfx_image_detect_format(payload);
    if (format != GFX_IMAGE_FORMAT_C_ARRAY) {
        return ESP_ERR_INVALID_ARG;
    }

    const gfx_image_dsc_t *image_desc = (const gfx_image_dsc_t *)payload;
    ESP_RETURN_ON_ERROR(gfx_image_validate_dsc(image_desc), TAG, "c array open: invalid image descriptor");
    dsc->data = image_desc->data;
    dsc->data_size = image_desc->data_size;

    return ESP_OK;
}

static void gfx_image_decoder_c_array_close_cb(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc)
{
    (void)decoder;
    (void)dsc;
}

esp_err_t gfx_image_decoder_init(void)
{
    esp_err_t ret = gfx_image_decoder_register(&s_gfx_image_decoder_c_array);
    if (ret != ESP_OK) {
        return ret;
    }

    GFX_LOGD(TAG, "init image decoder: %d decoders registered", s_decoder_count);
    return ESP_OK;
}

esp_err_t gfx_image_decoder_deinit(void)
{
    for (int i = 0; i < s_decoder_count; i++) {
        s_registered_decoders[i] = NULL;
    }

    s_decoder_count = 0;

    GFX_LOGD(TAG, "deinit image decoder: registry cleared");
    return ESP_OK;
}
