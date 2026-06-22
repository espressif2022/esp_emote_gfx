/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NOTE: This file contains code derived from LVGL v8.4
 * Copyright (c) 2024 LVGL LLC
 * Used for Unicode glyph index search and font format decoding
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_FONT_LVGL
#include "common/gfx_log_priv.h"
#include "fonts/gfx_font_lvgl_priv.h"
#include "fonts/gfx_font_priv.h"

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "font_lvgl";

#if GFX_HOST_BUILD
typedef struct {
    const uint8_t *glyph_dsc_bin;
    uint8_t glyph_dsc_stride;
} gfx_font_lv_runtime_t;
#endif

/**********************
 *   STATIC PROTOTYPES
 **********************/

static int unicode_list_compare(const void *ref, const void *element);
static void *lv_use_utils_bsearch(const void *key, const void *base, uint32_t n, uint32_t size,
                                  int (*cmp)(const void *pRef, const void *pElement));

static uint32_t gfx_font_lv_get_glyph_index(const lv_font_t *font, uint32_t unicode);
static bool gfx_font_lv_get_glyph_dsc(gfx_font_handle_t font_adapter, void *glyph_dsc, uint32_t unicode, uint32_t unicode_next);
static const uint8_t *gfx_font_lv_get_glyph_bitmap(gfx_font_handle_t font_adapter, uint32_t unicode, void *glyph_dsc);
static int gfx_font_lv_get_glyph_width(gfx_font_handle_t font_adapter, uint32_t unicode);
static int gfx_font_lv_get_line_height(gfx_font_handle_t font_adapter);
static int gfx_font_lv_get_base_line(gfx_font_handle_t font_adapter);
static uint8_t gfx_font_lv_get_pixel_value(gfx_font_handle_t font_adapter, const uint8_t *bitmap, int32_t x, int32_t y, int32_t box_w);
static int gfx_font_lv_adjust_baseline_offset(gfx_font_handle_t font_adapter, void *glyph_dsc);
static int gfx_font_lv_get_advance_width(gfx_font_handle_t font_adapter, void *glyph_dsc);
static uint16_t gfx_font_lv_bin_u16(const uint8_t *addr);
static uint32_t gfx_font_lv_bin_u32(const uint8_t *addr);

static void *malloc_cpy(void *src, size_t sz);
static void addr_add(void **addr, uintptr_t add);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void *malloc_cpy(void *src, size_t sz)
{
    void *p = malloc(sz);
    if (!p) {
        GFX_LOGE(TAG, "load lvgl font: allocate memory failed");
        return NULL;
    }
    memcpy(p, src, sz);
    return p;
}

static void addr_add(void **addr, uintptr_t add)
{
    if (*addr) {
        *addr = (void *)((uintptr_t) * addr + add);
    }
}

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

static void *lv_use_utils_bsearch(const void *key, const void *base, uint32_t n, uint32_t size,
                                  int (*cmp)(const void *pRef, const void *pElement))
{
    const char *middle;
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

/**********************
 *   INTERNAL FONT INTERFACE FUNCTIONS
 **********************/

static uint32_t gfx_font_lv_get_glyph_index(const lv_font_t *font, uint32_t unicode)
{
    if (!font) {
        return 0;
    }

    const lv_font_fmt_txt_dsc_t *dsc = font->dsc;

    for (uint16_t i = 0; i < dsc->cmap_num; i++) {
        const lv_font_fmt_txt_cmap_t *cmap = &dsc->cmaps[i];

        uint32_t rcp = unicode - cmap->range_start;
        if (rcp > cmap->range_length) {
            continue;
        }

        if (cmap->type == LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            if (unicode >= cmap->range_start &&
                    unicode < cmap->range_start + cmap->range_length) {
                return cmap->glyph_id_start + (unicode - cmap->range_start);
            }
        } else if (cmap->type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            const uint8_t *gid_ofs_8 = cmap->glyph_id_ofs_list;
            if (gid_ofs_8[rcp] == 0 && unicode != cmap->range_start) {
                continue;
            }
            return cmap->glyph_id_start + gid_ofs_8[rcp];
        } else if (cmap->type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY) {
            if (cmap->unicode_list && cmap->list_length > 0) {
                uint16_t key = (uint16_t)rcp;
                uint16_t *found = (uint16_t *)lv_use_utils_bsearch(&key, cmap->unicode_list, cmap->list_length,
                                  sizeof(cmap->unicode_list[0]), unicode_list_compare);
                if (found) {
                    uintptr_t offset = found - cmap->unicode_list;
                    return cmap->glyph_id_start + offset;
                }
            }
        } else if (dsc->cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
            uint16_t key = rcp;
            uint16_t *p = lv_use_utils_bsearch(&key, dsc->cmaps[i].unicode_list, dsc->cmaps[i].list_length,
                                               sizeof(dsc->cmaps[i].unicode_list[0]), unicode_list_compare);

            if (p) {
                uintptr_t ofs = p - dsc->cmaps[i].unicode_list;
                const uint16_t *gid_ofs_16 = dsc->cmaps[i].glyph_id_ofs_list;
                return dsc->cmaps[i].glyph_id_start + gid_ofs_16[ofs];
            }
        }
    }

    return 0;
}

static bool gfx_font_lv_get_glyph_dsc(gfx_font_handle_t font_adapter, void *glyph_dsc, uint32_t unicode, uint32_t unicode_next)
{
    if (!font_adapter || !glyph_dsc) {
        return false;
    }

    const lv_font_t *lvgl_font = (const lv_font_t *)font_adapter->font;
    if (!lvgl_font || !lvgl_font->dsc) {
        return false;
    }

    uint32_t glyph_index = gfx_font_lv_get_glyph_index(lvgl_font, unicode);
    if (glyph_index == 0) {
        return false;
    }

    const lv_font_fmt_txt_dsc_t *dsc = lvgl_font->dsc;
    if (glyph_index >= 65536 || !dsc->glyph_dsc) {
        return false;
    }

#if GFX_HOST_BUILD
    const gfx_font_lv_runtime_t *runtime = (const gfx_font_lv_runtime_t *)lvgl_font->user_data;
    if (runtime != NULL && runtime->glyph_dsc_bin != NULL && runtime->glyph_dsc_stride == 16U) {
        const uint8_t *src_glyph = runtime->glyph_dsc_bin + (size_t)glyph_index * 16U;
        gfx_glyph_dsc_t *out_glyph = (gfx_glyph_dsc_t *)glyph_dsc;

        out_glyph->bitmap_index = gfx_font_lv_bin_u32(src_glyph);
        out_glyph->adv_w = gfx_font_lv_bin_u32(src_glyph + 4);
        out_glyph->box_w = gfx_font_lv_bin_u16(src_glyph + 8);
        out_glyph->box_h = gfx_font_lv_bin_u16(src_glyph + 10);
        out_glyph->ofs_x = (int16_t)gfx_font_lv_bin_u16(src_glyph + 12);
        out_glyph->ofs_y = (int16_t)gfx_font_lv_bin_u16(src_glyph + 14);
        return true;
    }
#endif

    const lv_font_fmt_txt_glyph_dsc_t *src_glyph = &dsc->glyph_dsc[glyph_index];

    gfx_glyph_dsc_t *out_glyph = (gfx_glyph_dsc_t *)glyph_dsc;
    out_glyph->bitmap_index = src_glyph->bitmap_index;
    out_glyph->adv_w = src_glyph->adv_w;
    out_glyph->box_w = src_glyph->box_w;
    out_glyph->box_h = src_glyph->box_h;
    out_glyph->ofs_x = src_glyph->ofs_x;
    out_glyph->ofs_y = src_glyph->ofs_y;

    return true;
}

static const uint8_t *gfx_font_lv_get_glyph_bitmap(gfx_font_handle_t font_adapter, uint32_t unicode, void *glyph_dsc)
{
    if (!font_adapter || !font_adapter->font) {
        return NULL;
    }

    lv_font_t *lvgl_font = (lv_font_t *)font_adapter->font;
    gfx_glyph_dsc_t *glyph = (gfx_glyph_dsc_t *)glyph_dsc;

    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)lvgl_font->dsc;
    if (!dsc || !dsc->glyph_bitmap) {
        return NULL;
    }

    return &dsc->glyph_bitmap[glyph->bitmap_index];
}

static int gfx_font_lv_get_glyph_width(gfx_font_handle_t font_adapter, uint32_t unicode)
{
    if (!font_adapter || !font_adapter->font) {
        return -1;
    }

    gfx_glyph_dsc_t glyph_dsc;

    if (!gfx_font_lv_get_glyph_dsc(font_adapter, &glyph_dsc, unicode, 0)) {
        return -1;
    }

    int advance_pixels = (glyph_dsc.adv_w >> 4);
    int actual_width = glyph_dsc.box_w + glyph_dsc.ofs_x;
    return (advance_pixels > actual_width) ? advance_pixels : actual_width;
}

static int gfx_font_lv_get_line_height(gfx_font_handle_t font_adapter)
{
    const lv_font_t *lvgl_font = (const lv_font_t *)font_adapter->font;
    return lvgl_font->line_height;
}

static int gfx_font_lv_get_base_line(gfx_font_handle_t font_adapter)
{
    const lv_font_t *lvgl_font = (const lv_font_t *)font_adapter->font;
    return lvgl_font->base_line;
}

static uint8_t gfx_font_lv_get_pixel_value(gfx_font_handle_t font_adapter, const uint8_t *bitmap, int32_t x, int32_t y, int32_t box_w)
{
    const lv_font_t *lvgl_font = (const lv_font_t *)font_adapter->font;
    if (!bitmap || x < 0 || y < 0 || x >= box_w) {
        return 0;
    }

    uint8_t bpp = 1;
    if (lvgl_font && lvgl_font->dsc) {
        const lv_font_fmt_txt_dsc_t *dsc = (const lv_font_fmt_txt_dsc_t *)lvgl_font->dsc;
        bpp = dsc->bpp;
    }

    uint8_t pixel_value = 0;

    if (bpp == 1) {
        uint32_t bit_index = y * box_w + x;
        uint32_t byte_index = bit_index / 8;
        uint8_t bit_pos = bit_index % 8;
        pixel_value = (bitmap[byte_index] >> (7 - bit_pos)) & 0x01;
        pixel_value = pixel_value ? 255 : 0;
    } else if (bpp == 2) {
        uint32_t bit_index = (y * box_w + x) * 2;
        uint32_t byte_index = bit_index / 8;
        uint8_t bit_pos = bit_index % 8;
        pixel_value = (bitmap[byte_index] >> (6 - bit_pos)) & 0x03;
        pixel_value = pixel_value * 85;
    } else if (bpp == 4) {
        uint32_t bit_index = (y * box_w + x) * 4;
        uint32_t byte_index = bit_index / 8;
        uint8_t bit_pos = bit_index % 8;
        if (bit_pos == 0) {
            pixel_value = (bitmap[byte_index] >> 4) & 0x0F;
        } else {
            pixel_value = bitmap[byte_index] & 0x0F;
        }
        pixel_value = pixel_value * 17;
    } else if (bpp == 8) {
        pixel_value = bitmap[y * box_w + x];
    }

    return pixel_value;
}

static int gfx_font_lv_adjust_baseline_offset(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    const lv_font_t *lvgl_font = (const lv_font_t *)font_adapter->font;
    if (!lvgl_font) {
        GFX_LOGE(TAG, "query lvgl font: lvgl font is NULL");
        return 0;
    }

    gfx_glyph_dsc_t *dsc = (gfx_glyph_dsc_t *)glyph_dsc;

    int line_height = gfx_font_lv_get_line_height(font_adapter);
    int base_line = gfx_font_lv_get_base_line(font_adapter);
    int adjusted_ofs_y = line_height - base_line - dsc->box_h - dsc->ofs_y;

    return adjusted_ofs_y;
}

static int gfx_font_lv_get_advance_width(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    if (!font_adapter || !glyph_dsc) {
        return 0;
    }

    gfx_glyph_dsc_t *dsc = (gfx_glyph_dsc_t *)glyph_dsc;
    int advance_pixels = (dsc->adv_w >> 4);
    int actual_width = dsc->box_w + dsc->ofs_x;
    return (advance_pixels > actual_width) ? advance_pixels : actual_width;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

bool gfx_is_lvgl_font(const void *font)
{
    if (!font) {
        return false;
    }

    const lv_font_t *lvgl_font = (const lv_font_t *)font;

    if (lvgl_font->line_height > 0 && lvgl_font->line_height < 1000 &&
            lvgl_font->base_line >= 0 && lvgl_font->base_line <= lvgl_font->line_height &&
            lvgl_font->dsc != NULL) {

        const lv_font_fmt_txt_dsc_t *dsc = (const lv_font_fmt_txt_dsc_t *)lvgl_font->dsc;
        if (dsc->glyph_bitmap != NULL && dsc->glyph_dsc != NULL &&
                dsc->cmaps != NULL && dsc->cmap_num > 0 && dsc->cmap_num < 100) {
            return true;
        }
    }

    return false;
}

void gfx_font_lv_init_adapter(gfx_font_handle_t font_adapter, const void *font)
{
    font_adapter->font = (void *)font;
    font_adapter->get_glyph_dsc = gfx_font_lv_get_glyph_dsc;
    font_adapter->get_glyph_bitmap = gfx_font_lv_get_glyph_bitmap;
    font_adapter->get_glyph_width = gfx_font_lv_get_glyph_width;
    font_adapter->get_line_height = gfx_font_lv_get_line_height;
    font_adapter->get_base_line = gfx_font_lv_get_base_line;
    font_adapter->get_pixel_value = gfx_font_lv_get_pixel_value;
    font_adapter->adjust_baseline_offset = gfx_font_lv_adjust_baseline_offset;
    font_adapter->get_advance_width = gfx_font_lv_get_advance_width;
}

/**********************
 *   BINARY FONT CREATION FUNCTIONS
 **********************/

/*
 * The following code (gfx_font_lv_load_from_binary and gfx_font_lv_delete)
 * is derived from 78/xiaozhi-fonts project.
 * Original source: https://github.com/78/xiaozhi-fonts
 */

static uint8_t gfx_font_lv_bin_u8(const uint8_t *addr)
{
    return addr[0];
}

static uint16_t gfx_font_lv_bin_u16(const uint8_t *addr)
{
    return (uint16_t)addr[0] | ((uint16_t)addr[1] << 8);
}

static uint32_t gfx_font_lv_bin_u32(const uint8_t *addr)
{
    return (uint32_t)addr[0] |
           ((uint32_t)addr[1] << 8) |
           ((uint32_t)addr[2] << 16) |
           ((uint32_t)addr[3] << 24);
}

static void gfx_font_lv_free_runtime_font(lv_font_t *font)
{
    if (font == NULL) {
        return;
    }

    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    if (dsc != NULL) {
        free((void *)dsc->cmaps);
        free((void *)dsc->kern_dsc);
        free(dsc);
    }
#if GFX_HOST_BUILD
    free(font->user_data);
#endif
    free(font);
}

static bool gfx_font_lv_parse_cmaps(const uint8_t *bin_base, lv_font_fmt_txt_dsc_t *dsc, uint32_t cmaps_ofs)
{
    const uint8_t *cmaps_base = bin_base + cmaps_ofs;
    lv_font_fmt_txt_cmap_t *cmaps;

    if (dsc->cmap_num == 0U) {
        return true;
    }

    cmaps = calloc(dsc->cmap_num, sizeof(*cmaps));
    if (cmaps == NULL) {
        GFX_LOGE(TAG, "load lvgl font: allocate host cmaps failed");
        return false;
    }

    for (uint16_t i = 0; i < dsc->cmap_num; i++) {
        const uint8_t *src = cmaps_base + (size_t)i * 20U;
        uint32_t unicode_list_ofs;
        uint32_t glyph_id_ofs_list_ofs;

        cmaps[i].range_start = gfx_font_lv_bin_u32(src);
        cmaps[i].range_length = gfx_font_lv_bin_u16(src + 4);
        cmaps[i].glyph_id_start = gfx_font_lv_bin_u16(src + 6);

        unicode_list_ofs = gfx_font_lv_bin_u32(src + 8);
        glyph_id_ofs_list_ofs = gfx_font_lv_bin_u32(src + 12);
        cmaps[i].unicode_list = unicode_list_ofs != 0U ? (const uint16_t *)(cmaps_base + unicode_list_ofs) : NULL;
        cmaps[i].glyph_id_ofs_list = glyph_id_ofs_list_ofs != 0U ? (const void *)(cmaps_base + glyph_id_ofs_list_ofs) : NULL;

        cmaps[i].list_length = gfx_font_lv_bin_u16(src + 16);
        cmaps[i].type = (lv_font_fmt_txt_cmap_type_t)gfx_font_lv_bin_u8(src + 18);
    }

    dsc->cmaps = cmaps;
    return true;
}

static bool gfx_font_lv_parse_kern(const uint8_t *bin_base, lv_font_fmt_txt_dsc_t *dsc, uint32_t kern_ofs)
{
    const uint8_t *kern_base;

    if (kern_ofs == 0U) {
        return true;
    }

    kern_base = bin_base + kern_ofs;
    if (dsc->kern_classes == 1U) {
        lv_font_fmt_txt_kern_classes_t *kern = calloc(1, sizeof(*kern));
        if (kern == NULL) {
            GFX_LOGE(TAG, "load lvgl font: allocate kern classes failed");
            return false;
        }

        uint32_t pair_values_ofs = gfx_font_lv_bin_u32(kern_base);
        uint32_t left_map_ofs = gfx_font_lv_bin_u32(kern_base + 4);
        uint32_t right_map_ofs = gfx_font_lv_bin_u32(kern_base + 8);

        kern->class_pair_values = pair_values_ofs != 0U ? (const int8_t *)(kern_base + pair_values_ofs) : NULL;
        kern->left_class_mapping = left_map_ofs != 0U ? (const uint8_t *)(kern_base + left_map_ofs) : NULL;
        kern->right_class_mapping = right_map_ofs != 0U ? (const uint8_t *)(kern_base + right_map_ofs) : NULL;
        kern->left_class_cnt = gfx_font_lv_bin_u8(kern_base + 12);
        kern->right_class_cnt = gfx_font_lv_bin_u8(kern_base + 13);
        dsc->kern_dsc = kern;
    } else {
        lv_font_fmt_txt_kern_pair_t *kern = calloc(1, sizeof(*kern));
        if (kern == NULL) {
            GFX_LOGE(TAG, "load lvgl font: allocate kern pairs failed");
            return false;
        }

        uint32_t glyph_ids_ofs = gfx_font_lv_bin_u32(kern_base);
        uint32_t values_ofs = gfx_font_lv_bin_u32(kern_base + 4);
        uint32_t packed = gfx_font_lv_bin_u32(kern_base + 8);

        kern->glyph_ids = glyph_ids_ofs != 0U ? (const void *)(kern_base + glyph_ids_ofs) : NULL;
        kern->values = values_ofs != 0U ? (const int8_t *)(kern_base + values_ofs) : NULL;
        kern->pair_cnt = packed & 0x3FFFFFFFU;
        kern->glyph_ids_size = packed >> 30;
        dsc->kern_dsc = kern;
    }

    return true;
}

static lv_font_t *gfx_font_lv_parse_binary(uint8_t *bin_addr)
{
    const uint8_t *bin_base = bin_addr;
    uint32_t dsc_ofs = gfx_font_lv_bin_u32(bin_base + 24);
    uint32_t glyph_dsc_ofs;
    const uint8_t *src_dsc;
    uint16_t packed;
    lv_font_t *font;
    lv_font_fmt_txt_dsc_t *dsc;

    if (gfx_font_lv_bin_u32(bin_base + 12) == 0U || dsc_ofs == 0U) {
        return NULL;
    }

    font = calloc(1, sizeof(*font));
    if (font == NULL) {
        GFX_LOGE(TAG, "load lvgl font: allocate font failed");
        return NULL;
    }

    dsc = calloc(1, sizeof(*dsc));
    if (dsc == NULL) {
        GFX_LOGE(TAG, "load lvgl font: allocate font dsc failed");
        free(font);
        return NULL;
    }

    font->get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;
    font->line_height = (int32_t)gfx_font_lv_bin_u32(bin_base + 12);
    font->base_line = (int32_t)gfx_font_lv_bin_u32(bin_base + 16);
    font->subpx = gfx_font_lv_bin_u8(bin_base + 20) & 0x03U;
    font->kerning = (gfx_font_lv_bin_u8(bin_base + 20) >> 2) & 0x01U;
    font->static_bitmap = (gfx_font_lv_bin_u8(bin_base + 20) >> 3) & 0x01U;
    font->underline_position = (int8_t)gfx_font_lv_bin_u8(bin_base + 21);
    font->underline_thickness = (int8_t)gfx_font_lv_bin_u8(bin_base + 22);
    font->dsc = dsc;

    src_dsc = bin_base + dsc_ofs;
    glyph_dsc_ofs = gfx_font_lv_bin_u32(src_dsc + 4);
    dsc->glyph_bitmap = src_dsc + gfx_font_lv_bin_u32(src_dsc);
    dsc->glyph_dsc = (const lv_font_fmt_txt_glyph_dsc_t *)(src_dsc + glyph_dsc_ofs);
    dsc->kern_scale = gfx_font_lv_bin_u16(src_dsc + 16);

    packed = gfx_font_lv_bin_u16(src_dsc + 18);
    dsc->cmap_num = packed & 0x01FFU;
    dsc->bpp = (packed >> 9) & 0x0FU;
    dsc->kern_classes = (packed >> 13) & 0x01U;
    dsc->bitmap_format = (packed >> 14) & 0x03U;
    dsc->stride = gfx_font_lv_bin_u8(src_dsc + 20);

    if (!gfx_font_lv_parse_cmaps(src_dsc, dsc, gfx_font_lv_bin_u32(src_dsc + 8)) ||
            !gfx_font_lv_parse_kern(src_dsc, dsc, gfx_font_lv_bin_u32(src_dsc + 12))) {
        gfx_font_lv_free_runtime_font(font);
        return NULL;
    }

#if GFX_HOST_BUILD
    gfx_font_lv_runtime_t *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL) {
        gfx_font_lv_free_runtime_font(font);
        return NULL;
    }

    runtime->glyph_dsc_bin = (const uint8_t *)dsc->glyph_dsc;
    runtime->glyph_dsc_stride = glyph_dsc_ofs > gfx_font_lv_bin_u32(src_dsc) &&
                                glyph_dsc_ofs - gfx_font_lv_bin_u32(src_dsc) > 0xFFFFFU ? 16U : 8U;
    font->user_data = runtime;
#endif

    GFX_LOGI(TAG, "load lvgl font: binary line=%d base=%d cmap=%u bpp=%u",
             (int)font->line_height, (int)font->base_line,
             (unsigned)dsc->cmap_num, (unsigned)dsc->bpp);
    return font;
}

lv_font_t *gfx_font_lv_load_from_binary(uint8_t *bin_addr)
{
    if (!bin_addr) {
        GFX_LOGE(TAG, "load lvgl font: binary address is NULL");
        return NULL;
    }
#if GFX_HOST_BUILD
    return gfx_font_lv_parse_binary(bin_addr);
#else
    lv_font_t *font = malloc_cpy(bin_addr, sizeof(lv_font_t));
    if (!font) {
        return NULL;
    }

    font->get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;

    bin_addr += (uintptr_t)font->dsc;
    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)malloc_cpy(bin_addr, sizeof(lv_font_fmt_txt_dsc_t));
    if (!dsc) {
        free(font);
        return NULL;
    }
    font->dsc = dsc;

    addr_add((void **)&dsc->glyph_bitmap, (uintptr_t)bin_addr);
    addr_add((void **)&dsc->glyph_dsc, (uintptr_t)bin_addr);

    if (dsc->cmap_num) {
        uint8_t *cmaps_addr = bin_addr + (uintptr_t)dsc->cmaps;
        dsc->cmaps = (lv_font_fmt_txt_cmap_t *)malloc(sizeof(lv_font_fmt_txt_cmap_t) * dsc->cmap_num);
        if (!dsc->cmaps) {
            GFX_LOGE(TAG, "load lvgl font: allocate cmaps failed");
            free(dsc);
            free(font);
            return NULL;
        }

        uint8_t *ptr = cmaps_addr;
        for (int i = 0; i < dsc->cmap_num; i++) {
            lv_font_fmt_txt_cmap_t *cm = (lv_font_fmt_txt_cmap_t *)&dsc->cmaps[i];
            cm->range_start = *(uint32_t *)ptr;
            ptr += 4;
            cm->range_length = *(uint16_t *)ptr;
            ptr += 2;
            cm->glyph_id_start = *(uint16_t *)ptr;
            ptr += 2;
            cm->unicode_list = (const uint16_t *)(*(uint32_t *)ptr);
            ptr += 4;
            cm->glyph_id_ofs_list = (const void *)(*(uint32_t *)ptr);
            ptr += 4;
            cm->list_length = *(uint16_t *)ptr;
            ptr += 2;
            cm->type = (lv_font_fmt_txt_cmap_type_t) * (uint8_t *)ptr;
            ptr += 1;
            ptr += 1; // padding

            addr_add((void **)&cm->unicode_list, (uintptr_t)cmaps_addr);
            addr_add((void **)&cm->glyph_id_ofs_list, (uintptr_t)cmaps_addr);
        }
    }

    if (dsc->kern_dsc) {
        uint8_t *kern_addr = bin_addr + (uintptr_t)dsc->kern_dsc;
        if (dsc->kern_classes == 1) {
            lv_font_fmt_txt_kern_classes_t *kcl = (lv_font_fmt_txt_kern_classes_t *)malloc_cpy(kern_addr, sizeof(lv_font_fmt_txt_kern_classes_t));
            if (!kcl) {
                if (dsc->cmaps) {
                    free((void *)dsc->cmaps);
                }
                free(dsc);
                free(font);
                return NULL;
            }
            dsc->kern_dsc = kcl;
            addr_add((void **)&kcl->class_pair_values, (uintptr_t)kern_addr);
            addr_add((void **)&kcl->left_class_mapping, (uintptr_t)kern_addr);
            addr_add((void **)&kcl->right_class_mapping, (uintptr_t)kern_addr);
        } else if (dsc->kern_classes == 0) {
            lv_font_fmt_txt_kern_pair_t *kp = (lv_font_fmt_txt_kern_pair_t *)malloc_cpy(kern_addr, sizeof(lv_font_fmt_txt_kern_pair_t));
            if (!kp) {
                if (dsc->cmaps) {
                    free((void *)dsc->cmaps);
                }
                free(dsc);
                free(font);
                return NULL;
            }
            dsc->kern_dsc = kp;
            addr_add((void **)&kp->glyph_ids, (uintptr_t)kern_addr);
            addr_add((void **)&kp->values, (uintptr_t)kern_addr);
        }
    }

    return font;
#endif
}

void gfx_font_lv_delete(lv_font_t *font)
{
    if (!font) {
        return;
    }

    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    if (dsc) {
        if (dsc->cmaps) {
            free((void *)dsc->cmaps);
        }
        if (dsc->kern_dsc) {
            free((void *)dsc->kern_dsc);
        }
        free((void *)dsc);
    }
#if GFX_HOST_BUILD
    free(font->user_data);
#endif
    free((void *)font);
}
