/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "esp_log.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_FONT_FT
#include "common/gfx_log_priv.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_SIZES_H
#include "widget/gfx_label.h"
#include "widget/gfx_font_lvgl.h"
#include "widget/font/gfx_font_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *   STATIC VARIABLES
 **********************/

static const char *TAG = "font_ft";
static FT_Library s_library = NULL;
static gfx_ft_lib_handle_t s_font_lib = NULL;

/**********************
 *   STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_font_ft_lib_create_internal(void);
static esp_err_t gfx_font_ft_lib_cleanup_internal(void);
static esp_err_t gfx_font_ft_new_internal(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font);
static esp_err_t gfx_font_ft_delete_internal(gfx_font_t font);

static bool gfx_font_ft_get_glyph_dsc(gfx_font_handle_t font_adapter, void *glyph_dsc, uint32_t unicode, uint32_t unicode_next);
static const uint8_t *gfx_font_ft_get_glyph_bitmap(gfx_font_handle_t font_adapter, uint32_t unicode, void *glyph_dsc);
static int gfx_font_ft_get_glyph_width(gfx_font_handle_t font_adapter, uint32_t unicode);
static int gfx_font_ft_get_line_height(gfx_font_handle_t font_adapter);
static int gfx_font_ft_get_base_line(gfx_font_handle_t font_adapter);
static uint8_t gfx_font_ft_get_pixel_value(gfx_font_handle_t font_adapter, const uint8_t *bitmap, int32_t x, int32_t y, int32_t box_w);
static int gfx_font_ft_adjust_baseline_offset(gfx_font_handle_t font_adapter, void *glyph_dsc);
static int gfx_font_ft_get_advance_width(gfx_font_handle_t font_adapter, void *glyph_dsc);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static esp_err_t gfx_font_ft_lib_create_internal(void)
{
    FT_Error error;

    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)calloc(1, sizeof(gfx_ft_lib_t));
    ESP_RETURN_ON_FALSE(lib, ESP_ERR_NO_MEM, TAG, "no mem for FT library");

    lib->ft_face_head = NULL;

    error = FT_Init_FreeType(&s_library);
    if (error) {
        GFX_LOGE(TAG, "init freetype library: error=%d", error);
        free(lib);
        return ESP_ERR_INVALID_STATE;
    }

    lib->ft_library = s_library;
    lib->ft_face_head = NULL;
    s_font_lib = lib;
    return ESP_OK;
}

static esp_err_t gfx_font_ft_lib_cleanup_internal(void)
{
    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)s_font_lib;
    if (!lib) {
        return ESP_OK;
    }

    gfx_ft_face_entry_t *entry = lib->ft_face_head;
    while (entry != NULL) {
        gfx_ft_face_entry_t *next = entry->next;
        if (entry->face) {
            FT_Done_Face((FT_Face)entry->face);
        }
        free(entry);
        entry = next;
    }

    if (s_library) {
        FT_Done_FreeType(s_library);
        s_library = NULL;
    }

    free(lib);
    s_font_lib = NULL;

    return ESP_OK;
}

static esp_err_t gfx_font_ft_new_internal(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    ESP_RETURN_ON_FALSE(cfg->mem && cfg->mem_size, ESP_ERR_INVALID_ARG, TAG, "invalid memory input");

    FT_Face face = NULL;
    FT_Error error;

    gfx_ft_lib_t *lib = s_font_lib;
    ESP_RETURN_ON_FALSE(lib, ESP_ERR_INVALID_STATE, TAG, "font library is NULL");

    gfx_ft_face_entry_t *entry = lib->ft_face_head;
    while (entry != NULL) {
        if (entry->mem == cfg->mem) {
            face = (FT_Face)entry->face;
            break;
        }
        entry = entry->next;
    }

    if (!face) {
        error = FT_New_Memory_Face((FT_Library)lib->ft_library, cfg->mem, cfg->mem_size, 0, &face);
        ESP_RETURN_ON_FALSE(!error, ESP_ERR_INVALID_ARG, TAG, "error loading font");

        gfx_ft_face_entry_t *new_face_entry = (gfx_ft_face_entry_t *)calloc(1, sizeof(gfx_ft_face_entry_t));
        ESP_RETURN_ON_FALSE(new_face_entry, ESP_ERR_NO_MEM, TAG, "no mem for ft_face_entry");

        new_face_entry->face = face;
        new_face_entry->mem = cfg->mem;
        new_face_entry->next = lib->ft_face_head;
        lib->ft_face_head = new_face_entry;
    }

    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)calloc(1, sizeof(gfx_font_ft_t));
    ESP_RETURN_ON_FALSE(ft_font, ESP_ERR_NO_MEM, TAG, "no mem for ft_font");

    ft_font->face = face;
    ft_font->size = cfg->font_size;

    FT_Size size;
    FT_New_Size(face, &size);
    FT_Activate_Size(size);
    FT_Reference_Face(face);

    FT_Set_Pixel_Sizes(face, 0, cfg->font_size);

    ft_font->line_height = (face->size->metrics.height >> 6);
    ft_font->base_line = -(face->size->metrics.descender >> 6);

    FT_Fixed scale = face->size->metrics.y_scale;
    int8_t thickness = FT_MulFix(scale, face->underline_thickness) >> 6;
    ft_font->underline_position = FT_MulFix(scale, face->underline_position) >> 6;
    ft_font->underline_thickness = thickness < 1 ? 1 : thickness;

    *ret_font = (gfx_font_t)ft_font;

    return ESP_OK;
}

static esp_err_t gfx_font_ft_delete_internal(gfx_font_t font)
{
    ESP_RETURN_ON_FALSE(font, ESP_ERR_INVALID_ARG, TAG, "font is NULL");

    if (gfx_is_lvgl_font(font)) {
        return ESP_OK;
    }

    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font;
    free(ft_font);

    return ESP_OK;
}

static bool gfx_font_ft_get_glyph_dsc(gfx_font_handle_t font_adapter, void *glyph_dsc, uint32_t unicode, uint32_t unicode_next)
{
    gfx_glyph_dsc_t *dsc_out = (gfx_glyph_dsc_t *)glyph_dsc;

    if (unicode < 0x20) {
        dsc_out->adv_w = 0;
        dsc_out->box_h = 0;
        dsc_out->box_w = 0;
        dsc_out->ofs_x = 0;
        dsc_out->ofs_y = 0;
        return true;
    }

    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font_adapter->font;

    FT_Error error;
    FT_Face face = ft_font->face;

    error = FT_Set_Pixel_Sizes(face, 0, ft_font->size);
    if (error) {
        return false;
    }

    FT_UInt glyph_index = FT_Get_Char_Index(face, unicode);

    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
        return false;
    }

    FT_GlyphSlot slot = face->glyph;

    dsc_out->adv_w = (slot->advance.x >> 6) << 8;
    dsc_out->box_w = 0;
    dsc_out->box_h = 0;
    dsc_out->ofs_x = 0;
    dsc_out->ofs_y = 0;
    dsc_out->bitmap_index = 0;

    return true;
}

static const uint8_t *gfx_font_ft_get_glyph_bitmap(gfx_font_handle_t font_adapter, uint32_t unicode, void *glyph_dsc)
{
    gfx_glyph_dsc_t *glyph = (gfx_glyph_dsc_t *)glyph_dsc;
    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font_adapter->font;
    FT_Face face = ft_font->face;

    FT_Set_Pixel_Sizes(face, 0, ft_font->size);

    FT_UInt glyph_index = FT_Get_Char_Index(face, unicode);
    if (glyph_index == 0) {
        return NULL;
    }

    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
        return NULL;
    }

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        return NULL;
    }

    FT_GlyphSlot slot = face->glyph;

    glyph->adv_w = (slot->advance.x >> 6) << 8;
    glyph->box_w = slot->bitmap.width;
    glyph->box_h = slot->bitmap.rows;
    glyph->ofs_x = slot->bitmap_left;
    int line_height = (face->size->metrics.height >> 6);
    int base_line = -(face->size->metrics.descender >> 6);
    glyph->ofs_y = line_height - base_line - slot->bitmap_top;
    glyph->bitmap_index = 0;

    return (const uint8_t *)(face->glyph->bitmap.buffer);
}

static int gfx_font_ft_get_glyph_width(gfx_font_handle_t font_adapter, uint32_t unicode)
{
    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font_adapter->font;
    if (!ft_font || !ft_font->face) {
        return 0;
    }

    gfx_glyph_dsc_t glyph_dsc;
    if (!gfx_font_ft_get_glyph_dsc(font_adapter, &glyph_dsc, unicode, 0)) {
        return 0;
    }

    return (glyph_dsc.adv_w >> 8);
}

static int gfx_font_ft_get_line_height(gfx_font_handle_t font_adapter)
{
    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font_adapter->font;
    return ft_font->line_height;
}

static int gfx_font_ft_get_base_line(gfx_font_handle_t font_adapter)
{
    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font_adapter->font;
    return ft_font->base_line;
}

static uint8_t gfx_font_ft_get_pixel_value(gfx_font_handle_t font_adapter, const uint8_t *bitmap, int32_t x, int32_t y, int32_t box_w)
{
    (void)font_adapter;
    if (!bitmap || x < 0 || y < 0 || x >= box_w) {
        return 0;
    }

    uint8_t pixel_value = bitmap[y * box_w + x];
    return pixel_value;
}

static int gfx_font_ft_adjust_baseline_offset(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    if (!font_adapter || !glyph_dsc) {
        return 0;
    }

    gfx_glyph_dsc_t *dsc = (gfx_glyph_dsc_t *)glyph_dsc;
    return dsc->ofs_y;
}

static int gfx_font_ft_get_advance_width(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    if (!font_adapter || !glyph_dsc) {
        return 0;
    }

    gfx_glyph_dsc_t *dsc = (gfx_glyph_dsc_t *)glyph_dsc;
    int advance_pixels = (dsc->adv_w >> 8);
    return advance_pixels;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_ft_lib_create(void)
{
    return gfx_font_ft_lib_create_internal();
}

esp_err_t gfx_ft_lib_cleanup(void)
{
    return gfx_font_ft_lib_cleanup_internal();
}

esp_err_t gfx_label_new_font(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    return gfx_font_ft_new_internal(cfg, ret_font);
}

esp_err_t gfx_label_delete_font(gfx_font_t font)
{
    return gfx_font_ft_delete_internal(font);
}

void gfx_font_ft_init_adapter(gfx_font_handle_t font_adapter, const void *font)
{
    font_adapter->font = (void *)font;
    font_adapter->get_glyph_dsc = gfx_font_ft_get_glyph_dsc;
    font_adapter->get_glyph_bitmap = gfx_font_ft_get_glyph_bitmap;
    font_adapter->get_glyph_width = gfx_font_ft_get_glyph_width;
    font_adapter->get_line_height = gfx_font_ft_get_line_height;
    font_adapter->get_base_line = gfx_font_ft_get_base_line;
    font_adapter->get_pixel_value = gfx_font_ft_get_pixel_value;
    font_adapter->adjust_baseline_offset = gfx_font_ft_adjust_baseline_offset;
    font_adapter->get_advance_width = gfx_font_ft_get_advance_width;
}

#endif /* CONFIG_GFX_FONT_FREETYPE_SUPPORT */
