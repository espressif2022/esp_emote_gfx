/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_fs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Internal asset-store contract.
 *
 * These types describe how backends (mmap-assets, host directory, raw
 * partition) expose resolved byte ranges to the rest of the library. They are
 * intentionally kept out of the public header: applications use the stdio-style
 * file API (gfx_fs_fopen()/gfx_fs_load()) instead of poking views, flags,
 * and capability bits directly.
 * ========================================================================= */

typedef enum {
    GFX_FS_BACKEND_NONE = 0,
    GFX_FS_BACKEND_DIR,       /**< Host/Linux directory-backed assets */
    GFX_FS_BACKEND_MMAP,      /**< ESP-IDF mmap-assets-backed assets */
    GFX_FS_BACKEND_PARTITION, /**< ESP-IDF raw partition-backed assets */
} gfx_fs_backend_t;

typedef enum {
    GFX_FS_VIEW_FLAG_NONE        = 0,
    GFX_FS_VIEW_FLAG_DIRECT_ADDR = 1U << 0, /**< Data points to a backend direct-read address. */
    GFX_FS_VIEW_FLAG_OWNED       = 1U << 1, /**< View owns data and releases it on close. */
    GFX_FS_VIEW_FLAG_MAPPED      = 1U << 2, /**< Data comes from an mmap-style mapping. */
    GFX_FS_VIEW_FLAG_PERSISTENT  = 1U << 3, /**< Data remains stable while the store is open. */
} gfx_fs_view_flags_t;

typedef struct {
    uint32_t offset;
    size_t size;
    const char *name;
    int32_t id;
} gfx_fs_region_t;

typedef struct {
    bool open_by_name;
    bool open_by_id;
    bool open_region;
    bool can_direct_addr;
    bool can_owned_copy;
    bool can_force_copy;
    bool can_force_direct;
} gfx_fs_caps_t;

typedef struct {
    const void *data;            /**< Read-only resource bytes */
    size_t size;                 /**< Resource size in bytes */
    const char *name;            /**< Resource name/path when known */
    int32_t id;                  /**< Resource ID when known, or -1 */
    bool is_mapped;              /**< Compatibility alias for GFX_FS_VIEW_FLAG_MAPPED */
    uint32_t flags;              /**< GFX_FS_VIEW_FLAG_* */
    void *priv;                  /**< Internal backend-owned view state */
} gfx_fs_view_t;

typedef struct {
    gfx_err_t (*open_by_name)(gfx_fs_t *store, const char *name, gfx_fs_view_t *out_view);
    gfx_err_t (*open_by_id)(gfx_fs_t *store, int32_t id, gfx_fs_view_t *out_view);
    gfx_err_t (*open_region)(gfx_fs_t *store,
                             const gfx_fs_region_t *region,
                             gfx_fs_view_t *out_view);
    gfx_err_t (*get_caps)(const gfx_fs_t *store, gfx_fs_caps_t *out_caps);
    void (*view_close)(gfx_fs_view_t *view);
    void (*store_close)(gfx_fs_t *store);
} gfx_fs_vtable_t;

struct gfx_fs {
    gfx_fs_backend_t backend;
    const gfx_fs_vtable_t *vtable;
    void *backend_data;
};

typedef struct {
    gfx_fs_t *store;
} gfx_fs_view_state_base_t;

/* Backend constructors implemented per platform. */
gfx_err_t gfx_fs_open_config_port(const gfx_fs_config_t *config, gfx_fs_t **out_store);
gfx_err_t gfx_fs_open_dir_port(const char *root_dir, gfx_fs_t **out_store);
gfx_err_t gfx_fs_open_mmap_port(const gfx_fs_mmap_config_t *config, gfx_fs_t **out_store);

/* Internal view access used by the file/source layer and backend conformance
 * tests. Applications should use the public gfx_fs_fopen()/load() API. */
gfx_fs_backend_t gfx_fs_get_backend(const gfx_fs_t *store);
gfx_err_t gfx_fs_get_caps(const gfx_fs_t *store, gfx_fs_caps_t *out_caps);
gfx_err_t gfx_fs_view_open_by_name(gfx_fs_t *store, const char *name, gfx_fs_view_t *out_view);
gfx_err_t gfx_fs_view_open_by_id(gfx_fs_t *store, int32_t id, gfx_fs_view_t *out_view);
gfx_err_t gfx_fs_view_open_region(gfx_fs_t *store,
                                  const gfx_fs_region_t *region,
                                  gfx_fs_view_t *out_view);
void gfx_fs_view_close(gfx_fs_view_t *view);

#ifdef __cplusplus
}
#endif
