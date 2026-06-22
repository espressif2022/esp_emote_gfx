/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "core/base/gfx_fs_priv.h"

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : ".";
    const char *name = argc > 2 ? argv[2] : "TODO.md";
    gfx_fs_t *store = NULL;
    gfx_fs_view_t view;
    gfx_fs_caps_t caps;
    const gfx_fs_config_t config = {
        .type = GFX_FS_TYPE_DIR,
        .load_mode = GFX_FS_LOAD_PREFER_DIRECT,
        .root_dir = root,
    };

    gfx_err_t err = gfx_fs_open(&config, &store);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset dir failed: %d root=%s\n", err, root);
        return 1;
    }
    if (gfx_fs_get_backend(store) != GFX_FS_BACKEND_DIR) {
        fprintf(stderr, "unexpected backend: %d\n", gfx_fs_get_backend(store));
        gfx_fs_close(store);
        return 1;
    }
    err = gfx_fs_get_caps(store, &caps);
    if (err != GFX_OK || !caps.open_by_name || caps.open_by_id || !caps.can_direct_addr || !caps.can_owned_copy) {
        fprintf(stderr, "unexpected caps: err=%d by_name=%d by_id=%d direct=%d copy=%d\n",
                err, caps.open_by_name, caps.open_by_id, caps.can_direct_addr, caps.can_owned_copy);
        gfx_fs_close(store);
        return 1;
    }

    err = gfx_fs_view_open_by_name(store, name, &view);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset failed: %d name=%s\n", err, name);
        gfx_fs_close(store);
        return 1;
    }
    if (view.data == NULL || view.size == 0U || view.id != -1) {
        fprintf(stderr, "invalid view: data=%p size=%zu id=%d\n", view.data, view.size, (int)view.id);
        gfx_fs_view_close(&view);
        gfx_fs_close(store);
        return 1;
    }
    if (view.is_mapped && (view.flags & GFX_FS_VIEW_FLAG_MAPPED) == 0U) {
        fprintf(stderr, "mapped compatibility flag mismatch\n");
        gfx_fs_view_close(&view);
        gfx_fs_close(store);
        return 1;
    }
    if ((view.flags & (GFX_FS_VIEW_FLAG_DIRECT_ADDR | GFX_FS_VIEW_FLAG_OWNED)) == 0U) {
        fprintf(stderr, "view is neither direct nor owned: flags=0x%x\n", (unsigned)view.flags);
        gfx_fs_view_close(&view);
        gfx_fs_close(store);
        return 1;
    }

    printf("asset: name=%s size=%zu flags=0x%x mapped=%s first=0x%02x\n",
           view.name != NULL ? view.name : name,
           view.size,
           (unsigned)view.flags,
           view.is_mapped ? "yes" : "no",
           view.size > 0U ? ((const unsigned char *)view.data)[0] : 0U);

    gfx_fs_view_close(&view);
    if (view.id != -1 || view.data != NULL || view.size != 0U) {
        fprintf(stderr, "view not reset after close\n");
        gfx_fs_close(store);
        return 1;
    }
    err = gfx_fs_view_open_by_id(store, 0, &view);
    if (err != GFX_ERR_NOT_SUPPORTED) {
        fprintf(stderr, "directory open_by_id should be unsupported, got %d\n", err);
        gfx_fs_view_close(&view);
        gfx_fs_close(store);
        return 1;
    }

    /* Exercise the public stdio-style file API against the same default store. */
    gfx_fs_set_default(store);
    gfx_fs_file_t *file = gfx_fs_fopen(name);
    if (file == NULL) {
        fprintf(stderr, "gfx_fs_fopen failed: name=%s\n", name);
        gfx_fs_close(store);
        return 1;
    }
    size_t fsize = gfx_fs_fsize(file);
    const unsigned char *base = gfx_fs_fdata(file);
    unsigned char head[4] = {0};
    size_t got = gfx_fs_fread(file, head, sizeof(head));
    long pos = gfx_fs_ftell(file);
    if (fsize == 0U || base == NULL || got != sizeof(head) || pos != (long)sizeof(head) ||
            memcmp(head, base, sizeof(head)) != 0) {
        fprintf(stderr, "file api read mismatch: size=%zu base=%p got=%zu pos=%ld\n",
                fsize, (const void *)base, got, pos);
        gfx_fs_fclose(file);
        gfx_fs_close(store);
        return 1;
    }
    if (gfx_fs_fseek(file, 0, SEEK_SET) != 0 || gfx_fs_ftell(file) != 0 ||
            gfx_fs_fseek(file, 0, SEEK_END) != 0 || gfx_fs_ftell(file) != (long)fsize ||
            gfx_fs_fseek(file, -1, SEEK_SET) != -1 ||
            gfx_fs_fseek(file, (long)fsize + 1, SEEK_SET) != -1) {
        fprintf(stderr, "file api seek semantics failed\n");
        gfx_fs_fclose(file);
        gfx_fs_close(store);
        return 1;
    }
    gfx_fs_fclose(file);

    gfx_fs_set_default(NULL);
    gfx_fs_close(store);
    return 0;
}
