/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef LVGL_VERSION_MAJOR
#define LVGL_VERSION_MAJOR 8
#endif

#ifndef LVGL_VERSION_MINOR
#define LVGL_VERSION_MINOR 4
#endif

#ifndef LV_VERSION_CHECK
#define LV_VERSION_CHECK(major, minor, patch) \
    ((LVGL_VERSION_MAJOR > (major)) || \
     (LVGL_VERSION_MAJOR == (major) && LVGL_VERSION_MINOR >= (minor)))
#endif

#ifndef LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_CONST
#endif

#ifndef LV_ATTRIBUTE_EXTERN_DATA
#define LV_ATTRIBUTE_EXTERN_DATA extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t lv_coord_t;

typedef struct _lv_draw_buf_t lv_draw_buf_t;
typedef struct _lv_font_t lv_font_t;

typedef enum {
    LV_FONT_GLYPH_FORMAT_NONE = 0,
    LV_FONT_GLYPH_FORMAT_A1 = 0x01,
    LV_FONT_GLYPH_FORMAT_A2 = 0x02,
    LV_FONT_GLYPH_FORMAT_A3 = 0x03,
    LV_FONT_GLYPH_FORMAT_A4 = 0x04,
    LV_FONT_GLYPH_FORMAT_A8 = 0x08,
} lv_font_glyph_format_t;

typedef struct {
    const lv_font_t *resolved_font;
    uint16_t adv_w;
    uint16_t box_w;
    uint16_t box_h;
    int16_t ofs_x;
    int16_t ofs_y;
    uint16_t stride;
    lv_font_glyph_format_t format;
    uint8_t is_placeholder : 1;
    uint8_t req_raw_bitmap : 1;
    int32_t outline_stroke_width;
    union {
        uint32_t index;
        const void *src;
    } gid;
    void *entry;
} lv_font_glyph_dsc_t;

typedef enum {
    LV_FONT_SUBPX_NONE,
    LV_FONT_SUBPX_HOR,
    LV_FONT_SUBPX_VER,
    LV_FONT_SUBPX_BOTH,
} lv_font_subpx_t;

typedef enum {
    LV_FONT_KERNING_NORMAL,
    LV_FONT_KERNING_NONE,
} lv_font_kerning_t;

struct _lv_font_t {
    bool (*get_glyph_dsc)(const lv_font_t *, lv_font_glyph_dsc_t *, uint32_t letter, uint32_t letter_next);
    const void *(*get_glyph_bitmap)(lv_font_glyph_dsc_t *, lv_draw_buf_t *);
    void (*release_glyph)(const lv_font_t *, lv_font_glyph_dsc_t *);
    int32_t line_height;
    int32_t base_line;
    uint8_t subpx : 2;
    uint8_t kerning : 1;
    uint8_t static_bitmap : 1;
    int8_t underline_position;
    int8_t underline_thickness;
    const void *dsc;
    const lv_font_t *fallback;
    void *user_data;
};

#define LV_FONT_FMT_TXT_LARGE 0

typedef struct {
    uint32_t bitmap_index : 20;
    uint32_t adv_w : 12;
    uint8_t box_w;
    uint8_t box_h;
    int8_t ofs_x;
    int8_t ofs_y;
} lv_font_fmt_txt_glyph_dsc_t;

typedef enum {
    LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL,
    LV_FONT_FMT_TXT_CMAP_SPARSE_FULL,
    LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    LV_FONT_FMT_TXT_CMAP_SPARSE_TINY,
} lv_font_fmt_txt_cmap_type_t;

typedef struct {
    uint32_t range_start;
    uint16_t range_length;
    uint16_t glyph_id_start;
    const uint16_t *unicode_list;
    const void *glyph_id_ofs_list;
    uint16_t list_length;
    lv_font_fmt_txt_cmap_type_t type;
} lv_font_fmt_txt_cmap_t;

typedef struct {
    const void *glyph_ids;
    const int8_t *values;
    uint32_t pair_cnt : 30;
    uint32_t glyph_ids_size : 2;
} lv_font_fmt_txt_kern_pair_t;

typedef struct {
    const int8_t *class_pair_values;
    const uint8_t *left_class_mapping;
    const uint8_t *right_class_mapping;
    uint8_t left_class_cnt;
    uint8_t right_class_cnt;
} lv_font_fmt_txt_kern_classes_t;

typedef enum {
    LV_FONT_FMT_TXT_PLAIN = 0,
    LV_FONT_FMT_TXT_COMPRESSED = 1,
    LV_FONT_FMT_TXT_COMPRESSED_NO_PREFILTER = 2,
} lv_font_fmt_txt_bitmap_format_t;

typedef struct {
    uint32_t last_letter;
    uint32_t last_glyph_id;
} lv_font_fmt_txt_glyph_cache_t;

typedef struct {
    const uint8_t *glyph_bitmap;
    const lv_font_fmt_txt_glyph_dsc_t *glyph_dsc;
    const lv_font_fmt_txt_cmap_t *cmaps;
    const void *kern_dsc;
    uint16_t kern_scale;
    uint16_t cmap_num : 9;
    uint16_t bpp : 4;
    uint16_t kern_classes : 1;
    uint16_t bitmap_format : 2;
    lv_font_fmt_txt_glyph_cache_t *cache;
    uint8_t stride;
} lv_font_fmt_txt_dsc_t;

static inline bool lv_font_get_glyph_dsc_fmt_txt(const lv_font_t *font,
        lv_font_glyph_dsc_t *dsc_out,
        uint32_t unicode_letter,
        uint32_t unicode_letter_next)
{
    (void)font;
    (void)dsc_out;
    (void)unicode_letter;
    (void)unicode_letter_next;
    return false;
}

static inline const void *lv_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t *g_dsc,
        lv_draw_buf_t *draw_buf)
{
    (void)g_dsc;
    (void)draw_buf;
    return NULL;
}

#define LV_FONT_DECLARE(font_name) extern const lv_font_t font_name

#ifdef __cplusplus
}
#endif
