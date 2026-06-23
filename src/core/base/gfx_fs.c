/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "core/base/gfx_fs_priv.h"

static gfx_fs_t *s_default_fs;

#if GFX_HOST_BUILD
static gfx_err_t gfx_fs_open_unsupported(gfx_fs_t **out_fs)
{
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}
#endif

static void gfx_fs_entry_reset(gfx_fs_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }
    memset(entry, 0, sizeof(*entry));
}

gfx_err_t gfx_fs_open(const gfx_fs_open_config_t *config, gfx_fs_t **out_fs)
{
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    if (config == NULL || config->path_or_label == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    switch (config->source_type) {
    case GFX_FS_SOURCE_DIR:
        if (config->access_mode != GFX_FS_ACCESS_COPY) {
            return GFX_ERR_INVALID_ARG;
        }
        return gfx_fs_open_dir_port(config->path_or_label, out_fs);

    case GFX_FS_SOURCE_PARTITION:
#if GFX_HOST_BUILD
        return gfx_fs_open_unsupported(out_fs);
#else
        if (config->access_mode != GFX_FS_ACCESS_DIRECT &&
                config->access_mode != GFX_FS_ACCESS_COPY) {
            return GFX_ERR_INVALID_ARG;
        }
        return gfx_fs_open_partition_port(config->path_or_label, config->access_mode, out_fs);
#endif

    case GFX_FS_SOURCE_PACK_FILE:
        if (config->access_mode != GFX_FS_ACCESS_COPY) {
            return GFX_ERR_INVALID_ARG;
        }
        return gfx_fs_open_pack_file_port(config->path_or_label, out_fs);

    default:
        return GFX_ERR_INVALID_ARG;
    }
}

gfx_err_t gfx_fs_open_dir(const char *root_dir, gfx_fs_t **out_fs)
{
    if (root_dir == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_DIR,
        .access_mode = GFX_FS_ACCESS_COPY,
        .path_or_label = root_dir,
    }, out_fs);
}

gfx_fs_access_mode_t gfx_fs_get_access_mode(const gfx_fs_t *fs)
{
    if (fs == NULL) {
        return GFX_FS_ACCESS_COPY;
    }
    return fs->access_mode;
}

void gfx_fs_set_default(gfx_fs_t *fs)
{
    s_default_fs = fs;
}

gfx_fs_t *gfx_fs_get_default(void)
{
    return s_default_fs;
}

void gfx_fs_close(gfx_fs_t *fs)
{
    if (fs == NULL) {
        return;
    }

    if (fs == s_default_fs) {
        s_default_fs = NULL;
    }

    if (fs->vtable != NULL && fs->vtable->fs_close != NULL) {
        fs->vtable->fs_close(fs);
    }
}

gfx_err_t gfx_fs_entry_open_by_name(gfx_fs_t *fs, const char *name, gfx_fs_entry_t *out_entry)
{
    gfx_fs_entry_reset(out_entry);
    if (fs == NULL || name == NULL || out_entry == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (fs->vtable == NULL || fs->vtable->open_by_name == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return fs->vtable->open_by_name(fs, name, out_entry);
}

void gfx_fs_entry_close(gfx_fs_entry_t *entry)
{
    gfx_fs_entry_state_base_t *state;
    gfx_fs_t *fs;

    if (entry == NULL || entry->priv == NULL) {
        gfx_fs_entry_reset(entry);
        return;
    }

    state = (gfx_fs_entry_state_base_t *)entry->priv;
    fs = state->fs;
    if (fs->vtable != NULL && fs->vtable->entry_close != NULL) {
        fs->vtable->entry_close(entry);
    }
    gfx_fs_entry_reset(entry);
}
