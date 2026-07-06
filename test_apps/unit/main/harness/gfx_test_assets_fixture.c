/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_check.h"
#include "gfx_test_assets_fixture.h"

static const char *const TAG = "gfx_test_assets";

esp_err_t gfx_test_assets_open(gfx_test_assets_ctx_t *ctx, const char *partition_label,
                               uint32_t max_files, uint32_t checksum)
{
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    ESP_RETURN_ON_FALSE(partition_label != NULL && partition_label[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "partition_label invalid");

    ctx->handle = NULL;
    return mmap_assets_new(&(mmap_assets_config_t) {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {.mmap_enable = true, .full_check = true},
    }, &ctx->handle);
}

void gfx_test_assets_close(gfx_test_assets_ctx_t *ctx)
{
    if (ctx == NULL || ctx->handle == NULL) {
        return;
    }

    mmap_assets_del(ctx->handle);
    ctx->handle = NULL;
}
