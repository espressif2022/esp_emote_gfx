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
 * Filesystem source (mount point)
 *
 * A gfx_fs_t is an opened asset filesystem source (an mmap-assets partition or
 * a host/VFS directory). It plays the same role as a registered
 * filesystem driver: open it once, optionally make it the process default,
 * then access files by name through the file API below.
 * ========================================================================= */

typedef struct gfx_fs gfx_fs_t;

typedef enum {
    GFX_FS_SOURCE_DIR = 0,       /**< Loose asset files under a host/VFS directory. */
    GFX_FS_SOURCE_PACK_FILE,     /**< mmap-assets pack file on a filesystem path. */
    GFX_FS_SOURCE_PARTITION,     /**< mmap-assets pack stored in a flash partition. */
} gfx_fs_source_type_t;

typedef enum {
    GFX_FS_ACCESS_COPY = 0,      /**< Open the source in copy-backed mode when the backend supports it. */
    GFX_FS_ACCESS_DIRECT,        /**< Open the source in direct-address mode when the backend supports it. */
} gfx_fs_access_mode_t;

typedef struct {
    gfx_fs_source_type_t source_type;
    gfx_fs_access_mode_t access_mode;
    const char *path_or_label;
} gfx_fs_open_config_t;

/**
 * @brief Open an asset filesystem source.
 *
 * This separates "where assets live" from the source-level access strategy:
 * partition sources can be opened direct-address or copy-backed, while pack
 * files and directories currently use copy-backed source mode. The mode does
 * not describe ownership of each opened file; use gfx_fs_fdata() to check
 * whether a file exposes a stable byte base.
 *
 * @param config Source and access configuration.
 * @param out_fs Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open(const gfx_fs_open_config_t *config, gfx_fs_t **out_fs);

/**
 * @brief Open a host directory as a filesystem source.
 *
 * Names passed to the file API are resolved relative to @p root_dir. Available
 * on host/Linux builds; ESP-IDF builds can use gfx_fs_open() with
 * GFX_FS_SOURCE_PACK_FILE or GFX_FS_SOURCE_PARTITION.
 *
 * @param root_dir  Directory that contains asset files.
 * @param out_fs Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_dir(const char *root_dir, gfx_fs_t **out_fs);

/**
 * @brief Get the source-level access strategy of an opened fs.
 *
 * @param fs Filesystem source handle.
 * @return Configured source access mode; GFX_FS_ACCESS_COPY for NULL.
 */
gfx_fs_access_mode_t gfx_fs_get_access_mode(const gfx_fs_t *fs);

/**
 * @brief Set the process-wide default fs.
 *
 * Decoders, widgets, and gfx_fs_fopen()/gfx_fs_load() use this fs to
 * resolve name/path based sources. The caller owns the fs lifetime and must
 * keep it valid while it is set.
 *
 * @param fs Default fs; NULL clears it.
 */
void gfx_fs_set_default(gfx_fs_t *fs);

/**
 * @brief Get the process-wide default fs.
 * @return Current default fs, or NULL if not set.
 */
gfx_fs_t *gfx_fs_get_default(void);

/**
 * @brief Close a filesystem source and release backend state.
 * @param fs FS returned by gfx_fs_open_*(); NULL is allowed.
 */
void gfx_fs_close(gfx_fs_t *fs);

/* =========================================================================
 * Asset file (stdio-style access)
 *
 * gfx_fs_file_t is an opaque handle to an open asset, modelled on stdio
 * (fopen/fread/fseek/ftell/fclose) to keep the learning curve flat. Backends
 * are hidden: when a file has a stable whole-asset byte base, gfx_fs_fdata()
 * returns it; otherwise callers can read through gfx_fs_fread().
 * ========================================================================= */

typedef struct gfx_fs_file gfx_fs_file_t;

/**
 * @brief Open an asset by name/path using the default fs.
 *
 * Resolution order:
 * 1. The process-wide default fs.
 * 2. A plain filesystem fopen() fallback for SPIFFS/FATFS/SD or host paths.
 *
 * @param name Resource name or filesystem path.
 * @return Open file handle, or NULL on failure. Release with gfx_fs_fclose().
 */
gfx_fs_file_t *gfx_fs_fopen(const char *name);

/**
 * @brief Open an asset by name from an explicit fs.
 *
 * Unlike gfx_fs_fopen(), this resolves only through @p fs (no filesystem
 * fallback). Most application and decoder code should prefer gfx_fs_fopen()
 * after setting the default fs; this helper is mainly for tests and advanced
 * callers that intentionally bypass fallback.
 *
 * @param fs FS to resolve against.
 * @param name  Resource name.
 * @return Open file handle, or NULL on failure. Release with gfx_fs_fclose().
 */
gfx_fs_file_t *gfx_fs_fopen_from(gfx_fs_t *fs, const char *name);

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
 * Returns a pointer to the full asset bytes when the opened file has a stable
 * byte base. The pointer stays valid until gfx_fs_fclose(). Returns NULL when
 * the file must be read through gfx_fs_fread().
 *
 * @param file File handle.
 * @return Direct read-only base, or NULL when no stable base is available.
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
 * One-shot blob load (whole-file copy convenience)
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
 * @brief Load an asset by name/path into a read-only whole-file blob.
 *
 * This is the simplest entry for "give me the whole file"; it hides backend
 * selection entirely. Resolution order:
 * 1. The process-wide default fs, configured via gfx_fs_set_default().
 * 2. A plain filesystem fopen()/fread() fallback into an owned heap buffer,
 *    covering SPIFFS/FATFS/SD or host paths when no default fs is set.
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
