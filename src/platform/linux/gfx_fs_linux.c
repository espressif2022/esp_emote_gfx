/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/base/gfx_fs_priv.h"

#define GFX_FS_PACK_MAGIC0              'M'
#define GFX_FS_PACK_MAGIC1              'M'
#define GFX_FS_PACK_MAGIC2              'A'
#define GFX_FS_PACK_MAGIC3              'P'
#define GFX_FS_PACK_HEADER_SIZE         32U
#define GFX_FS_PACK_LEGACY_HEADER_SIZE  12U
#define GFX_FS_PACK_LEGACY_NAME_LEN     32U
#define GFX_FS_PACK_FILE_MAGIC          0x5A5AU
#define GFX_FS_PACK_FILE_MAGIC_LEN      2U

typedef struct {
    char *root_dir;
} gfx_fs_dir_backend_t;

typedef struct {
    gfx_fs_entry_state_base_t base;
    char *name;
    void *mapped;
    size_t mapped_size;
    void *owned;
} gfx_fs_dir_entry_t;

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
} gfx_fs_pack_item_t;

typedef struct {
    char *path;
    void *mapped;
    size_t mapped_size;
    gfx_fs_pack_item_t *items;
    uint32_t item_count;
} gfx_fs_pack_backend_t;

typedef struct {
    gfx_fs_entry_state_base_t base;
} gfx_fs_pack_entry_t;

static uint16_t gfx_fs_read_le16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;

    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t gfx_fs_read_le32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;

    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint32_t gfx_fs_pack_checksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0;

    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }

    return checksum & 0xFFFFU;
}

static char *gfx_fs_strdup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s) + 1U;
    char *copy = malloc(len);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len);
    return copy;
}

static bool gfx_fs_name_is_safe(const char *name)
{
    if (name == NULL || name[0] == '\0' || name[0] == '/') {
        return false;
    }

    const char *p = name;
    while (*p != '\0') {
        if ((p[0] == '.' && p[1] == '.' && (p[2] == '\0' || p[2] == '/')) ||
                (p[0] == '/' && p[1] == '.' && p[2] == '.' && (p[3] == '\0' || p[3] == '/'))) {
            return false;
        }
        p++;
    }
    return true;
}

static char *gfx_fs_join_path(const char *root, const char *name)
{
    size_t root_len = strlen(root);
    size_t name_len = strlen(name);
    bool need_sep = root_len > 0U && root[root_len - 1U] != '/';
    size_t total = root_len + (need_sep ? 1U : 0U) + name_len + 1U;
    char *path = malloc(total);
    if (path == NULL) {
        return NULL;
    }

    memcpy(path, root, root_len);
    size_t pos = root_len;
    if (need_sep) {
        path[pos++] = '/';
    }
    memcpy(path + pos, name, name_len + 1U);
    return path;
}

static gfx_err_t gfx_fs_read_file(int fd, size_t size, void **out_data)
{
    uint8_t *buf = malloc(size);
    if (buf == NULL) {
        return GFX_ERR_NO_MEM;
    }

    size_t done = 0;
    while (done < size) {
        ssize_t ret = read(fd, buf + done, size - done);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buf);
            return GFX_FAIL;
        }
        if (ret == 0) {
            free(buf);
            return GFX_ERR_INVALID_SIZE;
        }
        done += (size_t)ret;
    }

    *out_data = buf;
    return GFX_OK;
}

static gfx_err_t gfx_fs_dir_open_by_name(gfx_fs_t *fs, const char *name, gfx_fs_entry_t *out_entry)
{
    gfx_fs_dir_backend_t *backend = (gfx_fs_dir_backend_t *)fs->backend_data;
    gfx_fs_dir_entry_t *state = NULL;
    char *path = NULL;
    int fd = -1;
    struct stat st;
    gfx_err_t err = GFX_FAIL;

    if (backend == NULL || !gfx_fs_name_is_safe(name)) {
        return GFX_ERR_INVALID_ARG;
    }

    path = gfx_fs_join_path(backend->root_dir, name);
    if (path == NULL) {
        return GFX_ERR_NO_MEM;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        err = errno == ENOENT ? GFX_ERR_NOT_FOUND : GFX_FAIL;
        goto cleanup;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        err = GFX_ERR_INVALID_ARG;
        goto cleanup;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }
    state->base.fs = fs;
    state->name = gfx_fs_strdup(name);
    if (state->name == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }

    size_t size = (size_t)st.st_size;
    if (size > 0U) {
        void *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped != MAP_FAILED) {
            state->mapped = mapped;
            state->mapped_size = size;
            out_entry->data = mapped;
        } else {
            err = gfx_fs_read_file(fd, size, &state->owned);
            if (err != GFX_OK) {
                goto cleanup;
            }
            out_entry->data = state->owned;
        }
    }

    out_entry->size = size;
    out_entry->priv = state;
    state = NULL;
    err = GFX_OK;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    free(path);
    if (state != NULL) {
        free(state->name);
        free(state);
    }
    return err;
}

static void gfx_fs_dir_entry_close(gfx_fs_entry_t *entry)
{
    gfx_fs_dir_entry_t *state = entry != NULL ? (gfx_fs_dir_entry_t *)entry->priv : NULL;
    if (state == NULL) {
        return;
    }

    if (state->mapped != NULL && state->mapped_size > 0U) {
        munmap(state->mapped, state->mapped_size);
    }
    free(state->owned);
    free(state->name);
    free(state);
}

static void gfx_fs_dir_fs_close(gfx_fs_t *fs)
{
    if (fs == NULL) {
        return;
    }

    gfx_fs_dir_backend_t *backend = (gfx_fs_dir_backend_t *)fs->backend_data;
    if (backend != NULL) {
        free(backend->root_dir);
        free(backend);
    }
    free(fs);
}

static const gfx_fs_vtable_t s_gfx_fs_dir_vtable = {
    .open_by_name = gfx_fs_dir_open_by_name,
    .entry_close = gfx_fs_dir_entry_close,
    .fs_close = gfx_fs_dir_fs_close,
};

static gfx_err_t gfx_fs_pack_open_by_name(gfx_fs_t *fs, const char *name, gfx_fs_entry_t *out_entry)
{
    gfx_fs_pack_backend_t *backend = (gfx_fs_pack_backend_t *)fs->backend_data;
    gfx_fs_pack_entry_t *state;

    if (backend == NULL || name == NULL || out_entry == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < backend->item_count; i++) {
        const gfx_fs_pack_item_t *item = &backend->items[i];
        if (item->name != NULL && strcmp(item->name, name) == 0) {
            state = calloc(1, sizeof(*state));
            if (state == NULL) {
                return GFX_ERR_NO_MEM;
            }
            state->base.fs = fs;
            out_entry->data = item->data;
            out_entry->size = item->size;
            out_entry->priv = state;
            return GFX_OK;
        }
    }

    return GFX_ERR_NOT_FOUND;
}

static void gfx_fs_pack_entry_close(gfx_fs_entry_t *entry)
{
    gfx_fs_pack_entry_t *state = entry != NULL ? (gfx_fs_pack_entry_t *)entry->priv : NULL;

    free(state);
}

static void gfx_fs_pack_fs_close(gfx_fs_t *fs)
{
    gfx_fs_pack_backend_t *backend;

    if (fs == NULL) {
        return;
    }

    backend = (gfx_fs_pack_backend_t *)fs->backend_data;
    if (backend != NULL) {
        if (backend->mapped != NULL && backend->mapped_size > 0U) {
            munmap(backend->mapped, backend->mapped_size);
        }
        free(backend->items);
        free(backend->path);
        free(backend);
    }
    free(fs);
}

static const gfx_fs_vtable_t s_gfx_fs_pack_vtable = {
    .open_by_name = gfx_fs_pack_open_by_name,
    .entry_close = gfx_fs_pack_entry_close,
    .fs_close = gfx_fs_pack_fs_close,
};

gfx_err_t gfx_fs_open_dir_port(const char *root_dir, gfx_fs_t **out_fs)
{
    gfx_fs_t *fs = NULL;
    gfx_fs_dir_backend_t *backend = NULL;
    struct stat st;

    if (root_dir == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (stat(root_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return GFX_ERR_NOT_FOUND;
    }

    fs = calloc(1, sizeof(*fs));
    backend = calloc(1, sizeof(*backend));
    if (fs == NULL || backend == NULL) {
        free(fs);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    backend->root_dir = gfx_fs_strdup(root_dir);
    if (backend->root_dir == NULL) {
        free(fs);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    fs->access_mode = GFX_FS_ACCESS_COPY;
    fs->vtable = &s_gfx_fs_dir_vtable;
    fs->backend_data = backend;
    *out_fs = fs;
    return GFX_OK;
}

gfx_err_t gfx_fs_open_pack_file_port(const char *file_path, gfx_fs_t **out_fs)
{
    gfx_fs_t *fs = NULL;
    gfx_fs_pack_backend_t *backend = NULL;
    gfx_fs_pack_item_t *items = NULL;
    int fd = -1;
    struct stat st;
    void *mapped = NULL;
    const uint8_t *bytes;
    uint32_t files;
    uint32_t checksum;
    uint32_t payload_len;
    uint32_t name_len;
    uint32_t table_offset;
    uint32_t stride;
    size_t table_len;
    size_t data_offset;
    gfx_err_t err = GFX_FAIL;

    if (file_path == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_fs = NULL;

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        return errno == ENOENT ? GFX_ERR_NOT_FOUND : GFX_FAIL;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        err = GFX_ERR_INVALID_ARG;
        goto cleanup;
    }

    mapped = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        mapped = NULL;
        err = GFX_FAIL;
        goto cleanup;
    }
    bytes = (const uint8_t *)mapped;

    if ((size_t)st.st_size >= GFX_FS_PACK_HEADER_SIZE &&
            bytes[0] == GFX_FS_PACK_MAGIC0 && bytes[1] == GFX_FS_PACK_MAGIC1 &&
            bytes[2] == GFX_FS_PACK_MAGIC2 && bytes[3] == GFX_FS_PACK_MAGIC3) {
        name_len = gfx_fs_read_le32(bytes + 8);
        files = gfx_fs_read_le32(bytes + 12);
        checksum = gfx_fs_read_le32(bytes + 16);
        payload_len = gfx_fs_read_le32(bytes + 20);
        table_offset = GFX_FS_PACK_HEADER_SIZE;
    } else {
        if ((size_t)st.st_size < GFX_FS_PACK_LEGACY_HEADER_SIZE) {
            err = GFX_ERR_INVALID_SIZE;
            goto cleanup;
        }
        files = gfx_fs_read_le32(bytes);
        checksum = gfx_fs_read_le32(bytes + 4);
        payload_len = gfx_fs_read_le32(bytes + 8);
        name_len = GFX_FS_PACK_LEGACY_NAME_LEN;
        table_offset = GFX_FS_PACK_LEGACY_HEADER_SIZE;
    }

    if (files == 0U || name_len == 0U || name_len > 1024U ||
            payload_len == 0U || table_offset > (size_t)st.st_size ||
            (size_t)payload_len > (size_t)st.st_size - table_offset) {
        err = GFX_ERR_INVALID_SIZE;
        goto cleanup;
    }

    if (gfx_fs_pack_checksum(bytes + table_offset, payload_len) != (checksum & 0xFFFFU)) {
        err = GFX_ERR_INVALID_CRC;
        goto cleanup;
    }

    stride = name_len + 12U;
    table_len = (size_t)files * stride;
    if (stride < name_len || table_len / stride != files || table_len > payload_len) {
        err = GFX_ERR_INVALID_SIZE;
        goto cleanup;
    }
    data_offset = (size_t)table_offset + table_len;

    items = calloc(files, sizeof(*items));
    if (items == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }

    for (uint32_t i = 0; i < files; i++) {
        const uint8_t *entry = bytes + table_offset + (size_t)i * stride;
        uint32_t asset_size = gfx_fs_read_le32(entry + stride - 12U);
        uint32_t asset_offset = gfx_fs_read_le32(entry + stride - 8U);
        size_t payload_pos = data_offset + asset_offset;

        if (memchr(entry, '\0', name_len) == NULL ||
                payload_pos < data_offset ||
                payload_pos + GFX_FS_PACK_FILE_MAGIC_LEN > (size_t)st.st_size ||
                asset_size > (size_t)st.st_size - payload_pos - GFX_FS_PACK_FILE_MAGIC_LEN ||
                gfx_fs_read_le16(bytes + payload_pos) != GFX_FS_PACK_FILE_MAGIC) {
            err = GFX_ERR_INVALID_SIZE;
            goto cleanup;
        }

        items[i].name = (const char *)entry;
        items[i].data = bytes + payload_pos + GFX_FS_PACK_FILE_MAGIC_LEN;
        items[i].size = asset_size;
    }

    fs = calloc(1, sizeof(*fs));
    backend = calloc(1, sizeof(*backend));
    if (fs == NULL || backend == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }

    backend->path = gfx_fs_strdup(file_path);
    if (backend->path == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }
    backend->mapped = mapped;
    backend->mapped_size = (size_t)st.st_size;
    backend->items = items;
    backend->item_count = files;
    fs->access_mode = GFX_FS_ACCESS_COPY;
    fs->vtable = &s_gfx_fs_pack_vtable;
    fs->backend_data = backend;
    *out_fs = fs;

    mapped = NULL;
    items = NULL;
    backend = NULL;
    fs = NULL;
    err = GFX_OK;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    if (mapped != NULL) {
        munmap(mapped, (size_t)st.st_size);
    }
    free(items);
    if (backend != NULL) {
        free(backend->path);
        free(backend);
    }
    free(fs);
    return err;
}
