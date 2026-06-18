/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/gfx_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolved read-only file source.
 *
 * Loads a whole file once into a resident, read-only byte range. Direct
 * (mmap-assets / raw partition mmap) backends stay zero-copy through the asset
 * view; copy backends and the fread fallback own a heap buffer that is released
 * by gfx_asset_source_release().
 *
 * This is the single file-loading entry shared by image and animation
 * decoders, so all asset-store backends (mmap-assets, VFS/dir fread, raw
 * partition) and the plain VFS fread fallback are handled in one place.
 */
typedef struct {
    gfx_asset_view_t view;   /**< Default-store view when used (else zeroed). */
    uint8_t *owned_data;     /**< Heap buffer when the fread fallback was used. */
    const uint8_t *data;     /**< Resolved read-only bytes (view or owned). */
    size_t size;             /**< Resolved size in bytes. */
} gfx_asset_source_t;

/**
 * @brief Load a whole file by name/path into a resident read-only range.
 *
 * Resolution order:
 * 1. Default asset store gfx_asset_open_by_name() (covers mmap-assets direct
 *    address, VFS/dir fread to owned buffer, and raw partition mmap/read).
 * 2. VFS fopen/fread fallback into an owned heap buffer (covers plain SPIFFS /
 *    FATFS / SD paths when no default store is configured).
 *
 * @param path Resource name or filesystem path.
 * @param out  Output source; release with gfx_asset_source_release().
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t gfx_asset_source_load(const char *path, gfx_asset_source_t *out);

/**
 * @brief Release any view/buffer owned by a loaded source. Idempotent.
 *
 * @param out Source previously filled by gfx_asset_source_load(); NULL allowed.
 */
void gfx_asset_source_release(gfx_asset_source_t *out);

#ifdef __cplusplus
}
#endif
