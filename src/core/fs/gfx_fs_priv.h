/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/fs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Internal fs contract.
 *
 * These types describe how backends (mmap-assets and host/VFS directory)
 * expose resolved byte ranges to the rest of the library. They are
 * intentionally kept out of the public header: applications use the stdio-style
 * file API (gfx_fs_fopen()/gfx_fs_load()) instead of poking entries directly.
 * ========================================================================= */

typedef struct {
    const void *data;            /**< Read-only resource bytes */
    size_t size;                 /**< Resource size in bytes */
    void *priv;                  /**< Internal backend-owned entry state */
} gfx_fs_entry_t;

typedef struct {
    gfx_err_t (*open_by_name)(gfx_asset_source_t *fs, const char *name, gfx_fs_entry_t *out_entry);
    void (*entry_close)(gfx_fs_entry_t *entry);
    void (*fs_close)(gfx_asset_source_t *fs);
} gfx_fs_vtable_t;

struct gfx_asset_source {
    gfx_fs_access_mode_t access_mode;
    const gfx_fs_vtable_t *vtable;
    void *backend_data;
};

typedef struct {
    gfx_asset_source_t *fs;
} gfx_fs_entry_state_base_t;

#ifndef GFX_FS_MOUNT_MAX
#define GFX_FS_MOUNT_MAX 4
#endif
#ifndef GFX_FS_MOUNT_PREFIX_MAX
#define GFX_FS_MOUNT_PREFIX_MAX 64
#endif

typedef struct {
    char prefix[GFX_FS_MOUNT_PREFIX_MAX];
    gfx_asset_source_t *fs;
} gfx_fs_mount_t;

/**
 * @brief Resolve @p name against the global mount table.
 *
 * On success, @p out_fs and @p out_subname are filled for backend lookup.
 * @p out_subname points into @p name or @p name itself; it is valid only while
 * @p name remains alive.
 */
bool gfx_fs_resolve_mount(const char *name, gfx_asset_source_t **out_fs, const char **out_subname);

/* Backend constructors implemented per platform. */
gfx_err_t gfx_fs_open_dir_port(const char *root_dir, gfx_asset_source_t **out_fs);

gfx_err_t gfx_fs_open_partition_port(const char *partition_label, gfx_fs_access_mode_t access_mode,
                                     gfx_asset_source_t **out_fs);
gfx_err_t gfx_fs_open_pack_file_port(const char *file_path, gfx_asset_source_t **out_fs);

/* Internal entry access used by the file/source layer. Applications should use
 * the public gfx_fs_fopen()/load() API. */
gfx_err_t gfx_fs_entry_open_by_name(gfx_asset_source_t *fs, const char *name, gfx_fs_entry_t *out_entry);
void gfx_fs_entry_close(gfx_fs_entry_t *entry);

/**
 * @brief Fill @p out_blob from an already-open @p file.
 *
 * On success, ownership of @p file transfers into the blob. If the file exposes
 * a stable byte base through gfx_fs_fdata(), the blob keeps the file alive; if
 * not, the file is copied into blob-owned memory and closed. On failure, @p file
 * stays open and the caller must gfx_fs_fclose() it.
 */
gfx_err_t gfx_fs_blob_take_file(gfx_fs_file_t *file, gfx_fs_blob_t *out_blob);

#ifdef __cplusplus
}
#endif
