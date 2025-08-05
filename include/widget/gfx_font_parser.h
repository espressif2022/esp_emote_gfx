/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

#define GFX_FONT_SUBPX_NONE    0

/**********************
 *      TYPEDEFS
 **********************/

/* Forward declarations */
typedef struct _gfx_font_glyph_dsc_t gfx_font_glyph_dsc_t;
typedef struct _gfx_lvgl_font_t gfx_lvgl_font_t;

/**
 * Font type enumeration
 */
typedef enum {
    GFX_FONT_TYPE_FREETYPE,    /**< FreeType font (TTF/OTF) */
    GFX_FONT_TYPE_LVGL_C,      /**< LVGL C format font */
} gfx_font_type_t;

/**
 * LVGL glyph description structure
 */
typedef struct _gfx_font_glyph_dsc_t {
    uint32_t bitmap_index;      /**< Start index in the bitmap array */
    uint32_t adv_w;            /**< Advance width */
    uint16_t box_w;            /**< Width of the glyph's bounding box */
    uint16_t box_h;            /**< Height of the glyph's bounding box */
    int16_t ofs_x;             /**< X offset of the bounding box */
    int16_t ofs_y;             /**< Y offset of the bounding box */
} gfx_font_glyph_dsc_t;

/**
 * Unified font handle structure
 */
typedef struct {
    gfx_font_type_t type;          /**< Font type */
    union {
        void *freetype_face;       /**< FreeType face handle */
        const gfx_lvgl_font_t *lvgl_font; /**< LVGL font structure */
    } font;
    const char *name;              /**< Font name */
} gfx_font_handle_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Parse LVGL C format font data
 * @param font_data Pointer to the font structure (e.g., &font_16)
 * @param font_name Name for the font
 * @param ret_handle Pointer to store the created font handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_parse_lvgl_font(const gfx_lvgl_font_t *font_data, const char *font_name, gfx_font_handle_t **ret_handle);

/**
 * @brief Convert external LVGL font (like your font_16) to internal format
 * @param external_font Pointer to external font structure
 * @param font_name Name for the font  
 * @param ret_handle Pointer to store the created font handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_convert_external_lvgl_font(const void *external_font, const char *font_name, gfx_font_handle_t **ret_handle);

/**
 * @brief Get glyph information from LVGL font
 * @param font LVGL font structure
 * @param unicode Unicode character
 * @param glyph_dsc Output glyph descriptor
 * @return true if glyph found, false otherwise
 */
bool gfx_lvgl_font_get_glyph_dsc(const gfx_lvgl_font_t *font, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc);

/**
 * @brief Get glyph bitmap from LVGL font
 * @param font LVGL font structure
 * @param glyph_dsc Glyph descriptor
 * @return Pointer to glyph bitmap data
 */
const uint8_t *gfx_lvgl_font_get_glyph_bitmap(const gfx_lvgl_font_t *font, const gfx_font_glyph_dsc_t *glyph_dsc);

/**
 * @brief Get advance width for a character from LVGL font
 * @param font LVGL font structure
 * @param unicode Unicode character
 * @return Advance width in pixels (multiplied by 256 for sub-pixel precision)
 */
uint32_t gfx_lvgl_font_get_glyph_width(const gfx_lvgl_font_t *font, uint32_t unicode);

/**
 * @brief Get glyph information from FreeType font (unified interface)
 * @param face FreeType face handle
 * @param font_size Font size in pixels
 * @param unicode Unicode character
 * @param glyph_dsc Output glyph descriptor
 * @return true if glyph found, false otherwise
 */
bool gfx_freetype_font_get_glyph_dsc(void *face, uint8_t font_size, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc);

/**
 * @brief Get glyph bitmap from FreeType font (unified interface)
 * @param face FreeType face handle
 * @param font_size Font size in pixels
 * @param glyph_dsc Glyph descriptor
 * @param bitmap_buffer Output buffer for bitmap data
 * @param buffer_size Size of the output buffer
 * @return Pointer to glyph bitmap data, or NULL on error
 */
const uint8_t *gfx_freetype_font_get_glyph_bitmap(void *face, uint8_t font_size, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc);

/**
 * @brief Get advance width for a character from FreeType font
 * @param face FreeType face handle
 * @param font_size Font size in pixels
 * @param unicode Unicode character
 * @return Advance width in pixels (multiplied by 256 for sub-pixel precision)
 */
uint32_t gfx_freetype_font_get_glyph_width(void *face, uint8_t font_size, uint32_t unicode);

/**
 * @brief Detect font type based on font pointer structure
 * @param font_ptr Pointer to font (either gfx_lvgl_font_t* or FT_Face)
 * @return Font type
 */
gfx_font_type_t gfx_detect_font_type(void *font_ptr);

#ifdef __cplusplus
}
#endif 