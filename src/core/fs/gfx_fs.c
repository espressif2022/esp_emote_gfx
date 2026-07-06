/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "core/fs/gfx_fs_priv.h"

static gfx_fs_mount_t s_mount_table[GFX_FS_MOUNT_MAX];

#if GFX_HOST_BUILD
static gfx_err_t gfx_fs_open_unsupported(gfx_asset_source_t **out_fs)
{
    if (out_fs != NULL) {
        *out_fs = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}
#endif

static void gfx_fs_drop_mounts(gfx_asset_source_t *fs)
{
    if (fs == NULL) {
        return;
    }

    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs == fs) {
            s_mount_table[i].fs = NULL;
            s_mount_table[i].prefix[0] = '\0';
        }
    }
}

static void gfx_fs_entry_reset(gfx_fs_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }
    memset(entry, 0, sizeof(*entry));
}

gfx_err_t gfx_fs_open(const gfx_fs_open_config_t *config, gfx_asset_source_t **out_fs)
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

gfx_err_t gfx_fs_open_dir(const char *root_dir, gfx_asset_source_t **out_fs)
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

gfx_err_t gfx_fs_open_pack(const char *file_path, gfx_asset_source_t **out_fs)
{
    if (file_path == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_PACK_FILE,
        .access_mode = GFX_FS_ACCESS_COPY,
        .path_or_label = file_path,
    }, out_fs);
}

gfx_err_t gfx_fs_open_partition(const char *label, gfx_asset_source_t **out_fs)
{
    if (label == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_PARTITION,
        .access_mode = GFX_FS_ACCESS_DIRECT,
        .path_or_label = label,
    }, out_fs);
}

gfx_err_t gfx_fs_open_partition_copy(const char *label, gfx_asset_source_t **out_fs)
{
    if (label == NULL || out_fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_PARTITION,
        .access_mode = GFX_FS_ACCESS_COPY,
        .path_or_label = label,
    }, out_fs);
}

gfx_fs_access_mode_t gfx_fs_get_access_mode(const gfx_asset_source_t *fs)
{
    if (fs == NULL) {
        return GFX_FS_ACCESS_COPY;
    }
    return fs->access_mode;
}

gfx_err_t gfx_fs_mount(const char *prefix, gfx_asset_source_t *fs)
{
    if (prefix == NULL || fs == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs != NULL &&
                strcmp(s_mount_table[i].prefix, prefix) == 0) {
            s_mount_table[i].fs = fs;
            return GFX_OK;
        }
    }

    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs == NULL) {
            (void)snprintf(s_mount_table[i].prefix, GFX_FS_MOUNT_PREFIX_MAX, "%s", prefix);
            s_mount_table[i].fs = fs;
            return GFX_OK;
        }
    }

    return GFX_ERR_NO_MEM;
}

gfx_err_t gfx_fs_unmount(const char *prefix)
{
    if (prefix == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        if (s_mount_table[i].fs != NULL &&
                strcmp(s_mount_table[i].prefix, prefix) == 0) {
            s_mount_table[i].fs = NULL;
            s_mount_table[i].prefix[0] = '\0';
            return GFX_OK;
        }
    }

    return GFX_ERR_NOT_FOUND;
}

bool gfx_fs_resolve_mount(const char *name, gfx_asset_source_t **out_fs, const char **out_subname)
{
    const gfx_fs_mount_t *best = NULL;
    size_t best_len = 0;

    if (name == NULL || out_fs == NULL || out_subname == NULL) {
        return false;
    }

    *out_fs = NULL;
    *out_subname = NULL;

    for (int i = 0; i < GFX_FS_MOUNT_MAX; i++) {
        const gfx_fs_mount_t *mount = &s_mount_table[i];
        size_t plen;

        if (mount->fs == NULL) {
            continue;
        }

        plen = strlen(mount->prefix);
        if (plen < best_len) {
            continue;
        }

        if (plen == 0U) {
            if (best == NULL) {
                best = mount;
                best_len = 0U;
            }
            continue;
        }

        if (strncmp(name, mount->prefix, plen) == 0 &&
                (name[plen] == '/' || name[plen] == '\0')) {
            best = mount;
            best_len = plen;
        }
    }

    if (best == NULL) {
        return false;
    }

    {
        const char *subname = name + best_len;

        if (*subname == '/') {
            subname++;
        }
        if (*subname == '\0') {
            subname = name;
        }

        *out_fs = best->fs;
        *out_subname = subname;
    }
    return true;
}

void gfx_fs_close(gfx_asset_source_t *fs)
{
    if (fs == NULL) {
        return;
    }

    gfx_fs_drop_mounts(fs);

    if (fs->vtable != NULL && fs->vtable->fs_close != NULL) {
        fs->vtable->fs_close(fs);
    }
}

gfx_err_t gfx_fs_entry_open_by_name(gfx_asset_source_t *fs, const char *name, gfx_fs_entry_t *out_entry)
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
    gfx_asset_source_t *fs;

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
