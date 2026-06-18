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

/**
 * @brief On-demand read-only stream over a file source.
 *
 * Unlike gfx_asset_source_load(), this does not keep the whole file resident.
 * Two backends are exposed through one shape:
 * - `mapped != NULL`: a zero-copy memory-mapped/direct backend. `read()` is a
 *   bounds-checked memcpy from `mapped + offset`; callers may also decode
 *   straight from `mapped` because the mapping stays valid until close.
 * - `mapped == NULL`: a streamed backend (VFS fread). `read()` seeks and reads
 *   the requested range into the caller buffer; only the requested bytes are
 *   ever in RAM.
 *
 * Fields are owned by gfx_asset_source_open_stream() and released by
 * gfx_asset_source_close_stream(). Do not copy the struct while it is open.
 */
typedef struct gfx_asset_stream {
    size_t size;                /**< Total stream size in bytes. */
    const uint8_t *mapped;      /**< Zero-copy base, or NULL for streamed reads. */
    void *fp;                   /**< FILE* for the streamed backend, else NULL. */
    gfx_asset_view_t view;      /**< Held view for the mapped backend, else zeroed. */
} gfx_asset_stream_t;

/**
 * @brief Open a file source as an on-demand stream.
 *
 * Resolution order favours real streaming:
 * 1. fopen(path): if it opens, use a streamed FILE* backend (SPIFFS/FATFS/SD or
 *    a host directory file) so only requested bytes are resident.
 * 2. Otherwise fall back to the default asset store. A mapped/direct view
 *    becomes a zero-copy stream; a copy-backed view is kept as a resident
 *    fallback (no streaming benefit, but still correct).
 *
 * @param path Resource name or filesystem path.
 * @param out  Output stream; release with gfx_asset_source_close_stream().
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t gfx_asset_source_open_stream(const char *path, gfx_asset_stream_t *out);

/**
 * @brief Read a byte range from a stream into a caller buffer.
 *
 * @param s      Stream opened by gfx_asset_source_open_stream().
 * @param offset Absolute byte offset into the source.
 * @param len    Number of bytes to read.
 * @param dst    Destination buffer of at least `len` bytes.
 * @return ESP_OK on success, or an esp_err_t error code (including a short read
 *         or an out-of-range request).
 */
esp_err_t gfx_asset_source_stream_read(gfx_asset_stream_t *s, size_t offset, size_t len, uint8_t *dst);

/**
 * @brief Release any handle/view owned by a stream. Idempotent.
 *
 * @param s Stream previously opened by gfx_asset_source_open_stream(); NULL allowed.
 */
void gfx_asset_source_close_stream(gfx_asset_stream_t *s);

#ifdef __cplusplus
}
#endif
