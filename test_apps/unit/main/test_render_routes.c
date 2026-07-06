/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>

#include "gfx.h"
#include "unity.h"

#include "core/display/gfx_backend_priv.h"
#include "render/gfx_render_priv.h"

typedef struct {
    gfx_backend_t base;
    uint32_t fill_calls;
    uint32_t blit_calls;
    uint32_t blend_calls;
    uint32_t scale_calls;
    uint32_t transform_calls;
    int16_t last_transform_angle;
} test_route_backend_t;

static gfx_err_t test_route_fill(gfx_backend_t *backend, gfx_display_t *disp,
                                 const gfx_backend_surface_t *dst, const gfx_area_t *area,
                                 gfx_color_t color, gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)area;
    (void)color;
    (void)opa;
    ((test_route_backend_t *)backend)->fill_calls++;
    return GFX_OK;
}

static gfx_err_t test_route_blit(gfx_backend_t *backend, gfx_display_t *disp,
                                 const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                 const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_x;
    (void)src_y;
    ((test_route_backend_t *)backend)->blit_calls++;
    return GFX_OK;
}

static gfx_err_t test_route_blend(gfx_backend_t *backend, gfx_display_t *disp,
                                  const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                  const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y,
                                  gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_x;
    (void)src_y;
    (void)opa;
    ((test_route_backend_t *)backend)->blend_calls++;
    return GFX_OK;
}

static gfx_err_t test_route_scale(gfx_backend_t *backend, gfx_display_t *disp,
                                  const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                  const gfx_backend_image_t *src, const gfx_area_t *src_area,
                                  gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_area;
    (void)opa;
    ((test_route_backend_t *)backend)->scale_calls++;
    return GFX_OK;
}

static gfx_err_t test_route_transform(gfx_backend_t *backend, gfx_display_t *disp,
                                      const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                      const gfx_backend_image_t *src, const gfx_area_t *src_area,
                                      int16_t angle, gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_area;
    (void)opa;
    ((test_route_backend_t *)backend)->transform_calls++;
    ((test_route_backend_t *)backend)->last_transform_angle = angle;
    return GFX_OK;
}

static const gfx_backend_ops_t s_test_route_backend_ops = {
    .flush = NULL,
    .wait_flush = NULL,
    .destroy = NULL,
};

static const gfx_draw_ops_t s_test_route_draw_ops = {
    .fill = test_route_fill,
    .blit = test_route_blit,
    .blend = test_route_blend,
    .scale = test_route_scale,
    .transform = test_route_transform,
};

static test_route_backend_t *test_route_backend_create(void)
{
    test_route_backend_t *backend = calloc(1, sizeof(*backend));

    if (backend == NULL) {
        return NULL;
    }

    backend->base.ops = &s_test_route_backend_ops;
    backend->base.draw_ops = &s_test_route_draw_ops;
    backend->base.alignment = gfx_backend_get_alignment(NULL);
    backend->base.caps = GFX_BACKEND_CAP_FILL | GFX_BACKEND_CAP_BLIT | GFX_BACKEND_CAP_BLEND |
                         GFX_BACKEND_CAP_SCALE | GFX_BACKEND_CAP_TRANSFORM;
    return backend;
}

static gfx_handle_t test_core_create(void)
{
    return gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
}

static void test_route_setup_display(test_route_backend_t **backend_out, gfx_handle_t *handle_out,
                                     gfx_display_t **disp_out)
{
    test_route_backend_t *backend = test_route_backend_create();
    gfx_handle_t handle = test_core_create();
    gfx_display_t *disp;

    TEST_ASSERT_NOT_NULL(backend);
    TEST_ASSERT_NOT_NULL(handle);

    disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 64,
        .v_res = 64,
        .backend = &backend->base,
        .buffers = {
            .buf_pixels = 64 * 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);

    *backend_out = backend;
    *handle_out = handle;
    *disp_out = disp;
}

TEST_CASE("render: opaque blit routes to backend blit", "[unit][render][route]")
{
    test_route_backend_t *backend = NULL;
    gfx_handle_t handle = NULL;
    gfx_display_t *disp = NULL;
    gfx_draw_ctx_t ctx;
    static const uint8_t s_pixels[4 * 4 * 2];

    test_route_setup_display(&backend, &handle, &disp);

    ctx = (gfx_draw_ctx_t) {
        .buf = disp->buf.buf1,
        .buf_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .clip_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .stride = 64,
        .format = disp->format.render_format,
        .pixel_size = gfx_color_format_get_size(disp->format.render_format),
    };

    gfx_backend_image_t src = {
        .pixels = s_pixels,
        .stride = 4,
        .format = GFX_COLOR_FORMAT_RGB565,
    };
    gfx_area_t area = { .x1 = 4, .y1 = 2, .x2 = 8, .y2 = 6 };

    TEST_ASSERT_TRUE(gfx_render_backend_image(disp, &ctx, &area, &src, 0, 0, 0xFFU));
    TEST_ASSERT_EQUAL_UINT32(1, backend->blit_calls);
    TEST_ASSERT_EQUAL_UINT32(0, backend->blend_calls);

    gfx_core_deinit(handle);
}

TEST_CASE("render: global alpha routes to backend blend", "[unit][render][route]")
{
    test_route_backend_t *backend = NULL;
    gfx_handle_t handle = NULL;
    gfx_display_t *disp = NULL;
    gfx_draw_ctx_t ctx;
    static const uint8_t s_pixels[4 * 4 * 2];

    test_route_setup_display(&backend, &handle, &disp);

    ctx = (gfx_draw_ctx_t) {
        .buf = disp->buf.buf1,
        .buf_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .clip_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .stride = 64,
        .format = disp->format.render_format,
        .pixel_size = gfx_color_format_get_size(disp->format.render_format),
    };

    gfx_backend_image_t src = {
        .pixels = s_pixels,
        .stride = 4,
        .format = GFX_COLOR_FORMAT_RGB565,
    };
    gfx_area_t area = { .x1 = 4, .y1 = 2, .x2 = 8, .y2 = 6 };

    TEST_ASSERT_TRUE(gfx_render_backend_image(disp, &ctx, &area, &src, 0, 0, 0x80U));
    TEST_ASSERT_EQUAL_UINT32(0, backend->blit_calls);
    TEST_ASSERT_EQUAL_UINT32(1, backend->blend_calls);

    gfx_core_deinit(handle);
}

TEST_CASE("render: scale routes to backend scale", "[unit][render][route]")
{
    test_route_backend_t *backend = NULL;
    gfx_handle_t handle = NULL;
    gfx_display_t *disp = NULL;
    gfx_draw_ctx_t ctx;
    static const uint8_t s_pixels[8 * 8 * 2];

    test_route_setup_display(&backend, &handle, &disp);

    ctx = (gfx_draw_ctx_t) {
        .buf = disp->buf.buf1,
        .buf_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .clip_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .stride = 64,
        .format = disp->format.render_format,
        .pixel_size = gfx_color_format_get_size(disp->format.render_format),
    };

    gfx_backend_image_t src = {
        .pixels = s_pixels,
        .stride = 8,
        .format = GFX_COLOR_FORMAT_RGB565,
    };
    gfx_area_t dst_area = { .x1 = 4, .y1 = 2, .x2 = 12, .y2 = 10 };
    gfx_area_t src_area = { .x1 = 0, .y1 = 0, .x2 = 8, .y2 = 8 };

    TEST_ASSERT_TRUE(gfx_render_backend_scale(disp, &ctx, &dst_area, &ctx.clip_area, &src, &src_area, 0xFFU));
    TEST_ASSERT_EQUAL_UINT32(1, backend->scale_calls);

    gfx_core_deinit(handle);
}

TEST_CASE("render: transform routes to backend transform", "[unit][render][route]")
{
    test_route_backend_t *backend = NULL;
    gfx_handle_t handle = NULL;
    gfx_display_t *disp = NULL;
    gfx_draw_ctx_t ctx;
    static const uint8_t s_pixels[8 * 4 * 2];

    test_route_setup_display(&backend, &handle, &disp);

    ctx = (gfx_draw_ctx_t) {
        .buf = disp->buf.buf1,
        .buf_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .clip_area = { .x1 = 0, .y1 = 0, .x2 = 64, .y2 = 16 },
        .stride = 64,
        .format = disp->format.render_format,
        .pixel_size = gfx_color_format_get_size(disp->format.render_format),
    };

    gfx_backend_image_t src = {
        .pixels = s_pixels,
        .stride = 8,
        .format = GFX_COLOR_FORMAT_RGB565,
    };
    gfx_area_t dst_area = { .x1 = 4, .y1 = 2, .x2 = 8, .y2 = 10 };
    gfx_area_t src_area = { .x1 = 0, .y1 = 0, .x2 = 8, .y2 = 4 };

    TEST_ASSERT_TRUE(gfx_render_backend_transform(disp, &ctx, &dst_area, &ctx.clip_area,
                     &src, &src_area, 90, 0xFFU));
    TEST_ASSERT_EQUAL_UINT32(1, backend->transform_calls);
    TEST_ASSERT_EQUAL_INT16(90, backend->last_transform_angle);

    gfx_core_deinit(handle);
}
