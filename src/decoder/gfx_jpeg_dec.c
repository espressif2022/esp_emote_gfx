/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "decoder/gfx_jpeg_dec.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_jpeg_dec.h"

static const char *TAG = "gfx_jpeg_dec";

esp_err_t gfx_jpeg_decode(const uint8_t *in, uint32_t insize, uint8_t *out, size_t out_size, uint32_t *w, uint32_t *h, bool swap)
{
    ESP_RETURN_ON_FALSE(in && out && w && h, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(out_size > 0, ESP_ERR_INVALID_SIZE, TAG, "Invalid output buffer size");

    jpeg_error_t ret;
    jpeg_dec_config_t config = {
        .output_type = swap ? JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        ESP_LOGE(TAG, "Failed to open jpeg decoder");
        return ESP_FAIL;
    }

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !out_info) {
        if (jpeg_io) {
            free(jpeg_io);
        }
        if (out_info) {
            free(out_info);
        }
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to allocate memory for jpeg decoder");
        return ESP_FAIL;
    }

    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;

    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {
        *w = out_info->width;
        *h = out_info->height;

        size_t required_size = out_info->width * out_info->height * 2; // RGB565 = 2 bytes per pixel
        if (out_size < required_size) {
            ESP_LOGE(TAG, "Output buffer too small: need %zu, got %zu", required_size, out_size);
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            return ESP_ERR_INVALID_SIZE;
        }

        jpeg_io->outbuf = out;
        ret = jpeg_dec_process(jpeg_dec, jpeg_io);
        if (ret != JPEG_ERR_OK) {
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            ESP_LOGE(TAG, "Failed to decode jpeg:[%d]", ret);
            return ESP_FAIL;
        }
    } else {
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to parse jpeg header");
        return ESP_FAIL;
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);

    return ESP_OK;
}
