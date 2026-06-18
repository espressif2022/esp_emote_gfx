/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/* Pixel size constants */
#define GFX_PIXEL_SIZE_32BPP   4  /**< 32-bit color format: 4 bytes per pixel */
#define GFX_PIXEL_SIZE_24BPP   3  /**< 24-bit color format: 3 bytes per pixel */
#define GFX_PIXEL_SIZE_16BPP   2  /**< 16-bit color format: 2 bytes per pixel */
#define GFX_PIXEL_SIZE_8BPP    1  /**< 8-bit format: 1 byte per pixel */

/**
 * @brief Calculate buffer pointer with offset for 16-bit format (RGB565)
 * @param buffer Base buffer pointer (any type)
 * @param y_offset Vertical offset in pixels
 * @param stride Width of buffer in pixels
 * @param x_offset Horizontal offset in pixels
 * @return Calculated uint8_t pointer with offset applied
 */
#define GFX_BUFFER_OFFSET_16BPP(buffer, y_offset, stride, x_offset) \
    ((uint8_t *)((uint8_t *)(buffer) + \
                     (y_offset) * (stride) * GFX_PIXEL_SIZE_16BPP + \
                     (x_offset) * GFX_PIXEL_SIZE_16BPP))

/**
 * @brief Calculate buffer pointer with offset for 24-bit format (RGB/BGR888)
 * @param buffer Base buffer pointer (any type)
 * @param y_offset Vertical offset in pixels
 * @param stride Width of buffer in pixels
 * @param x_offset Horizontal offset in pixels
 * @return Calculated uint8_t pointer with offset applied
 */
#define GFX_BUFFER_OFFSET_24BPP(buffer, y_offset, stride, x_offset) \
    ((uint8_t *)((uint8_t *)(buffer) + \
                     (y_offset) * (stride) * GFX_PIXEL_SIZE_24BPP + \
                     (x_offset) * GFX_PIXEL_SIZE_24BPP))

/**
 * @brief Calculate buffer pointer with offset for 8-bit format
 * @param buffer Base buffer pointer (any type)
 * @param y_offset Vertical offset in pixels
 * @param stride Width of buffer in pixels
 * @param x_offset Horizontal offset in pixels
 * @return Calculated uint8_t pointer with offset applied
 */
#define GFX_BUFFER_OFFSET_8BPP(buffer, y_offset, stride, x_offset) \
    ((uint8_t *)((uint8_t *)(buffer) + \
                 (y_offset) * (stride) * GFX_PIXEL_SIZE_8BPP + \
                 (x_offset) * GFX_PIXEL_SIZE_8BPP))

/**
 * @brief Calculate buffer pointer with offset for 4-bit format (2 pixels per byte)
 * @param buffer Base buffer pointer (any type)
 * @param y_offset Vertical offset in pixels
 * @param stride Width of buffer in pixels (will be divided by 2)
 * @param x_offset Horizontal offset in pixels (will be divided by 2)
 * @return Calculated uint8_t pointer with offset applied
 */
#define GFX_BUFFER_OFFSET_4BPP(buffer, y_offset, stride, x_offset) \
    ((uint8_t *)((uint8_t *)(buffer) + \
                 (y_offset) * ((stride) / 2) + \
                 (x_offset) / 2))

/**
 * @brief Swap a 16-bit color value when the target byte order requires it.
 * @param color 16-bit RGB565 value.
 * @param swap Whether to swap the byte order.
 * @return Original or byte-swapped 16-bit value.
 */
static inline uint16_t gfx_color_maybe_swap_u16(uint16_t color, bool swap)
{
    return swap ? (uint16_t)__builtin_bswap16(color) : color;
}

/**
 * @brief Convert a semantic gfx_color_t to native framebuffer order.
 *
 * Use this helper only when writing raw 16-bit pixels directly into a buffer
 * or calling raw fill helpers that do not accept a separate `swap` argument.
 *
 * @param color Semantic RGB565 color value.
 * @param swap Whether the target buffer expects swapped byte order.
 * @return Native-order 16-bit pixel value for the target buffer.
 */
static inline uint16_t gfx_color_to_native_u16(gfx_color_t color, bool swap)
{
    return gfx_color_maybe_swap_u16(color.full, swap);
}

/**
 * @brief Convert a native framebuffer pixel back to semantic RGB565.
 * @param native_color Native-order 16-bit pixel value read from a target buffer.
 * @param swap Whether the target buffer expects swapped byte order.
 * @return Semantic RGB565 color value.
 */
static inline gfx_color_t gfx_color_from_native_u16(uint16_t native_color, bool swap)
{
    return (gfx_color_t) {
        .full = gfx_color_maybe_swap_u16(native_color, swap),
    };
}

/**
 * @brief Convert RGB888 channels to semantic RGB565.
 * @param r Red channel, 0-255.
 * @param g Green channel, 0-255.
 * @param b Blue channel, 0-255.
 * @return Semantic RGB565 color value.
 */
static inline gfx_color_t gfx_color_from_rgb888(uint8_t r, uint8_t g, uint8_t b)
{
    return (gfx_color_t) {
        .full = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                           ((uint16_t)(g & 0xFCU) << 3) |
                           ((uint16_t)b >> 3)),
    };
}

static inline void gfx_color_to_rgb888(uint16_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (r != NULL) {
        uint8_t r5 = (uint8_t)((rgb565 >> 11) & 0x1fU);
        *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    }
    if (g != NULL) {
        uint8_t g6 = (uint8_t)((rgb565 >> 5) & 0x3fU);
        *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    }
    if (b != NULL) {
        uint8_t b5 = (uint8_t)(rgb565 & 0x1fU);
        *b = (uint8_t)((b5 << 3) | (b5 >> 2));
    }
}

/**
 * @brief Get bits per pixel for a color format.
 * @param cf Color format.
 * @return Bits per pixel, or 0 for unsupported/unknown formats.
 */
static inline uint8_t gfx_color_format_get_bpp(gfx_color_format_t cf)
{
    switch (cf) {
    case GFX_COLOR_FORMAT_RGB565:
    case GFX_COLOR_FORMAT_RGB565_SWAPPED:
    case GFX_COLOR_FORMAT_RGB565A8:
    case GFX_COLOR_FORMAT_RGB565A8_SWAPPED:
        return 16;
    case GFX_COLOR_FORMAT_RGB888:
    case GFX_COLOR_FORMAT_BGR888:
    case GFX_COLOR_FORMAT_RGB888A8:
        return 24;
    case GFX_COLOR_FORMAT_XRGB8888:
    case GFX_COLOR_FORMAT_ARGB8888:
        return 32;
    default:
        return 0;
    }
}

/**
 * @brief Get bytes per pixel for a color format.
 * @param cf Color format.
 * @return Bytes per pixel, or 0 for unsupported/unknown formats.
 */
static inline uint8_t gfx_color_format_get_size(gfx_color_format_t cf)
{
    return (uint8_t)((gfx_color_format_get_bpp(cf) + 7U) >> 3);
}

/**
 * @brief Check whether a format is an RGB565-family format.
 * @param cf Color format.
 * @return True when the color payload is RGB565-based.
 */
static inline bool gfx_color_format_is_rgb565(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_RGB565 ||
           cf == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
           cf == GFX_COLOR_FORMAT_RGB565A8 ||
           cf == GFX_COLOR_FORMAT_RGB565A8_SWAPPED;
}

/**
 * @brief Check whether a color format can be used as an image source.
 * @param cf Color format.
 * @return True when the draw path can sample this image format.
 */
static inline bool gfx_color_format_is_image_supported(gfx_color_format_t cf)
{
    return gfx_color_format_is_rgb565(cf) ||
           cf == GFX_COLOR_FORMAT_RGB888 ||
           cf == GFX_COLOR_FORMAT_BGR888 ||
           cf == GFX_COLOR_FORMAT_RGB888A8 ||
           cf == GFX_COLOR_FORMAT_XRGB8888 ||
           cf == GFX_COLOR_FORMAT_ARGB8888;
}

static inline bool gfx_color_format_is_24bit(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_RGB888 ||
           cf == GFX_COLOR_FORMAT_BGR888 ||
           cf == GFX_COLOR_FORMAT_RGB888A8;
}

static inline bool gfx_color_format_is_bgr888(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_BGR888;
}

/**
 * @brief Check whether a format carries a separate alpha payload.
 * @param cf Color format.
 * @return True when the format has a trailing A8 plane after the color payload.
 */
static inline bool gfx_color_format_has_plane_alpha(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_RGB565A8 ||
           cf == GFX_COLOR_FORMAT_RGB565A8_SWAPPED ||
           cf == GFX_COLOR_FORMAT_RGB888A8;
}

/**
 * @brief Check whether alpha is stored inside each pixel.
 * @param cf Color format.
 * @return True when the format has per-pixel alpha.
 */
static inline bool gfx_color_format_has_pixel_alpha(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_ARGB8888;
}

/**
 * @brief Check whether a format carries any alpha information.
 * @param cf Color format.
 * @return True when the format has pixel alpha or a trailing alpha plane.
 */
static inline bool gfx_color_format_has_alpha(gfx_color_format_t cf)
{
    return gfx_color_format_has_plane_alpha(cf) ||
           gfx_color_format_has_pixel_alpha(cf);
}

/**
 * @brief Check whether RGB565 bytes are stored high-byte first.
 * @param cf Color format.
 * @return True for high-byte-first RGB565-family formats.
 */
static inline bool gfx_color_format_is_rgb565_swapped(gfx_color_format_t cf)
{
    return cf == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
           cf == GFX_COLOR_FORMAT_RGB565A8_SWAPPED;
}

/**
 * @brief Read an RGB565-family byte stream as a semantic RGB565 value.
 * @param src Pointer to two RGB565 payload bytes.
 * @param cf RGB565-family format describing byte order.
 * @return Semantic RGB565 value.
 */
static inline uint16_t gfx_color_read_rgb565_bytes(const uint8_t *src, gfx_color_format_t cf)
{
    if (gfx_color_format_is_rgb565_swapped(cf)) {
        return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    }
    return (uint16_t)(((uint16_t)src[1] << 8) | src[0]);
}

static inline void gfx_color_read_rgb888_bytes(const uint8_t *src, gfx_color_format_t cf,
        uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (gfx_color_format_is_bgr888(cf)) {
        if (b != NULL) {
            *b = src[0];
        }
        if (g != NULL) {
            *g = src[1];
        }
        if (r != NULL) {
            *r = src[2];
        }
        return;
    }

    if (r != NULL) {
        *r = src[0];
    }
    if (g != NULL) {
        *g = src[1];
    }
    if (b != NULL) {
        *b = src[2];
    }
}

static inline void gfx_color_write_rgb888_bytes(uint8_t *dst, gfx_color_format_t cf,
        uint8_t r, uint8_t g, uint8_t b)
{
    if (gfx_color_format_is_bgr888(cf)) {
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        return;
    }

    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
}

/**
 * @brief Write a semantic RGB565 value into an RGB565-family byte stream.
 * @param dst Pointer to two writable bytes.
 * @param cf RGB565-family format describing byte order.
 * @param color Semantic RGB565 value.
 */
static inline void gfx_color_write_rgb565_bytes(uint8_t *dst, gfx_color_format_t cf, uint16_t color)
{
    if (gfx_color_format_is_rgb565_swapped(cf)) {
        dst[0] = (uint8_t)((color >> 8) & 0xffU);
        dst[1] = (uint8_t)(color & 0xffU);
    } else {
        dst[0] = (uint8_t)(color & 0xffU);
        dst[1] = (uint8_t)((color >> 8) & 0xffU);
    }
}

#ifdef __cplusplus
}
#endif
