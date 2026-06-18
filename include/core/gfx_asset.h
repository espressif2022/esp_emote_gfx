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

typedef struct {
    const char *partition_label; /**< ESP-IDF mmap-assets partition label. */
    int32_t max_files;           /**< Optional expected asset count; 0 means read from asset header. */
    uint32_t checksum;           /**< Optional expected asset checksum; 0 means read from asset header. */
    bool full_check;             /**< Enable mmap-assets full consistency check. */
} gfx_asset_mmap_config_t;

typedef enum {
    GFX_ASSET_BACKEND_NONE = 0,
    GFX_ASSET_BACKEND_DIR,       /**< Host/Linux directory-backed assets */
    GFX_ASSET_BACKEND_MMAP,      /**< ESP-IDF mmap-assets-backed assets */
    GFX_ASSET_BACKEND_PARTITION, /**< ESP-IDF raw partition-backed assets */
} gfx_asset_backend_t;

typedef enum {
    GFX_ASSET_STORE_TYPE_AUTO = 0,
    GFX_ASSET_STORE_TYPE_DIR,
    GFX_ASSET_STORE_TYPE_MMAP_ASSETS,
    GFX_ASSET_STORE_TYPE_PARTITION,
    GFX_ASSET_STORE_TYPE_MEMORY_TABLE,
} gfx_asset_store_type_t;

typedef enum {
    GFX_ASSET_LOAD_PREFER_DIRECT = 0,
    GFX_ASSET_LOAD_FORCE_COPY,
    GFX_ASSET_LOAD_FORCE_DIRECT,
} gfx_asset_load_mode_t;

typedef enum {
    GFX_ASSET_VIEW_FLAG_NONE        = 0,
    GFX_ASSET_VIEW_FLAG_DIRECT_ADDR = 1U << 0, /**< Data points to a backend direct-read address. */
    GFX_ASSET_VIEW_FLAG_OWNED       = 1U << 1, /**< View owns data and releases it on close. */
    GFX_ASSET_VIEW_FLAG_MAPPED      = 1U << 2, /**< Data comes from an mmap-style mapping. */
    GFX_ASSET_VIEW_FLAG_PERSISTENT  = 1U << 3, /**< Data remains stable while the store is open. */
} gfx_asset_view_flags_t;

typedef struct {
    gfx_asset_store_type_t type;
    gfx_asset_load_mode_t load_mode;
    const char *root_dir;          /**< Directory root for file/VFS backed stores. */
    const char *partition_label;   /**< mmap-assets or raw partition label. */
    int32_t max_files;             /**< mmap-assets expected file count; 0 means read from header. */
    uint32_t checksum;             /**< mmap-assets checksum; 0 means read from header. */
    bool full_check;               /**< Enable mmap-assets full consistency check. */
    uint32_t alloc_caps;           /**< ESP-IDF heap caps for copy-backed reads; 0 means default malloc. */
} gfx_asset_store_config_t;

typedef struct {
    uint32_t offset;
    size_t size;
    const char *name;
    int32_t id;
} gfx_asset_region_t;

typedef struct {
    bool open_by_name;
    bool open_by_id;
    bool open_region;
    bool can_direct_addr;
    bool can_owned_copy;
    bool can_force_copy;
    bool can_force_direct;
} gfx_asset_store_caps_t;

typedef struct {
    const void *data;            /**< Read-only resource bytes */
    size_t size;                 /**< Resource size in bytes */
    const char *name;            /**< Resource name/path when known */
    int32_t id;                  /**< Resource ID when known, or -1 */
    bool is_mapped;              /**< Compatibility alias for GFX_ASSET_VIEW_FLAG_MAPPED */
    uint32_t flags;              /**< GFX_ASSET_VIEW_FLAG_* */
    void *priv;                  /**< Internal backend-owned view state */
} gfx_asset_view_t;

/**
 * @brief Open an asset store from a unified configuration.
 *
 * This is the preferred constructor for new code. Backend-specific helpers are
 * kept as compatibility wrappers.
 *
 * @param config Store configuration.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_store_open(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store);

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
 * @brief Open an ESP-IDF mmap-assets partition as an asset store.
 *
 * On ESP-IDF this wraps the esp_mmap_assets component. Host/Linux builds
 * return GFX_ERR_NOT_SUPPORTED.
 *
 * @param config mmap-assets store configuration.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_store_open_mmap(const gfx_asset_mmap_config_t *config, gfx_asset_store_t **out_store);

/**
 * @brief Set the process-wide default asset store.
 *
 * Decoders and widgets may use this store to resolve name/path based sources.
 * The caller owns the store lifetime and must keep it valid while it is set.
 *
 * @param store Default store; NULL clears it.
 */
void gfx_asset_set_default_store(gfx_asset_store_t *store);

/**
 * @brief Get the process-wide default asset store.
 *
 * @return Current default store, or NULL if not set.
 */
gfx_asset_store_t *gfx_asset_get_default_store(void);

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
 * @brief Query backend capabilities for a store.
 *
 * @param store Asset store.
 * @param out_caps Output capability flags.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_store_get_caps(const gfx_asset_store_t *store, gfx_asset_store_caps_t *out_caps);

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
 * @brief Open an explicit byte region from a region-capable store.
 *
 * The resource layer does not interpret how the region was discovered. This API
 * is intended for raw partition or memory-table callers that already know
 * offset and size.
 *
 * @param store Asset store.
 * @param region Region descriptor.
 * @param out_view Output resource view.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_asset_open_region(gfx_asset_store_t *store,
                                const gfx_asset_region_t *region,
                                gfx_asset_view_t *out_view);

/**
 * @brief Close a resource view returned by gfx_asset_open_*().
 * @param view Resource view; NULL is allowed.
 */
void gfx_asset_view_close(gfx_asset_view_t *view);

#ifdef __cplusplus
}
#endif
