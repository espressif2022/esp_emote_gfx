/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_font_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

#define GFX_ATTRIBUTE_LARGE_CONST   const   /**< Attribute for large constant data */

/**********************
 *      TYPEDEFS
 **********************/

/* Function pointer types for font operations */
typedef bool (*gfx_font_get_glyph_dsc_cb_t)(const gfx_lvgl_font_t *, gfx_font_glyph_dsc_t *, uint32_t letter, uint32_t letter_next);
typedef const void * (*gfx_font_get_glyph_bitmap_cb_t)(const gfx_font_glyph_dsc_t *, void *);

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
 * LVGL font structure (mirrors lv_font_t) - Internal definition
 */
struct _gfx_lvgl_font_t {
    gfx_font_get_glyph_dsc_cb_t get_glyph_dsc;     /**< Function pointer to get glyph's data */
    gfx_font_get_glyph_bitmap_cb_t get_glyph_bitmap;  /**< Function pointer to get glyph's bitmap */
    uint16_t line_height;          /**< The maximum line height required by the font */
    uint16_t base_line;            /**< Baseline measured from the bottom of the line */
    uint8_t subpx;                 /**< Subpixel configuration */
    int8_t underline_position;     /**< Underline position */
    uint8_t underline_thickness;   /**< Underline thickness */
    const gfx_font_fmt_txt_dsc_t *dsc; /**< The custom font data */
    bool static_bitmap;            /**< Static bitmap flag */
    const void *fallback;          /**< Fallback font */
    const void *user_data;         /**< User data */
};

/**********************
 * INTERNAL PROTOTYPES
 **********************/

/**
 * @brief Standard LVGL-compatible functions for font rendering
 */
bool gfx_font_get_glyph_dsc_fmt_txt(const gfx_lvgl_font_t *font, gfx_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next);

const void *gfx_font_get_bitmap_fmt_txt(const gfx_font_glyph_dsc_t *dsc_in, void *draw_buf);

#ifdef __cplusplus
}
#endif 