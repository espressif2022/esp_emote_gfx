/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_aaf_format_ctx_t *gfx_aaf_format_handle_t;

esp_err_t gfx_aaf_format_init(const uint8_t *data, size_t data_len, gfx_aaf_format_handle_t *ret_parser);

esp_err_t gfx_aaf_format_deinit(gfx_aaf_format_handle_t handle);

int gfx_aaf_format_get_total_frames(gfx_aaf_format_handle_t handle);

int gfx_aaf_format_get_frame_size(gfx_aaf_format_handle_t handle, int index);

const uint8_t *gfx_aaf_format_get_frame_data(gfx_aaf_format_handle_t handle, int index);

#ifdef __cplusplus
}
#endif 