/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "core/base/gfx_asset_priv.h"

gfx_err_t gfx_asset_store_open_dir(const char *root_dir, gfx_asset_store_t **out_store)
{
    if (root_dir == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    *out_store = NULL;
    return gfx_asset_store_open_dir_port(root_dir, out_store);
}

void gfx_asset_store_close(gfx_asset_store_t *store)
{
    if (store == NULL) {
        return;
    }

    if (store->vtable != NULL && store->vtable->store_close != NULL) {
        store->vtable->store_close(store);
    }
}

gfx_asset_backend_t gfx_asset_store_get_backend(const gfx_asset_store_t *store)
{
    return store != NULL ? store->backend : GFX_ASSET_BACKEND_NONE;
}

gfx_err_t gfx_asset_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    if (out_view != NULL) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->id = -1;
    }
    if (store == NULL || name == NULL || out_view == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (store->vtable == NULL || store->vtable->open_by_name == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return store->vtable->open_by_name(store, name, out_view);
}

gfx_err_t gfx_asset_open_by_id(gfx_asset_store_t *store, int32_t id, gfx_asset_view_t *out_view)
{
    if (out_view != NULL) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->id = -1;
    }
    if (store == NULL || id < 0 || out_view == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (store->vtable == NULL || store->vtable->open_by_id == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return store->vtable->open_by_id(store, id, out_view);
}

void gfx_asset_view_close(gfx_asset_view_t *view)
{
    gfx_asset_view_state_base_t *state;
    gfx_asset_store_t *store;

    if (view == NULL || view->priv == NULL) {
        if (view != NULL) {
            memset(view, 0, sizeof(*view));
            view->id = -1;
        }
        return;
    }

    state = (gfx_asset_view_state_base_t *)view->priv;
    store = state->store;
    if (store->vtable != NULL && store->vtable->view_close != NULL) {
        store->vtable->view_close(view);
    }
    memset(view, 0, sizeof(*view));
    view->id = -1;
}
