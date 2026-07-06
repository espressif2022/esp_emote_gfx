/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_check.h"
#include "gfx_test_core_fixture.h"

static const char *const TAG = "gfx_test_core";

esp_err_t gfx_test_core_open(gfx_test_core_ctx_t *ctx)
{
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");

    ctx->handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    ESP_RETURN_ON_FALSE(ctx->handle != NULL, ESP_FAIL, TAG, "gfx_core_init failed");

    return ESP_OK;
}

void gfx_test_core_close(gfx_test_core_ctx_t *ctx)
{
    if (ctx == NULL || ctx->handle == NULL) {
        return;
    }

    gfx_core_deinit(ctx->handle);
    ctx->handle = NULL;
}

esp_err_t gfx_test_core_lock(gfx_test_core_ctx_t *ctx)
{
    ESP_RETURN_ON_FALSE(ctx != NULL && ctx->handle != NULL, ESP_ERR_INVALID_STATE, TAG, "core not open");
    return gfx_core_lock(ctx->handle);
}

void gfx_test_core_unlock(gfx_test_core_ctx_t *ctx)
{
    if (ctx != NULL && ctx->handle != NULL) {
        gfx_core_unlock(ctx->handle);
    }
}
