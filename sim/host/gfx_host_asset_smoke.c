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

    gfx_err_t err = gfx_asset_store_open_dir(root, &store);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset dir failed: %d root=%s\n", err, root);
        return 1;
    }

    err = gfx_asset_open_by_name(store, name, &view);
    if (err != GFX_OK) {
        fprintf(stderr, "open asset failed: %d name=%s\n", err, name);
        gfx_asset_store_close(store);
        return 1;
    }

    printf("asset: name=%s size=%zu mapped=%s first=0x%02x\n",
           view.name != NULL ? view.name : name,
           view.size,
           view.is_mapped ? "yes" : "no",
           view.size > 0U ? ((const unsigned char *)view.data)[0] : 0U);

    gfx_asset_view_close(&view);
    gfx_asset_store_close(store);
    return 0;
}
