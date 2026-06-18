/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/image.h"
#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/runtime/gfx_core_priv.h"
#include "common/gfx_mesh_frac.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"

typedef struct {
    gfx_backend_t base;
    uint32_t fill_calls;
    uint32_t blit_calls;
    uint32_t blend_calls;
} test_backend_t;

static void expect_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "alignment smoke failed: %s\n", message);
        exit(1);
    }
}

static gfx_err_t test_backend_flush(gfx_backend_t *backend, gfx_display_t *disp,
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

static gfx_err_t test_backend_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;
    (void)disp;
    return GFX_OK;
}

static void test_backend_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static gfx_err_t test_backend_fill(gfx_backend_t *backend, gfx_display_t *disp,
                                   const gfx_backend_surface_t *dst, const gfx_area_t *area,
                                   gfx_color_t color, gfx_opa_t opa)
{
    (void)disp;
    (void)dst;
    (void)area;
    (void)color;
    (void)opa;
    ((test_backend_t *)backend)->fill_calls++;
    return GFX_OK;
}

static gfx_err_t test_backend_blit(gfx_backend_t *backend, gfx_display_t *disp,
                                   const gfx_backend_surface_t *dst, const gfx_area_t *dst_area,
                                   const gfx_backend_image_t *src, gfx_coord_t src_x, gfx_coord_t src_y)
{
    (void)disp;
    (void)dst;
    (void)dst_area;
    (void)src;
    (void)src_x;
    (void)src_y;
    ((test_backend_t *)backend)->blit_calls++;
    return GFX_OK;
}

static gfx_err_t test_backend_blend(gfx_backend_t *backend, gfx_display_t *disp,
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
    ((test_backend_t *)backend)->blend_calls++;
    return GFX_OK;
}

static const gfx_backend_vtable_t s_test_backend_vtable = {
    .flush = test_backend_flush,
    .wait_flush = test_backend_wait_flush,
    .destroy = test_backend_destroy,
};

static const gfx_draw_ops_t s_test_draw_ops = {
    .fill = test_backend_fill,
    .blit = test_backend_blit,
    .blend = test_backend_blend,
};

static test_backend_t *test_backend_create(gfx_render_alignment_t alignment)
{
    test_backend_t *backend = calloc(1, sizeof(*backend));
    if (backend == NULL) {
        return NULL;
    }

    backend->base.vtable = &s_test_backend_vtable;
    backend->base.draw_ops = &s_test_draw_ops;
    backend->base.alignment = alignment;
    backend->base.caps = GFX_BACKEND_CAP_FLUSH | GFX_BACKEND_CAP_FILL |
                         GFX_BACKEND_CAP_BLIT | GFX_BACKEND_CAP_BLEND;
    return backend;
}

static const uint8_t s_rgb565_pixels[8 * 8 * 2] = {0};
static const uint8_t s_rgb565a8_pixels[(8 * 8 * 2) + (8 * 8)] = {0};

static const gfx_image_dsc_t s_rgb565_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 8,
        .h = 8,
        .stride = 16,
    },
    .data_size = sizeof(s_rgb565_pixels),
    .data = s_rgb565_pixels,
};

static const gfx_image_dsc_t s_rgb565a8_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565A8,
        .w = 8,
        .h = 8,
        .stride = 16,
    },
    .data_size = sizeof(s_rgb565a8_pixels),
    .data = s_rgb565a8_pixels,
};

static const uint8_t s_rgb888_precise_pixel[3] = {0x11, 0x22, 0x33};

static const gfx_image_dsc_t s_rgb888_precise_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB888,
        .w = 1,
        .h = 1,
        .stride = 3,
    },
    .data_size = sizeof(s_rgb888_precise_pixel),
    .data = s_rgb888_precise_pixel,
};

static const uint8_t s_rgb565_red_pixel[2] = {0x00, 0xf8};

static const gfx_image_dsc_t s_rgb565_red_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = sizeof(s_rgb565_red_pixel),
    .data = s_rgb565_red_pixel,
};

static const uint32_t s_xrgb8888_precise_pixel[1] = {0xff112233U};

static const gfx_image_dsc_t s_xrgb8888_precise_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_XRGB8888,
        .w = 1,
        .h = 1,
        .stride = 4,
    },
    .data_size = sizeof(s_xrgb8888_precise_pixel),
    .data = (const uint8_t *)s_xrgb8888_precise_pixel,
};

static const uint32_t s_argb8888_half_red_pixel[1] = {0x80ff0000U};

static const gfx_image_dsc_t s_argb8888_half_red_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_ARGB8888,
        .w = 1,
        .h = 1,
        .stride = 4,
    },
    .data_size = sizeof(s_argb8888_half_red_pixel),
    .data = (const uint8_t *)s_argb8888_half_red_pixel,
};

static const uint8_t s_rgb565_gradient_pixels[8] = {
    0x00, 0xf8,
    0xe0, 0x07,
    0x1f, 0x00,
    0xff, 0xff,
};

static const gfx_image_dsc_t s_rgb565_gradient_image = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 2,
        .h = 2,
        .stride = 4,
    },
    .data_size = sizeof(s_rgb565_gradient_pixels),
    .data = s_rgb565_gradient_pixels,
};

static void add_test_image(gfx_display_t *disp, const gfx_image_dsc_t *image, gfx_coord_t x, gfx_coord_t y)
{
    gfx_object_t *obj = gfx_image_create(disp);
    expect_true(obj != NULL, "image create");
    expect_true(gfx_image_set_source_desc(obj, &(gfx_image_src_t) {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = image,
    }) == GFX_OK, "image set source");
    expect_true(gfx_object_set_pos(obj, x, y) == GFX_OK, "image set pos");
}

static gfx_handle_t create_core(void)
{
    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    expect_true(handle != NULL, "core init");
    return handle;
}

static void test_internal_buffer_alignment(void)
{
    gfx_handle_t handle = create_core();
    test_backend_t *backend = test_backend_create((gfx_render_alignment_t) {
        .width_px = 8,
        .height_px = 4,
        .stride_bytes = 16,
        .addr_bytes = 64,
    });
    expect_true(backend != NULL, "backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 32,
        .v_res = 16,
        .backend = &backend->base,
        .buffers = {
            .buf_pixels = 32 * 8,
        },
    });
    expect_true(disp != NULL, "display add with aligned backend");
    expect_true(gfx_render_is_addr_aligned(disp->buf.buf1, &backend->base.alignment), "buf1 address alignment");
    add_test_image(disp, &s_rgb565_image, 8, 4);
    add_test_image(disp, &s_rgb565a8_image, 16, 4);

    gfx_core_refresh_now(handle);
    expect_true(backend->fill_calls > 0, "aligned fill op used");
    expect_true(backend->blit_calls > 0, "aligned image blit op used");
    expect_true(backend->blend_calls > 0, "aligned image blend op used");

    gfx_core_deinit(handle);
}

static void test_external_misaligned_buffer_fallback(void)
{
    gfx_handle_t handle = create_core();
    test_backend_t *backend = test_backend_create((gfx_render_alignment_t) {
        .width_px = 8,
        .height_px = 4,
        .stride_bytes = 16,
        .addr_bytes = 64,
    });
    expect_true(backend != NULL, "backend create for fallback");

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
    expect_true(disp != NULL, "display add with external buffer");
    expect_true(!gfx_render_is_addr_aligned(disp->buf.buf1, &backend->base.alignment), "external buffer is misaligned");
    add_test_image(disp, &s_rgb565_image, 8, 4);

    gfx_core_refresh_now(handle);
    expect_true(backend->fill_calls == 0, "misaligned fill op falls back");
    expect_true(backend->blit_calls == 0, "misaligned blit op falls back");

    gfx_core_deinit(handle);
}

static void test_rgb888_output_flush(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    expect_true(backend != NULL, "rgb888 memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 4,
        },
    });
    expect_true(disp != NULL, "rgb888 display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_RGB888, "display output format is rgb888");
    expect_true(disp->format.render_format == GFX_COLOR_FORMAT_RGB888, "rgb888 display render format is rgb888");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xff0000)) == GFX_OK, "set red background");
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh rgb888 display");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "rgb888 framebuffer data");
    expect_true(gfx_memory_backend_get_color_format(backend) == GFX_COLOR_FORMAT_RGB888, "memory backend rgb888 format");
    expect_true(gfx_memory_backend_get_buffer_bytes(backend) == 2U * 2U * 3U, "rgb888 framebuffer bytes");
    expect_true(buf[0] == 0xff && buf[1] == 0x00 && buf[2] == 0x00, "rgb888 first pixel is red");
    expect_true(buf[9] == 0xff && buf[10] == 0x00 && buf[11] == 0x00, "rgb888 last pixel is red");

    gfx_core_deinit(handle);
}

static void test_bgr888_output_flush(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_BGR888,
    });
    expect_true(backend != NULL, "bgr888 memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 2,
        .color_format = GFX_COLOR_FORMAT_BGR888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 4,
        },
    });
    expect_true(disp != NULL, "bgr888 display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_BGR888, "display output format is bgr888");
    expect_true(disp->format.render_format == GFX_COLOR_FORMAT_BGR888, "bgr888 display render format is bgr888");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xff0000)) == GFX_OK, "set red background on bgr888");
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh bgr888 display");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "bgr888 framebuffer data");
    expect_true(gfx_memory_backend_get_color_format(backend) == GFX_COLOR_FORMAT_BGR888, "memory backend bgr888 format");
    expect_true(gfx_memory_backend_get_buffer_bytes(backend) == 2U * 2U * 3U, "bgr888 framebuffer bytes");
    expect_true(buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0xff, "bgr888 first pixel is red in bgr order");
    expect_true(buf[9] == 0x00 && buf[10] == 0x00 && buf[11] == 0xff, "bgr888 last pixel is red in bgr order");

    gfx_core_deinit(handle);
}

static void test_xrgb8888_output_flush(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
    });
    expect_true(backend != NULL, "xrgb8888 memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "xrgb8888 display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_XRGB8888, "display output format is xrgb8888");
    expect_true(disp->format.render_format == GFX_COLOR_FORMAT_XRGB8888, "xrgb8888 display render format is xrgb8888");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x112233)) == GFX_OK, "set xrgb8888 background");
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh xrgb8888 display");

    const uint32_t *buf = (const uint32_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "xrgb8888 framebuffer data");
    expect_true(gfx_memory_backend_get_color_format(backend) == GFX_COLOR_FORMAT_XRGB8888, "memory backend xrgb8888 format");
    expect_true(gfx_memory_backend_get_buffer_bytes(backend) == 4U, "xrgb8888 framebuffer bytes");
    expect_true(buf[0] == 0xff102031U, "xrgb8888 pixel follows rgb565 semantic fill result");

    gfx_core_deinit(handle);
}

static void test_rgb565_swapped_output_flush(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
    });
    expect_true(backend != NULL, "rgb565 swapped memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "rgb565 swapped display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_RGB565_SWAPPED, "display output format is rgb565 swapped");
    expect_true(disp->format.render_format == GFX_COLOR_FORMAT_RGB565_SWAPPED, "rgb565 swapped display render format matches output");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)) == GFX_OK, "set swapped red background");
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh rgb565 swapped display");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "rgb565 swapped framebuffer data");
    expect_true(gfx_memory_backend_get_color_format(backend) == GFX_COLOR_FORMAT_RGB565_SWAPPED, "memory backend rgb565 swapped format");
    expect_true(gfx_memory_backend_get_buffer_bytes(backend) == 2U, "rgb565 swapped framebuffer bytes");
    expect_true(buf[0] == 0xf8 && buf[1] == 0x00, "rgb565 swapped pixel byte order is high then low");

    gfx_core_deinit(handle);
}

static void test_rgb565_output_keeps_high_byte_first_order(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
    });
    expect_true(backend != NULL, "legacy swap backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "rgb565 display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_RGB565, "rgb565 output remains rgb565");
    expect_true(disp->format.render_format == GFX_COLOR_FORMAT_RGB565, "rgb565 render remains rgb565");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0xf80000)) == GFX_OK, "set rgb565 red background");
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh rgb565 display");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "rgb565 framebuffer data");
    expect_true(gfx_memory_backend_get_color_format(backend) == GFX_COLOR_FORMAT_RGB565, "rgb565 backend format");
    expect_true(gfx_memory_backend_get_buffer_bytes(backend) == 2U, "rgb565 framebuffer bytes");
    expect_true(buf[0] == 0x00 && buf[1] == 0xf8, "rgb565 pixel byte order is low then high");

    gfx_core_deinit(handle);
}

static void test_rgb565_style_and_image_use_same_byte_order(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
    });
    expect_true(backend != NULL, "legacy rgb565 style/image backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .buffers = {
            .buf_pixels = 2,
        },
    });
    expect_true(disp != NULL, "rgb565 style/image display add");
    expect_true(gfx_display_get_color_format(disp) == GFX_COLOR_FORMAT_RGB565, "style/image output is rgb565");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x00ff00)) == GFX_OK, "set rgb565 green background");
    add_test_image(disp, &s_rgb565_red_image, 0, 0);
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh rgb565 style/image display");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "rgb565 style/image framebuffer data");
    expect_true(buf[0] == 0x00 && buf[1] == 0xf8, "rgb565 image pixel is low then high");
    expect_true(buf[2] == 0xe0 && buf[3] == 0x07, "rgb565 style pixel is low then high");

    gfx_core_deinit(handle);
}

static void test_rgb565_swapped_mask_draw_uses_swapped_bytes(void)
{
    uint8_t dest[8] = {0};
    gfx_area_t area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    const gfx_opa_t mask[4] = {0xff, 0x00, 0x80, 0xff};

    gfx_sw_blend_surface_fill(dest, 2, GFX_COLOR_FORMAT_RGB565_SWAPPED, &area, GFX_COLOR_HEX(0x0000ff), 0xff);
    gfx_sw_blend_mask_draw_fmt(dest, 2, GFX_COLOR_FORMAT_RGB565_SWAPPED,
                               mask, 2, &area, GFX_COLOR_HEX(0xf80000), 0xff);

    expect_true(dest[0] == 0xf8 && dest[1] == 0x00, "swapped mask opaque pixel is high then low");
    expect_true(dest[2] == 0x00 && dest[3] == 0x1f, "swapped mask transparent pixel keeps blue");
    expect_true(dest[6] == 0xf8 && dest[7] == 0x00, "swapped mask second opaque pixel is high then low");
}

static void test_rgb565_swapped_scale_draw_preserves_swapped_bytes(void)
{
    uint8_t dest[16] = {0};
    gfx_area_t dst_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t clip_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t src_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};

    gfx_sw_blend_img_scale_draw_fmt(dest, 2, GFX_COLOR_FORMAT_RGB565_SWAPPED,
                                    s_rgb565_gradient_image.data, 2,
                                    NULL, 0,
                                    &dst_area, &clip_area, &src_area,
                                    GFX_COLOR_FORMAT_RGB565, 0xff);

    expect_true(dest[0] == 0xf8 && dest[1] == 0x00, "swapped scale red pixel is high then low");
    expect_true(dest[2] == 0x07 && dest[3] == 0xe0, "swapped scale green pixel is high then low");
    expect_true(dest[4] == 0x00 && dest[5] == 0x1f, "swapped scale blue pixel is high then low");
    expect_true(dest[6] == 0xff && dest[7] == 0xff, "swapped scale white pixel is unchanged");
}

static void test_rgb565_swapped_img_draw_preserves_swapped_bytes(void)
{
    uint8_t dest[8] = {0};
    gfx_area_t area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};

    gfx_sw_blend_img_draw_fmt(dest, 2, GFX_COLOR_FORMAT_RGB565_SWAPPED,
                              s_rgb565_gradient_image.data, 2,
                              NULL, 0,
                              &area, GFX_COLOR_FORMAT_RGB565, 0xff);

    expect_true(dest[0] == 0xf8 && dest[1] == 0x00, "swapped blit red pixel is high then low");
    expect_true(dest[2] == 0x07 && dest[3] == 0xe0, "swapped blit green pixel is high then low");
    expect_true(dest[4] == 0x00 && dest[5] == 0x1f, "swapped blit blue pixel is high then low");
    expect_true(dest[6] == 0xff && dest[7] == 0xff, "swapped blit white pixel is unchanged");
}

static void test_rgb888_image_preserves_24bit_color(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    expect_true(backend != NULL, "rgb888 precise backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "rgb888 precise display add");
    add_test_image(disp, &s_rgb888_precise_image, 0, 0);
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh precise rgb888 image");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "rgb888 precise buffer");
    expect_true(buf[0] == 0x11 && buf[1] == 0x22 && buf[2] == 0x33,
                "rgb888 source preserved without rgb565 quantization");

    gfx_core_deinit(handle);
}

static void test_xrgb8888_image_is_opaque(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    expect_true(backend != NULL, "xrgb backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "xrgb display add");
    add_test_image(disp, &s_xrgb8888_precise_image, 0, 0);
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh xrgb8888 image");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf[0] == 0x11 && buf[1] == 0x22 && buf[2] == 0x33,
                "xrgb8888 source copied as opaque rgb");

    gfx_core_deinit(handle);
}

static void test_xrgb8888_render_output_preserves_precise_rgb_sources(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
    });
    expect_true(backend != NULL, "xrgb8888 precise backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 2,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_XRGB8888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 2,
        },
    });
    expect_true(disp != NULL, "xrgb8888 precise display add");
    add_test_image(disp, &s_rgb888_precise_image, 0, 0);
    add_test_image(disp, &s_xrgb8888_precise_image, 1, 0);
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh xrgb8888 precise images");

    const uint32_t *buf = (const uint32_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf != NULL, "xrgb8888 precise buffer");
    expect_true(buf[0] == 0xff112233U, "rgb888 source preserved in xrgb8888 output");
    expect_true(buf[1] == 0xff112233U, "xrgb8888 source preserved in xrgb8888 output");

    gfx_core_deinit(handle);
}

static void test_xrgb8888_scale_preserves_precise_rgb_sources(void)
{
    uint32_t dest[4] = {0};
    gfx_area_t dst_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t clip_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t src_area = {.x1 = 0, .y1 = 0, .x2 = 1, .y2 = 1};

    gfx_sw_blend_img_scale_draw_fmt(dest, 2, GFX_COLOR_FORMAT_XRGB8888,
                                    s_rgb888_precise_image.data, 1,
                                    NULL, 0,
                                    &dst_area, &clip_area, &src_area,
                                    GFX_COLOR_FORMAT_RGB888, 0xff);

    expect_true(dest[0] == 0xff112233U, "scaled rgb888 source preserved in xrgb8888 output [0]");
    expect_true(dest[1] == 0xff112233U, "scaled rgb888 source preserved in xrgb8888 output [1]");
    expect_true(dest[2] == 0xff112233U, "scaled rgb888 source preserved in xrgb8888 output [2]");
    expect_true(dest[3] == 0xff112233U, "scaled rgb888 source preserved in xrgb8888 output [3]");
}

static void test_xrgb8888_triangle_preserves_precise_rgb_sources(void)
{
    uint32_t dest[4] = {0};
    gfx_area_t buf_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t clip_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_sw_blend_img_vertex_t v0 = {
        .x = 0,
        .y = 0,
        .u = 0,
        .v = 0,
    };
    gfx_sw_blend_img_vertex_t v1 = {
        .x = 2 * GFX_MESH_FRAC_ONE,
        .y = 0,
        .u = 0,
        .v = 0,
    };
    gfx_sw_blend_img_vertex_t v2 = {
        .x = 0,
        .y = 2 * GFX_MESH_FRAC_ONE,
        .u = 0,
        .v = 0,
    };

    gfx_sw_blend_img_triangle_draw(dest, 2, GFX_COLOR_FORMAT_XRGB8888,
                                   &buf_area, &clip_area,
                                   s_rgb888_precise_image.data, 1, 1,
                                   NULL, 0,
                                   0xff,
                                   &v0, &v1, &v2,
                                   0,
                                   NULL, 0,
                                   GFX_COLOR_FORMAT_RGB888);

    expect_true(dest[0] == 0xff112233U, "triangle rgb888 source preserved in xrgb8888 output [0]");
    expect_true(dest[1] == 0xff112233U, "triangle rgb888 source preserved in xrgb8888 output [1]");
    expect_true(dest[2] == 0xff112233U, "triangle rgb888 source preserved in xrgb8888 output [2]");
}

static void test_argb8888_triangle_blends_pixel_alpha(void)
{
    uint8_t dest[12];
    gfx_area_t buf_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_area_t clip_area = {.x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2};
    gfx_sw_blend_img_vertex_t v0 = {
        .x = 0,
        .y = 0,
        .u = 0,
        .v = 0,
    };
    gfx_sw_blend_img_vertex_t v1 = {
        .x = 2 * GFX_MESH_FRAC_ONE,
        .y = 0,
        .u = 0,
        .v = 0,
    };
    gfx_sw_blend_img_vertex_t v2 = {
        .x = 0,
        .y = 2 * GFX_MESH_FRAC_ONE,
        .u = 0,
        .v = 0,
    };

    memset(dest, 0, sizeof(dest));
    gfx_sw_blend_surface_fill(dest, 2, GFX_COLOR_FORMAT_RGB888, &buf_area, GFX_COLOR_HEX(0x0000ff), 0xff);

    gfx_sw_blend_img_triangle_draw(dest, 2, GFX_COLOR_FORMAT_RGB888,
                                   &buf_area, &clip_area,
                                   s_argb8888_half_red_image.data, 1, 1,
                                   NULL, 0,
                                   0xff,
                                   &v0, &v1, &v2,
                                   0,
                                   NULL, 0,
                                   GFX_COLOR_FORMAT_ARGB8888);

    expect_true(dest[0] == 0x80 && dest[1] == 0x00 && dest[2] == 0x7f,
                "triangle argb source alpha blends into rgb888 output [0]");
    expect_true(dest[3] == 0x80 && dest[4] == 0x00 && dest[5] == 0x7f,
                "triangle argb source alpha blends into rgb888 output [1]");
    expect_true(dest[6] == 0x80 && dest[7] == 0x00 && dest[8] == 0x7f,
                "triangle argb source alpha blends into rgb888 output [2]");
}

static void test_argb8888_image_blends_pixel_alpha(void)
{
    gfx_handle_t handle = create_core();
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
    });
    expect_true(backend != NULL, "argb backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 1,
        .v_res = 1,
        .color_format = GFX_COLOR_FORMAT_RGB888,
        .backend = backend,
        .buffers = {
            .buf_pixels = 1,
        },
    });
    expect_true(disp != NULL, "argb display add");
    expect_true(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x0000ff)) == GFX_OK, "set argb blend bg");
    add_test_image(disp, &s_argb8888_half_red_image, 0, 0);
    expect_true(gfx_core_refresh_now(handle) == GFX_OK, "refresh argb8888 image");

    const uint8_t *buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    expect_true(buf[0] == 0x80 && buf[1] == 0x00 && buf[2] == 0x7f,
                "argb8888 pixel alpha blends red over blue");

    gfx_core_deinit(handle);
}

int main(void)
{
    test_internal_buffer_alignment();
    test_external_misaligned_buffer_fallback();
    test_rgb888_output_flush();
    test_bgr888_output_flush();
    test_xrgb8888_output_flush();
    test_rgb565_swapped_output_flush();
    test_rgb565_output_keeps_high_byte_first_order();
    test_rgb565_style_and_image_use_same_byte_order();
    test_rgb565_swapped_mask_draw_uses_swapped_bytes();
    test_rgb565_swapped_img_draw_preserves_swapped_bytes();
    test_rgb565_swapped_scale_draw_preserves_swapped_bytes();
    test_rgb888_image_preserves_24bit_color();
    test_xrgb8888_image_is_opaque();
    test_xrgb8888_render_output_preserves_precise_rgb_sources();
    test_xrgb8888_scale_preserves_precise_rgb_sources();
    test_xrgb8888_triangle_preserves_precise_rgb_sources();
    test_argb8888_image_blends_pixel_alpha();
    test_argb8888_triangle_blends_pixel_alpha();
    printf("alignment smoke: ok\n");
    return 0;
}
