/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gfx_err_t (*open_by_name)(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view);
    gfx_err_t (*open_by_id)(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view);
    void (*view_close)(gfx_asset_view_t *view);
    void (*store_close)(gfx_asset_store_t *store);
} gfx_asset_store_vtable_t;

struct gfx_asset_store {
    gfx_asset_backend_t backend;
    const gfx_asset_store_vtable_t *vtable;
    void *backend_data;
};

typedef struct {
    gfx_asset_store_t *store;
} gfx_asset_view_state_base_t;

gfx_err_t gfx_asset_store_open_dir_port(const char *root_dir, gfx_asset_store_t **out_store);

#ifdef __cplusplus
}
#endif
