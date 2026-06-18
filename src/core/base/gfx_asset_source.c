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

esp_err_t gfx_asset_source_open_stream(const char *path, gfx_asset_stream_t *out)
{
    ESP_RETURN_ON_FALSE(path != NULL && out != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "open stream: invalid args");

    memset(out, 0, sizeof(*out));

    /* Prefer a real streamed handle: only requested bytes ever become resident.
     * mmap-asset names are not fopen-able and fall through to the store. */
    FILE *fp = fopen(path, "rb");
    if (fp != NULL) {
        long size;
        if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
            fclose(fp);
            return ESP_FAIL;
        }
        out->fp = fp;
        out->size = (size_t)size;
        out->mapped = NULL;
        return ESP_OK;
    }

    /* Fall back to the default store. A mapped/direct view streams zero-copy; a
     * copy-backed view stays resident (correct, just no streaming benefit). */
    gfx_asset_store_t *store = gfx_asset_get_default_store();
    if (store != NULL) {
        gfx_err_t asset_err = gfx_asset_open_by_name(store, path, &out->view);
        if (asset_err == GFX_OK && out->view.data != NULL && out->view.size > 0U) {
            out->mapped = (const uint8_t *)out->view.data;
            out->size = out->view.size;
            return ESP_OK;
        }
        gfx_asset_view_close(&out->view);
        memset(&out->view, 0, sizeof(out->view));
    }

    GFX_LOGE(TAG, "open stream: cannot open %s", path);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_asset_source_stream_read(gfx_asset_stream_t *s, size_t offset, size_t len, uint8_t *dst)
{
    ESP_RETURN_ON_FALSE(s != NULL && dst != NULL, ESP_ERR_INVALID_ARG, TAG, "stream read: invalid args");
    if (len == 0U) {
        return ESP_OK;
    }
    /* Reject ranges that fall outside the source without overflowing. */
    ESP_RETURN_ON_FALSE(offset <= s->size && len <= s->size - offset, ESP_ERR_INVALID_SIZE, TAG,
                        "stream read: range [%zu,+%zu) exceeds size %zu", offset, len, s->size);

    if (s->mapped != NULL) {
        memcpy(dst, s->mapped + offset, len);
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(s->fp != NULL, ESP_ERR_INVALID_STATE, TAG, "stream read: no backend");
    FILE *fp = (FILE *)s->fp;
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    if (fread(dst, 1, len, fp) != len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void gfx_asset_source_close_stream(gfx_asset_stream_t *s)
{
    if (s == NULL) {
        return;
    }
    if (s->fp != NULL) {
        fclose((FILE *)s->fp);
    }
    gfx_asset_view_close(&s->view);
    memset(s, 0, sizeof(*s));
}
