/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "../../include_priv/widget/gfx_font_parser.h"

/*********************
 *      DEFINES
 *********************/

static const char *TAG = "gfx_font_parser";

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Compare function for binary search in unicode list
 */
static int unicode_list_compare(const void *ref, const void *element)
{
    uint16_t ref_val = *(const uint16_t *)ref;
    uint16_t element_val = *(const uint16_t *)element;

    if (ref_val < element_val) {
        return -1;
    }
    if (ref_val > element_val) {
        return 1;
    }
    return 0;
}

void * gfx_utils_bsearch(const void * key, const void * base, uint32_t n, uint32_t size,
                         int (*cmp)(const void * pRef, const void * pElement))
{
    const char * middle;
    int32_t c;

    for (middle = base; n != 0;) {
        middle += (n / 2) * size;
        if ((c = (*cmp)(key, middle)) > 0) {
            n    = (n / 2) - ((n & 1) == 0);
            base = (middle += size);
        } else if (c < 0) {
            n /= 2;
            middle = base;
        } else {
            return (char *)middle;
        }
    }
    return NULL;
}

/**
 * Map unicode character to glyph index using LVGL character mapping
 */
static uint32_t gfx_font_get_glyph_index(const gfx_lvgl_font_t *font, uint32_t unicode)
{
    if (!font || !font->dsc || !font->dsc->cmaps) {
        return 0;
    }

    const gfx_font_fmt_txt_dsc_t *dsc = font->dsc;

    for (uint16_t i = 0; i < dsc->cmap_num; i++) {
        const gfx_font_cmap_t *cmap = &dsc->cmaps[i];

        uint32_t rcp = unicode - cmap->range_start;
        if (rcp > cmap->range_length) {
            continue;
        }

        if (cmap->type == GFX_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            if (unicode >= cmap->range_start &&
                    unicode < cmap->range_start + cmap->range_length) {
                return cmap->glyph_id_start + (unicode - cmap->range_start);
            }
        } else if (cmap->type == GFX_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            const uint8_t * gid_ofs_8 = cmap->glyph_id_ofs_list;
            return cmap->glyph_id_start + gid_ofs_8[rcp];
        } else if (cmap->type == GFX_FONT_FMT_TXT_CMAP_SPARSE_TINY) {
            if (cmap->unicode_list && cmap->list_length > 0) {
                uint16_t key = (uint16_t)rcp;
                uint16_t *found = (uint16_t *)gfx_utils_bsearch(&key, cmap->unicode_list, cmap->list_length,
                                                                sizeof(cmap->unicode_list[0]), unicode_list_compare);
                if (found) {
                    uintptr_t offset = found - cmap->unicode_list;
                    return cmap->glyph_id_start + offset;
                }
            }
        }
    }

    return 0; // Glyph not found
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

bool gfx_lvgl_font_get_glyph_dsc(const gfx_lvgl_font_t *font, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc)
{
    if (!font || !glyph_dsc || !font->dsc) {
        return false;
    }

    uint32_t glyph_index = gfx_font_get_glyph_index(font, unicode);
    ESP_LOGD(TAG, "glyph_index: %lu, unicode: %lu", glyph_index, unicode);
    if (glyph_index == 0) {
        return false; // Glyph not found
    }

    const gfx_font_fmt_txt_dsc_t *dsc = font->dsc;
    if (glyph_index >= 65536 || !dsc->glyph_dsc) { // Reasonable bounds check
        return false;
    }

    // Copy glyph descriptor
    // Note: We assume glyph_dsc structure is compatible between external and internal formats
    const void *src_glyph = &dsc->glyph_dsc[glyph_index];
    memcpy(glyph_dsc, src_glyph, sizeof(gfx_font_glyph_dsc_t));

    return true;
}

const uint8_t *gfx_lvgl_font_get_glyph_bitmap(const gfx_lvgl_font_t *font, const gfx_font_glyph_dsc_t *glyph_dsc)
{
    if (!font || !glyph_dsc || !font->dsc || !font->dsc->glyph_bitmap) {
        return NULL;
    }

    return &font->dsc->glyph_bitmap[glyph_dsc->bitmap_index];
}

uint32_t gfx_lvgl_font_get_glyph_width(const gfx_lvgl_font_t *font, uint32_t unicode)
{
    gfx_font_glyph_dsc_t glyph_dsc;
    if (gfx_lvgl_font_get_glyph_dsc(font, unicode, &glyph_dsc)) {
        return glyph_dsc.adv_w;
    }
    return 0;
}

bool gfx_freetype_font_get_glyph_dsc(void *face, uint8_t font_size, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc)
{
    if (!face || !glyph_dsc) {
        return false;
    }

    FT_Face ft_face = (FT_Face)face;
    FT_Error error;

    // Set font size
    error = FT_Set_Pixel_Sizes(ft_face, 0, font_size);
    if (error) {
        ESP_LOGE(TAG, "Failed to set font size: %d", error);
        return false;
    }

    // Get glyph index
    FT_UInt glyph_index = FT_Get_Char_Index(ft_face, unicode);
    if (glyph_index == 0) {
        return false; // Glyph not found
    }

    // Load glyph
    error = FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
        ESP_LOGE(TAG, "Failed to load glyph: %d", error);
        return false;
    }

    // Convert FreeType metrics to LVGL-compatible format
    FT_GlyphSlot slot = ft_face->glyph;

    // Calculate advance width in 1/256 pixels (LVGL format)
    glyph_dsc->adv_w = (slot->advance.x >> 6) << 8;  // Convert from 1/64 to 1/256 pixels
    glyph_dsc->box_w = 0; // Will be set after rendering
    glyph_dsc->box_h = 0; // Will be set after rendering
    glyph_dsc->ofs_x = 0; // Will be set after rendering
    glyph_dsc->ofs_y = 0; // Will be set after rendering
    glyph_dsc->bitmap_index = 0; // Not used for FreeType

    return true;
}

const uint8_t *gfx_freetype_font_get_glyph_bitmap(void *face, uint8_t font_size, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc)
{
    if (!face || !glyph_dsc) {
        return NULL;
    }

    FT_Face ft_face = (FT_Face)face;
    FT_Error error;

    // Set font size
    error = FT_Set_Pixel_Sizes(ft_face, 0, font_size);
    if (error) {
        ESP_LOGE(TAG, "Failed to set font size: %d", error);
        return NULL;
    }

    // Get glyph index
    FT_UInt glyph_index = FT_Get_Char_Index(ft_face, unicode);
    if (glyph_index == 0) {
        return NULL; // Glyph not found
    }

    // Load and render glyph
    error = FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
        ESP_LOGE(TAG, "Failed to load glyph: %d", error);
        return NULL;
    }

    error = FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        ESP_LOGE(TAG, "Failed to render glyph: %d", error);
        return NULL;
    }

    FT_GlyphSlot slot = ft_face->glyph;

    // Update glyph descriptor with actual rendered dimensions
    glyph_dsc->box_w = slot->bitmap.width;
    glyph_dsc->box_h = slot->bitmap.rows;
    glyph_dsc->ofs_x = slot->bitmap_left;
    // Convert FreeType baseline to LVGL format
    int line_height = (ft_face->size->metrics.height >> 6);
    int base_line = -(ft_face->size->metrics.descender >> 6);
    glyph_dsc->ofs_y = line_height - base_line - slot->bitmap_top;

    // Return bitmap buffer directly (caller must use immediately)
    return slot->bitmap.buffer;
}

uint32_t gfx_freetype_font_get_glyph_width(void *face, uint8_t font_size, uint32_t unicode)
{
    gfx_font_glyph_dsc_t glyph_dsc;
    if (gfx_freetype_font_get_glyph_dsc(face, font_size, unicode, &glyph_dsc)) {
        return glyph_dsc.adv_w;
    }
    return 0;
}

/**
 * @brief Detect font type based on font pointer structure
 * @param font_ptr Pointer to font (either gfx_lvgl_font_t* or FT_Face)
 * @return Font type
 */
gfx_font_type_t gfx_detect_font_type(void *font_ptr)
{
    if (!font_ptr) {
        return GFX_FONT_TYPE_FREETYPE;
    }

    gfx_lvgl_font_t *lvgl_font = (gfx_lvgl_font_t *)font_ptr;

    if (lvgl_font->dsc != NULL &&
            lvgl_font->line_height > 0 &&
            lvgl_font->line_height < 1000) {
        return GFX_FONT_TYPE_LVGL_C;
    }

    return GFX_FONT_TYPE_FREETYPE;
}