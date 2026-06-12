/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core/base/gfx_asset_priv.h"

gfx_err_t gfx_asset_store_open_dir_port(const char *root_dir, gfx_asset_store_t **out_store)
{
    (void)root_dir;
    if (out_store != NULL) {
        *out_store = NULL;
    }
    return GFX_ERR_NOT_SUPPORTED;
}
