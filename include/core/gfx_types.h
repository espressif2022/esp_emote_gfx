/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/* Basic types */
typedef uint8_t     gfx_opa_t;      /**< Opacity (0-255) */
typedef int16_t     gfx_coord_t;    /**< Coordinate type */

/** Graphics handle type */
typedef void       *gfx_handle_t;      /**< Graphics handle type */

/* Color type with full member for compatibility */
typedef union {
    uint16_t full;                  /**< Full 16-bit color value */
} gfx_color_t;

/* Area structure */
typedef struct {
    gfx_coord_t x1;
    gfx_coord_t y1;
    gfx_coord_t x2;
    gfx_coord_t y2;
} gfx_area_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Convert a 32-bit hexadecimal color to gfx_color_t
 * @param c The 32-bit hexadecimal color to convert
 * @return Converted color in gfx_color_t type
 */
gfx_color_t gfx_color_hex(uint32_t c);


/**********************
 *      MACROS
 **********************/

/* Pixel size constants */
#define GFX_PIXEL_SIZE_16BPP   2  /**< 16-bit color format: 2 bytes per pixel */
#define GFX_PIXEL_SIZE_8BPP    1  /**< 8-bit format: 1 byte per pixel */

/**
 * @brief Calculate buffer pointer with offset for 16-bit format (RGB565)
 * @param buffer Base buffer pointer (any type)
 * @param y_offset Vertical offset in pixels
 * @param stride Width of buffer in pixels
 * @param x_offset Horizontal offset in pixels
 * @return Calculated gfx_color_t pointer with offset applied
 */
#define GFX_BUFFER_OFFSET_16BPP(buffer, y_offset, stride, x_offset) \
    ((uint8_t *)((uint8_t *)(buffer) + \
                     (y_offset) * (stride) * GFX_PIXEL_SIZE_16BPP + \
                     (x_offset) * GFX_PIXEL_SIZE_16BPP))

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

#define GFX_COLOR_HEX(color) ((gfx_color_t)gfx_color_hex(color))

#ifdef __cplusplus
}
#endif
