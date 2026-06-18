/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_mmap_assets.h"
#include "esp_partition.h"

#include "core/base/gfx_asset_priv.h"

typedef struct {
    mmap_assets_handle_t handle;
    int32_t max_files;
} gfx_asset_mmap_backend_t;

typedef struct {
    gfx_asset_view_state_base_t base;
} gfx_asset_mmap_view_t;

typedef struct {
    char *root_dir;
    uint32_t alloc_caps;
} gfx_asset_vfs_backend_t;

typedef struct {
    gfx_asset_view_state_base_t base;
    char *name;
    void *owned;
} gfx_asset_vfs_view_t;

typedef struct {
    const esp_partition_t *partition;
    gfx_asset_load_mode_t load_mode;
    uint32_t alloc_caps;
} gfx_asset_partition_backend_t;

typedef struct {
    gfx_asset_view_state_base_t base;
    char *name;
    void *owned;
    esp_partition_mmap_handle_t mmap_handle;
} gfx_asset_partition_view_t;

static gfx_err_t gfx_asset_mmap_err_to_gfx(esp_err_t err)
{
    switch (err) {
    case ESP_OK:
        return GFX_OK;
    case ESP_ERR_NO_MEM:
        return GFX_ERR_NO_MEM;
    case ESP_ERR_INVALID_ARG:
        return GFX_ERR_INVALID_ARG;
    case ESP_ERR_INVALID_SIZE:
        return GFX_ERR_INVALID_SIZE;
    case ESP_ERR_NOT_FOUND:
        return GFX_ERR_NOT_FOUND;
    default:
        return GFX_FAIL;
    }
}

static char *gfx_asset_strdup(const char *s)
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

static void *gfx_asset_alloc(size_t size, uint32_t alloc_caps)
{
    if (alloc_caps != 0U) {
        return heap_caps_malloc(size, alloc_caps);
    }
    return malloc(size);
}

static bool gfx_asset_name_is_safe(const char *name)
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

static char *gfx_asset_join_path(const char *root, const char *name)
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

static gfx_err_t gfx_asset_mmap_open_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    gfx_asset_mmap_backend_t *backend = (gfx_asset_mmap_backend_t *)store->backend_data;
    gfx_asset_mmap_view_t *state;
    const void *data;
    const char *name;
    int size;

    if (backend == NULL || backend->handle == NULL || id < 0 ||
            (backend->max_files > 0 && id >= backend->max_files)) {
        return GFX_ERR_INVALID_ARG;
    }

    data = mmap_assets_get_mem(backend->handle, (int)id);
    size = mmap_assets_get_size(backend->handle, (int)id);
    if (data == NULL || size < 0) {
        return GFX_ERR_NOT_FOUND;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return GFX_ERR_NO_MEM;
    }
    state->base.store = store;

    name = mmap_assets_get_name(backend->handle, (int)id);
    out_view->data = data;
    out_view->size = (size_t)size;
    out_view->name = name;
    out_view->id = id;
    out_view->is_mapped = true;
    out_view->flags = GFX_ASSET_VIEW_FLAG_DIRECT_ADDR |
                      GFX_ASSET_VIEW_FLAG_MAPPED |
                      GFX_ASSET_VIEW_FLAG_PERSISTENT;
    out_view->priv = state;
    return GFX_OK;
}

static gfx_err_t gfx_asset_mmap_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    gfx_asset_mmap_backend_t *backend = (gfx_asset_mmap_backend_t *)store->backend_data;

    if (backend == NULL || backend->handle == NULL || name == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int32_t i = 0; i < backend->max_files; i++) {
        const char *asset_name = mmap_assets_get_name(backend->handle, (int)i);
        if (asset_name != NULL && strcmp(asset_name, name) == 0) {
            return gfx_asset_mmap_open_id(store, i, out_view);
        }
    }

    return GFX_ERR_NOT_FOUND;
}

static gfx_err_t gfx_asset_mmap_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    return gfx_asset_mmap_open_id(store, id, out_view);
}

static gfx_err_t gfx_asset_mmap_get_caps(const gfx_asset_store_t *store, gfx_asset_store_caps_t *out_caps)
{
    (void)store;
    if (out_caps == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_caps = (gfx_asset_store_caps_t) {
        .open_by_name = true,
        .open_by_id = true,
        .open_region = false,
        .can_direct_addr = true,
        .can_owned_copy = false,
        .can_force_copy = false,
        .can_force_direct = true,
    };
    return GFX_OK;
}

static void gfx_asset_mmap_view_close(gfx_asset_view_t *view)
{
    free(view != NULL ? view->priv : NULL);
}

static void gfx_asset_mmap_store_close(gfx_asset_store_t *store)
{
    gfx_asset_mmap_backend_t *backend;

    if (store == NULL) {
        return;
    }

    backend = (gfx_asset_mmap_backend_t *)store->backend_data;
    if (backend != NULL) {
        if (backend->handle != NULL) {
            (void)mmap_assets_del(backend->handle);
        }
        free(backend);
    }
    free(store);
}

static const gfx_asset_store_vtable_t s_gfx_asset_mmap_vtable = {
    .open_by_name = gfx_asset_mmap_open_by_name,
    .open_by_id = gfx_asset_mmap_open_by_id,
    .get_caps = gfx_asset_mmap_get_caps,
    .view_close = gfx_asset_mmap_view_close,
    .store_close = gfx_asset_mmap_store_close,
};

static gfx_err_t gfx_asset_vfs_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    gfx_asset_vfs_backend_t *backend = (gfx_asset_vfs_backend_t *)store->backend_data;
    gfx_asset_vfs_view_t *state = NULL;
    char *path = NULL;
    FILE *fp = NULL;
    long file_size;
    size_t size;
    gfx_err_t err = GFX_FAIL;

    if (backend == NULL || !gfx_asset_name_is_safe(name)) {
        return GFX_ERR_INVALID_ARG;
    }

    path = gfx_asset_join_path(backend->root_dir, name);
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
    state->base.store = store;
    state->name = gfx_asset_strdup(name);
    if (state->name == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }

    size = (size_t)file_size;
    if (size > 0U) {
        state->owned = gfx_asset_alloc(size, backend->alloc_caps);
        if (state->owned == NULL) {
            err = GFX_ERR_NO_MEM;
            goto cleanup;
        }
        if (fread(state->owned, 1, size, fp) != size) {
            err = GFX_FAIL;
            goto cleanup;
        }
        out_view->data = state->owned;
        out_view->flags = GFX_ASSET_VIEW_FLAG_OWNED;
    }

    out_view->size = size;
    out_view->name = state->name;
    out_view->id = -1;
    out_view->is_mapped = false;
    out_view->priv = state;
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

static gfx_err_t gfx_asset_vfs_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    (void)store;
    (void)id;
    (void)out_view;
    return GFX_ERR_NOT_SUPPORTED;
}

static gfx_err_t gfx_asset_vfs_get_caps(const gfx_asset_store_t *store, gfx_asset_store_caps_t *out_caps)
{
    (void)store;
    if (out_caps == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_caps = (gfx_asset_store_caps_t) {
        .open_by_name = true,
        .open_by_id = false,
        .open_region = false,
        .can_direct_addr = false,
        .can_owned_copy = true,
        .can_force_copy = true,
        .can_force_direct = false,
    };
    return GFX_OK;
}

static void gfx_asset_vfs_view_close(gfx_asset_view_t *view)
{
    gfx_asset_vfs_view_t *state = view != NULL ? (gfx_asset_vfs_view_t *)view->priv : NULL;
    if (state == NULL) {
        return;
    }
    free(state->owned);
    free(state->name);
    free(state);
}

static void gfx_asset_vfs_store_close(gfx_asset_store_t *store)
{
    if (store == NULL) {
        return;
    }
    gfx_asset_vfs_backend_t *backend = (gfx_asset_vfs_backend_t *)store->backend_data;
    if (backend != NULL) {
        free(backend->root_dir);
        free(backend);
    }
    free(store);
}

static const gfx_asset_store_vtable_t s_gfx_asset_vfs_vtable = {
    .open_by_name = gfx_asset_vfs_open_by_name,
    .open_by_id = gfx_asset_vfs_open_by_id,
    .get_caps = gfx_asset_vfs_get_caps,
    .view_close = gfx_asset_vfs_view_close,
    .store_close = gfx_asset_vfs_store_close,
};

static gfx_err_t gfx_asset_partition_open_region(gfx_asset_store_t *store,
        const gfx_asset_region_t *region,
        gfx_asset_view_t *out_view)
{
    gfx_asset_partition_backend_t *backend = (gfx_asset_partition_backend_t *)store->backend_data;
    gfx_asset_partition_view_t *state = NULL;
    const void *mapped = NULL;
    esp_err_t ret;
    gfx_err_t err = GFX_FAIL;

    if (backend == NULL || backend->partition == NULL || region == NULL || region->size == 0U) {
        return GFX_ERR_INVALID_ARG;
    }
    if ((uint64_t)region->offset + region->size > backend->partition->size) {
        return GFX_ERR_INVALID_SIZE;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return GFX_ERR_NO_MEM;
    }
    state->base.store = store;
    state->mmap_handle = 0;
    if (region->name != NULL) {
        state->name = gfx_asset_strdup(region->name);
        if (state->name == NULL) {
            err = GFX_ERR_NO_MEM;
            goto cleanup;
        }
    }

    if (backend->load_mode != GFX_ASSET_LOAD_FORCE_COPY) {
        ret = esp_partition_mmap(backend->partition,
                                 region->offset,
                                 region->size,
                                 ESP_PARTITION_MMAP_DATA,
                                 &mapped,
                                 &state->mmap_handle);
        if (ret == ESP_OK) {
            out_view->data = mapped;
            out_view->flags = GFX_ASSET_VIEW_FLAG_DIRECT_ADDR | GFX_ASSET_VIEW_FLAG_MAPPED;
            out_view->is_mapped = true;
            err = GFX_OK;
            goto fill_view;
        }
        if (backend->load_mode == GFX_ASSET_LOAD_FORCE_DIRECT) {
            err = gfx_asset_mmap_err_to_gfx(ret);
            goto cleanup;
        }
    }

    state->owned = gfx_asset_alloc(region->size, backend->alloc_caps);
    if (state->owned == NULL) {
        err = GFX_ERR_NO_MEM;
        goto cleanup;
    }
    ret = esp_partition_read(backend->partition, region->offset, state->owned, region->size);
    if (ret != ESP_OK) {
        err = gfx_asset_mmap_err_to_gfx(ret);
        goto cleanup;
    }
    out_view->data = state->owned;
    out_view->flags = GFX_ASSET_VIEW_FLAG_OWNED;
    out_view->is_mapped = false;
    err = GFX_OK;

fill_view:
    out_view->size = region->size;
    out_view->name = state->name != NULL ? state->name : region->name;
    out_view->id = region->id;
    out_view->priv = state;
    state = NULL;

cleanup:
    if (state != NULL) {
        if (state->mmap_handle != 0) {
            esp_partition_munmap(state->mmap_handle);
        }
        free(state->owned);
        free(state->name);
        free(state);
    }
    return err;
}

static gfx_err_t gfx_asset_partition_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    (void)store;
    (void)name;
    (void)out_view;
    return GFX_ERR_NOT_SUPPORTED;
}

static gfx_err_t gfx_asset_partition_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    (void)store;
    (void)id;
    (void)out_view;
    return GFX_ERR_NOT_SUPPORTED;
}

static gfx_err_t gfx_asset_partition_get_caps(const gfx_asset_store_t *store, gfx_asset_store_caps_t *out_caps)
{
    gfx_asset_partition_backend_t *backend = store != NULL ? (gfx_asset_partition_backend_t *)store->backend_data : NULL;
    if (backend == NULL || out_caps == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_caps = (gfx_asset_store_caps_t) {
        .open_by_name = false,
        .open_by_id = false,
        .open_region = true,
        .can_direct_addr = backend->load_mode != GFX_ASSET_LOAD_FORCE_COPY,
        .can_owned_copy = backend->load_mode != GFX_ASSET_LOAD_FORCE_DIRECT,
        .can_force_copy = true,
        .can_force_direct = true,
    };
    return GFX_OK;
}

static void gfx_asset_partition_view_close(gfx_asset_view_t *view)
{
    gfx_asset_partition_view_t *state = view != NULL ? (gfx_asset_partition_view_t *)view->priv : NULL;
    if (state == NULL) {
        return;
    }
    if (state->mmap_handle != 0) {
        esp_partition_munmap(state->mmap_handle);
    }
    free(state->owned);
    free(state->name);
    free(state);
}

static void gfx_asset_partition_store_close(gfx_asset_store_t *store)
{
    if (store == NULL) {
        return;
    }
    free(store->backend_data);
    free(store);
}

static const gfx_asset_store_vtable_t s_gfx_asset_partition_vtable = {
    .open_by_name = gfx_asset_partition_open_by_name,
    .open_by_id = gfx_asset_partition_open_by_id,
    .open_region = gfx_asset_partition_open_region,
    .get_caps = gfx_asset_partition_get_caps,
    .view_close = gfx_asset_partition_view_close,
    .store_close = gfx_asset_partition_store_close,
};

static gfx_err_t gfx_asset_store_open_mmap_config(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store)
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_mmap_backend_t *backend = NULL;
    esp_err_t ret;
    const mmap_assets_config_t mmap_config = {
        .partition_label = config->partition_label,
        .max_files = (int)config->max_files,
        .checksum = config->checksum,
        .flags = {
            .mmap_enable = true,
            .full_check = config->full_check,
        },
    };

    if (config == NULL || out_store == NULL || config->partition_label == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_store = NULL;

    store = calloc(1, sizeof(*store));
    backend = calloc(1, sizeof(*backend));
    if (store == NULL || backend == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    ret = mmap_assets_new(&mmap_config, &backend->handle);
    if (ret != ESP_OK) {
        free(store);
        free(backend);
        return gfx_asset_mmap_err_to_gfx(ret);
    }

    backend->max_files = mmap_assets_get_stored_files(backend->handle);
    if (backend->max_files <= 0) {
        (void)mmap_assets_del(backend->handle);
        free(store);
        free(backend);
        return GFX_ERR_INVALID_SIZE;
    }
    store->backend = GFX_ASSET_BACKEND_MMAP;
    store->vtable = &s_gfx_asset_mmap_vtable;
    store->backend_data = backend;
    *out_store = store;
    return GFX_OK;
}

static gfx_err_t gfx_asset_store_open_vfs_config(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store)
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_vfs_backend_t *backend = NULL;

    if (config == NULL || out_store == NULL || config->root_dir == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    store = calloc(1, sizeof(*store));
    backend = calloc(1, sizeof(*backend));
    if (store == NULL || backend == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }
    backend->root_dir = gfx_asset_strdup(config->root_dir);
    if (backend->root_dir == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }
    backend->alloc_caps = config->alloc_caps;

    store->backend = GFX_ASSET_BACKEND_DIR;
    store->vtable = &s_gfx_asset_vfs_vtable;
    store->backend_data = backend;
    *out_store = store;
    return GFX_OK;
}

static gfx_err_t gfx_asset_store_open_partition_config(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store)
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_partition_backend_t *backend = NULL;
    const esp_partition_t *partition = NULL;

    if (config == NULL || out_store == NULL || config->partition_label == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
                                         ESP_PARTITION_SUBTYPE_ANY,
                                         config->partition_label);
    if (partition == NULL) {
        return GFX_ERR_NOT_FOUND;
    }

    store = calloc(1, sizeof(*store));
    backend = calloc(1, sizeof(*backend));
    if (store == NULL || backend == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    backend->partition = partition;
    backend->load_mode = config->load_mode;
    backend->alloc_caps = config->alloc_caps;
    store->backend = GFX_ASSET_BACKEND_PARTITION;
    store->vtable = &s_gfx_asset_partition_vtable;
    store->backend_data = backend;
    *out_store = store;
    return GFX_OK;
}

gfx_err_t gfx_asset_store_open_config_port(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store)
{
    if (config == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *out_store = NULL;

    switch (config->type) {
    case GFX_ASSET_STORE_TYPE_AUTO:
        if (config->partition_label != NULL) {
            return gfx_asset_store_open_mmap_config(config, out_store);
        }
        if (config->root_dir != NULL) {
            return gfx_asset_store_open_vfs_config(config, out_store);
        }
        return GFX_ERR_INVALID_ARG;
    case GFX_ASSET_STORE_TYPE_DIR:
        return gfx_asset_store_open_vfs_config(config, out_store);
    case GFX_ASSET_STORE_TYPE_MMAP_ASSETS:
        return gfx_asset_store_open_mmap_config(config, out_store);
    case GFX_ASSET_STORE_TYPE_PARTITION:
        return gfx_asset_store_open_partition_config(config, out_store);
    case GFX_ASSET_STORE_TYPE_MEMORY_TABLE:
    default:
        return GFX_ERR_NOT_SUPPORTED;
    }
}

gfx_err_t gfx_asset_store_open_dir_port(const char *root_dir, gfx_asset_store_t **out_store)
{
    const gfx_asset_store_config_t config = {
        .type = GFX_ASSET_STORE_TYPE_DIR,
        .load_mode = GFX_ASSET_LOAD_FORCE_COPY,
        .root_dir = root_dir,
    };
    return gfx_asset_store_open_vfs_config(&config, out_store);
}

gfx_err_t gfx_asset_store_open_mmap_port(const gfx_asset_mmap_config_t *config, gfx_asset_store_t **out_store)
{
    const gfx_asset_store_config_t store_config = {
        .type = GFX_ASSET_STORE_TYPE_MMAP_ASSETS,
        .load_mode = GFX_ASSET_LOAD_PREFER_DIRECT,
        .partition_label = config != NULL ? config->partition_label : NULL,
        .max_files = config != NULL ? config->max_files : 0,
        .checksum = config != NULL ? config->checksum : 0U,
        .full_check = config != NULL ? config->full_check : false,
    };
    if (config == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    return gfx_asset_store_open_mmap_config(&store_config, out_store);
}
