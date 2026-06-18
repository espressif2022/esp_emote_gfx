/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "core/gfx_asset.h"

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : ".";
    const char *name = argc > 2 ? argv[2] : "TODO.md";
    gfx_asset_store_t *store = NULL;
    gfx_asset_view_t view;
    gfx_asset_store_caps_t caps;
    const gfx_asset_store_config_t config = {
        .type = GFX_ASSET_STORE_TYPE_DIR,
        .load_mode = GFX_ASSET_LOAD_PREFER_DIRECT,
        .root_dir = root,
    };

    gfx_err_t err = gfx_asset_store_open(&config, &store);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset dir failed: %d root=%s\n", err, root);
        return 1;
    }
    if (gfx_asset_store_get_backend(store) != GFX_ASSET_BACKEND_DIR) {
        fprintf(stderr, "unexpected backend: %d\n", gfx_asset_store_get_backend(store));
        gfx_asset_store_close(store);
        return 1;
    }
    err = gfx_asset_store_get_caps(store, &caps);
    if (err != GFX_OK || !caps.open_by_name || caps.open_by_id || !caps.can_direct_addr || !caps.can_owned_copy) {
        fprintf(stderr, "unexpected caps: err=%d by_name=%d by_id=%d direct=%d copy=%d\n",
                err, caps.open_by_name, caps.open_by_id, caps.can_direct_addr, caps.can_owned_copy);
        gfx_asset_store_close(store);
        return 1;
    }

    err = gfx_asset_open_by_name(store, name, &view);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset failed: %d name=%s\n", err, name);
        gfx_asset_store_close(store);
        return 1;
    }
    if (view.data == NULL || view.size == 0U || view.id != -1) {
        fprintf(stderr, "invalid view: data=%p size=%zu id=%d\n", view.data, view.size, (int)view.id);
        gfx_asset_view_close(&view);
        gfx_asset_store_close(store);
        return 1;
    }
    if (view.is_mapped && (view.flags & GFX_ASSET_VIEW_FLAG_MAPPED) == 0U) {
        fprintf(stderr, "mapped compatibility flag mismatch\n");
        gfx_asset_view_close(&view);
        gfx_asset_store_close(store);
        return 1;
    }
    if ((view.flags & (GFX_ASSET_VIEW_FLAG_DIRECT_ADDR | GFX_ASSET_VIEW_FLAG_OWNED)) == 0U) {
        fprintf(stderr, "view is neither direct nor owned: flags=0x%x\n", (unsigned)view.flags);
        gfx_asset_view_close(&view);
        gfx_asset_store_close(store);
        return 1;
    }

    printf("asset: name=%s size=%zu flags=0x%x mapped=%s first=0x%02x\n",
           view.name != NULL ? view.name : name,
           view.size,
           (unsigned)view.flags,
           view.is_mapped ? "yes" : "no",
           view.size > 0U ? ((const unsigned char *)view.data)[0] : 0U);

    gfx_asset_view_close(&view);
    if (view.id != -1 || view.data != NULL || view.size != 0U) {
        fprintf(stderr, "view not reset after close\n");
        gfx_asset_store_close(store);
        return 1;
    }
    err = gfx_asset_open_by_id(store, 0, &view);
    if (err != GFX_ERR_NOT_SUPPORTED) {
        fprintf(stderr, "directory open_by_id should be unsupported, got %d\n", err);
        gfx_asset_view_close(&view);
        gfx_asset_store_close(store);
        return 1;
    }
    gfx_asset_store_close(store);
    return 0;
}
