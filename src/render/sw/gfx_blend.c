/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdlib.h>
#include <string.h>

#include "common/gfx_comm.h"
#include "common/gfx_config_internal.h"
#include "common/gfx_mesh_frac.h"
#include "core/display/gfx_refresh_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "platform/gfx_platform.h"

/*********************
 *      DEFINES
 *********************/

#define OPA_MAX      253  /*Opacities above this will fully cover*/
#define OPA_TRANSP   0
#define OPA_COVER    0xFF

#define FILL_NORMAL_MASK_PX(color, native_color, swap)                              \
    if(*mask == OPA_COVER) *dest_buf = native_color;                \
    else *dest_buf = gfx_sw_blend_mix_to_native(color, *dest_buf, *mask, swap);     \
    mask++;                                                     \
    dest_buf++;

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gfx_sw_rgba8888_t;

typedef struct {
    uint16_t rgb565;
    gfx_opa_t alpha;
} gfx_sw_rgb565a_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static gfx_draw_perf_stats_t *s_active_perf_stats = NULL;

/**********************
 *   STATIC FUNCTIONS
 **********************/

static int32_t gfx_sw_blend_isqrt_i64(uint64_t value)
{
    uint64_t op, res, one;

    if (value <= 1U) {
        return (int32_t)value;
    }

    op = value;
    res = 0U;
    one = 1ULL << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0U) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return (int32_t)res;
}

static inline int32_t gfx_sw_blend_clamp_coord(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static inline int32_t gfx_sw_blend_floor_q8_to_int(int32_t value_q8)
{
    const int shift = GFX_MESH_FRAC_SHIFT;
    const int32_t mask = GFX_MESH_FRAC_MASK;

    if (value_q8 >= 0) {
        return value_q8 >> shift;
    }
    return -(((-value_q8) + mask) >> shift);
}

static inline int32_t gfx_sw_blend_ceil_q8_to_int(int32_t value_q8)
{
    const int shift = GFX_MESH_FRAC_SHIFT;
    const int32_t mask = GFX_MESH_FRAC_MASK;

    if (value_q8 >= 0) {
        return (value_q8 + mask) >> shift;
    }
    return -((-value_q8) >> shift);
}

static inline bool gfx_sw_blend_triangle_sample_inside(int64_t area_2x,
        int64_t w0, int64_t w1, int64_t w2)
{
    if (area_2x > 0) {
        return (w0 >= 0) && (w1 >= 0) && (w2 >= 0);
    }
    return (w0 <= 0) && (w1 <= 0) && (w2 <= 0);
}

static inline uint64_t gfx_blend_perf_elapsed_us(int64_t start_us)
{
    return (uint64_t)(gfx_platform_time_us() - start_us);
}

static inline bool gfx_sw_blend_format_is_rgb888_dst(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_RGB888 ||
           format == GFX_COLOR_FORMAT_BGR888 ||
           format == GFX_COLOR_FORMAT_XRGB8888;
}

static inline bool gfx_sw_blend_format_is_supported_dst(gfx_color_format_t format)
{
    return gfx_color_format_is_rgb565(format) ||
           gfx_sw_blend_format_is_rgb888_dst(format);
}

static inline bool gfx_sw_blend_format_is_rgb888_src(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_RGB888 ||
           format == GFX_COLOR_FORMAT_BGR888 ||
           format == GFX_COLOR_FORMAT_RGB888A8 ||
           format == GFX_COLOR_FORMAT_XRGB8888 ||
           format == GFX_COLOR_FORMAT_ARGB8888;
}

static inline uint8_t gfx_sw_blend_dst_pixel_size(gfx_color_format_t format)
{
    return format == GFX_COLOR_FORMAT_XRGB8888 ? GFX_PIXEL_SIZE_32BPP : gfx_color_format_get_size(format);
}

static inline gfx_sw_rgba8888_t gfx_sw_rgb565_to_rgba8888(uint16_t rgb565, uint8_t alpha)
{
    gfx_sw_rgba8888_t rgba;
    uint8_t r5 = (uint8_t)((rgb565 >> 11) & 0x1fU);
    uint8_t g6 = (uint8_t)((rgb565 >> 5) & 0x3fU);
    uint8_t b5 = (uint8_t)(rgb565 & 0x1fU);

    rgba.r = (uint8_t)((r5 << 3) | (r5 >> 2));
    rgba.g = (uint8_t)((g6 << 2) | (g6 >> 4));
    rgba.b = (uint8_t)((b5 << 3) | (b5 >> 2));
    rgba.a = alpha;
    return rgba;
}

static inline uint16_t gfx_sw_rgba8888_to_rgb565(gfx_sw_rgba8888_t rgba)
{
    return (uint16_t)(((uint16_t)(rgba.r & 0xF8U) << 8) |
                      ((uint16_t)(rgba.g & 0xFCU) << 3) |
                      ((uint16_t)rgba.b >> 3));
}

static inline gfx_sw_rgba8888_t gfx_sw_read_rgb565_pixel(const uint8_t *src,
        gfx_color_format_t format)
{
    return gfx_sw_rgb565_to_rgba8888(gfx_color_read_rgb565_bytes(src, format), OPA_COVER);
}

static inline gfx_sw_rgba8888_t gfx_sw_read_src_rgb565_pixel(const uint8_t *src,
        gfx_color_format_t format)
{
    return gfx_sw_rgb565_to_rgba8888(gfx_color_read_rgb565_bytes(src, format), OPA_COVER);
}

static inline gfx_sw_rgba8888_t gfx_sw_read_rgb888_pixel(const uint8_t *src,
        gfx_color_format_t format)
{
    gfx_sw_rgba8888_t rgba;

    if (format == GFX_COLOR_FORMAT_ARGB8888 || format == GFX_COLOR_FORMAT_XRGB8888) {
        uint32_t px;
        memcpy(&px, src, sizeof(px));
        rgba.r = (uint8_t)((px >> 16) & 0xffU);
        rgba.g = (uint8_t)((px >> 8) & 0xffU);
        rgba.b = (uint8_t)(px & 0xffU);
        rgba.a = format == GFX_COLOR_FORMAT_ARGB8888 ? (uint8_t)((px >> 24) & 0xffU) : OPA_COVER;
        return rgba;
    }

    gfx_color_read_rgb888_bytes(src, format, &rgba.r, &rgba.g, &rgba.b);
    rgba.a = OPA_COVER;
    return rgba;
}

static inline gfx_sw_rgba8888_t gfx_sw_read_src_pixel(const uint8_t *src,
        gfx_color_format_t format)
{
    if (gfx_color_format_is_rgb565(format)) {
        return gfx_sw_read_src_rgb565_pixel(src, format);
    }
    return gfx_sw_read_rgb888_pixel(src, format);
}

static inline gfx_sw_rgba8888_t gfx_sw_read_dst_pixel(const uint8_t *dst,
        gfx_color_format_t format)
{
    if (gfx_color_format_is_rgb565(format)) {
        return gfx_sw_read_rgb565_pixel(dst, format);
    }
    return gfx_sw_read_rgb888_pixel(dst, format);
}

static inline void gfx_sw_write_dst_pixel(uint8_t *dst, gfx_color_format_t format,
        gfx_sw_rgba8888_t rgba)
{
    if (gfx_color_format_is_rgb565(format)) {
        gfx_color_write_rgb565_bytes(dst, format, gfx_sw_rgba8888_to_rgb565(rgba));
        return;
    }

    if (format == GFX_COLOR_FORMAT_XRGB8888) {
        uint32_t px = 0xff000000U | ((uint32_t)rgba.r << 16) |
                      ((uint32_t)rgba.g << 8) | rgba.b;
        memcpy(dst, &px, sizeof(px));
        return;
    }
    gfx_color_write_rgb888_bytes(dst, format, rgba.r, rgba.g, rgba.b);
}

static inline uint16_t gfx_sw_rgb565_fill_word_for_format(gfx_color_format_t format,
        uint16_t color)
{
    uint8_t bytes[2];
    uint16_t word;

    gfx_color_write_rgb565_bytes(bytes, format, color);
    memcpy(&word, bytes, sizeof(word));
    return word;
}

static inline uint16_t gfx_sw_rgb565_mix_u16(uint16_t src565, uint16_t dst565, gfx_opa_t opa)
{
    gfx_color_t src = {.full = src565};
    gfx_color_t dst = {.full = dst565};
    return gfx_blend_color_mix(src, dst, opa).full;
}

static inline gfx_sw_rgba8888_t gfx_sw_mix_rgba8888(gfx_sw_rgba8888_t src,
        gfx_sw_rgba8888_t dst,
        gfx_opa_t opa)
{
    uint16_t inv = (uint16_t)(255U - opa);
    gfx_sw_rgba8888_t out;

    out.r = (uint8_t)(((uint16_t)src.r * opa + (uint16_t)dst.r * inv + 127U) / 255U);
    out.g = (uint8_t)(((uint16_t)src.g * opa + (uint16_t)dst.g * inv + 127U) / 255U);
    out.b = (uint8_t)(((uint16_t)src.b * opa + (uint16_t)dst.b * inv + 127U) / 255U);
    out.a = OPA_COVER;
    return out;
}

static inline gfx_opa_t gfx_sw_mix_opa(gfx_opa_t a, gfx_opa_t b)
{
    return (gfx_opa_t)(((uint32_t)a * b + 127U) / 255U);
}

static inline void gfx_sw_blend_one_fmt(uint8_t *dst, gfx_color_format_t dst_format,
                                        gfx_sw_rgba8888_t src, gfx_opa_t opa)
{
    if (opa == OPA_TRANSP) {
        return;
    }
    if (src.a != OPA_COVER) {
        opa = gfx_sw_mix_opa(opa, src.a);
        if (opa == OPA_TRANSP) {
            return;
        }
    }
    if (opa >= OPA_MAX) {
        gfx_sw_write_dst_pixel(dst, dst_format, src);
        return;
    }

    gfx_sw_rgba8888_t dst_rgba = gfx_sw_read_dst_pixel(dst, dst_format);
    gfx_sw_write_dst_pixel(dst, dst_format, gfx_sw_mix_rgba8888(src, dst_rgba, opa));
}

static inline gfx_color_t gfx_sw_blend_img_src_to_semantic_at(const void *src_buf, size_t index,
        gfx_color_format_t src_format)
{
    const uint8_t *src = (const uint8_t *)src_buf + index * gfx_color_format_get_size(src_format);
    gfx_color_t src_color;

    if (src_format == GFX_COLOR_FORMAT_RGB888 ||
            src_format == GFX_COLOR_FORMAT_BGR888 ||
            src_format == GFX_COLOR_FORMAT_RGB888A8) {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        gfx_color_read_rgb888_bytes(src, src_format, &r, &g, &b);
        src_color = gfx_color_from_rgb888(r, g, b);
    } else if (gfx_color_format_is_rgb565_swapped(src_format)) {
        src_color.full = ((uint16_t)src[1] << 8) | src[0];
    } else {
        src_color.full = ((uint16_t)src[0] << 8) | src[1];
    }
    return src_color;
}

static inline gfx_sw_rgb565a_t gfx_sw_read_src_rgb565a_pixel(const uint8_t *src,
        gfx_color_format_t src_format)
{
    gfx_sw_rgb565a_t px = {
        .rgb565 = 0,
        .alpha = OPA_COVER,
    };

    if (gfx_color_format_is_rgb565(src_format)) {
        px.rgb565 = gfx_color_read_rgb565_bytes(src, src_format);
        return px;
    }

    if (src_format == GFX_COLOR_FORMAT_RGB888 ||
            src_format == GFX_COLOR_FORMAT_BGR888 ||
            src_format == GFX_COLOR_FORMAT_RGB888A8) {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        gfx_color_read_rgb888_bytes(src, src_format, &r, &g, &b);
        px.rgb565 = gfx_color_from_rgb888(r, g, b).full;
        return px;
    }

    gfx_sw_rgba8888_t rgba = gfx_sw_read_rgb888_pixel(src, src_format);
    px.rgb565 = gfx_sw_rgba8888_to_rgb565(rgba);
    px.alpha = rgba.a;
    return px;
}

static void gfx_sw_blend_mask_draw_rgb565(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_area_t *area, uint16_t color565, gfx_opa_t opa)
{
    int32_t w = area->x2 - area->x1;
    int32_t h = area->y2 - area->y1;
    uint8_t *dst_base = (uint8_t *)dest_buf;
    uint16_t native_fill = gfx_sw_rgb565_fill_word_for_format(dest_format, color565);

    for (int32_t y = 0; y < h; y++) {
        uint8_t *dst_row = dst_base + ((size_t)(area->y1 + y) * dest_stride + (size_t)area->x1) * 2U;
        const gfx_opa_t *mask_row = mask + (size_t)y * mask_stride;

        for (int32_t x = 0; x < w; x++) {
            gfx_opa_t px_opa = gfx_sw_mix_opa(opa, mask_row[x]);
            uint8_t *dst_px = dst_row + (size_t)x * 2U;

            if (px_opa == OPA_TRANSP) {
                continue;
            }
            if (px_opa >= OPA_MAX) {
                memcpy(dst_px, &native_fill, sizeof(native_fill));
                continue;
            }

            uint16_t dst565 = gfx_color_read_rgb565_bytes(dst_px, dest_format);
            uint16_t out565 = gfx_sw_rgb565_mix_u16(color565, dst565, px_opa);
            gfx_color_write_rgb565_bytes(dst_px, dest_format, out565);
        }
    }
}

static void gfx_sw_blend_mask_color_draw_rgb565(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_color_t *color_mask, gfx_coord_t color_mask_stride,
        const gfx_area_t *area, gfx_opa_t opa)
{
    int32_t w = area->x2 - area->x1;
    int32_t h = area->y2 - area->y1;
    uint8_t *dst_base = (uint8_t *)dest_buf;

    for (int32_t y = 0; y < h; y++) {
        uint8_t *dst_row = dst_base + ((size_t)(area->y1 + y) * dest_stride + (size_t)area->x1) * 2U;
        const gfx_opa_t *mask_row = mask + (size_t)y * mask_stride;
        const gfx_color_t *color_row = color_mask + (size_t)y * color_mask_stride;

        for (int32_t x = 0; x < w; x++) {
            gfx_opa_t px_opa = gfx_sw_mix_opa(opa, mask_row[x]);
            uint8_t *dst_px = dst_row + (size_t)x * 2U;
            uint16_t src565 = color_row[x].full;

            if (px_opa == OPA_TRANSP) {
                continue;
            }
            if (px_opa >= OPA_MAX) {
                gfx_color_write_rgb565_bytes(dst_px, dest_format, src565);
                continue;
            }

            uint16_t dst565 = gfx_color_read_rgb565_bytes(dst_px, dest_format);
            uint16_t out565 = gfx_sw_rgb565_mix_u16(src565, dst565, px_opa);
            gfx_color_write_rgb565_bytes(dst_px, dest_format, out565);
        }
    }
}

static void gfx_sw_blend_img_scale_draw_rgb565(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const void *src_buf, gfx_coord_t src_stride,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_area_t *dst_area, const gfx_area_t *clip_area,
        const gfx_area_t *src_area,
        gfx_color_format_t src_format, gfx_opa_t opa)
{
    gfx_area_t draw_area;
    int32_t dst_w;
    int32_t dst_h;
    int32_t src_w;
    int32_t src_h;
    uint8_t src_px_size;
    uint8_t *dst_base = (uint8_t *)dest_buf;
    const uint8_t *src_base = (const uint8_t *)src_buf;

    if (!gfx_area_intersect_exclusive(&draw_area, dst_area, clip_area)) {
        return;
    }

    dst_w = dst_area->x2 - dst_area->x1;
    dst_h = dst_area->y2 - dst_area->y1;
    src_w = src_area->x2 - src_area->x1;
    src_h = src_area->y2 - src_area->y1;
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) {
        return;
    }

    src_px_size = gfx_color_format_get_size(src_format);
    if (src_px_size == 0U) {
        return;
    }

    for (gfx_coord_t y = draw_area.y1; y < draw_area.y2; y++) {
        int32_t dst_rel_y = y - dst_area->y1;
        int32_t src_y = src_area->y1 + (dst_rel_y * src_h) / dst_h;
        uint8_t *dst_row = dst_base + ((size_t)y * dest_stride + (size_t)draw_area.x1) * 2U;

        if (src_y < src_area->y1) {
            src_y = src_area->y1;
        } else if (src_y >= src_area->y2) {
            src_y = src_area->y2 - 1;
        }

        for (gfx_coord_t x = draw_area.x1; x < draw_area.x2; x++) {
            int32_t dst_rel_x = x - dst_area->x1;
            int32_t src_x = src_area->x1 + (dst_rel_x * src_w) / dst_w;
            gfx_opa_t px_opa = opa;
            uint8_t *dst_px = dst_row + (size_t)(x - draw_area.x1) * 2U;
            const uint8_t *src_px;
            gfx_sw_rgb565a_t src565a;

            if (src_x < src_area->x1) {
                src_x = src_area->x1;
            } else if (src_x >= src_area->x2) {
                src_x = src_area->x2 - 1;
            }

            src_px = src_base + ((size_t)src_y * src_stride + (size_t)src_x) * src_px_size;
            src565a = gfx_sw_read_src_rgb565a_pixel(src_px, src_format);
            if (mask != NULL) {
                px_opa = gfx_sw_mix_opa(px_opa, mask[(size_t)src_y * mask_stride + (size_t)src_x]);
            }
            if (src565a.alpha != OPA_COVER) {
                px_opa = gfx_sw_mix_opa(px_opa, src565a.alpha);
            }
            if (px_opa == OPA_TRANSP) {
                continue;
            }
            if (px_opa >= OPA_MAX) {
                gfx_color_write_rgb565_bytes(dst_px, dest_format, src565a.rgb565);
                continue;
            }

            uint16_t dst565 = gfx_color_read_rgb565_bytes(dst_px, dest_format);
            uint16_t out565 = gfx_sw_rgb565_mix_u16(src565a.rgb565, dst565, px_opa);
            gfx_color_write_rgb565_bytes(dst_px, dest_format, out565);
        }
    }
}

static void gfx_sw_blend_img_draw_rgb565(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const void *src_buf, gfx_coord_t src_stride,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_area_t *clip_area, gfx_color_format_t src_format,
        gfx_opa_t opa)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    uint8_t src_px_size = gfx_color_format_get_size(src_format);
    uint8_t *dst_base = (uint8_t *)dest_buf;
    const uint8_t *src_base = (const uint8_t *)src_buf;

    if (w <= 0 || h <= 0 || src_px_size == 0U) {
        return;
    }

    if (mask == NULL && opa >= OPA_MAX && dest_format == src_format &&
            gfx_color_format_is_rgb565(dest_format)) {
        size_t row_bytes = (size_t)w * 2U;
        for (int32_t y = 0; y < h; y++) {
            uint8_t *dst_row = dst_base + ((size_t)(clip_area->y1 + y) * dest_stride +
                                           (size_t)clip_area->x1) * 2U;
            memcpy(dst_row, src_base, row_bytes);
            src_base += (size_t)src_stride * 2U;
        }
        return;
    }

    for (int32_t y = 0; y < h; y++) {
        uint8_t *dst_row = dst_base + ((size_t)(clip_area->y1 + y) * dest_stride +
                                       (size_t)clip_area->x1) * 2U;
        const uint8_t *src_row = src_base + (size_t)y * src_stride * src_px_size;
        const gfx_opa_t *mask_row = mask != NULL ? mask + (size_t)y * mask_stride : NULL;

        for (int32_t x = 0; x < w; x++) {
            gfx_opa_t px_opa = opa;
            const uint8_t *src_px = src_row + (size_t)x * src_px_size;
            uint8_t *dst_px = dst_row + (size_t)x * 2U;
            gfx_sw_rgb565a_t src565a = gfx_sw_read_src_rgb565a_pixel(src_px, src_format);

            if (mask_row != NULL) {
                px_opa = gfx_sw_mix_opa(px_opa, mask_row[x]);
            }
            if (src565a.alpha != OPA_COVER) {
                px_opa = gfx_sw_mix_opa(px_opa, src565a.alpha);
            }
            if (px_opa == OPA_TRANSP) {
                continue;
            }
            if (px_opa >= OPA_MAX) {
                gfx_color_write_rgb565_bytes(dst_px, dest_format, src565a.rgb565);
                continue;
            }

            uint16_t dst565 = gfx_color_read_rgb565_bytes(dst_px, dest_format);
            uint16_t out565 = gfx_sw_rgb565_mix_u16(src565a.rgb565, dst565, px_opa);
            gfx_color_write_rgb565_bytes(dst_px, dest_format, out565);
        }
    }
}

static inline void gfx_sw_write_rgb888_dst_rgb(uint8_t *dst_px,
        gfx_color_format_t dest_format,
        uint8_t r, uint8_t g, uint8_t b)
{
    if (dest_format == GFX_COLOR_FORMAT_XRGB8888) {
        uint32_t px = 0xff000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        memcpy(dst_px, &px, sizeof(px));
        return;
    }

    gfx_color_write_rgb888_bytes(dst_px, dest_format, r, g, b);
}

static inline void gfx_sw_blend_sample_to_rgb565_dst(uint8_t *dst_px,
        gfx_color_format_t dest_format,
        const uint8_t *src_px,
        gfx_color_format_t src_format,
        gfx_opa_t opa)
{
    gfx_sw_rgb565a_t src565a = gfx_sw_read_src_rgb565a_pixel(src_px, src_format);

    if (src565a.alpha != OPA_COVER) {
        opa = gfx_sw_mix_opa(opa, src565a.alpha);
    }
    if (opa == OPA_TRANSP) {
        return;
    }
    if (opa >= OPA_MAX) {
        gfx_color_write_rgb565_bytes(dst_px, dest_format, src565a.rgb565);
        return;
    }

    uint16_t dst565 = gfx_color_read_rgb565_bytes(dst_px, dest_format);
    uint16_t out565 = gfx_sw_rgb565_mix_u16(src565a.rgb565, dst565, opa);
    gfx_color_write_rgb565_bytes(dst_px, dest_format, out565);
}

static inline void gfx_sw_blend_sample_to_rgb888_dst(uint8_t *dst_px,
        gfx_color_format_t dest_format,
        const uint8_t *src_px,
        gfx_color_format_t src_format,
        gfx_opa_t opa)
{
    if (opa >= OPA_MAX &&
            (src_format == GFX_COLOR_FORMAT_RGB888 ||
             src_format == GFX_COLOR_FORMAT_BGR888 ||
             src_format == GFX_COLOR_FORMAT_XRGB8888)) {
        uint8_t r;
        uint8_t g;
        uint8_t b;

        if (src_format == GFX_COLOR_FORMAT_XRGB8888) {
            uint32_t px;
            memcpy(&px, src_px, sizeof(px));
            r = (uint8_t)((px >> 16) & 0xffU);
            g = (uint8_t)((px >> 8) & 0xffU);
            b = (uint8_t)(px & 0xffU);
        } else {
            gfx_color_read_rgb888_bytes(src_px, src_format, &r, &g, &b);
        }

        gfx_sw_write_rgb888_dst_rgb(dst_px, dest_format, r, g, b);
        return;
    }

    gfx_sw_rgba8888_t src = gfx_sw_read_src_pixel(src_px, src_format);
    gfx_sw_blend_one_fmt(dst_px, dest_format, src, opa);
}

static void gfx_sw_blend_img_draw_rgb888_dst(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const void *src_buf, gfx_coord_t src_stride,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_area_t *clip_area, gfx_color_format_t src_format,
        gfx_opa_t opa)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    uint8_t dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    uint8_t src_px_size = gfx_color_format_get_size(src_format);
    uint8_t *dst_base = (uint8_t *)dest_buf;
    const uint8_t *src_base = (const uint8_t *)src_buf;

    if (w <= 0 || h <= 0 || dst_px_size == 0U || src_px_size == 0U) {
        return;
    }

    if (mask == NULL && opa >= OPA_MAX && dest_format == src_format) {
        size_t row_bytes = (size_t)w * dst_px_size;
        for (int32_t y = 0; y < h; y++) {
            uint8_t *dst_row = dst_base + ((size_t)(clip_area->y1 + y) * dest_stride +
                                           (size_t)clip_area->x1) * dst_px_size;
            memcpy(dst_row, src_base, row_bytes);
            src_base += (size_t)src_stride * src_px_size;
        }
        return;
    }

    for (int32_t y = 0; y < h; y++) {
        uint8_t *dst_row = dst_base + ((size_t)(clip_area->y1 + y) * dest_stride +
                                       (size_t)clip_area->x1) * dst_px_size;
        const uint8_t *src_row = src_base + (size_t)y * src_stride * src_px_size;
        const gfx_opa_t *mask_row = mask != NULL ? mask + (size_t)y * mask_stride : NULL;

        if (mask_row == NULL && opa >= OPA_MAX) {
            if (src_format == GFX_COLOR_FORMAT_RGB888 || src_format == GFX_COLOR_FORMAT_BGR888) {
                for (int32_t x = 0; x < w; x++) {
                    const uint8_t *src_px = src_row + (size_t)x * src_px_size;
                    uint8_t r;
                    uint8_t g;
                    uint8_t b;
                    gfx_color_read_rgb888_bytes(src_px, src_format, &r, &g, &b);
                    gfx_sw_write_rgb888_dst_rgb(dst_row + (size_t)x * dst_px_size, dest_format, r, g, b);
                }
                continue;
            }

            if (src_format == GFX_COLOR_FORMAT_XRGB8888) {
                for (int32_t x = 0; x < w; x++) {
                    const uint8_t *src_px = src_row + (size_t)x * src_px_size;
                    uint32_t px;
                    memcpy(&px, src_px, sizeof(px));
                    gfx_sw_write_rgb888_dst_rgb(dst_row + (size_t)x * dst_px_size,
                                                dest_format,
                                                (uint8_t)((px >> 16) & 0xffU),
                                                (uint8_t)((px >> 8) & 0xffU),
                                                (uint8_t)(px & 0xffU));
                }
                continue;
            }
        }

        for (int32_t x = 0; x < w; x++) {
            gfx_opa_t px_opa = opa;
            const uint8_t *src_px = src_row + (size_t)x * src_px_size;

            if (mask_row != NULL) {
                px_opa = gfx_sw_mix_opa(px_opa, mask_row[x]);
            }

            gfx_sw_blend_sample_to_rgb888_dst(dst_row + (size_t)x * dst_px_size,
                                              dest_format, src_px, src_format, px_opa);
        }
    }
}

static void gfx_sw_blend_img_scale_draw_rgb888_dst(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const void *src_buf, gfx_coord_t src_stride,
        const gfx_opa_t *mask, gfx_coord_t mask_stride,
        const gfx_area_t *dst_area, const gfx_area_t *clip_area,
        const gfx_area_t *src_area,
        gfx_color_format_t src_format, gfx_opa_t opa)
{
    gfx_area_t draw_area;
    int32_t dst_w;
    int32_t dst_h;
    int32_t src_w;
    int32_t src_h;
    uint8_t dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    uint8_t src_px_size = gfx_color_format_get_size(src_format);
    uint8_t *dst_base = (uint8_t *)dest_buf;
    const uint8_t *src_base = (const uint8_t *)src_buf;

    if (!gfx_area_intersect_exclusive(&draw_area, dst_area, clip_area)) {
        return;
    }

    dst_w = dst_area->x2 - dst_area->x1;
    dst_h = dst_area->y2 - dst_area->y1;
    src_w = src_area->x2 - src_area->x1;
    src_h = src_area->y2 - src_area->y1;
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0 ||
            dst_px_size == 0U || src_px_size == 0U) {
        return;
    }

    for (gfx_coord_t y = draw_area.y1; y < draw_area.y2; y++) {
        int32_t dst_rel_y = y - dst_area->y1;
        int32_t src_y = src_area->y1 + (dst_rel_y * src_h) / dst_h;
        uint8_t *dst = dst_base + ((size_t)y * dest_stride + (size_t)draw_area.x1) * dst_px_size;

        if (src_y < src_area->y1) {
            src_y = src_area->y1;
        } else if (src_y >= src_area->y2) {
            src_y = src_area->y2 - 1;
        }

        for (gfx_coord_t x = draw_area.x1; x < draw_area.x2; x++) {
            int32_t dst_rel_x = x - dst_area->x1;
            int32_t src_x = src_area->x1 + (dst_rel_x * src_w) / dst_w;
            gfx_opa_t px_opa = opa;

            if (src_x < src_area->x1) {
                src_x = src_area->x1;
            } else if (src_x >= src_area->x2) {
                src_x = src_area->x2 - 1;
            }

            const uint8_t *src_px = src_base + ((size_t)src_y * src_stride + (size_t)src_x) * src_px_size;

            if (mask != NULL) {
                px_opa = gfx_sw_mix_opa(px_opa, mask[(size_t)src_y * mask_stride + (size_t)src_x]);
            }

            gfx_sw_blend_sample_to_rgb888_dst(dst, dest_format, src_px, src_format, px_opa);

            dst += dst_px_size;
        }
    }
}

static void gfx_sw_blend_polygon_fill_scanline_fallback(void *dest_buf, gfx_coord_t dest_stride,
        gfx_color_format_t dest_format,
        const gfx_area_t *buf_area, const gfx_area_t *clip_area,
        gfx_color_t color,
        gfx_opa_t opa,
        const int32_t *vx, const int32_t *vy,
        int vertex_count)
{
#define POLY_FRAC   GFX_MESH_FRAC_SHIFT
#define POLY_ONE    GFX_MESH_FRAC_ONE
#define POLY_HALF   GFX_MESH_FRAC_HALF
#define POLY_MASK   GFX_MESH_FRAC_MASK
#define POLY_MAX_IX GFX_BLEND_POLYGON_MAX_INTERSECTIONS
#define POLY_SUB_SAMPLES GFX_BLEND_POLYGON_SUB_SAMPLES
#define POLY_COV_MAX_W   GFX_BLEND_POLYGON_COVERAGE_MAX_WIDTH

    int32_t min_yq, max_yq, y_start, y_end;
    int32_t x_clip_lo, x_clip_hi;
    uint8_t dst_px_size;
    uint8_t *dst_base = (uint8_t *)dest_buf;
    gfx_sw_rgba8888_t fill;

    if (dest_buf == NULL || dest_stride <= 0 || opa == 0U ||
            !gfx_sw_blend_format_is_supported_dst(dest_format)) {
        return;
    }

    dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    if (dst_px_size == 0U) {
        return;
    }
    fill = gfx_sw_rgb565_to_rgba8888(color.full, OPA_COVER);

    min_yq = vy[0];
    max_yq = vy[0];
    for (int i = 1; i < vertex_count; i++) {
        if (vy[i] < min_yq) {
            min_yq = vy[i];
        }
        if (vy[i] > max_yq) {
            max_yq = vy[i];
        }
    }

    y_start = (min_yq >= 0) ? (min_yq >> POLY_FRAC) : -(((-min_yq) + POLY_MASK) >> POLY_FRAC);
    y_end = (max_yq >= 0) ? ((max_yq + POLY_MASK) >> POLY_FRAC) : -((-max_yq) >> POLY_FRAC);

    y_start = MAX(y_start, MAX(buf_area->y1, clip_area->y1));
    y_end = MIN(y_end, MIN(buf_area->y2 - 1, clip_area->y2 - 1));
    x_clip_lo = MAX(buf_area->x1, clip_area->x1);
    x_clip_hi = MIN(buf_area->x2 - 1, clip_area->x2 - 1);

    {
        int32_t min_xq = vx[0], max_xq = vx[0];
        for (int i = 1; i < vertex_count; i++) {
            if (vx[i] < min_xq) {
                min_xq = vx[i];
            }
            if (vx[i] > max_xq) {
                max_xq = vx[i];
            }
        }

        int32_t cov_full_x0 = (min_xq >= 0) ? (min_xq >> POLY_FRAC) : -(((-min_xq) + POLY_MASK) >> POLY_FRAC);
        int32_t cov_full_x1 = (max_xq >= 0) ? ((max_xq + POLY_MASK) >> POLY_FRAC) : -((-max_xq) >> POLY_FRAC);
        cov_full_x0 = MAX(cov_full_x0, x_clip_lo);
        cov_full_x1 = MIN(cov_full_x1, x_clip_hi);
        if (cov_full_x1 < cov_full_x0) {
            return;
        }

        uint16_t cov_buf[POLY_COV_MAX_W];

        for (int32_t cov_x0 = cov_full_x0; cov_x0 <= cov_full_x1; cov_x0 += POLY_COV_MAX_W) {
            int32_t cov_x1 = MIN(cov_x0 + POLY_COV_MAX_W - 1, cov_full_x1);
            int32_t cov_w = cov_x1 - cov_x0 + 1;

            for (int32_t y = y_start; y <= y_end; y++) {
#if GFX_BLEND_POLYGON_INWARD_AA || GFX_BLEND_POLYGON_SOLID_HARD_EDGE
                int32_t center_ix[POLY_MAX_IX];
                int center_ic = 0;
#endif
                memset(cov_buf, 0, (size_t)cov_w * sizeof(uint16_t));

#if GFX_BLEND_POLYGON_INWARD_AA || GFX_BLEND_POLYGON_SOLID_HARD_EDGE
                /*
                 * Keep AA inward-only: partially covered pixels whose centre lies
                 * outside the polygon are not touched.  This avoids a bright/dark
                 * "coat" caused by blending edge coverage with stale/background
                 * pixels outside filled Bezier loops.
                 */
                {
                    int32_t yc = y * POLY_ONE + POLY_HALF;
                    for (int e = 0; e < vertex_count; e++) {
                        int en = (e + 1 < vertex_count) ? e + 1 : 0;
                        int32_t y1 = vy[e], y2 = vy[en];
                        if ((y1 <= yc && y2 > yc) || (y2 <= yc && y1 > yc)) {
                            int32_t dy = y2 - y1;
                            int32_t xi = (int32_t)((int64_t)vx[e] + (int64_t)(yc - y1) * (vx[en] - vx[e]) / dy);
                            if (center_ic < POLY_MAX_IX) {
                                center_ix[center_ic++] = xi;
                            }
                        }
                    }
                    for (int a = 1; a < center_ic; a++) {
                        int32_t tmp = center_ix[a];
                        int b = a - 1;
                        while (b >= 0 && center_ix[b] > tmp) {
                            center_ix[b + 1] = center_ix[b];
                            b--;
                        }
                        center_ix[b + 1] = tmp;
                    }
                }
#endif

#if GFX_BLEND_POLYGON_SOLID_HARD_EDGE
                if (opa >= OPA_COVER) {
                    uint8_t *row = dst_base + (size_t)(y - buf_area->y1) * dest_stride * dst_px_size;
                    for (int p = 0; p + 1 < center_ic; p += 2) {
                        for (int32_t x = cov_x0; x <= cov_x1; x++) {
                            int32_t xc = x * POLY_ONE + POLY_HALF;
                            if (xc >= center_ix[p] && xc < center_ix[p + 1]) {
                                gfx_sw_write_dst_pixel(row + (size_t)(x - buf_area->x1) * dst_px_size,
                                                       dest_format, fill);
                            }
                        }
                    }
                    continue;
                }
#endif

                for (int s = 0; s < POLY_SUB_SAMPLES; s++) {
                    int32_t yc = y * POLY_ONE + (2 * s + 1) * POLY_ONE / (2 * POLY_SUB_SAMPLES);
                    int32_t ix[POLY_MAX_IX];
                    int ic = 0;

                    for (int e = 0; e < vertex_count; e++) {
                        int en = (e + 1 < vertex_count) ? e + 1 : 0;
                        int32_t y1 = vy[e], y2 = vy[en];
                        if ((y1 <= yc && y2 > yc) || (y2 <= yc && y1 > yc)) {
                            int32_t dy = y2 - y1;
                            int32_t xi = (int32_t)((int64_t)vx[e] + (int64_t)(yc - y1) * (vx[en] - vx[e]) / dy);
                            if (ic < POLY_MAX_IX) {
                                ix[ic++] = xi;
                            }
                        }
                    }

                    if (ic < 2) {
                        continue;
                    }

                    for (int a = 1; a < ic; a++) {
                        int32_t tmp = ix[a];
                        int b = a - 1;
                        while (b >= 0 && ix[b] > tmp) {
                            ix[b + 1] = ix[b];
                            b--;
                        }
                        ix[b + 1] = tmp;
                    }

                    for (int p = 0; p + 1 < ic; p += 2) {
                        int32_t xlq = ix[p];
                        int32_t xrq = ix[p + 1];
                        if (xlq >= xrq) {
                            continue;
                        }

                        int32_t xl = (xlq >= 0) ? (xlq >> POLY_FRAC) : -(((-xlq) + POLY_MASK) >> POLY_FRAC);
                        int32_t xr = (xrq >= 0) ? (xrq >> POLY_FRAC) : -((-xrq) >> POLY_FRAC);
                        if (xr < cov_x0 || xl > cov_x1) {
                            continue;
                        }

                        if (xl == xr) {
                            if (xl >= cov_x0 && xl <= cov_x1) {
                                cov_buf[xl - cov_x0] += (uint16_t)((xrq - xlq) * 255 / POLY_ONE);
                            }
                            continue;
                        }

                        if (xl >= cov_x0 && xl <= cov_x1) {
                            int32_t frac = xlq & POLY_MASK;
                            cov_buf[xl - cov_x0] += (uint16_t)((POLY_ONE - frac) * 255 / POLY_ONE);
                        }
                        {
                            int32_t fs = MAX(xl + 1, cov_x0);
                            int32_t fe = MIN(xr - 1, cov_x1);
                            for (int32_t x = fs; x <= fe; x++) {
                                cov_buf[x - cov_x0] += 255;
                            }
                        }
                        if (xr >= cov_x0 && xr <= cov_x1) {
                            int32_t frac = xrq & POLY_MASK;
                            if (frac > 0) {
                                cov_buf[xr - cov_x0] += (uint16_t)(frac * 255 / POLY_ONE);
                            }
                        }
                    }
                }

                uint8_t *row = dst_base + (size_t)(y - buf_area->y1) * dest_stride * dst_px_size;
                for (int32_t x = cov_x0; x <= cov_x1; x++) {
                    uint16_t c = cov_buf[x - cov_x0];
                    uint8_t *dst_px = row + (size_t)(x - buf_area->x1) * dst_px_size;
                    if (c == 0) {
                        continue;
                    }
                    gfx_opa_t px_opa = (c >= POLY_SUB_SAMPLES * 255) ? 255 : (gfx_opa_t)(c / POLY_SUB_SAMPLES);
                    if (opa < OPA_COVER) {
                        px_opa = (gfx_opa_t)(((uint32_t)px_opa * opa + 128U) >> 8);
                    }
                    if (px_opa >= OPA_MAX) {
                        gfx_sw_write_dst_pixel(dst_px, dest_format, fill);
                    } else if (px_opa > 0U) {
#if GFX_BLEND_POLYGON_INWARD_AA || GFX_BLEND_POLYGON_SOLID_HARD_EDGE
                        bool center_inside = false;
                        int32_t xc = x * POLY_ONE + POLY_HALF;
                        for (int p = 0; p + 1 < center_ic; p += 2) {
                            if (xc >= center_ix[p] && xc < center_ix[p + 1]) {
                                center_inside = true;
                                break;
                            }
                        }
#endif
#if GFX_BLEND_POLYGON_INWARD_AA
                        if (!center_inside) {
                            continue;
                        }
#endif
#if GFX_BLEND_POLYGON_SOLID_HARD_EDGE
                        if (opa >= OPA_COVER && center_inside) {
                            gfx_sw_write_dst_pixel(dst_px, dest_format, fill);
                        } else {
                            gfx_sw_blend_one_fmt(dst_px, dest_format, fill, px_opa);
                        }
#else
                        gfx_sw_blend_one_fmt(dst_px, dest_format, fill, px_opa);
#endif
                    }
                }
            }
        }
    }

#undef POLY_FRAC
#undef POLY_ONE
#undef POLY_HALF
#undef POLY_MASK
#undef POLY_MAX_IX
#undef POLY_SUB_SAMPLES
#undef POLY_COV_MAX_W
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_sw_blend_perf_reset(gfx_draw_perf_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
}

void gfx_sw_blend_perf_bind(gfx_draw_perf_stats_t *stats)
{
    s_active_perf_stats = stats;
}

void gfx_sw_blend_perf_unbind(void)
{
    s_active_perf_stats = NULL;
}

gfx_color_t gfx_blend_color_mix(gfx_color_t c1, gfx_color_t c2, uint8_t mix)
{
    gfx_color_t ret;

    /*Source: https://stackoverflow.com/a/50012418/1999969*/
    mix = (uint32_t)((uint32_t)mix + 4) >> 3;
    uint32_t bg = (uint32_t)((uint32_t)c2.full | ((uint32_t)c2.full << 16)) &
                  0x7E0F81F; /*0b00000111111000001111100000011111*/
    uint32_t fg = (uint32_t)((uint32_t)c1.full | ((uint32_t)c1.full << 16)) & 0x7E0F81F;
    uint32_t result = ((((fg - bg) * mix) >> 5) + bg) & 0x7E0F81F;
    ret.full = (uint16_t)((result >> 16) | result);

    return ret;
}

void gfx_sw_blend_fill(uint16_t *buf, uint16_t color, size_t pixels)
{
    if ((color & 0xFF) == (color >> 8)) {
        memset(buf, color & 0xFF, pixels * sizeof(uint16_t));
    } else {
        uint32_t color32 = ((uint32_t)color << 16) | color;

        if (((uintptr_t)buf & 0x3) && pixels > 0) {
            *buf++ = color;
            pixels--;
        }

        uint32_t *buf32 = (uint32_t *)buf;
        size_t pairs = pixels / 2;

        for (size_t i = 0; i < pairs; i++) {
            buf32[i] = color32;
        }

        if (pixels & 1) {
            buf[pixels - 1] = color;
        }
    }
}

void gfx_sw_blend_fill_area(uint16_t *dest_buf, gfx_coord_t dest_stride,
                            const gfx_area_t *area, uint16_t color)
{
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || area == NULL) {
        return;
    }
    int32_t w = area->x2 - area->x1;
    int32_t h = area->y2 - area->y1;
    if (w <= 0 || h <= 0) {
        return;
    }
    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }
    for (int32_t y = area->y1; y < area->y2; y++) {
        uint16_t *row = dest_buf + (size_t)y * dest_stride + area->x1;
        gfx_sw_blend_fill(row, color, (size_t)w);
    }
    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->fill.calls++;
        s_active_perf_stats->fill.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->fill.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_surface_fill(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    int32_t w;
    int32_t h;

    if (dest_buf == NULL || area == NULL || dest_stride <= 0 || opa == OPA_TRANSP ||
            !gfx_sw_blend_format_is_supported_dst(dest_format)) {
        return;
    }

    w = area->x2 - area->x1;
    h = area->y2 - area->y1;
    if (w <= 0 || h <= 0) {
        return;
    }

    if (gfx_color_format_is_rgb565(dest_format)) {
        if (opa >= OPA_MAX) {
            uint16_t fill_word = gfx_sw_rgb565_fill_word_for_format(dest_format, color.full);
            gfx_sw_blend_fill_area((uint16_t *)dest_buf, dest_stride, area, fill_word);
            return;
        }

        uint8_t *base = (uint8_t *)dest_buf;
        uint8_t dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
        gfx_sw_rgba8888_t src = gfx_sw_rgb565_to_rgba8888(color.full, OPA_COVER);
        for (int32_t y = 0; y < h; y++) {
            uint8_t *row = base + ((size_t)(area->y1 + y) * dest_stride + (size_t)area->x1) * dst_px_size;
            for (int32_t x = 0; x < w; x++) {
                gfx_sw_blend_one_fmt(row + (size_t)x * dst_px_size, dest_format, src, opa);
            }
        }
        return;
    }

    uint8_t dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    gfx_sw_rgba8888_t src = gfx_sw_rgb565_to_rgba8888(color.full, OPA_COVER);
    uint8_t *base = (uint8_t *)dest_buf;

    for (int32_t y = 0; y < h; y++) {
        uint8_t *row = base + ((size_t)(area->y1 + y) * dest_stride + (size_t)area->x1) * dst_px_size;
        if (opa >= OPA_MAX) {
            for (int32_t x = 0; x < w; x++) {
                gfx_sw_write_dst_pixel(row + (size_t)x * dst_px_size, dest_format, src);
            }
        } else {
            for (int32_t x = 0; x < w; x++) {
                gfx_sw_blend_one_fmt(row + (size_t)x * dst_px_size, dest_format, src, opa);
            }
        }
    }
}

void gfx_sw_blend_mask_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                gfx_color_format_t dest_format,
                                const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    int32_t w;
    int32_t h;
    uint8_t dst_px_size;
    uint8_t *dst_base = (uint8_t *)dest_buf;
    gfx_sw_rgba8888_t src;
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || area == NULL || dest_stride <= 0 || opa == OPA_TRANSP ||
            !gfx_sw_blend_format_is_supported_dst(dest_format)) {
        return;
    }

    w = area->x2 - area->x1;
    h = area->y2 - area->y1;
    dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    if (w <= 0 || h <= 0 || dst_px_size == 0U) {
        return;
    }

    if (mask == NULL) {
        gfx_sw_blend_surface_fill(dest_buf, dest_stride, dest_format, area, color, opa);
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }

    if (gfx_color_format_is_rgb565(dest_format)) {
        gfx_sw_blend_mask_draw_rgb565(dest_buf, dest_stride, dest_format,
                                      mask, mask_stride, area, color.full, opa);
    } else {
        src = gfx_sw_rgb565_to_rgba8888(color.full, OPA_COVER);
        for (int32_t y = 0; y < h; y++) {
            uint8_t *dst_row = dst_base + ((size_t)(area->y1 + y) * dest_stride +
                                           (size_t)area->x1) * dst_px_size;
            const gfx_opa_t *mask_row = mask + (size_t)y * mask_stride;
            for (int32_t x = 0; x < w; x++) {
                gfx_opa_t px_opa = gfx_sw_mix_opa(opa, mask_row[x]);
                gfx_sw_blend_one_fmt(dst_row + (size_t)x * dst_px_size, dest_format, src, px_opa);
            }
        }
    }

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->solid.calls++;
        s_active_perf_stats->solid.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->solid.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_mask_color_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                      gfx_color_format_t dest_format,
                                      const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                      const gfx_color_t *color_mask, gfx_coord_t color_mask_stride,
                                      const gfx_area_t *area, gfx_opa_t opa)
{
    int32_t w;
    int32_t h;
    uint8_t dst_px_size;
    uint8_t *dst_base = (uint8_t *)dest_buf;
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || mask == NULL || color_mask == NULL || area == NULL ||
            dest_stride <= 0 || mask_stride <= 0 || color_mask_stride <= 0 ||
            opa == OPA_TRANSP || !gfx_sw_blend_format_is_supported_dst(dest_format)) {
        return;
    }

    w = area->x2 - area->x1;
    h = area->y2 - area->y1;
    dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    if (w <= 0 || h <= 0 || dst_px_size == 0U) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }

    if (gfx_color_format_is_rgb565(dest_format)) {
        gfx_sw_blend_mask_color_draw_rgb565(dest_buf, dest_stride, dest_format,
                                            mask, mask_stride, color_mask, color_mask_stride,
                                            area, opa);
    } else {
        for (int32_t y = 0; y < h; y++) {
            uint8_t *dst_row = dst_base + ((size_t)(area->y1 + y) * dest_stride +
                                           (size_t)area->x1) * dst_px_size;
            const gfx_opa_t *mask_row = mask + (size_t)y * mask_stride;
            const gfx_color_t *color_row = color_mask + (size_t)y * color_mask_stride;
            for (int32_t x = 0; x < w; x++) {
                gfx_opa_t px_opa = gfx_sw_mix_opa(opa, mask_row[x]);
                if (px_opa != OPA_TRANSP) {
                    gfx_sw_rgba8888_t src = gfx_sw_rgb565_to_rgba8888(color_row[x].full, OPA_COVER);
                    gfx_sw_blend_one_fmt(dst_row + (size_t)x * dst_px_size, dest_format, src, px_opa);
                }
            }
        }
    }

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->solid.calls++;
        s_active_perf_stats->solid.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->solid.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_img_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const void *src_buf, gfx_coord_t src_stride,
                               const gfx_opa_t *mask, gfx_coord_t mask_stride,
                               gfx_area_t *clip_area, gfx_color_format_t src_format,
                               gfx_opa_t opa)
{
    int32_t w;
    int32_t h;
    int64_t perf_start_us = 0;
    uint8_t dst_px_size;
    uint8_t src_px_size;

    if (dest_buf == NULL || src_buf == NULL || clip_area == NULL || dest_stride <= 0 ||
            src_stride <= 0 || opa == OPA_TRANSP ||
            !gfx_sw_blend_format_is_supported_dst(dest_format) ||
            !gfx_color_format_is_image_supported(src_format)) {
        return;
    }

    w = clip_area->x2 - clip_area->x1;
    h = clip_area->y2 - clip_area->y1;
    dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    src_px_size = gfx_color_format_get_size(src_format);
    if (w <= 0 || h <= 0 || dst_px_size == 0U || src_px_size == 0U) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }

    if (gfx_color_format_is_rgb565(dest_format)) {
        gfx_sw_blend_img_draw_rgb565(dest_buf, dest_stride, dest_format,
                                     src_buf, src_stride,
                                     mask, mask_stride,
                                     clip_area, src_format, opa);
        goto perf_done;
    }

    gfx_sw_blend_img_draw_rgb888_dst(dest_buf, dest_stride, dest_format,
                                     src_buf, src_stride,
                                     mask, mask_stride,
                                     clip_area, src_format, opa);

perf_done:
    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->image.calls++;
        s_active_perf_stats->image.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->image.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_img_scale_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                     gfx_color_format_t dest_format,
                                     const void *src_buf, gfx_coord_t src_stride,
                                     const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                     const gfx_area_t *dst_area, const gfx_area_t *clip_area,
                                     const gfx_area_t *src_area,
                                     gfx_color_format_t src_format, gfx_opa_t opa)
{
    if (dest_buf == NULL || src_buf == NULL || dst_area == NULL || clip_area == NULL ||
            src_area == NULL || dest_stride <= 0 || src_stride <= 0 || opa == OPA_TRANSP ||
            !gfx_sw_blend_format_is_supported_dst(dest_format) ||
            !gfx_color_format_is_image_supported(src_format)) {
        return;
    }

    if (gfx_color_format_is_rgb565(dest_format)) {
        gfx_sw_blend_img_scale_draw_rgb565(dest_buf, dest_stride, dest_format,
                                           src_buf, src_stride, mask, mask_stride,
                                           dst_area, clip_area, src_area, src_format, opa);
        return;
    }

    gfx_sw_blend_img_scale_draw_rgb888_dst(dest_buf, dest_stride, dest_format,
                                           src_buf, src_stride, mask, mask_stride,
                                           dst_area, clip_area, src_area, src_format, opa);
}

void gfx_sw_blend_img_triangle_draw(void *dest_buf, gfx_coord_t dest_stride,
                                    gfx_color_format_t dest_format,
                                    const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                    const void *src_buf, gfx_coord_t src_stride, gfx_coord_t src_height,
                                    const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                    gfx_opa_t opa,
                                    const gfx_sw_blend_img_vertex_t *v0,
                                    const gfx_sw_blend_img_vertex_t *v1,
                                    const gfx_sw_blend_img_vertex_t *v2,
                                    uint8_t internal_edges,
                                    const gfx_sw_blend_aa_edge_t *extra_aa_edges,
                                    uint8_t extra_aa_count,
                                    gfx_color_format_t src_format)
{
    /*
     * Fixed-point incremental edge-walking rasterizer.
     *
     * Instead of computing 3 edge functions + 2 float divides per pixel,
     * we pre-compute linear step constants and walk the edge values with
     * pure integer adds in the inner loop.
     *
     * Fractional precision: 16 bits (sufficient for sub-1024px coordinates).
     */
#define FRAC_BITS     16
#define FRAC_HALF     (1 << (FRAC_BITS - 1))
#define XY_SUB_ONE    GFX_MESH_FRAC_ONE
#define XY_SUB_HALF   GFX_MESH_FRAC_HALF

    int64_t area_2x;
    int32_t min_x, min_y, max_x, max_y;
    int64_t perf_start_us = 0;
    uint64_t perf_raster_pixels = 0;

    /* Edge function step constants (mesh subpixel geometry) */
    int32_t e0_a, e0_b, e1_a, e1_b;
    int64_t e0_step_x, e0_step_y, e1_step_x, e1_step_y;

    /* Edge anti-aliasing: edge lengths in mesh fixed-point space */
    int32_t e0_len, e1_len, e2_len;

    /* UV interpolation gradients (fixed-point) */
    int32_t du_dx, dv_dx, du_dy, dv_dy;
    uint8_t dst_px_size;
    uint8_t src_px_size;
    const bool dst_is_rgb565 = gfx_color_format_is_rgb565(dest_format);
    uint8_t *dst_base = (uint8_t *)dest_buf;
    const uint8_t *src_base = (const uint8_t *)src_buf;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            src_buf == NULL || v0 == NULL || v1 == NULL || v2 == NULL ||
            src_stride <= 0 || src_height <= 0 || opa == 0U ||
            !gfx_sw_blend_format_is_supported_dst(dest_format) ||
            !gfx_color_format_is_image_supported(src_format)) {
        return;
    }
    dst_px_size = gfx_sw_blend_dst_pixel_size(dest_format);
    src_px_size = gfx_color_format_get_size(src_format);
    if (dst_px_size == 0U || src_px_size == 0U) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }

    /*
     * Compute 2× signed area of the triangle (integer).
     * Must match the edge function convention: E0 + E1 + E2 = area_2x.
     * area_2x = edge_func(v0, v1, v2) = (v2-v0) × (v1-v0) z-component
     */
    area_2x = (int64_t)(v2->x - v0->x) * (int64_t)(v1->y - v0->y)
              - (int64_t)(v2->y - v0->y) * (int64_t)(v1->x - v0->x);

    if (area_2x == 0) {
        return; /* Degenerate triangle */
    }

    /* Bounding box (expanded by 1 pixel for edge anti-aliasing) */
    min_x = gfx_sw_blend_floor_q8_to_int(MIN(v0->x, MIN(v1->x, v2->x))) - 1;
    min_y = gfx_sw_blend_floor_q8_to_int(MIN(v0->y, MIN(v1->y, v2->y))) - 1;
    max_x = gfx_sw_blend_ceil_q8_to_int(MAX(v0->x, MAX(v1->x, v2->x))) + 1;
    max_y = gfx_sw_blend_ceil_q8_to_int(MAX(v0->y, MAX(v1->y, v2->y))) + 1;

    /* Clip to buffer and clip area */
    min_x = MAX(min_x, MAX(buf_area->x1, clip_area->x1));
    min_y = MAX(min_y, MAX(buf_area->y1, clip_area->y1));
    max_x = MIN(max_x, MIN(buf_area->x2 - 1, clip_area->x2 - 1));
    max_y = MIN(max_y, MIN(buf_area->y2 - 1, clip_area->y2 - 1));

    if (min_x > max_x || min_y > max_y) {
        return;
    }
    perf_raster_pixels = (uint64_t)(max_x - min_x + 1) * (uint64_t)(max_y - min_y + 1);

    /*
     * Edge function for w0 (weight of v0):
     *   E0(x,y) = (x - v1x)*(v2y - v1y) - (y - v1y)*(v2x - v1x)
     *   dE0/dx = (v2y - v1y) = e0_a
     *   dE0/dy = (v1x - v2x) = e0_b  (note: negated x-diff)
     *
     * Edge function for w1 (weight of v1):
     *   E1(x,y) = (x - v2x)*(v0y - v2y) - (y - v2y)*(v0x - v2x)
     *   dE1/dx = (v0y - v2y) = e1_a
     *   dE1/dy = (v2x - v0x) = e1_b
     */
    e0_a = (int32_t)(v2->y - v1->y);
    e0_b = (int32_t)(v1->x - v2->x);
    e1_a = (int32_t)(v0->y - v2->y);
    e1_b = (int32_t)(v2->x - v0->x);
    e0_step_x = (int64_t)e0_a * XY_SUB_ONE;
    e0_step_y = (int64_t)e0_b * XY_SUB_ONE;
    e1_step_x = (int64_t)e1_a * XY_SUB_ONE;
    e1_step_y = (int64_t)e1_b * XY_SUB_ONE;

    {
        int32_t e2_a = -(e0_a + e1_a);
        int32_t e2_b = -(e0_b + e1_b);
        e0_len = gfx_sw_blend_isqrt_i64((uint64_t)((int64_t)e0_a * e0_a + (int64_t)e0_b * e0_b));
        e1_len = gfx_sw_blend_isqrt_i64((uint64_t)((int64_t)e1_a * e1_a + (int64_t)e1_b * e1_b));
        e2_len = gfx_sw_blend_isqrt_i64((uint64_t)((int64_t)e2_a * e2_a + (int64_t)e2_b * e2_b));
        if (e0_len < 1) {
            e0_len = 1;
        }
        if (e1_len < 1) {
            e1_len = 1;
        }
        if (e2_len < 1) {
            e2_len = 1;
        }
    }

    /*
     * UV gradient computation (fixed-point).
     *
     * u(x,y) = w0*u0 + w1*u1 + w2*u2  where w_i = E_i / area_2x
     *
     * du/dx = (e0_a * u0 + e1_a * u1 + (-e0_a - e1_a) * u2) / area_2x
     *       = (e0_a * (u0 - u2) + e1_a * (u1 - u2)) / area_2x
     *
     * We compute in fixed-point by shifting numerator up by FRAC_BITS.
     */
    {
        int64_t num;

        num = e0_step_x * (v0->u - v2->u) + e1_step_x * (v1->u - v2->u);
        du_dx = (int32_t)((num << FRAC_BITS) / area_2x);

        num = e0_step_x * (v0->v - v2->v) + e1_step_x * (v1->v - v2->v);
        dv_dx = (int32_t)((num << FRAC_BITS) / area_2x);

        num = e0_step_y * (v0->u - v2->u) + e1_step_y * (v1->u - v2->u);
        du_dy = (int32_t)((num << FRAC_BITS) / area_2x);

        num = e0_step_y * (v0->v - v2->v) + e1_step_y * (v1->v - v2->v);
        dv_dy = (int32_t)((num << FRAC_BITS) / area_2x);
    }

    /*
     * Compute edge function values at the starting point (min_x, min_y).
     */
    {
        int32_t sample_x_q8 = min_x * XY_SUB_ONE + XY_SUB_HALF;
        int32_t sample_y_q8 = min_y * XY_SUB_ONE + XY_SUB_HALF;
        int64_t w0_row = (int64_t)(sample_x_q8 - v1->x) * e0_a + (int64_t)(sample_y_q8 - v1->y) * e0_b;
        int64_t w1_row = (int64_t)(sample_x_q8 - v2->x) * e1_a + (int64_t)(sample_y_q8 - v2->y) * e1_b;

        /* UV at starting point (fixed-point) */
        int64_t u_start_num = (int64_t)w0_row * v0->u + (int64_t)w1_row * v1->u
                              + (int64_t)(area_2x - w0_row - w1_row) * v2->u;
        int64_t v_start_num = (int64_t)w0_row * v0->v + (int64_t)w1_row * v1->v
                              + (int64_t)(area_2x - w0_row - w1_row) * v2->v;
        int32_t u_row = (int32_t)((u_start_num << FRAC_BITS) / area_2x);
        int32_t v_row = (int32_t)((v_start_num << FRAC_BITS) / area_2x);

        const bool inward = (internal_edges & GFX_BLEND_TRI_AA_INWARD) != 0;
        uint8_t xaa_n = (inward && extra_aa_edges != NULL) ? extra_aa_count : 0;
        if (xaa_n > GFX_BLEND_MAX_EXTRA_AA_EDGES) {
            xaa_n = GFX_BLEND_MAX_EXTRA_AA_EDGES;
        }
        int64_t xaa_row[GFX_BLEND_MAX_EXTRA_AA_EDGES];
        int64_t xaa_sx[GFX_BLEND_MAX_EXTRA_AA_EDGES];
        int64_t xaa_sy[GFX_BLEND_MAX_EXTRA_AA_EDGES];
        for (uint8_t ei = 0; ei < xaa_n; ei++) {
            xaa_sx[ei] = (int64_t)extra_aa_edges[ei].a * XY_SUB_ONE;
            xaa_sy[ei] = (int64_t)extra_aa_edges[ei].b * XY_SUB_ONE;
            xaa_row[ei] = (int64_t)(sample_x_q8 - extra_aa_edges[ei].vx) * extra_aa_edges[ei].a
                          + (int64_t)(sample_y_q8 - extra_aa_edges[ei].vy) * extra_aa_edges[ei].b;
        }

        for (int32_t y = min_y; y <= max_y; y++) {
            int64_t w0 = w0_row;
            int64_t w1 = w1_row;
            int32_t u_cur = u_row;
            int32_t v_cur = v_row;
            int64_t xaa[GFX_BLEND_MAX_EXTRA_AA_EDGES];
            for (uint8_t ei = 0; ei < xaa_n; ei++) {
                xaa[ei] = xaa_row[ei];
            }
            uint8_t *dst_row = dst_base + ((size_t)(y - buf_area->y1) * dest_stride
                                           + (size_t)(min_x - buf_area->x1)) * dst_px_size;

            for (int32_t x = min_x; x <= max_x; x++) {
                int64_t w2 = area_2x - w0 - w1;

                if (gfx_sw_blend_triangle_sample_inside(area_2x, w0, w1, w2)) {
                    /* Inside triangle */
                    int32_t src_x = (u_cur + FRAC_HALF) >> FRAC_BITS;
                    int32_t src_y = (v_cur + FRAC_HALF) >> FRAC_BITS;
                    src_x = gfx_sw_blend_clamp_coord(src_x, 0, src_stride - 1);
                    src_y = gfx_sw_blend_clamp_coord(src_y, 0, src_height - 1);
                    const uint8_t *src_px = src_base + ((size_t)src_y * src_stride + (size_t)src_x) * src_px_size;
                    gfx_opa_t final_opa = opa;

                    /* Inward AA: fade pixels near non-internal outer edges */
                    if (inward) {
                        int32_t min_id = GFX_BLEND_TRI_EDGE_AA_RANGE;
                        uint8_t emask = internal_edges & 0x07;
                        if (area_2x > 0) {
                            if (!(emask & 0x01)) {
                                int32_t d = (int32_t)(w0 / e0_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            if (!(emask & 0x02)) {
                                int32_t d = (int32_t)(w1 / e1_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            if (!(emask & 0x04)) {
                                int32_t d = (int32_t)(w2 / e2_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            for (uint8_t ei = 0; ei < xaa_n; ei++) {
                                int32_t d = (int32_t)(xaa[ei] / extra_aa_edges[ei].len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                        } else {
                            if (!(emask & 0x01)) {
                                int32_t d = (int32_t)((-w0) / e0_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            if (!(emask & 0x02)) {
                                int32_t d = (int32_t)((-w1) / e1_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            if (!(emask & 0x04)) {
                                int32_t d = (int32_t)((-w2) / e2_len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                            for (uint8_t ei = 0; ei < xaa_n; ei++) {
                                int32_t d = (int32_t)((-xaa[ei]) / extra_aa_edges[ei].len);
                                if (d < min_id) {
                                    min_id = d;
                                }
                            }
                        }
                        if (min_id < GFX_BLEND_TRI_EDGE_AA_RANGE) {
                            gfx_opa_t edge_opa = (gfx_opa_t)((int32_t)min_id * 255 / GFX_BLEND_TRI_EDGE_AA_RANGE);
                            final_opa = (gfx_opa_t)(((uint32_t)final_opa * edge_opa + 128U) >> 8);
                            if (final_opa == 0U) {
                                goto next_pixel;
                            }
                        }
                    }

                    if (mask != NULL) {
                        gfx_opa_t src_opa = mask[(size_t)src_y * mask_stride + (size_t)src_x];
                        final_opa = (gfx_opa_t)(((uint32_t)final_opa * src_opa + 128) >> 8);
                    }
                    if (final_opa == 0U) {
                        goto next_pixel;
                    } else {
                        if (dst_is_rgb565) {
                            gfx_sw_blend_sample_to_rgb565_dst(dst_row, dest_format, src_px, src_format, final_opa);
                        } else {
                            gfx_sw_blend_sample_to_rgb888_dst(dst_row, dest_format, src_px, src_format, final_opa);
                        }
                    }
                } else {
                    /* Outside triangle */
                    if (inward) {
                        goto next_pixel;
                    }
                    /* Outward edge AA: blend if within 1 px.
                     * Skip distance for edges flagged as internal (shared
                     * with an adjacent triangle) to avoid dark-seam artifacts.
                     * Bit 0 = edge 0 (v1→v2), bit 1 = edge 1 (v2→v0),
                     * bit 2 = edge 2 (v0→v1). */
                    int32_t max_od = 0;
                    if (area_2x > 0) {
                        if (w0 < 0 && !(internal_edges & 0x01)) {
                            int32_t d = (int32_t)((-w0) / e0_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                        if (w1 < 0 && !(internal_edges & 0x02)) {
                            int32_t d = (int32_t)((-w1) / e1_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                        if (w2 < 0 && !(internal_edges & 0x04)) {
                            int32_t d = (int32_t)((-w2) / e2_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                    } else {
                        if (w0 > 0 && !(internal_edges & 0x01)) {
                            int32_t d = (int32_t)(w0 / e0_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                        if (w1 > 0 && !(internal_edges & 0x02)) {
                            int32_t d = (int32_t)(w1 / e1_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                        if (w2 > 0 && !(internal_edges & 0x04)) {
                            int32_t d = (int32_t)(w2 / e2_len);
                            if (d > max_od) {
                                max_od = d;
                            }
                        }
                    }

                    if (max_od > 0 && max_od < GFX_BLEND_TRI_EDGE_AA_RANGE) {
                        gfx_opa_t aa_opa = (gfx_opa_t)((GFX_BLEND_TRI_EDGE_AA_RANGE - max_od) * 255 / GFX_BLEND_TRI_EDGE_AA_RANGE);
                        int32_t src_x = (u_cur + FRAC_HALF) >> FRAC_BITS;
                        int32_t src_y = (v_cur + FRAC_HALF) >> FRAC_BITS;
                        src_x = gfx_sw_blend_clamp_coord(src_x, 0, src_stride - 1);
                        src_y = gfx_sw_blend_clamp_coord(src_y, 0, src_height - 1);
                        const uint8_t *src_px = src_base + ((size_t)src_y * src_stride + (size_t)src_x) * src_px_size;
                        if (opa < OPA_COVER) {
                            aa_opa = (gfx_opa_t)(((uint32_t)aa_opa * opa + 128U) >> 8);
                        }
                        if (mask != NULL) {
                            gfx_opa_t src_opa = mask[(size_t)src_y * mask_stride + (size_t)src_x];
                            aa_opa = (gfx_opa_t)(((uint32_t)aa_opa * src_opa + 128) >> 8);
                        }
                        if (aa_opa > 0) {
                            if (dst_is_rgb565) {
                                gfx_sw_blend_sample_to_rgb565_dst(dst_row, dest_format, src_px, src_format, aa_opa);
                            } else {
                                gfx_sw_blend_sample_to_rgb888_dst(dst_row, dest_format, src_px, src_format, aa_opa);
                            }
                        }
                    }
                }

next_pixel:
                w0 += e0_step_x;
                w1 += e1_step_x;
                u_cur += du_dx;
                v_cur += dv_dx;
                for (uint8_t ei = 0; ei < xaa_n; ei++) {
                    xaa[ei] += xaa_sx[ei];
                }
                dst_row += dst_px_size;
            }

            w0_row += e0_step_y;
            w1_row += e1_step_y;
            u_row += du_dy;
            v_row += dv_dy;
            for (uint8_t ei = 0; ei < xaa_n; ei++) {
                xaa_row[ei] += xaa_sy[ei];
            }
        }
    }

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->shape.calls++;
        s_active_perf_stats->shape.pixels += perf_raster_pixels;
        s_active_perf_stats->shape.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }

#undef FRAC_BITS
#undef FRAC_HALF
#undef XY_SUB_ONE
#undef XY_SUB_HALF
}

void gfx_sw_blend_polygon_fill(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                               gfx_color_t color,
                               gfx_opa_t opa,
                               const int32_t *vx, const int32_t *vy,
                               int vertex_count)
{
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            opa == 0U ||
            vx == NULL || vy == NULL || vertex_count < 3) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = gfx_platform_time_us();
    }

    gfx_sw_blend_polygon_fill_scanline_fallback(dest_buf, dest_stride, dest_format,
            buf_area, clip_area,
            color, opa, vx, vy, vertex_count);

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->shape.calls++;
        s_active_perf_stats->shape.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}
