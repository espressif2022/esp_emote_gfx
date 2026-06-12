/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "esp_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/runtime/gfx_core_priv.h"

static const char *const TAG = "disp_backend";

static gfx_err_t gfx_callback_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels)
{
    gfx_callback_backend_t *cb_backend = (gfx_callback_backend_t *)backend;

    ESP_RETURN_ON_FALSE(cb_backend != NULL && disp != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "callback backend flush: invalid args");
    if (cb_backend->flush_cb == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(disp->sync.event_group != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "callback backend flush: event group is NULL");

    gfx_platform_event_clear(disp->sync.event_group, WAIT_FLUSH_DONE);
    cb_backend->flush_cb(disp, x1, y1, x2, y2, pixels);
    return ESP_OK;
}

static gfx_err_t gfx_callback_backend_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    gfx_callback_backend_t *cb_backend = (gfx_callback_backend_t *)backend;

    ESP_RETURN_ON_FALSE(cb_backend != NULL && disp != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "callback backend wait: invalid args");
    if (cb_backend->flush_cb == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(disp->sync.event_group != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "callback backend wait: event group is NULL");

    gfx_platform_event_wait(disp->sync.event_group, WAIT_FLUSH_DONE, true, false, GFX_PLATFORM_WAIT_FOREVER);
    return ESP_OK;
}

static void gfx_callback_backend_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static const gfx_backend_vtable_t s_callback_backend_vtable = {
    .flush = gfx_callback_backend_flush,
    .wait_flush = gfx_callback_backend_wait_flush,
    .destroy = gfx_callback_backend_destroy,
};

gfx_err_t gfx_backend_flush(gfx_display_t *disp,
                            gfx_coord_t x1, gfx_coord_t y1,
                            gfx_coord_t x2, gfx_coord_t y2,
                            const void *pixels)
{
    if (disp == NULL || disp->backend == NULL || disp->backend->vtable == NULL ||
            disp->backend->vtable->flush == NULL) {
        return ESP_OK;
    }
    return disp->backend->vtable->flush(disp->backend, disp, x1, y1, x2, y2, pixels);
}

gfx_err_t gfx_backend_wait_flush(gfx_display_t *disp)
{
    if (disp == NULL || disp->backend == NULL || disp->backend->vtable == NULL ||
            disp->backend->vtable->wait_flush == NULL) {
        return ESP_OK;
    }
    return disp->backend->vtable->wait_flush(disp->backend, disp);
}

void gfx_backend_destroy(gfx_backend_t *backend)
{
    if (backend == NULL) {
        return;
    }
    if (backend->vtable != NULL && backend->vtable->destroy != NULL) {
        backend->vtable->destroy(backend);
        return;
    }
    free(backend);
}

uint32_t gfx_backend_get_caps(const gfx_backend_t *backend)
{
    return backend != NULL ? backend->caps : GFX_BACKEND_CAP_NONE;
}

bool gfx_backend_has_caps(const gfx_backend_t *backend, uint32_t caps)
{
    if (caps == GFX_BACKEND_CAP_NONE) {
        return true;
    }

    return backend != NULL && (backend->caps & caps) == caps;
}

const gfx_draw_ops_t *gfx_backend_get_draw_ops(const gfx_backend_t *backend)
{
    return backend != NULL ? backend->draw_ops : NULL;
}

gfx_backend_t *gfx_callback_backend_create(gfx_display_flush_cb_t flush_cb,
        void *user_data)
{
    gfx_callback_backend_t *backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        GFX_LOGE(TAG, "callback backend create: no mem");
        return NULL;
    }

    backend->base.vtable = &s_callback_backend_vtable;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH;
    backend->base.user_data = user_data;
    backend->flush_cb = flush_cb;
    return &backend->base;
}
