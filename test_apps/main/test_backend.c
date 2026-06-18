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
#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "gfx/widgets/image.h"
#include "render/gfx_render_priv.h"

typedef struct {
    gfx_backend_t base;
    uint32_t fill_calls;
    uint32_t blit_calls;
    uint32_t blend_calls;
} test_aligned_backend_t;

typedef struct {
    uint32_t calls;
    gfx_coord_t x1;
    gfx_coord_t y1;
    gfx_coord_t x2;
    gfx_coord_t y2;
    const void *data;
    gfx_coord_t last_stride;
} test_flush_state_t;

typedef struct {
    gfx_backend_t base;
    void *last_pixels;
    gfx_coord_t last_stride;
    gfx_coord_t last_x1;
    gfx_coord_t last_y1;
    gfx_coord_t last_x2;
    gfx_coord_t last_y2;
    uint32_t flush_calls;
} test_capture_backend_t;

static gfx_err_t test_aligned_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    (void)backend;
    (void)disp;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)pixels;
    (void)stride;
    return GFX_OK;
}

static gfx_err_t test_aligned_backend_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;
    (void)disp;
    return GFX_OK;
}

static void test_aligned_backend_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static gfx_err_t test_aligned_backend_fill(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_backend_surface_t *dst, const gfx_area_t *area,
        gfx_color_t color, gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)area;
    (void)color;
    (void)opa;
    ((test_aligned_backend_t *)backend)->fill_calls++;
    return GFX_OK;
}

static gfx_err_t test_aligned_backend_blit(gfx_backend_t *backend, gfx_display_t *disp,
        const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
        const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_x;
    (void)src_y;
    ((test_aligned_backend_t *)backend)->blit_calls++;
    return GFX_OK;
}

static gfx_err_t test_aligned_backend_blend(gfx_backend_t *backend, gfx_display_t *disp,
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
    ((test_aligned_backend_t *)backend)->blend_calls++;
    return GFX_OK;
}

static const gfx_backend_vtable_t s_test_aligned_backend_vtable = {
    .flush = test_aligned_backend_flush,
    .wait_flush = test_aligned_backend_wait_flush,
    .destroy = test_aligned_backend_destroy,
};

static const gfx_draw_ops_t s_test_aligned_draw_ops = {
    .fill = test_aligned_backend_fill,
    .blit = test_aligned_backend_blit,
    .blend = test_aligned_backend_blend,
};

static test_aligned_backend_t *test_aligned_backend_create(gfx_render_alignment_t alignment)
{
    test_aligned_backend_t *backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        return NULL;
    }

    backend->base.vtable = &s_test_aligned_backend_vtable;
    backend->base.draw_ops = &s_test_aligned_draw_ops;
    backend->base.alignment = alignment;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH | GFX_BACKEND_CAP_FILL |
                         GFX_BACKEND_CAP_BLIT | GFX_BACKEND_CAP_BLEND;
    return backend;
}

static const uint8_t s_test_rgb565_pixels[8 * 8 * 2] = {0};
static const uint8_t s_test_rgb565a8_pixels[(8 * 8 * 2) + (8 * 8)] = {0};

static const gfx_image_dsc_t s_test_rgb565_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 8,
        .h = 8,
        .stride = 16,
    },
    .data_size = sizeof(s_test_rgb565_pixels),
    .data = s_test_rgb565_pixels,
};

static const gfx_image_dsc_t s_test_rgb565a8_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565A8,
        .w = 8,
        .h = 8,
        .stride = 16,
    },
    .data_size = sizeof(s_test_rgb565a8_pixels),
    .data = s_test_rgb565a8_pixels,
};

static gfx_handle_t test_core_create(void)
{
    return gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
}

static bool test_add_image(gfx_display_t *disp, const gfx_image_dsc_t *image, gfx_coord_t x, gfx_coord_t y)
{
    gfx_object_t *obj = gfx_image_create(disp);
    if (obj == NULL) {
        return false;
    }
    if (gfx_image_set_source_desc(obj, &(gfx_image_src_t) {
    .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
    .data = image,
}) != GFX_OK) {
        return false;
    }
    return gfx_object_set_pos(obj, x, y) == GFX_OK;
}

static void test_legacy_flush_cb(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1,
                                 gfx_coord_t x2, gfx_coord_t y2, const void *data)
{
    test_flush_state_t *state = (test_flush_state_t *)gfx_display_get_user_data(disp);
    if (state != NULL) {
        state->calls++;
        state->x1 = x1;
        state->y1 = y1;
        state->x2 = x2;
        state->y2 = y2;
        state->data = data;
    }
    gfx_display_flush_ready(disp, true);
}

typedef struct {
    gfx_backend_t base;
    test_flush_state_t *state;
} test_stride_backend_t;

static gfx_err_t test_stride_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    test_stride_backend_t *state_backend = (test_stride_backend_t *)backend;

    (void)disp;
    if (state_backend != NULL && state_backend->state != NULL) {
        state_backend->state->calls++;
        state_backend->state->x1 = x1;
        state_backend->state->y1 = y1;
        state_backend->state->x2 = x2;
        state_backend->state->y2 = y2;
        state_backend->state->data = pixels;
        state_backend->state->last_stride = stride;
    }
    return GFX_OK;
}

static gfx_err_t test_stride_backend_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;
    (void)disp;
    return GFX_OK;
}

static void test_stride_backend_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static gfx_backend_t *test_stride_backend_create(test_flush_state_t *state)
{
    static const gfx_backend_vtable_t s_stride_backend_vtable = {
        .flush = test_stride_backend_flush,
        .wait_flush = test_stride_backend_wait_flush,
        .destroy = test_stride_backend_destroy,
    };
    test_stride_backend_t *backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        return NULL;
    }

    backend->base.vtable = &s_stride_backend_vtable;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH;
    backend->base.alignment = gfx_backend_get_alignment(NULL);
    backend->state = state;
    return &backend->base;
}

static void test_expect_rgb888_pixel_bytes(const uint8_t *buf, uint16_t width, gfx_color_format_t format,
        uint16_t x, uint16_t y, uint8_t c0, uint8_t c1, uint8_t c2)
{
    const uint8_t *px = buf + (((size_t)y * width) + x) * 3U;

    TEST_ASSERT_TRUE(format == GFX_COLOR_FORMAT_RGB888 || format == GFX_COLOR_FORMAT_BGR888);
    TEST_ASSERT_EQUAL_HEX8(c0, px[0]);
    TEST_ASSERT_EQUAL_HEX8(c1, px[1]);
    TEST_ASSERT_EQUAL_HEX8(c2, px[2]);
}

static gfx_err_t test_capture_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    (void)disp;
    test_capture_backend_t *capture = (test_capture_backend_t *)backend;
    if (capture != NULL) {
        capture->last_pixels = (void *)pixels;
        capture->last_stride = stride;
        capture->last_x1 = x1;
        capture->last_y1 = y1;
        capture->last_x2 = x2;
        capture->last_y2 = y2;
        capture->flush_calls++;
    }
    return GFX_OK;
}

static gfx_err_t test_capture_backend_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;
    (void)disp;
    return GFX_OK;
}

static void test_capture_backend_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static gfx_backend_t *test_capture_backend_create(void)
{
    static const gfx_backend_vtable_t s_capture_backend_vtable = {
        .flush = test_capture_backend_flush,
        .wait_flush = test_capture_backend_wait_flush,
        .destroy = test_capture_backend_destroy,
    };
    test_capture_backend_t *backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        return NULL;
    }

    backend->base.vtable = &s_capture_backend_vtable;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH;
    backend->base.alignment = gfx_backend_get_alignment(NULL);
    return &backend->base;
}

TEST_CASE("backend: memory framebuffer lifecycle", "[backend]")
{
    uint16_t ext_buf[4] = {0};
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .buffer = ext_buf,
        .buffer_pixels = 4,
    });
    TEST_ASSERT_NOT_NULL(backend);

    TEST_ASSERT_EQUAL(4, gfx_memory_backend_get_buffer_pixels(backend));
    TEST_ASSERT_EQUAL_PTR(ext_buf, gfx_memory_backend_get_buffer(backend));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_memory_backend_clear(backend, GFX_COLOR_HEX(0x00ff00)));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, ext_buf[0]);
    TEST_ASSERT_EQUAL_HEX16(0x07e0, ext_buf[3]);

    gfx_memory_backend_delete(backend);
}

TEST_CASE("backend: RGB888 display output flushes RGB888 bytes", "[backend][format]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 4,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB888, gfx_display_get_color_format(disp));
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB888, disp->format.render_format);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xff0000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB888, gfx_memory_backend_get_color_format(backend));
    TEST_ASSERT_EQUAL(2 * 2 * 3, gfx_memory_backend_get_buffer_bytes(backend));
    TEST_ASSERT_EQUAL_HEX8(0xff, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0xff, buf[9]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[10]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[11]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: BGR888 display output flushes BGR888 bytes", "[backend][format]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_BGR888,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_BGR888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 4,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_BGR888, gfx_display_get_color_format(disp));
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_BGR888, disp->format.render_format);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xff0000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_BGR888, gfx_memory_backend_get_color_format(backend));
    TEST_ASSERT_EQUAL(2 * 2 * 3, gfx_memory_backend_get_buffer_bytes(backend));
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0xff, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[9]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[10]);
    TEST_ASSERT_EQUAL_HEX8(0xff, buf[11]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: XRGB8888 display output flushes XRGB8888 pixels", "[backend][format]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_XRGB8888, gfx_display_get_color_format(disp));
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_XRGB8888, disp->format.render_format);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x112233)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint32_t *buf = (const uint32_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_XRGB8888, gfx_memory_backend_get_color_format(backend));
    TEST_ASSERT_EQUAL(4, gfx_memory_backend_get_buffer_bytes(backend));
    TEST_ASSERT_EQUAL_HEX32(0xff102031, buf[0]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: RGB565_SWAPPED display output keeps high-byte-first semantic order", "[backend][format]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565_SWAPPED, gfx_display_get_color_format(disp));
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565_SWAPPED, disp->format.render_format);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565_SWAPPED, gfx_memory_backend_get_color_format(backend));
    TEST_ASSERT_EQUAL(2, gfx_memory_backend_get_buffer_bytes(backend));
    TEST_ASSERT_EQUAL_HEX8(0xf8, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: explicit RGB565 output keeps low-byte-first order", "[backend][format]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565, gfx_display_get_color_format(disp));
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565, disp->format.render_format);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL(GFX_COLOR_FORMAT_RGB565, gfx_memory_backend_get_color_format(backend));
    TEST_ASSERT_EQUAL(2, gfx_memory_backend_get_buffer_bytes(backend));
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xf8, buf[1]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: ESP-IDF aligned render buffer enables draw ops", "[backend][alignment]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    test_aligned_backend_t *backend = test_aligned_backend_create((gfx_render_alignment_t) {
        .width_px = 8,
        .height_px = 4,
        .stride_bytes = 16,
        .addr_bytes = 64,
    });
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 32,
        .v_res = 16,
        .backend = &backend->base,
        .buffers = {
            .buf_pixels = 32 * 8,
        },
    });

    bool display_created = disp != NULL;
    bool buffer_aligned = display_created &&
                          gfx_render_is_addr_aligned(disp->buf.buf1, &backend->base.alignment);
    bool rgb565_added = display_created && test_add_image(disp, &s_test_rgb565_image, 8, 4);
    bool rgb565a8_added = display_created && test_add_image(disp, &s_test_rgb565a8_image, 16, 4);
    gfx_err_t refresh_ret = display_created ? gfx_core_refresh_now(handle) : GFX_ERR_INVALID_STATE;
    uint32_t fill_calls = backend->fill_calls;
    uint32_t blit_calls = backend->blit_calls;
    uint32_t blend_calls = backend->blend_calls;

    gfx_core_deinit(handle);

    TEST_ASSERT_TRUE(display_created);
    TEST_ASSERT_TRUE(buffer_aligned);
    TEST_ASSERT_TRUE(rgb565_added);
    TEST_ASSERT_TRUE(rgb565a8_added);
    TEST_ASSERT_EQUAL(GFX_OK, refresh_ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, fill_calls);
    TEST_ASSERT_GREATER_THAN_UINT32(0, blit_calls);
    TEST_ASSERT_GREATER_THAN_UINT32(0, blend_calls);
}

TEST_CASE("backend: ESP-IDF external misaligned buffer falls back to software", "[backend][alignment]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    test_aligned_backend_t *backend = test_aligned_backend_create((gfx_render_alignment_t) {
        .width_px = 8,
        .height_px = 4,
        .stride_bytes = 16,
        .addr_bytes = 64,
    });
    TEST_ASSERT_NOT_NULL(backend);

    uint8_t raw[(32 * 8 * sizeof(uint16_t)) + 64];
    uintptr_t raw_addr = (uintptr_t)raw;
    uint16_t *misaligned = (uint16_t *)((raw_addr + 2U) & ~(uintptr_t)1U);
    if (((uintptr_t)misaligned % 64U) == 0U) {
        misaligned = (uint16_t *)((uint8_t *)misaligned + 2U);
    }

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 32,
        .v_res = 16,
        .backend = &backend->base,
        .buffers = {
            .buf1 = misaligned,
            .buf_pixels = 32 * 8,
        },
    });

    bool display_created = disp != NULL;
    bool buffer_misaligned = display_created &&
                             !gfx_render_is_addr_aligned(disp->buf.buf1, &backend->base.alignment);
    bool image_added = display_created && test_add_image(disp, &s_test_rgb565_image, 8, 4);
    gfx_err_t refresh_ret = display_created ? gfx_core_refresh_now(handle) : GFX_ERR_INVALID_STATE;
    uint32_t fill_calls = backend->fill_calls;
    uint32_t blit_calls = backend->blit_calls;

    gfx_core_deinit(handle);

    TEST_ASSERT_TRUE(display_created);
    TEST_ASSERT_TRUE(buffer_misaligned);
    TEST_ASSERT_TRUE(image_added);
    TEST_ASSERT_EQUAL(GFX_OK, refresh_ret);
    TEST_ASSERT_EQUAL_UINT32(0, fill_calls);
    TEST_ASSERT_EQUAL_UINT32(0, blit_calls);
}

TEST_CASE("backend: legacy flush callback accepts compact render stride", "[backend][flush_cb]")
{
    gfx_handle_t handle = test_core_create();
    TEST_ASSERT_NOT_NULL(handle);

    test_flush_state_t flush_state = {0};
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 8,
        .v_res = 8,
        .flush_cb = test_legacy_flush_cb,
        .user_data = &flush_state,
        .buffers = {
            .buf_pixels = 8 * 4,
        },
    });

    bool display_created = disp != NULL;
    gfx_err_t refresh_ret = display_created ? gfx_core_refresh_now(handle) : GFX_ERR_INVALID_STATE;
    uint32_t calls = flush_state.calls;
    gfx_coord_t x1 = flush_state.x1;
    gfx_coord_t y1 = flush_state.y1;
    gfx_coord_t x2 = flush_state.x2;
    gfx_coord_t y2 = flush_state.y2;
    const void *data = flush_state.data;

    gfx_core_deinit(handle);

    TEST_ASSERT_TRUE(display_created);
    TEST_ASSERT_EQUAL(GFX_OK, refresh_ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, calls);
    TEST_ASSERT_EQUAL(0, x1);
    TEST_ASSERT_EQUAL(0, y1);
    TEST_ASSERT_EQUAL(8, x2 - x1);
    TEST_ASSERT_LESS_OR_EQUAL(4, y2 - y1);
    TEST_ASSERT_NOT_NULL(data);
}

TEST_CASE("backend: full-frame RGB565 flush uses screen stride and frame origin", "[backend][flush][format]")
{
    gfx_handle_t handle = test_core_create();
    test_flush_state_t flush_state = {0};
    gfx_backend_t *backend = test_stride_backend_create(&flush_state);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 4,
        .v_res = 4,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .flags = {
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)flush_state.data;
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN_UINT32(0, flush_state.calls);
    TEST_ASSERT_EQUAL(4, flush_state.last_stride);
    TEST_ASSERT_EQUAL_PTR(disp->buf.buf1, flush_state.data);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xf8, buf[1]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: full-frame RGB565_SWAPPED flush uses screen stride and high-byte-first semantic bytes", "[backend][flush][format]")
{
    gfx_handle_t handle = test_core_create();
    test_flush_state_t flush_state = {0};
    gfx_backend_t *backend = test_stride_backend_create(&flush_state);
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 4,
        .v_res = 4,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
        .backend = backend,
        .flags = {
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    const uint8_t *buf = (const uint8_t *)flush_state.data;
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN_UINT32(0, flush_state.calls);
    TEST_ASSERT_EQUAL(4, flush_state.last_stride);
    TEST_ASSERT_EQUAL_PTR(disp->buf.buf1, flush_state.data);
    TEST_ASSERT_EQUAL_HEX8(0xf8, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: full-frame double buffer redraws full screen from partial invalidation", "[backend][flush][full_frame]")
{
    gfx_handle_t handle = test_core_create();
    gfx_backend_t *backend = test_capture_backend_create();
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    test_capture_backend_t *capture = (test_capture_backend_t *)backend;
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 4,
        .v_res = 4,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .flags = {
            .double_buffer = true,
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_NOT_NULL(disp->buf.buf1);
    TEST_ASSERT_NOT_NULL(disp->buf.buf2);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x000000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 0, .y1 = 0, .x2 = 1, .y2 = 1,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf2, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf2 = (const uint8_t *)capture->last_pixels;
    TEST_ASSERT_EQUAL_HEX16(0xf800, gfx_color_read_rgb565_bytes(buf2 + 0, GFX_COLOR_FORMAT_RGB565));
    TEST_ASSERT_EQUAL_HEX16(0xf800, gfx_color_read_rgb565_bytes(buf2 + ((3 * 4 + 3) * 2), GFX_COLOR_FORMAT_RGB565));

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 2, .y1 = 2, .x2 = 3, .y2 = 3,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x001f00)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf1, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf1 = (const uint8_t *)capture->last_pixels;
    TEST_ASSERT_EQUAL_HEX16(0x07e0, gfx_color_read_rgb565_bytes(buf1 + 0, GFX_COLOR_FORMAT_RGB565));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, gfx_color_read_rgb565_bytes(buf1 + ((3 * 4 + 3) * 2), GFX_COLOR_FORMAT_RGB565));

    gfx_core_deinit(handle);
}

TEST_CASE("backend: full-frame double buffer redraws full screen from partial invalidation for rgb565 swapped", "[backend][flush][full_frame]")
{
    gfx_handle_t handle = test_core_create();
    gfx_backend_t *backend = test_capture_backend_create();
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    test_capture_backend_t *capture = (test_capture_backend_t *)backend;
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 4,
        .v_res = 4,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
        .backend = backend,
        .flags = {
            .double_buffer = true,
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_NOT_NULL(disp->buf.buf1);
    TEST_ASSERT_NOT_NULL(disp->buf.buf2);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x000000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 0, .y1 = 0, .x2 = 1, .y2 = 1,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf2, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf2 = (const uint8_t *)capture->last_pixels;
    TEST_ASSERT_EQUAL_HEX16(0xf800, gfx_color_read_rgb565_bytes(buf2 + 0, GFX_COLOR_FORMAT_RGB565_SWAPPED));
    TEST_ASSERT_EQUAL_HEX16(0xf800, gfx_color_read_rgb565_bytes(buf2 + ((3 * 4 + 3) * 2), GFX_COLOR_FORMAT_RGB565_SWAPPED));

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 2, .y1 = 2, .x2 = 3, .y2 = 3,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x001f00)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf1, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf1 = (const uint8_t *)capture->last_pixels;
    TEST_ASSERT_EQUAL_HEX16(0x07e0, gfx_color_read_rgb565_bytes(buf1 + 0, GFX_COLOR_FORMAT_RGB565_SWAPPED));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, gfx_color_read_rgb565_bytes(buf1 + ((3 * 4 + 3) * 2), GFX_COLOR_FORMAT_RGB565_SWAPPED));

    gfx_core_deinit(handle);
}

TEST_CASE("backend: full-frame double buffer redraws full screen from partial invalidation for rgb888", "[backend][flush][full_frame]")
{
    gfx_handle_t handle = test_core_create();
    gfx_backend_t *backend = test_capture_backend_create();
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    test_capture_backend_t *capture = (test_capture_backend_t *)backend;
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 4,
        .v_res = 4,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .flags = {
            .double_buffer = true,
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 16,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_NOT_NULL(disp->buf.buf1);
    TEST_ASSERT_NOT_NULL(disp->buf.buf2);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x000000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 0, .y1 = 0, .x2 = 1, .y2 = 1,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xff0000)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf2, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf2 = (const uint8_t *)capture->last_pixels;
    test_expect_rgb888_pixel_bytes(buf2, 4, GFX_COLOR_FORMAT_RGB888, 0, 0, 0xff, 0x00, 0x00);
    test_expect_rgb888_pixel_bytes(buf2, 4, GFX_COLOR_FORMAT_RGB888, 3, 3, 0xff, 0x00, 0x00);

    gfx_invalidate_area_disp(disp, &(gfx_area_t) {
        .x1 = 2, .y1 = 2, .x2 = 3, .y2 = 3,
    });
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x00ff00)));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));

    TEST_ASSERT_EQUAL_PTR(disp->buf.buf1, capture->last_pixels);
    TEST_ASSERT_EQUAL(4, capture->last_stride);
    TEST_ASSERT_EQUAL(0, capture->last_x1);
    TEST_ASSERT_EQUAL(0, capture->last_y1);
    TEST_ASSERT_EQUAL(4, capture->last_x2);
    TEST_ASSERT_EQUAL(4, capture->last_y2);
    const uint8_t *buf1 = (const uint8_t *)capture->last_pixels;
    test_expect_rgb888_pixel_bytes(buf1, 4, GFX_COLOR_FORMAT_RGB888, 0, 0, 0x00, 0xff, 0x00);
    test_expect_rgb888_pixel_bytes(buf1, 4, GFX_COLOR_FORMAT_RGB888, 3, 3, 0x00, 0xff, 0x00);

    gfx_core_deinit(handle);
}

TEST_CASE("backend: repeated visibility no-op does not flush", "[backend][object][flush]")
{
    gfx_handle_t handle = test_core_create();
    gfx_backend_t *backend = test_capture_backend_create();
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_NOT_NULL(backend);

    test_capture_backend_t *capture = (test_capture_backend_t *)backend;
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 8,
        .v_res = 8,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .flags = {
            .full_frame = true,
        },
        .buffers = {
            .buf_pixels = 64,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);

    gfx_object_t *obj = gfx_image_create(disp);
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_image_set_source_desc(obj, &(gfx_image_src_t) {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &s_test_rgb565_image,
    }));

    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));
    uint32_t flush_after_setup = capture->flush_calls;
    TEST_ASSERT_GREATER_THAN_UINT32(0, flush_after_setup);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_visible(obj, false));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));
    uint32_t flush_after_hide = capture->flush_calls;
    TEST_ASSERT_EQUAL_UINT32(flush_after_setup + 1U, flush_after_hide);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_visible(obj, false));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(handle));
    TEST_ASSERT_EQUAL_UINT32(flush_after_hide, capture->flush_calls);

    gfx_core_deinit(handle);
}
