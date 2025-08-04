/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
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

        if (unicode < cmap->range_start) {
            continue;
        }

        if (cmap->type == GFX_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            // Simple range mapping
            if (unicode >= cmap->range_start &&
                    unicode < cmap->range_start + cmap->range_length) {
                return cmap->glyph_id_start + (unicode - cmap->range_start);
            }
        } else if (cmap->type == GFX_FONT_FMT_TXT_CMAP_SPARSE_TINY) {
            // Sparse mapping using binary search
            if (cmap->unicode_list && cmap->list_length > 0) {
                // Convert unicode to 16-bit for lookup (may truncate for very large Unicode values)
                uint32_t rcp = unicode - cmap->range_start;
                uint16_t key = (uint16_t)rcp;
                uint16_t *found = (uint16_t *)gfx_utils_bsearch(&key, cmap->unicode_list, cmap->list_length,
                                                                sizeof(cmap->unicode_list[0]), unicode_list_compare);

                ESP_LOGI(TAG, "unicode: %lu, found: %p", unicode, found);
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
    ESP_LOGI(TAG, "glyph_index: %lu, unicode: %lu", glyph_index, unicode);
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