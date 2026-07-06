/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_backend gfx_backend_t;

typedef enum {
    GFX_BACKEND_CAP_NONE       = 0,
    GFX_BACKEND_CAP_FLUSH      = 1U << 0,
    GFX_BACKEND_CAP_FILL       = 1U << 1,
    GFX_BACKEND_CAP_BLIT       = 1U << 2,
    GFX_BACKEND_CAP_BLEND      = 1U << 3,
    GFX_BACKEND_CAP_SCALE      = 1U << 4,
    GFX_BACKEND_CAP_TRANSFORM  = 1U << 5,
    GFX_BACKEND_CAP_DRAW_GLYPH = 1U << 6,
    GFX_BACKEND_CAP_PRESENT    = 1U << 7,
} gfx_backend_cap_t;

typedef struct {
    uint16_t width_px;
    uint16_t height_px;
    uint16_t stride_bytes;
    uint16_t addr_bytes;
} gfx_render_alignment_t;

typedef struct {
    void *buf;
    gfx_area_t area;
    gfx_coord_t stride;
    gfx_color_format_t format;
} gfx_backend_surface_t;

typedef struct {
    const void *pixels;
    gfx_coord_t stride;
    gfx_color_format_t format;
    const gfx_opa_t *alpha;
    gfx_coord_t alpha_stride;
} gfx_backend_image_t;

/* Optional backend acceleration hooks. Widget draw callbacks stay separate. */
typedef struct {
    gfx_err_t (*fill)(gfx_backend_t *backend, gfx_display_t *disp,
                      const gfx_backend_surface_t *dst, const gfx_area_t *area,
                      gfx_color_t color, gfx_opa_t opa);
    gfx_err_t (*blit)(gfx_backend_t *backend, gfx_display_t *disp,
                      const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                      const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y);
    gfx_err_t (*blend)(gfx_backend_t *backend, gfx_display_t *disp,
                       const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                       const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y,
                       gfx_opa_t opa);
    gfx_err_t (*scale)(gfx_backend_t *backend, gfx_display_t *disp,
                       const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                       const gfx_backend_image_t *src, const gfx_area_t *src_area,
                       gfx_opa_t opa);
    gfx_err_t (*transform)(gfx_backend_t *backend, gfx_display_t *disp,
                           const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                           const gfx_backend_image_t *src, const gfx_area_t *src_area,
                           int16_t angle, gfx_opa_t opa);
    gfx_err_t (*draw_glyph)(gfx_backend_t *backend, gfx_display_t *disp,
                            const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                            const gfx_opa_t *mask, gfx_coord_t mask_stride,
                            gfx_color_t color, gfx_opa_t opa);
    gfx_err_t (*present)(gfx_backend_t *backend, gfx_display_t *disp);
} gfx_draw_ops_t;

typedef struct {
    gfx_err_t (*flush)(gfx_backend_t *backend, gfx_display_t *disp,
                       gfx_coord_t x1, gfx_coord_t y1,
                       gfx_coord_t x2, gfx_coord_t y2,
                       const void *pixels, gfx_coord_t stride);
    gfx_err_t (*wait_flush)(gfx_backend_t *backend, gfx_display_t *disp);
    void (*destroy)(gfx_backend_t *backend);
} gfx_backend_ops_t;

struct gfx_backend {
    const gfx_backend_ops_t *ops;
    const gfx_draw_ops_t *draw_ops;
    gfx_render_alignment_t alignment;
    uint32_t caps;
    void *user_data;
};

typedef struct {
    gfx_backend_t base;
    gfx_display_flush_cb_t flush_cb;
} gfx_callback_backend_t;

gfx_err_t gfx_backend_flush(gfx_display_t *disp,
                            gfx_coord_t x1, gfx_coord_t y1,
                            gfx_coord_t x2, gfx_coord_t y2,
                            const void *pixels, gfx_coord_t stride);
gfx_err_t gfx_backend_wait_flush(gfx_display_t *disp);
void gfx_backend_destroy(gfx_backend_t *backend);
uint32_t gfx_backend_get_caps(const gfx_backend_t *backend);
bool gfx_backend_has_caps(const gfx_backend_t *backend, uint32_t caps);
const gfx_draw_ops_t *gfx_backend_get_draw_ops(const gfx_backend_t *backend);
gfx_render_alignment_t gfx_backend_get_alignment(const gfx_backend_t *backend);
gfx_backend_t *gfx_backend_create_custom(const gfx_backend_ops_t *ops,
        const gfx_draw_ops_t *draw_ops,
        gfx_render_alignment_t alignment,
        uint32_t caps,
        void *user_data);

gfx_backend_t *gfx_callback_backend_create(gfx_display_flush_cb_t flush_cb,
        void *user_data);

#ifdef __cplusplus
}
#endif
