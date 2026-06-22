/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "gfx/backends/memory.h"
#include "common/gfx_types_priv.h"
#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "platform/gfx_platform.h"
#include "platform/gfx_platform_accel_priv.h"

struct gfx_memory_backend {
    gfx_backend_t base;
    uint8_t *buffer;
    uint32_t h_res;
    uint32_t v_res;
    gfx_color_format_t format;
    uint8_t pixel_size;
    bool ext_buffer;
};

static const char *const TAG = "memory_backend";

static gfx_render_alignment_t gfx_memory_backend_merge_alignment(gfx_render_alignment_t base,
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

static gfx_color_format_t gfx_memory_backend_resolve_format(const gfx_memory_backend_config_t *cfg)
{
    if (cfg == NULL || cfg->color_format == GFX_COLOR_FORMAT_UNKNOWN ||
            cfg->color_format == GFX_COLOR_FORMAT_NATIVE) {
        return GFX_COLOR_FORMAT_RGB565;
    }

    return cfg->color_format;
}

static uint16_t gfx_memory_backend_read_rgb565_semantic(const uint8_t *src,
        gfx_color_format_t format)
{
    if (format == GFX_COLOR_FORMAT_RGB888 || format == GFX_COLOR_FORMAT_BGR888) {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        gfx_color_read_rgb888_bytes(src, format, &r, &g, &b);
        return gfx_color_from_rgb888(r, g, b).full;
    } else if (format == GFX_COLOR_FORMAT_XRGB8888 || format == GFX_COLOR_FORMAT_ARGB8888) {
        uint32_t px;
        memcpy(&px, src, sizeof(px));
        return gfx_color_from_rgb888((uint8_t)((px >> 16) & 0xffU),
                                     (uint8_t)((px >> 8) & 0xffU),
                                     (uint8_t)(px & 0xffU)).full;
    }

    return gfx_color_read_rgb565_bytes(src, format);
}

static void gfx_memory_backend_write_pixel(uint8_t *dst, gfx_color_format_t format,
        uint16_t rgb565)
{
    if (format == GFX_COLOR_FORMAT_RGB888 || format == GFX_COLOR_FORMAT_BGR888) {
        uint32_t r = (rgb565 >> 11) & 0x1fU;
        uint32_t g = (rgb565 >> 5) & 0x3fU;
        uint32_t b = rgb565 & 0x1fU;

        gfx_color_write_rgb888_bytes(dst, format,
                                     (uint8_t)((r << 3) | (r >> 2)),
                                     (uint8_t)((g << 2) | (g >> 4)),
                                     (uint8_t)((b << 3) | (b >> 2)));
        return;
    } else if (format == GFX_COLOR_FORMAT_XRGB8888 || format == GFX_COLOR_FORMAT_ARGB8888) {
        uint32_t r = (rgb565 >> 11) & 0x1fU;
        uint32_t g = (rgb565 >> 5) & 0x3fU;
        uint32_t b = rgb565 & 0x1fU;
        uint32_t px = 0xff000000U |
                      (((r << 3) | (r >> 2)) << 16) |
                      (((g << 2) | (g >> 4)) << 8) |
                      ((b << 3) | (b >> 2));
        memcpy(dst, &px, sizeof(px));
        return;
    }

    gfx_color_write_rgb565_bytes(dst, format, rgb565);
}

static gfx_err_t gfx_memory_backend_flush_impl(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    const uint8_t *src = (const uint8_t *)pixels;

    GFX_RETURN_ON_FALSE(mem != NULL && src != NULL, GFX_ERR_INVALID_ARG, TAG, "flush: invalid args");
    GFX_RETURN_ON_FALSE(x1 >= 0 && y1 >= 0 && x2 >= x1 && y2 >= y1,
                        GFX_ERR_INVALID_ARG, TAG, "flush: invalid area");
    GFX_RETURN_ON_FALSE((uint32_t)x2 <= mem->h_res && (uint32_t)y2 <= mem->v_res,
                        GFX_ERR_INVALID_ARG, TAG, "flush: area out of range");

    uint32_t w = (uint32_t)(x2 - x1);
    uint32_t h = (uint32_t)(y2 - y1);
    uint32_t src_stride = stride > 0 ? (uint32_t)stride : w;
    size_t src_offset = 0;
    gfx_color_format_t src_format = disp != NULL ? disp->format.output_format : GFX_COLOR_FORMAT_RGB565;
    uint8_t src_pixel_size = gfx_color_format_get_size(src_format);

    GFX_RETURN_ON_FALSE(src_pixel_size > 0U, GFX_ERR_NOT_SUPPORTED, TAG, "flush: unsupported source format");

    if (disp != NULL && disp->flags.full_frame) {
        src_stride = disp->res.h_res;
        src_offset = (size_t)y1 * src_stride + (size_t)x1;
    }

    for (uint32_t y = 0; y < h; y++) {
        uint8_t *dst_row = mem->buffer + (((size_t)y1 + y) * mem->h_res + (uint32_t)x1) * mem->pixel_size;
        const uint8_t *src_row = src + (src_offset + (size_t)y * src_stride) * src_pixel_size;
        if (src_format == mem->format) {
            memcpy(dst_row, src_row, (size_t)w * mem->pixel_size);
            continue;
        }
        for (uint32_t x = 0; x < w; x++) {
            uint16_t semantic = gfx_memory_backend_read_rgb565_semantic(src_row + (size_t)x * src_pixel_size,
                                src_format);
            gfx_memory_backend_write_pixel(dst_row + (size_t)x * mem->pixel_size, mem->format, semantic);
        }
    }
    return GFX_OK;
}

static gfx_err_t gfx_memory_backend_wait_flush_impl(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;
    (void)disp;
    return GFX_OK;
}

static void gfx_memory_backend_destroy_impl(gfx_backend_t *backend)
{
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    if (mem == NULL) {
        return;
    }
    if (!mem->ext_buffer) {
        gfx_platform_free(mem->buffer);
    }
    free(mem);
}

static const gfx_backend_vtable_t s_memory_backend_vtable = {
    .flush = gfx_memory_backend_flush_impl,
    .wait_flush = gfx_memory_backend_wait_flush_impl,
    .destroy = gfx_memory_backend_destroy_impl,
};

gfx_backend_t *gfx_memory_backend_create(const gfx_memory_backend_config_t *cfg)
{
    GFX_RETURN_ON_FALSE(cfg != NULL && cfg->h_res > 0 && cfg->v_res > 0,
                        NULL, TAG, "create: invalid config");

    size_t pixels = (size_t)cfg->h_res * cfg->v_res;
    gfx_color_format_t format = gfx_memory_backend_resolve_format(cfg);
    uint8_t pixel_size = gfx_color_format_get_size(format);
    GFX_RETURN_ON_FALSE(format == GFX_COLOR_FORMAT_RGB565 ||
                        format == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
                        format == GFX_COLOR_FORMAT_RGB888 ||
                        format == GFX_COLOR_FORMAT_BGR888 ||
                        format == GFX_COLOR_FORMAT_XRGB8888 ||
                        format == GFX_COLOR_FORMAT_ARGB8888,
                        NULL, TAG, "create: unsupported format %u", (unsigned)format);
    GFX_RETURN_ON_FALSE(pixel_size > 0U, NULL, TAG, "create: invalid pixel size");

    gfx_memory_backend_t *mem = calloc(1, sizeof(*mem));
    const gfx_draw_ops_t *draw_ops = gfx_platform_accel_get_draw_ops();
    uint32_t draw_caps = gfx_platform_accel_get_caps();
    GFX_RETURN_ON_FALSE(mem != NULL, NULL, TAG, "create: no mem for backend");

    mem->base.vtable = &s_memory_backend_vtable;
    mem->base.draw_ops = draw_ops;
    mem->base.caps = GFX_BACKEND_CAP_FLUSH | draw_caps;
    mem->base.alignment = gfx_memory_backend_merge_alignment(gfx_backend_get_alignment(NULL),
                          gfx_platform_accel_get_alignment());
    mem->h_res = cfg->h_res;
    mem->v_res = cfg->v_res;
    mem->format = format;
    mem->pixel_size = pixel_size;

    if (cfg->buffer != NULL) {
        if (cfg->buffer_pixels < pixels) {
            GFX_LOGE(TAG, "create: external buffer too small");
            free(mem);
            return NULL;
        }
        mem->buffer = (uint8_t *)cfg->buffer;
        mem->ext_buffer = true;
    } else {
        mem->buffer = gfx_platform_calloc(pixels, pixel_size, GFX_PLATFORM_HEAP_DEFAULT);
        if (mem->buffer == NULL) {
            GFX_LOGE(TAG, "create: no mem for framebuffer");
            free(mem);
            return NULL;
        }
        mem->ext_buffer = false;
    }

    return &mem->base;
}

void gfx_memory_backend_delete(gfx_backend_t *backend)
{
    gfx_backend_destroy(backend);
}

const uint16_t *gfx_memory_backend_get_buffer(const gfx_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? (const uint16_t *)mem->buffer : NULL;
}

const void *gfx_memory_backend_get_buffer_data(const gfx_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? mem->buffer : NULL;
}

gfx_color_format_t gfx_memory_backend_get_color_format(const gfx_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? mem->format : GFX_COLOR_FORMAT_UNKNOWN;
}

size_t gfx_memory_backend_get_buffer_pixels(const gfx_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? (size_t)mem->h_res * mem->v_res : 0;
}

size_t gfx_memory_backend_get_buffer_bytes(const gfx_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? (size_t)mem->h_res * mem->v_res * mem->pixel_size : 0;
}

gfx_err_t gfx_memory_backend_clear(gfx_backend_t *backend, gfx_color_t color)
{
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    GFX_RETURN_ON_FALSE(mem != NULL && mem->buffer != NULL, GFX_ERR_INVALID_ARG, TAG, "clear: invalid backend");

    size_t pixels = (size_t)mem->h_res * mem->v_res;
    for (size_t i = 0; i < pixels; i++) {
        gfx_memory_backend_write_pixel(mem->buffer + i * mem->pixel_size, mem->format, color.full);
    }
    return GFX_OK;
}
