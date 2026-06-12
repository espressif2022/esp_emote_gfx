/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/gfx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_asset_store gfx_asset_store_t;

typedef enum {
    GFX_ASSET_BACKEND_NONE = 0,
    GFX_ASSET_BACKEND_DIR,       /**< Host/Linux directory-backed assets */
    GFX_ASSET_BACKEND_MMAP,      /**< ESP-IDF mmap-assets-backed assets */
} gfx_asset_backend_t;

typedef struct {
    const void *data;            /**< Read-only resource bytes */
    size_t size;                 /**< Resource size in bytes */
    const char *name;            /**< Resource name/path when known */
    int32_t id;                  /**< Resource ID when known, or -1 */
    bool is_mapped;              /**< True when data is backed by an mmap-style view */
    void *priv;                  /**< Internal backend-owned view state */
} gfx_asset_view_t;

/**
 * @brief Open a host directory as an asset store.
 *
 * Paths passed to gfx_asset_open_by_name() are resolved relative to root_dir.
 * This API is available on host/Linux builds. ESP-IDF builds return
 * GFX_ERR_NOT_SUPPORTED until an mmap-assets store is opened through the ESP
 * adapter.
 *
 * @param root_dir Directory that contains asset files.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_store_open_dir(const char *root_dir, gfx_asset_store_t **out_store);

/**
 * @brief Close an asset store and release backend state.
 * @param store Store returned by gfx_asset_store_open_*(); NULL is allowed.
 */
void gfx_asset_store_close(gfx_asset_store_t *store);

/**
 * @brief Get the backend kind for a store.
 * @param store Asset store.
 * @return Backend kind, or GFX_ASSET_BACKEND_NONE for NULL.
 */
gfx_asset_backend_t gfx_asset_store_get_backend(const gfx_asset_store_t *store);

/**
 * @brief Open a resource by name/path.
 *
 * The returned view stays valid until gfx_asset_view_close() is called.
 *
 * @param store Asset store.
 * @param name Resource name or relative path.
 * @param out_view Output resource view.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view);

/**
 * @brief Open a resource by numeric ID.
 *
 * Directory stores require a manifest before this can resolve IDs, so the
 * first implementation returns GFX_ERR_NOT_SUPPORTED for host directory stores.
 *
 * @param store Asset store.
 * @param id Resource ID.
 * @param out_view Output resource view.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view);

/**
 * @brief Close a resource view returned by gfx_asset_open_*().
 * @param view Resource view; NULL is allowed.
 */
void gfx_asset_view_close(gfx_asset_view_t *view);

#ifdef __cplusplus
}
#endif
