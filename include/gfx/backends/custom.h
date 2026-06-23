/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t width_px;
    uint16_t height_px;
    uint16_t stride_bytes;
    uint16_t addr_bytes;
} gfx_custom_backend_alignment_t;

typedef struct {
    void *buf;
    gfx_area_t area;
    gfx_coord_t stride;
    gfx_color_format_t format;
} gfx_custom_backend_surface_t;

typedef struct {
    const void *pixels;
    gfx_coord_t stride;
    gfx_color_format_t format;
    const gfx_opa_t *alpha;
    gfx_coord_t alpha_stride;
} gfx_custom_backend_image_t;

typedef gfx_err_t (*gfx_custom_backend_flush_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride);
typedef gfx_err_t (*gfx_custom_backend_wait_flush_fn_t)(gfx_backend_t *backend, gfx_display_t *disp);
typedef void (*gfx_custom_backend_destroy_fn_t)(gfx_backend_t *backend);

typedef gfx_err_t (*gfx_custom_backend_fill_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *area,
        gfx_color_t color, gfx_opa_t opa);
typedef gfx_err_t (*gfx_custom_backend_blit_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_custom_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y);
typedef gfx_err_t (*gfx_custom_backend_blend_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_custom_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y,
        gfx_opa_t opa);
typedef gfx_err_t (*gfx_custom_backend_scale_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_custom_backend_image_t *src, const gfx_area_t *src_area,
        gfx_opa_t opa);
typedef gfx_err_t (*gfx_custom_backend_transform_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_custom_backend_image_t *src, const gfx_area_t *src_area,
        int16_t angle, gfx_opa_t opa);
typedef gfx_err_t (*gfx_custom_backend_draw_glyph_fn_t)(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_custom_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        gfx_color_t color, gfx_opa_t opa);
typedef gfx_err_t (*gfx_custom_backend_present_fn_t)(gfx_backend_t *backend, gfx_display_t *disp);

typedef struct {
    gfx_custom_backend_fill_fn_t fill;
    gfx_custom_backend_blit_fn_t blit;
    gfx_custom_backend_blend_fn_t blend;
    gfx_custom_backend_scale_fn_t scale;
    gfx_custom_backend_transform_fn_t transform;
    gfx_custom_backend_draw_glyph_fn_t draw_glyph;
    gfx_custom_backend_present_fn_t present;
} gfx_custom_backend_draw_ops_t;

typedef struct {
    gfx_custom_backend_flush_fn_t flush;
    gfx_custom_backend_wait_flush_fn_t wait_flush;
    gfx_custom_backend_destroy_fn_t destroy;
} gfx_custom_backend_ops_t;

typedef struct {
    const gfx_custom_backend_ops_t *ops;
    const gfx_custom_backend_draw_ops_t *draw_ops;
    gfx_custom_backend_alignment_t alignment;
    uint32_t caps;
    void *user_data;
} gfx_custom_backend_config_t;

gfx_backend_t *gfx_custom_backend_create(const gfx_custom_backend_config_t *cfg);
void gfx_custom_backend_delete(gfx_backend_t *backend);

#ifdef __cplusplus
}
#endif
