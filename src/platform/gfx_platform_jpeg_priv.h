/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gfx_platform_jpeg_init(void);
void gfx_platform_jpeg_deinit(void);

bool gfx_platform_jpeg_is_available(void);

esp_err_t gfx_platform_jpeg_get_info(const uint8_t *in_data, size_t in_size,
                                     uint32_t *out_width, uint32_t *out_height);

esp_err_t gfx_platform_jpeg_decode_rgb888(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size);

esp_err_t gfx_platform_jpeg_decode_rgb565_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size);

esp_err_t gfx_platform_jpeg_decode_rgb565(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size);

#ifdef __cplusplus
}
#endif
