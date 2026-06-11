/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "core/gfx_disp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_disp_backend gfx_disp_backend_t;

typedef struct {
    esp_err_t (*flush)(gfx_disp_backend_t *backend, gfx_disp_t *disp,
                       gfx_coord_t x1, gfx_coord_t y1,
                       gfx_coord_t x2, gfx_coord_t y2,
                       const void *pixels);
    esp_err_t (*wait_flush)(gfx_disp_backend_t *backend, gfx_disp_t *disp);
    void (*destroy)(gfx_disp_backend_t *backend);
} gfx_disp_backend_vtable_t;

struct gfx_disp_backend {
    const gfx_disp_backend_vtable_t *vtable;
    void *user_data;
};

typedef struct {
    gfx_disp_backend_t base;
    gfx_disp_flush_cb_t flush_cb;
} gfx_disp_callback_backend_t;

esp_err_t gfx_disp_backend_flush(gfx_disp_t *disp,
                                 gfx_coord_t x1, gfx_coord_t y1,
                                 gfx_coord_t x2, gfx_coord_t y2,
                                 const void *pixels);
esp_err_t gfx_disp_backend_wait_flush(gfx_disp_t *disp);
void gfx_disp_backend_destroy(gfx_disp_backend_t *backend);

gfx_disp_backend_t *gfx_disp_callback_backend_create(gfx_disp_flush_cb_t flush_cb,
        void *user_data);

#ifdef __cplusplus
}
#endif
