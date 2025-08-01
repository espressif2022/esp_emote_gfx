/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "widget/gfx_font_parser.h"

/*********************
 *      DEFINES
 *********************/

static const char *TAG = "gfx_font_parser";

/**********************
 *   STATIC FUNCTIONS
 **********************/

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
        
        if (unicode < cmap->range_start) {
            continue;
        }
        
        if (cmap->type == GFX_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            // Simple range mapping
            if (unicode >= cmap->range_start && 
                unicode < cmap->range_start + cmap->range_length) {
                return cmap->glyph_id_start + (unicode - cmap->range_start);
            }
        }
        // Add support for other mapping types as needed
    }
    
    return 0; // Glyph not found
}

/**
 * Convert external LVGL font structures to internal format
 */
static esp_err_t convert_external_font_structures(const void *external_font, gfx_lvgl_font_t *internal_font)
{
    // This is a simplified conversion - you may need to adjust based on your actual LVGL font structure
    // The external font should have a similar structure but with potentially different field names
    
    // For now, we'll assume the external font has the same structure as our internal format
    // In practice, you might need to map fields differently
    
    // Cast to a temporary structure that matches your external font format
    typedef struct {
        const void *get_glyph_dsc;
        const void *get_glyph_bitmap; 
        uint16_t line_height;
        uint16_t base_line;
        uint8_t subpx;
        int8_t underline_position;
        uint8_t underline_thickness;
        bool static_bitmap;
        const void *dsc;  // This points to the font descriptor
        const void *fallback;
        const void *user_data;
    } external_lv_font_t;
    
    const external_lv_font_t *ext_font = (const external_lv_font_t *)external_font;
    
    // Copy the font properties
    internal_font->get_glyph_dsc = ext_font->get_glyph_dsc;
    internal_font->get_glyph_bitmap = ext_font->get_glyph_bitmap;
    internal_font->line_height = ext_font->line_height;
    internal_font->base_line = ext_font->base_line;
    internal_font->subpx = ext_font->subpx;
    internal_font->underline_position = ext_font->underline_position;
    internal_font->underline_thickness = ext_font->underline_thickness;
    internal_font->static_bitmap = ext_font->static_bitmap;
    internal_font->fallback = ext_font->fallback;
    internal_font->user_data = ext_font->user_data;
    
    // Convert the font descriptor
    if (ext_font->dsc) {
        // Cast the external descriptor to our internal format
        // Note: This assumes the structures are compatible
        internal_font->dsc = (const gfx_font_fmt_txt_dsc_t *)ext_font->dsc;
    } else {
        internal_font->dsc = NULL;
    }
    
    return ESP_OK;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

esp_err_t gfx_parse_lvgl_font(const gfx_lvgl_font_t *font_data, const char *font_name, gfx_font_handle_t **ret_handle)
{
    ESP_RETURN_ON_FALSE(font_data && font_name && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    
    // Allocate font handle
    gfx_font_handle_t *handle = (gfx_font_handle_t *)calloc(1, sizeof(gfx_font_handle_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "no memory for font handle");
    
    // Allocate name string
    handle->name = strdup(font_name);
    if (!handle->name) {
        free(handle);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "no memory for font name");
    }
    
    // Set font type and data
    handle->type = GFX_FONT_TYPE_LVGL_C;
    handle->font.lvgl_font = font_data;
    
    *ret_handle = handle;
    
    ESP_LOGI(TAG, "Parsed LVGL font: %s", font_name);
    
    return ESP_OK;
}

esp_err_t gfx_convert_external_lvgl_font(const void *external_font, const char *font_name, gfx_font_handle_t **ret_handle)
{
    ESP_RETURN_ON_FALSE(external_font && font_name && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    
    // Allocate font handle
    gfx_font_handle_t *handle = (gfx_font_handle_t *)calloc(1, sizeof(gfx_font_handle_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "no memory for font handle");
    
    // Allocate internal font structure
    gfx_lvgl_font_t *internal_font = (gfx_lvgl_font_t *)calloc(1, sizeof(gfx_lvgl_font_t));
    if (!internal_font) {
        free(handle);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "no memory for internal font");
    }
    
    // Convert external font structure to internal format
    esp_err_t ret = convert_external_font_structures(external_font, internal_font);
    if (ret != ESP_OK) {
        free(internal_font);
        free(handle);
        ESP_RETURN_ON_ERROR(ret, TAG, "failed to convert external font");
    }
    
    // Allocate name string
    handle->name = strdup(font_name);
    if (!handle->name) {
        free(internal_font);
        free(handle);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "no memory for font name");
    }
    
    // Set font type and data
    handle->type = GFX_FONT_TYPE_LVGL_C;
    handle->font.lvgl_font = internal_font;
    
    *ret_handle = handle;
    
    ESP_LOGI(TAG, "Converted external LVGL font: %s", font_name);
    
    return ESP_OK;
}

bool gfx_lvgl_font_get_glyph_dsc(const gfx_lvgl_font_t *font, uint32_t unicode, gfx_font_glyph_dsc_t *glyph_dsc)
{
    if (!font || !glyph_dsc || !font->dsc) {
        return false;
    }
    
    uint32_t glyph_index = gfx_font_get_glyph_index(font, unicode);
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

/**
 * @brief Get advance width for a character from LVGL font
 * @param font LVGL font structure
 * @param unicode Unicode character
 * @return Advance width in pixels (multiplied by 256 for sub-pixel precision)
 */
uint32_t gfx_lvgl_font_get_glyph_width(const gfx_lvgl_font_t *font, uint32_t unicode)
{
    gfx_font_glyph_dsc_t glyph_dsc;
    if (gfx_lvgl_font_get_glyph_dsc(font, unicode, &glyph_dsc)) {
        return glyph_dsc.adv_w;
    }
    return 0;
} 