/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/base/gfx_fs_priv.h"
#include "platform/gfx_platform.h"

struct gfx_fs_file {
    gfx_fs_view_t view;
    FILE *fp;
    const uint8_t *data;
    size_t size;
    size_t pos;
};

typedef struct {
    gfx_fs_file_t *file;
    uint8_t *owned;
} gfx_fs_blob_holder_t;

static gfx_fs_file_t *gfx_fs_file_alloc(void)
{
    return calloc(1, sizeof(gfx_fs_file_t));
}

static gfx_fs_file_t *gfx_fs_file_open_store(gfx_fs_t *store, const char *name)
{
    gfx_fs_file_t *file;
    gfx_err_t err;

    if (store == NULL || name == NULL) {
        return NULL;
    }

    file = gfx_fs_file_alloc();
    if (file == NULL) {
        return NULL;
    }

    err = gfx_fs_view_open_by_name(store, name, &file->view);
    if (err != GFX_OK || file->view.data == NULL || file->view.size == 0U) {
        gfx_fs_view_close(&file->view);
        free(file);
        return NULL;
    }

    file->data = (const uint8_t *)file->view.data;
    file->size = file->view.size;
    return file;
}

static gfx_fs_file_t *gfx_fs_file_open_stdio(const char *name)
{
    gfx_fs_file_t *file;
    FILE *fp;
    long size;

    if (name == NULL) {
        return NULL;
    }

    fp = fopen(name, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    file = gfx_fs_file_alloc();
    if (file == NULL) {
        fclose(fp);
        return NULL;
    }

    file->fp = fp;
    file->size = (size_t)size;
    return file;
}

gfx_err_t gfx_fs_load(const char *name, gfx_fs_blob_t *out_blob)
{
    gfx_fs_blob_holder_t *holder;
    gfx_fs_file_t *file;
    const void *direct;
    size_t size;

    if (out_blob != NULL) {
        memset(out_blob, 0, sizeof(*out_blob));
    }
    if (name == NULL || out_blob == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    file = gfx_fs_fopen(name);
    if (file == NULL) {
        return GFX_ERR_NOT_FOUND;
    }

    holder = calloc(1, sizeof(*holder));
    if (holder == NULL) {
        gfx_fs_fclose(file);
        return GFX_ERR_NO_MEM;
    }

    size = gfx_fs_fsize(file);
    direct = gfx_fs_fdata(file);
    if (direct != NULL) {
        holder->file = file;
        out_blob->data = direct;
        out_blob->size = size;
        out_blob->_holder = holder;
        return GFX_OK;
    }

    holder->owned = gfx_platform_aligned_alloc(16, size, GFX_PLATFORM_HEAP_DEFAULT);
    if (holder->owned == NULL) {
        gfx_fs_fclose(file);
        free(holder);
        return GFX_ERR_NO_MEM;
    }
    if (gfx_fs_fread(file, holder->owned, size) != size) {
        gfx_platform_free(holder->owned);
        gfx_fs_fclose(file);
        free(holder);
        return GFX_FAIL;
    }

    gfx_fs_fclose(file);
    out_blob->data = holder->owned;
    out_blob->size = size;
    out_blob->_holder = holder;
    return GFX_OK;
}

void gfx_fs_unload(gfx_fs_blob_t *blob)
{
    gfx_fs_blob_holder_t *holder;

    if (blob == NULL) {
        return;
    }

    holder = (gfx_fs_blob_holder_t *)blob->_holder;
    if (holder != NULL) {
        gfx_fs_fclose(holder->file);
        gfx_platform_free(holder->owned);
        free(holder);
    }
    memset(blob, 0, sizeof(*blob));
}

gfx_fs_file_t *gfx_fs_fopen(const char *name)
{
    gfx_fs_file_t *file;
    gfx_fs_t *store;

    if (name == NULL) {
        return NULL;
    }

    store = gfx_fs_get_default();
    file = gfx_fs_file_open_store(store, name);
    if (file != NULL) {
        return file;
    }

    return gfx_fs_file_open_stdio(name);
}

gfx_fs_file_t *gfx_fs_fopen_from(gfx_fs_t *store, const char *name)
{
    return gfx_fs_file_open_store(store, name);
}

void gfx_fs_fclose(gfx_fs_file_t *file)
{
    if (file == NULL) {
        return;
    }
    if (file->fp != NULL) {
        fclose(file->fp);
    }
    gfx_fs_view_close(&file->view);
    free(file);
}

size_t gfx_fs_fsize(const gfx_fs_file_t *file)
{
    return file != NULL ? file->size : 0U;
}

const void *gfx_fs_fdata(const gfx_fs_file_t *file)
{
    return file != NULL ? file->data : NULL;
}

size_t gfx_fs_fread(gfx_fs_file_t *file, void *buf, size_t len)
{
    size_t avail;
    size_t got;

    if (file == NULL || buf == NULL || len == 0U) {
        return 0U;
    }

    avail = (file->pos <= file->size) ? (file->size - file->pos) : 0U;
    if (len > avail) {
        len = avail;
    }
    if (len == 0U) {
        return 0U;
    }

    if (file->data != NULL) {
        memcpy(buf, file->data + file->pos, len);
        file->pos += len;
        return len;
    }

    if (file->fp == NULL || fseek(file->fp, (long)file->pos, SEEK_SET) != 0) {
        return 0U;
    }
    got = fread(buf, 1, len, file->fp);
    file->pos += got;
    return got;
}

int gfx_fs_fseek(gfx_fs_file_t *file, long offset, int whence)
{
    long base;
    long target;

    if (file == NULL) {
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (long)file->pos;
        break;
    case SEEK_END:
        base = (long)file->size;
        break;
    default:
        return -1;
    }

    target = base + offset;
    if (target < 0 || (size_t)target > file->size) {
        return -1;
    }
    if (file->fp != NULL && fseek(file->fp, target, SEEK_SET) != 0) {
        return -1;
    }

    file->pos = (size_t)target;
    return 0;
}

long gfx_fs_ftell(const gfx_fs_file_t *file)
{
    return file != NULL ? (long)file->pos : -1;
}
