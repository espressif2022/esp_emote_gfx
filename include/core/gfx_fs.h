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

/* =========================================================================
 * Asset store (mount point)
 *
 * A store is an opened asset source (an mmap-assets partition, a host
 * directory, or a raw partition). It plays the same role as a registered
 * filesystem driver: open it once, optionally make it the process default,
 * then access files by name through the file API below.
 * ========================================================================= */

typedef struct gfx_fs gfx_fs_t;

typedef enum {
    GFX_FS_TYPE_AUTO = 0,
    GFX_FS_TYPE_DIR,
    GFX_FS_TYPE_MMAP_ASSETS,
    GFX_FS_TYPE_PARTITION,
    GFX_FS_TYPE_MEMORY_TABLE,
} gfx_fs_type_t;

typedef enum {
    GFX_FS_LOAD_PREFER_DIRECT = 0,
    GFX_FS_LOAD_FORCE_COPY,
    GFX_FS_LOAD_FORCE_DIRECT,
} gfx_fs_load_mode_t;

typedef struct {
    const char *partition_label; /**< ESP-IDF mmap-assets partition label. */
    int32_t max_files;           /**< Optional expected asset count; 0 means read from asset header. */
    uint32_t checksum;           /**< Optional expected asset checksum; 0 means read from asset header. */
    bool full_check;             /**< Enable mmap-assets full consistency check. */
} gfx_fs_mmap_config_t;

typedef struct {
    gfx_fs_type_t type;
    gfx_fs_load_mode_t load_mode;
    const char *root_dir;          /**< Directory root for file/VFS backed stores. */
    const char *partition_label;   /**< mmap-assets or raw partition label. */
    int32_t max_files;             /**< mmap-assets expected file count; 0 means read from header. */
    uint32_t checksum;             /**< mmap-assets checksum; 0 means read from header. */
    bool full_check;               /**< Enable mmap-assets full consistency check. */
    uint32_t alloc_caps;           /**< ESP-IDF heap caps for copy-backed reads; 0 means default malloc. */
} gfx_fs_config_t;

/**
 * @brief Open an asset store from a unified configuration.
 *
 * This is the preferred constructor for new code. Backend-specific helpers are
 * kept as convenience wrappers.
 *
 * @param config    Store configuration.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open(const gfx_fs_config_t *config, gfx_fs_t **out_store);

/**
 * @brief Open a host directory as an asset store.
 *
 * Names passed to the file API are resolved relative to @p root_dir. Available
 * on host/Linux builds; ESP-IDF builds use gfx_fs_open_mmap().
 *
 * @param root_dir  Directory that contains asset files.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_dir(const char *root_dir, gfx_fs_t **out_store);

/**
 * @brief Open an ESP-IDF mmap-assets partition as an asset store.
 *
 * On ESP-IDF this wraps the esp_mmap_assets component. Host/Linux builds
 * return GFX_ERR_NOT_SUPPORTED.
 *
 * @param config    mmap-assets store configuration.
 * @param out_store Output asset store handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_mmap(const gfx_fs_mmap_config_t *config, gfx_fs_t **out_store);

/**
 * @brief Set the process-wide default asset store.
 *
 * Decoders, widgets, and gfx_fs_fopen()/gfx_fs_load() use this store to
 * resolve name/path based sources. The caller owns the store lifetime and must
 * keep it valid while it is set.
 *
 * @param store Default store; NULL clears it.
 */
void gfx_fs_set_default(gfx_fs_t *store);

/**
 * @brief Get the process-wide default asset store.
 * @return Current default store, or NULL if not set.
 */
gfx_fs_t *gfx_fs_get_default(void);

/**
 * @brief Close an asset store and release backend state.
 * @param store Store returned by gfx_fs_open_*(); NULL is allowed.
 */
void gfx_fs_close(gfx_fs_t *store);

/* =========================================================================
 * Asset file (stdio-style access)
 *
 * gfx_fs_file_t is an opaque handle to an open asset, modelled on stdio
 * (fopen/fread/fseek/ftell/fclose) to keep the learning curve flat. Backends
 * are hidden: an mmap/direct backend exposes a zero-copy base through
 * gfx_fs_fdata(); a streamed backend (VFS fread) returns NULL there and
 * serves bytes through gfx_fs_fread().
 * ========================================================================= */

typedef struct gfx_fs_file gfx_fs_file_t;

/**
 * @brief Open an asset by name/path using the default store.
 *
 * Resolution order:
 * 1. The process-wide default asset store (mmap-assets direct address, dir/VFS
 *    copy, or raw partition).
 * 2. A plain filesystem fopen() (SPIFFS/FATFS/SD or a host path), streamed so
 *    only requested bytes become resident.
 *
 * @param name Resource name or filesystem path.
 * @return Open file handle, or NULL on failure. Release with gfx_fs_fclose().
 */
gfx_fs_file_t *gfx_fs_fopen(const char *name);

/**
 * @brief Open an asset by name from an explicit store.
 *
 * Unlike gfx_fs_fopen(), this resolves only through @p store (no filesystem
 * fallback). Most application and decoder code should prefer gfx_fs_fopen()
 * after setting the default store; this helper is mainly for tests and advanced
 * callers that intentionally bypass fallback.
 *
 * @param store Asset store to resolve against.
 * @param name  Resource name.
 * @return Open file handle, or NULL on failure. Release with gfx_fs_fclose().
 */
gfx_fs_file_t *gfx_fs_fopen_from(gfx_fs_t *store, const char *name);

/**
 * @brief Close a file handle opened by gfx_fs_fopen()/gfx_fs_fopen_from().
 * @param file File handle; NULL is allowed.
 */
void gfx_fs_fclose(gfx_fs_file_t *file);

/**
 * @brief Total size of the open asset in bytes.
 * @param file File handle.
 * @return Size in bytes, or 0 for NULL.
 */
size_t gfx_fs_fsize(const gfx_fs_file_t *file);

/**
 * @brief Zero-copy base address of the whole asset, when available.
 *
 * Returns a pointer to the full asset bytes for direct/mmap backends; the
 * pointer stays valid until gfx_fs_fclose(). Returns NULL for streamed
 * backends, in which case use gfx_fs_fread().
 *
 * @param file File handle.
 * @return Direct read-only base, or NULL when the backend is streamed.
 */
const void *gfx_fs_fdata(const gfx_fs_file_t *file);

/**
 * @brief Read up to @p len bytes from the current position into @p buf.
 *
 * Advances the read cursor by the number of bytes read.
 *
 * @param file File handle.
 * @param buf  Destination buffer of at least @p len bytes.
 * @param len  Maximum number of bytes to read.
 * @return Number of bytes actually read (0 at end-of-file or on error).
 */
size_t gfx_fs_fread(gfx_fs_file_t *file, void *buf, size_t len);

/**
 * @brief Move the read cursor.
 *
 * @param file   File handle.
 * @param offset Signed offset relative to @p whence.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END (from <stdio.h>).
 * @return 0 on success, -1 on invalid arguments or out-of-range position.
 */
int gfx_fs_fseek(gfx_fs_file_t *file, long offset, int whence);

/**
 * @brief Current read cursor position.
 * @param file File handle.
 * @return Byte offset from the start, or -1 for NULL.
 */
long gfx_fs_ftell(const gfx_fs_file_t *file);

/* =========================================================================
 * One-shot blob load (resident whole-file convenience)
 * ========================================================================= */

/**
 * @brief Loaded read-only asset blob produced by gfx_fs_load().
 *
 * Callers only read @ref data and @ref size; @ref _holder is internal
 * ownership state and must not be touched. Release with gfx_fs_unload().
 */
typedef struct {
    const void *data;            /**< Read-only bytes, valid until gfx_fs_unload(). */
    size_t size;                 /**< Size in bytes. */
    void *_holder;               /**< Internal: owned source state. */
} gfx_fs_blob_t;

/**
 * @brief Load an asset by name/path into a resident read-only blob.
 *
 * This is the simplest entry for "give me the whole file"; it hides backend
 * selection entirely. Resolution order:
 * 1. The process-wide default asset store (mmap-assets direct address, dir/VFS
 *    copy, or raw partition), configured via gfx_fs_set_default().
 * 2. A plain filesystem fopen()/fread() fallback into an owned heap buffer,
 *    covering SPIFFS/FATFS/SD or host paths when no default store is set.
 *
 * @param name     Resource name or filesystem path.
 * @param out_blob Output blob; release with gfx_fs_unload().
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_load(const char *name, gfx_fs_blob_t *out_blob);

/**
 * @brief Release a blob loaded by gfx_fs_load(). Idempotent; NULL allowed.
 * @param blob Blob previously filled by gfx_fs_load().
 */
void gfx_fs_unload(gfx_fs_blob_t *blob);

#ifdef __cplusplus
}
#endif
