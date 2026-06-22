/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_mmap_assets.h"

#include "core/base/gfx_fs_priv.h"
#include "platform/esp_idf/gfx_err_bridge.h"

typedef struct {
    mmap_assets_handle_t handle;
    int32_t max_files;
    bool direct_mem;
} gfx_fs_mmap_backend_t;

typedef struct {
    gfx_fs_entry_state_base_t base;
    void *owned;
} gfx_fs_mmap_entry_t;

typedef struct {
    char *root_dir;
} gfx_fs_vfs_backend_t;

typedef struct {
    gfx_fs_entry_state_base_t base;
    char *name;
    void *owned;
} gfx_fs_vfs_entry_t;

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

static gfx_err_t gfx_fs_mmap_open_id(gfx_fs_t *fs, int32_t id, gfx_fs_entry_t *out_entry)
{
    gfx_fs_mmap_backend_t *backend = (gfx_fs_mmap_backend_t *)fs->backend_data;
    gfx_fs_mmap_entry_t *state;
    const void *data;
    int size;

    if (backend == NULL || backend->handle == NULL || id < 0 ||
            (backend->max_files > 0 && id >= backend->max_files)) {
        return GFX_ERR_INVALID_ARG;
    }

    data = mmap_assets_get_mem(backend->handle, (int)id);
    size = mmap_assets_get_size(backend->handle, (int)id);
    if (size <= 0) {
        return GFX_ERR_NOT_FOUND;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return GFX_ERR_NO_MEM;
    }
    state->base.fs = fs;

    out_entry->size = (size_t)size;

    if (backend->direct_mem) {
        if (data == NULL) {
            free(state);
            return GFX_ERR_NOT_FOUND;
        }
        out_entry->data = data;
    } else {
        size_t copied;

        state->owned = malloc(out_entry->size);
        if (state->owned == NULL) {
            free(state);
            return GFX_ERR_NO_MEM;
        }
        if (data == NULL) {
            free(state->owned);
            free(state);
            return GFX_ERR_NOT_FOUND;
        }
        copied = mmap_assets_copy_mem(backend->handle, (size_t)data, state->owned, out_entry->size);
        if (copied != out_entry->size) {
            free(state->owned);
            free(state);
            return GFX_FAIL;
        }
        out_entry->data = state->owned;
    }

    out_entry->priv = state;
    return GFX_OK;
}

static gfx_err_t gfx_fs_mmap_open_by_name(gfx_fs_t *fs, const char *name, gfx_fs_entry_t *out_entry)
{
    gfx_fs_mmap_backend_t *backend = (gfx_fs_mmap_backend_t *)fs->backend_data;

    if (backend == NULL || backend->handle == NULL || name == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int32_t i = 0; i < backend->max_files; i++) {
        const char *asset_name = mmap_assets_get_name(backend->handle, (int)i);
        if (asset_name != NULL && strcmp(asset_name, name) == 0) {
            return gfx_fs_mmap_open_id(fs, i, out_entry);
        }
    }

    return GFX_ERR_NOT_FOUND;
}

static void gfx_fs_mmap_entry_close(gfx_fs_entry_t *entry)
{
    gfx_fs_mmap_entry_t *state;

    if (entry == NULL) {
        return;
    }

    state = (gfx_fs_mmap_entry_t *)entry->priv;
    if (state != NULL) {
        free(state->owned);
        free(state);
    }
}

static void gfx_fs_mmap_fs_close(gfx_fs_t *fs)
{
    gfx_fs_mmap_backend_t *backend;

    if (fs == NULL) {
        return;
    }

    backend = (gfx_fs_mmap_backend_t *)fs->backend_data;
    if (backend != NULL) {
        if (backend->handle != NULL) {
            (void)mmap_assets_del(backend->handle);
        }
        free(backend);
    }
    free(fs);
}

static const gfx_fs_vtable_t s_gfx_fs_mmap_vtable = {
    .open_by_name = gfx_fs_mmap_open_by_name,
    .entry_close = gfx_fs_mmap_entry_close,
    .fs_close = gfx_fs_mmap_fs_close,
};

static gfx_err_t gfx_fs_vfs_open_by_name(gfx_fs_t *fs, const char *name, gfx_fs_entry_t *out_entry)
{
    gfx_fs_vfs_backend_t *backend = (gfx_fs_vfs_backend_t *)fs->backend_data;
    gfx_fs_vfs_entry_t *state = NULL;
    char *path = NULL;
    FILE *fp = NULL;
    long file_size;
    size_t size;
    gfx_err_t err = GFX_FAIL;

    if (backend == NULL || !gfx_fs_name_is_safe(name)) {
        return GFX_ERR_INVALID_ARG;
    }

    path = gfx_fs_join_path(backend->root_dir, name);
    if (path == NULL) {
        return GFX_ERR_NO_MEM;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        err = GFX_ERR_NOT_FOUND;
        goto cleanup;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        err = GFX_FAIL;
        goto cleanup;
    }
    file_size = ftell(fp);
    if (file_size < 0) {
        err = GFX_FAIL;
        goto cleanup;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        err = GFX_FAIL;
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

    size = (size_t)file_size;
    if (size > 0U) {
        state->owned = malloc(size);
        if (state->owned == NULL) {
            err = GFX_ERR_NO_MEM;
            goto cleanup;
        }
        if (fread(state->owned, 1, size, fp) != size) {
            err = GFX_FAIL;
            goto cleanup;
        }
        out_entry->data = state->owned;
    }

    out_entry->size = size;
    out_entry->priv = state;
    state = NULL;
    err = GFX_OK;

cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    free(path);
    if (state != NULL) {
        free(state->owned);
        free(state->name);
        free(state);
    }
    return err;
}

static void gfx_fs_vfs_entry_close(gfx_fs_entry_t *entry)
{
    gfx_fs_vfs_entry_t *state = entry != NULL ? (gfx_fs_vfs_entry_t *)entry->priv : NULL;
    if (state == NULL) {
        return;
    }
    free(state->owned);
    free(state->name);
    free(state);
}

static void gfx_fs_vfs_fs_close(gfx_fs_t *fs)
{
    if (fs == NULL) {
        return;
    }
    gfx_fs_vfs_backend_t *backend = (gfx_fs_vfs_backend_t *)fs->backend_data;
    if (backend != NULL) {
        free(backend->root_dir);
        free(backend);
    }
    free(fs);
}

static const gfx_fs_vtable_t s_gfx_fs_vfs_vtable = {
    .open_by_name = gfx_fs_vfs_open_by_name,
    .entry_close = gfx_fs_vfs_entry_close,
    .fs_close = gfx_fs_vfs_fs_close,
};

gfx_err_t gfx_fs_open_partition_port(const char *partition_label, gfx_fs_access_mode_t access_mode,
                                     gfx_fs_t **out_fs)
{
    gfx_fs_t *fs = NULL;
    gfx_fs_mmap_backend_t *backend = NULL;
    esp_err_t ret;
    bool direct_mem = access_mode == GFX_FS_ACCESS_DIRECT;
    const mmap_assets_config_t mmap_config = {
        .partition_label = partition_label,
        .max_files = 0,
        .checksum = 0,
        .flags = {
            .mmap_enable = direct_mem,
            .full_check = true,
            .use_fs = false,
        },
    };

    if (partition_label == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (access_mode != GFX_FS_ACCESS_DIRECT && access_mode != GFX_FS_ACCESS_COPY) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_fs = NULL;

    fs = calloc(1, sizeof(*fs));
    backend = calloc(1, sizeof(*backend));
    if (fs == NULL || backend == NULL) {
        free(fs);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    ret = mmap_assets_new(&mmap_config, &backend->handle);
    if (ret != ESP_OK) {
        free(fs);
        free(backend);
        return gfx_err_from_esp(ret);
    }

    backend->max_files = mmap_assets_get_stored_files(backend->handle);
    if (backend->max_files <= 0) {
        (void)mmap_assets_del(backend->handle);
        free(fs);
        free(backend);
        return GFX_ERR_INVALID_SIZE;
    }
    backend->direct_mem = direct_mem;
    fs->access_mode = access_mode;
    fs->vtable = &s_gfx_fs_mmap_vtable;
    fs->backend_data = backend;
    *out_fs = fs;
    return GFX_OK;
}

gfx_err_t gfx_fs_open_pack_file_port(const char *file_path, gfx_fs_t **out_fs)
{
    gfx_fs_t *fs = NULL;
    gfx_fs_mmap_backend_t *backend = NULL;
    esp_err_t ret;
    const mmap_assets_config_t mmap_config = {
        .partition_label = file_path,
        .max_files = 0,
        .checksum = 0,
        .flags = {
            .mmap_enable = true,
            .full_check = true,
            .use_fs = true,
        },
    };

    if (file_path == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_fs = NULL;

    fs = calloc(1, sizeof(*fs));
    backend = calloc(1, sizeof(*backend));
    if (fs == NULL || backend == NULL) {
        free(fs);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    ret = mmap_assets_new(&mmap_config, &backend->handle);
    if (ret != ESP_OK) {
        free(fs);
        free(backend);
        return gfx_err_from_esp(ret);
    }

    backend->max_files = mmap_assets_get_stored_files(backend->handle);
    if (backend->max_files <= 0) {
        (void)mmap_assets_del(backend->handle);
        free(fs);
        free(backend);
        return GFX_ERR_INVALID_SIZE;
    }
    backend->direct_mem = false;
    fs->access_mode = GFX_FS_ACCESS_COPY;
    fs->vtable = &s_gfx_fs_mmap_vtable;
    fs->backend_data = backend;
    *out_fs = fs;
    return GFX_OK;
}

gfx_err_t gfx_fs_open_dir_port(const char *root_dir, gfx_fs_t **out_fs)
{
    gfx_fs_t *fs = NULL;
    gfx_fs_vfs_backend_t *backend = NULL;

    if (root_dir == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
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
    fs->vtable = &s_gfx_fs_vfs_vtable;
    fs->backend_data = backend;
    *out_fs = fs;
    return GFX_OK;
}
