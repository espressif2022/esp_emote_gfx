/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "core/base/gfx_asset_priv.h"

static gfx_asset_store_t *s_default_asset_store;

static void gfx_asset_view_reset(gfx_asset_view_t *view)
{
    if (view == NULL) {
        return;
    }
    memset(view, 0, sizeof(*view));
    view->id = -1;
}

gfx_err_t gfx_asset_store_open(const gfx_asset_store_config_t *config, gfx_asset_store_t **out_store)
{
    if (config == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    *out_store = NULL;
    return gfx_asset_store_open_config_port(config, out_store);
}

gfx_err_t gfx_asset_store_open_dir(const char *root_dir, gfx_asset_store_t **out_store)
{
    const gfx_asset_store_config_t config = {
        .type = GFX_ASSET_STORE_TYPE_DIR,
        .load_mode = GFX_ASSET_LOAD_PREFER_DIRECT,
        .root_dir = root_dir,
    };

    if (root_dir == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_asset_store_open(&config, out_store);
}

gfx_err_t gfx_asset_store_open_mmap(const gfx_asset_mmap_config_t *config, gfx_asset_store_t **out_store)
{
    const gfx_asset_store_config_t store_config = {
        .type = GFX_ASSET_STORE_TYPE_MMAP_ASSETS,
        .load_mode = GFX_ASSET_LOAD_PREFER_DIRECT,
        .partition_label = config != NULL ? config->partition_label : NULL,
        .max_files = config != NULL ? config->max_files : 0,
        .checksum = config != NULL ? config->checksum : 0U,
        .full_check = config != NULL ? config->full_check : false,
    };

    if (config == NULL || out_store == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    return gfx_asset_store_open(&store_config, out_store);
}

void gfx_asset_set_default_store(gfx_asset_store_t *store)
{
    s_default_asset_store = store;
}

gfx_asset_store_t *gfx_asset_get_default_store(void)
{
    return s_default_asset_store;
}

void gfx_asset_store_close(gfx_asset_store_t *store)
{
    if (store == NULL) {
        return;
    }

    if (store == s_default_asset_store) {
        s_default_asset_store = NULL;
    }

    if (store->vtable != NULL && store->vtable->store_close != NULL) {
        store->vtable->store_close(store);
    }
}

gfx_asset_backend_t gfx_asset_store_get_backend(const gfx_asset_store_t *store)
{
    return store != NULL ? store->backend : GFX_ASSET_BACKEND_NONE;
}

gfx_err_t gfx_asset_store_get_caps(const gfx_asset_store_t *store, gfx_asset_store_caps_t *out_caps)
{
    if (out_caps != NULL) {
        memset(out_caps, 0, sizeof(*out_caps));
    }
    if (store == NULL || out_caps == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (store->vtable == NULL || store->vtable->get_caps == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return store->vtable->get_caps(store, out_caps);
}

gfx_err_t gfx_asset_open_by_name(gfx_asset_store_t *store, const char *name, gfx_asset_view_t *out_view)
{
    gfx_asset_view_reset(out_view);
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
    gfx_asset_view_reset(out_view);
    if (store == NULL || id < 0 || out_view == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (store->vtable == NULL || store->vtable->open_by_id == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return store->vtable->open_by_id(store, id, out_view);
}

gfx_err_t gfx_asset_open_region(gfx_asset_store_t *store,
                                const gfx_asset_region_t *region,
                                gfx_asset_view_t *out_view)
{
    gfx_asset_view_reset(out_view);
    if (store == NULL || region == NULL || out_view == NULL || region->size == 0U) {
        return GFX_ERR_INVALID_ARG;
    }
    if (store->vtable == NULL || store->vtable->open_region == NULL) {
        return GFX_ERR_NOT_SUPPORTED;
    }

    return store->vtable->open_region(store, region, out_view);
}

void gfx_asset_view_close(gfx_asset_view_t *view)
{
    gfx_asset_view_state_base_t *state;
    gfx_asset_store_t *store;

    if (view == NULL || view->priv == NULL) {
        gfx_asset_view_reset(view);
        return;
    }

    state = (gfx_asset_view_state_base_t *)view->priv;
    store = state->store;
    if (store->vtable != NULL && store->vtable->view_close != NULL) {
        store->vtable->view_close(view);
    }
    gfx_asset_view_reset(view);
}
