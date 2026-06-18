/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"
#include "platform/gfx_platform.h"

#include "core/base/gfx_asset_source.h"

static const char *const TAG = "asset_src";

static esp_err_t gfx_asset_source_read_file(const char *path, uint8_t **out_data, size_t *out_size)
{
    FILE *fp = NULL;
    uint8_t *buf = NULL;
    long size;

    ESP_RETURN_ON_FALSE(path != NULL && out_data != NULL && out_size != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "read file: invalid args");

    fp = fopen(path, "rb");
    ESP_RETURN_ON_FALSE(fp != NULL, ESP_ERR_NOT_FOUND, TAG, "read file: open failed: %s", path);
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ESP_FAIL;
    }
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return ESP_ERR_INVALID_SIZE;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    buf = gfx_platform_aligned_alloc(16, (size_t)size, GFX_PLATFORM_HEAP_DEFAULT);
    if (buf == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        gfx_platform_free(buf);
        fclose(fp);
        return ESP_FAIL;
    }

    fclose(fp);
    *out_data = buf;
    *out_size = (size_t)size;
    return ESP_OK;
}

esp_err_t gfx_asset_source_load(const char *path, gfx_asset_source_t *out)
{
    gfx_asset_store_t *store;
    gfx_err_t asset_err;

    ESP_RETURN_ON_FALSE(path != NULL && out != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "load: invalid args");

    memset(out, 0, sizeof(*out));

    store = gfx_asset_get_default_store();
    if (store != NULL) {
        asset_err = gfx_asset_open_by_name(store, path, &out->view);
        if (asset_err == GFX_OK && out->view.data != NULL && out->view.size > 0U) {
            out->data = (const uint8_t *)out->view.data;
            out->size = out->view.size;
            return ESP_OK;
        }
        gfx_asset_view_close(&out->view);
        memset(&out->view, 0, sizeof(out->view));
    }

    ESP_RETURN_ON_ERROR(gfx_asset_source_read_file(path, &out->owned_data, &out->size),
                        TAG, "load: read file failed: %s", path);
    out->data = out->owned_data;
    return ESP_OK;
}

void gfx_asset_source_release(gfx_asset_source_t *out)
{
    if (out == NULL) {
        return;
    }

    gfx_asset_view_close(&out->view);
    if (out->owned_data != NULL) {
        gfx_platform_free(out->owned_data);
    }
    memset(out, 0, sizeof(*out));
}
