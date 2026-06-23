/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * Include convention
 * ------------------
 * Application and example code:  #include "gfx/fs.h"
 * Library internals (src/):      #include "core/gfx_fs.h"
 *
 * Both paths resolve to the same declarations.  The gfx/ prefix is the stable
 * public surface; core/ is the canonical location used by library internals.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/gfx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Asset source handle
 *
 * gfx_asset_source_t is an opened asset source — a single, already-mounted
 * resource origin such as a flash partition (mmap-assets pack), a pack file
 * on a filesystem, or a host/VFS directory.  It is NOT a filesystem type or
 * driver; think of it as a file-descriptor-like handle to one particular
 * collection of assets.
 *
 * Typical lifecycle:
 *   gfx_fs_open_partition("assets", &src);   // open the source
 *   gfx_fs_mount("", src);                   // register under a path prefix
 *   ...                                      // widgets resolve assets by name
 *   gfx_fs_close(src);                       // release (also unmounts)
 * ========================================================================= */

typedef struct gfx_asset_source gfx_asset_source_t;

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
gfx_err_t gfx_fs_open(const gfx_fs_open_config_t *config, gfx_asset_source_t **out_fs);

/**
 * @brief Open a directory as a filesystem source (host and VFS paths).
 *
 * Names passed to the file API are resolved relative to @p root_dir.
 * Works on host builds and any platform that exposes a POSIX-compatible
 * filesystem path (e.g. SPIFFS or LittleFS mounted via ESP-IDF VFS).
 *
 * @param root_dir  Directory that contains asset files.
 * @param out_fs Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_dir(const char *root_dir, gfx_asset_source_t **out_fs);

/**
 * @brief Open a mmap-assets pack file as a filesystem source.
 *
 * Convenience wrapper for gfx_fs_open() with GFX_FS_SOURCE_PACK_FILE /
 * GFX_FS_ACCESS_COPY.  Works anywhere the pack file is accessible as a
 * filesystem path (host, SPIFFS, FATFS, SD ...).
 *
 * @param file_path  Filesystem path to the .bin pack file.
 * @param out_fs     Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_pack(const char *file_path, gfx_asset_source_t **out_fs);

/**
 * @brief Open a flash partition as a filesystem source (direct / zero-copy).
 *
 * Convenience wrapper for gfx_fs_open() with GFX_FS_SOURCE_PARTITION /
 * GFX_FS_ACCESS_DIRECT.  Assets are served directly from flash via mmap;
 * no heap copy is made per file.  ESP-IDF target only.
 *
 * @param label  Partition label (must match the partition table entry).
 * @param out_fs Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_partition(const char *label, gfx_asset_source_t **out_fs);

/**
 * @brief Open a flash partition as a filesystem source (copy / heap-backed).
 *
 * Like gfx_fs_open_partition() but uses GFX_FS_ACCESS_COPY: each asset is
 * copied into a heap buffer on open.  Useful when the caller needs a
 * writable or cache-friendly copy of each asset.  ESP-IDF target only.
 *
 * @param label  Partition label.
 * @param out_fs Output filesystem source handle.
 * @return GFX_OK on success, or a GFX_ERR_* code.
 */
gfx_err_t gfx_fs_open_partition_copy(const char *label, gfx_asset_source_t **out_fs);

/**
 * @brief Get the source-level access strategy of an opened fs.
 *
 * @param fs Filesystem source handle.
 * @return Configured source access mode; GFX_FS_ACCESS_COPY for NULL.
 */
gfx_fs_access_mode_t gfx_fs_get_access_mode(const gfx_asset_source_t *fs);

/**
 * @brief Mount an fs at a path prefix for gfx_fs_fopen()/gfx_fs_load() routing.
 *
 * Prefix "" is the default asset namespace: all bare asset names (e.g.
 * "icon.bin") resolve through it. Longer prefixes such as "/spiffs" take
 * precedence for matching paths.
 *
 * @param prefix Path prefix without a trailing '/'.
 * @param fs Opened fs handle; caller owns lifetime until gfx_fs_close().
 * @return GFX_OK, GFX_ERR_NO_MEM if the mount table is full, or GFX_ERR_INVALID_ARG.
 */
gfx_err_t gfx_fs_mount(const char *prefix, gfx_asset_source_t *fs);

/**
 * @brief Unmount a path prefix without closing the fs handle.
 *
 * @param prefix Prefix passed to gfx_fs_mount().
 * @return GFX_OK or GFX_ERR_NOT_FOUND.
 */
gfx_err_t gfx_fs_unmount(const char *prefix);

/**
 * @brief Close a filesystem source and release backend state.
 * @param fs FS returned by gfx_fs_open_*(); NULL is allowed.
 */
void gfx_fs_close(gfx_asset_source_t *fs);

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
 * 1. Longest matching mount-table prefix (including "" as the default namespace).
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
gfx_fs_file_t *gfx_fs_fopen_from(gfx_asset_source_t *fs, const char *name);

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
 * One-shot blob load
 *
 * Use gfx_fs_load() when you need the entire file in memory at once
 * (e.g. to feed a decoder that expects a contiguous byte range).
 * Use gfx_fs_fopen() when you need streaming / seek access or want to
 * avoid a heap copy for mmap-backed sources.
 * ========================================================================= */

/**
 * @brief Read-only whole-file buffer produced by gfx_fs_load().
 *
 * Only @ref data and @ref size are meaningful to callers.  @ref _priv is
 * reserved for internal ownership tracking and must not be read or written
 * by application code.  Release the blob with gfx_fs_unload().
 */
typedef struct {
    const void *data;            /**< Read-only bytes, valid until gfx_fs_unload(). */
    size_t size;                 /**< Size in bytes. */
    void *_priv[2];              /**< Internal: [0] open file handle, [1] heap buffer. */
} gfx_fs_blob_t;

/**
 * @brief Load an asset by name/path into a read-only whole-file blob.
 *
 * Resolution order follows gfx_fs_fopen(): longest mount-table prefix
 * first, then a plain fopen() filesystem fallback.  For mmap-backed
 * sources the blob points directly into mapped flash; for copy-backed
 * sources the content is copied into a heap buffer.
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
