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

#include "core/base/gfx_asset_priv.h"

typedef struct {
    char *root_dir;
} gfx_asset_dir_backend_t;

typedef struct {
    gfx_asset_view_state_base_t base;
    char *name;
    void *mapped;
    size_t mapped_size;
    void *owned;
} gfx_asset_dir_view_t;

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

static gfx_err_t gfx_asset_read_file(int fd, size_t size, void **out_data)
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

static gfx_err_t gfx_asset_dir_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    gfx_asset_dir_backend_t *backend = (gfx_asset_dir_backend_t *)store->backend_data;
    gfx_asset_dir_view_t *state = NULL;
    char *path = NULL;
    int fd = -1;
    struct stat st;
    gfx_err_t err = GFX_FAIL;

    if (backend == NULL || !gfx_asset_name_is_safe(name)) {
        return GFX_ERR_INVALID_ARG;
    }

    path = gfx_asset_join_path(backend->root_dir, name);
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
    state->base.store = store;
    state->name = gfx_asset_strdup(name);
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
            out_view->data = mapped;
            out_view->is_mapped = true;
        } else {
            err = gfx_asset_read_file(fd, size, &state->owned);
            if (err != GFX_OK) {
                goto cleanup;
            }
            out_view->data = state->owned;
            out_view->is_mapped = false;
        }
    }

    out_view->size = size;
    out_view->name = state->name;
    out_view->id = -1;
    out_view->priv = state;
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

static gfx_err_t gfx_asset_dir_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    (void)store;
    (void)id;
    (void)out_view;
    return GFX_ERR_NOT_SUPPORTED;
}

static void gfx_asset_dir_view_close(gfx_asset_view_t *view)
{
    gfx_asset_dir_view_t *state = view != NULL ? (gfx_asset_dir_view_t *)view->priv : NULL;
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

static void gfx_asset_dir_store_close(gfx_asset_store_t *store)
{
    if (store == NULL) {
        return;
    }

    gfx_asset_dir_backend_t *backend = (gfx_asset_dir_backend_t *)store->backend_data;
    if (backend != NULL) {
        free(backend->root_dir);
        free(backend);
    }
    free(store);
}

static const gfx_asset_store_vtable_t s_gfx_asset_dir_vtable = {
    .open_by_name = gfx_asset_dir_open_by_name,
    .open_by_id = gfx_asset_dir_open_by_id,
    .view_close = gfx_asset_dir_view_close,
    .store_close = gfx_asset_dir_store_close,
};

gfx_err_t gfx_asset_store_open_dir_port(const char *root_dir, gfx_asset_store_t **out_store)
{
    gfx_asset_store_t *store = NULL;
    gfx_asset_dir_backend_t *backend = NULL;
    struct stat st;

    if (root_dir == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (stat(root_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return GFX_ERR_NOT_FOUND;
    }

    store = calloc(1, sizeof(*store));
    backend = calloc(1, sizeof(*backend));
    if (store == NULL || backend == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    backend->root_dir = gfx_asset_strdup(root_dir);
    if (backend->root_dir == NULL) {
        free(store);
        free(backend);
        return GFX_ERR_NO_MEM;
    }

    store->backend = GFX_ASSET_BACKEND_DIR;
    store->vtable = &s_gfx_asset_dir_vtable;
    store->backend_data = backend;
    *out_store = store;
    return GFX_OK;
}
