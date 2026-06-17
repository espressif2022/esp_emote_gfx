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
#include "platform/gfx_platform_accel_priv.h"

static const char *const TAG = "disp_backend";

static gfx_err_t gfx_callback_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    gfx_callback_backend_t *cb_backend = (gfx_callback_backend_t *)backend;

    ESP_RETURN_ON_FALSE(cb_backend != NULL && disp != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "callback backend flush: invalid args");
    if (cb_backend->flush_cb == NULL) {
        return ESP_OK;
    }
    gfx_coord_t expected_stride = disp->flags.full_frame ? (gfx_coord_t)disp->res.h_res : (x2 - x1);
    ESP_RETURN_ON_FALSE(stride == expected_stride, ESP_ERR_NOT_SUPPORTED,
                        TAG, "callback backend flush: aligned stride is not supported by legacy flush_cb");
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

static gfx_render_alignment_t gfx_backend_merge_alignment(gfx_render_alignment_t base,
        gfx_render_alignment_t extra)
{
    if (extra.width_px > base.width_px) {
        base.width_px = extra.width_px;
    }
    if (extra.height_px > base.height_px) {
        base.height_px = extra.height_px;
    }
    if (extra.stride_bytes > base.stride_bytes) {
        base.stride_bytes = extra.stride_bytes;
    }
    if (extra.addr_bytes > base.addr_bytes) {
        base.addr_bytes = extra.addr_bytes;
    }
    return base;
}

gfx_err_t gfx_backend_flush(gfx_display_t *disp,
                            gfx_coord_t x1, gfx_coord_t y1,
                            gfx_coord_t x2, gfx_coord_t y2,
                            const void *pixels, gfx_coord_t stride)
{
    if (disp == NULL || disp->backend == NULL || disp->backend->vtable == NULL ||
            disp->backend->vtable->flush == NULL) {
        return ESP_OK;
    }
    return disp->backend->vtable->flush(disp->backend, disp, x1, y1, x2, y2, pixels, stride);
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

gfx_render_alignment_t gfx_backend_get_alignment(const gfx_backend_t *backend)
{
    gfx_render_alignment_t alignment = {
        .width_px = 1,
        .height_px = 1,
        .stride_bytes = 1,
        .addr_bytes = 1,
    };

    if (backend == NULL) {
        return alignment;
    }

    if (backend->alignment.width_px > 0U) {
        alignment.width_px = backend->alignment.width_px;
    }
    if (backend->alignment.height_px > 0U) {
        alignment.height_px = backend->alignment.height_px;
    }
    if (backend->alignment.stride_bytes > 0U) {
        alignment.stride_bytes = backend->alignment.stride_bytes;
    }
    if (backend->alignment.addr_bytes > 0U) {
        alignment.addr_bytes = backend->alignment.addr_bytes;
    }

    return alignment;
}

gfx_backend_t *gfx_backend_create_custom(const gfx_backend_vtable_t *vtable,
        const gfx_draw_ops_t *draw_ops,
        gfx_render_alignment_t alignment,
        uint32_t caps,
        void *user_data)
{
    gfx_backend_t *backend;

    if (vtable == NULL && draw_ops == NULL) {
        GFX_LOGE(TAG, "custom backend create: vtable and draw_ops are NULL");
        return NULL;
    }

    backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        GFX_LOGE(TAG, "custom backend create: no mem");
        return NULL;
    }

    backend->vtable = vtable;
    backend->draw_ops = draw_ops;
    backend->alignment = gfx_backend_get_alignment(&(gfx_backend_t) {
        .alignment = alignment,
    });
    backend->caps = caps;
    backend->user_data = user_data;

    return backend;
}

gfx_backend_t *gfx_callback_backend_create(gfx_display_flush_cb_t flush_cb,
        void *user_data)
{
    gfx_callback_backend_t *backend = calloc(1, sizeof(*backend));
    const gfx_draw_ops_t *draw_ops = gfx_platform_accel_get_draw_ops();
    uint32_t draw_caps = gfx_platform_accel_get_caps();
    if (backend == NULL) {
        GFX_LOGE(TAG, "callback backend create: no mem");
        return NULL;
    }

    backend->base.vtable = &s_callback_backend_vtable;
    backend->base.draw_ops = draw_ops;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH | draw_caps;
    backend->base.alignment = gfx_backend_merge_alignment(gfx_backend_get_alignment(NULL),
                              gfx_platform_accel_get_alignment());
    backend->base.user_data = user_data;
    backend->flush_cb = flush_cb;
    return &backend->base;
}
