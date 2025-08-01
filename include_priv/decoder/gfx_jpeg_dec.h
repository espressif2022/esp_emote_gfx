/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_jpeg_dec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode JPEG data to specified output buffer
 * @param in Input JPEG data
 * @param insize Input data length
 * @param out Output buffer pointer
 * @param out_size Output buffer size
 * @param w Output image width
 * @param h Output image height
 * @param swap Whether to swap byte order (true: RGB565LE, false: RGB565BE)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_jpeg_decode(const uint8_t *in, uint32_t insize, uint8_t *out, size_t out_size, uint32_t *w, uint32_t *h, bool swap);

#ifdef __cplusplus
}
#endif 