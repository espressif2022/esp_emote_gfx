/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_COLOR_HEX(color) ((gfx_color_t)gfx_color_hex(color))

/**********************
 *      TYPEDEFS
 **********************/

/* Basic types */
typedef uint8_t     gfx_opa_t;      /**< Opacity (0-255) */
typedef int16_t     gfx_coord_t;    /**< Coordinate type */

/** Graphics handle type */
typedef void       *gfx_handle_t;      /**< Graphics handle type */

/* Semantic RGB565 color used by draw APIs and RGB565 framebuffers. */
typedef union {
    uint16_t full;                  /**< Semantic RGB565 color value */
} gfx_color_t;

/* Color format enumeration. Keep colors semantic; use formats for buffers/images. */
typedef enum {
    GFX_COLOR_FORMAT_UNKNOWN        = 0x00,
    GFX_COLOR_FORMAT_RGB565         = 0x04,  /**< RGB565 low-byte, high-byte payload */
    GFX_COLOR_FORMAT_RGB565_SWAPPED = 0x05,  /**< RGB565 high-byte, low-byte payload */
    GFX_COLOR_FORMAT_RGB565A8       = 0x0A,  /**< RGB565 payload followed by alpha payload */
    GFX_COLOR_FORMAT_RGB565A8_SWAPPED = 0x0B, /**< Swapped RGB565 payload followed by alpha payload */
    GFX_COLOR_FORMAT_RGB888         = 0x0F,  /**< RGB888 payload, 3 bytes per pixel */
    GFX_COLOR_FORMAT_BGR888         = 0x13,  /**< BGR888 payload, 3 bytes per pixel */
    GFX_COLOR_FORMAT_RGB888A8       = 0x10,  /**< RGB888 payload followed by alpha payload */
    GFX_COLOR_FORMAT_XRGB8888       = 0x11,  /**< XRGB8888 payload, X ignored, 4 bytes per pixel */
    GFX_COLOR_FORMAT_ARGB8888       = 0x12,  /**< ARGB8888 payload, alpha in high byte of 0xAARRGGBB */
    GFX_COLOR_FORMAT_NATIVE         = GFX_COLOR_FORMAT_RGB565,
    GFX_COLOR_FORMAT_NATIVE_WITH_ALPHA = GFX_COLOR_FORMAT_RGB565A8,
} gfx_color_format_t;

/* Area structure */
typedef struct {
    gfx_coord_t x1;
    gfx_coord_t y1;
    gfx_coord_t x2;
    gfx_coord_t y2;
} gfx_area_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Convert a 32-bit hexadecimal color to gfx_color_t
 * @param c The 32-bit hexadecimal color to convert
 * @return Converted color in gfx_color_t type
 */
gfx_color_t gfx_color_hex(uint32_t c);

#ifdef __cplusplus
}
#endif
