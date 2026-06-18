/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform/gfx_platform_jpeg_priv.h"

esp_err_t gfx_platform_jpeg_init(void)
{
    return ESP_OK;
}

void gfx_platform_jpeg_deinit(void)
{
}

bool gfx_platform_jpeg_is_available(void)
{
    return false;
}

esp_err_t gfx_platform_jpeg_get_info(const uint8_t *in_data, size_t in_size,
                                     uint32_t *out_width, uint32_t *out_height)
{
    (void)in_data;
    (void)in_size;
    (void)out_width;
    (void)out_height;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb565_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)width;
    (void)height;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gfx_platform_jpeg_decode_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    (void)in_data;
    (void)in_size;
    (void)out_data;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}
