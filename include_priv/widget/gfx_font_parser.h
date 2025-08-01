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

/**
 * Font type enumeration
 */
typedef enum {
    GFX_FONT_TYPE_FREETYPE,    /**< FreeType font (TTF/OTF) */
    GFX_FONT_TYPE_LVGL_C,      /**< LVGL C format font */
} gfx_font_type_t;

/**
 * LVGL character mapping types (from LVGL)
 */
typedef enum {
    GFX_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    GFX_FONT_FMT_TXT_CMAP_FORMAT0_FULL,
    GFX_FONT_FMT_TXT_CMAP_SPARSE_TINY,
    GFX_FONT_FMT_TXT_CMAP_SPARSE_FULL,
} gfx_font_fmt_txt_cmap_type_t;

/**
 * LVGL glyph description structure (mirrors lv_font_fmt_txt_glyph_dsc_t)
 */
typedef struct {
    uint32_t bitmap_index;      /**< Start index in the bitmap array */
    uint32_t adv_w;            /**< Advance width */
    uint16_t box_w;            /**< Width of the glyph's bounding box */
    uint16_t box_h;            /**< Height of the glyph's bounding box */
    int16_t ofs_x;             /**< X offset of the bounding box */
    int16_t ofs_y;             /**< Y offset of the bounding box */
} gfx_font_glyph_dsc_t;

/**
 * LVGL character mapping structure (mirrors lv_font_fmt_txt_cmap_t)
 */
typedef struct {
    uint32_t range_start;               /**< First character code in this range */
    uint32_t range_length;              /**< Number of characters in this range */
    uint32_t glyph_id_start;           /**< First glyph ID for this range */
    const uint32_t *unicode_list;      /**< List of unicode values (if sparse) */
    const void *glyph_id_ofs_list;     /**< List of glyph ID offsets (if sparse) */
    uint32_t list_length;              /**< Length of unicode_list and glyph_id_ofs_list */
    gfx_font_fmt_txt_cmap_type_t type; /**< Type of this character map */
} gfx_font_cmap_t;

/**
 * LVGL font descriptor structure (mirrors lv_font_fmt_txt_dsc_t)
 */
typedef struct {
    const uint8_t *glyph_bitmap;           /**< Bitmap data of all glyphs */
    const gfx_font_glyph_dsc_t *glyph_dsc; /**< Array of glyph descriptions */
    const gfx_font_cmap_t *cmaps;          /**< Array of character maps */
    const void *kern_dsc;                  /**< Kerning data (not used yet) */
    uint16_t kern_scale;                   /**< Kerning scaling */
    uint16_t cmap_num;                     /**< Number of character maps */
    uint16_t bpp;                          /**< Bits per pixel */
    uint16_t kern_classes;                 /**< Number of kerning classes */
    uint16_t bitmap_format;                /**< Bitmap format */
} gfx_font_fmt_txt_dsc_t;

/**
 * LVGL font structure (mirrors lv_font_t)
 */
typedef struct {
    const void *get_glyph_dsc;     /**< Function pointer to get glyph's data */
    const void *get_glyph_bitmap;  /**< Function pointer to get glyph's bitmap */
    uint16_t line_height;          /**< The maximum line height required by the font */
    uint16_t base_line;            /**< Baseline measured from the bottom of the line */
    uint8_t subpx;                 /**< Subpixel configuration */
    int8_t underline_position;     /**< Underline position */
    uint8_t underline_thickness;   /**< Underline thickness */
    const gfx_font_fmt_txt_dsc_t *dsc; /**< The custom font data */
    bool static_bitmap;            /**< Static bitmap flag */
    const void *fallback;          /**< Fallback font */
    const void *user_data;         /**< User data */
} gfx_lvgl_font_t;

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
 * @brief Convert external LVGL font (like your font_16) to internal format
 * @param external_font Pointer to external font structure
 * @param font_name Name for the font  
 * @param ret_handle Pointer to store the created font handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_convert_external_lvgl_font(const void *external_font, const char *font_name, gfx_font_handle_t **ret_handle);

#ifdef __cplusplus
}
#endif 