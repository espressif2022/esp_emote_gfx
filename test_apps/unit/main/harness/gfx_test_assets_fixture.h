/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "mmap_generate_assets_test.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_TEST_ASSETS_PARTITION_DEFAULT "assets_test"

typedef struct {
    mmap_assets_handle_t handle;
} gfx_test_assets_ctx_t;

esp_err_t gfx_test_assets_open(gfx_test_assets_ctx_t *ctx, const char *partition_label,
                               uint32_t max_files, uint32_t checksum);
void gfx_test_assets_close(gfx_test_assets_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
