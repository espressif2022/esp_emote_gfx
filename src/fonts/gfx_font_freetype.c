/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_FONT_FREETYPE
#include "common/gfx_log_priv.h"
#include "gfx/widgets/label.h"
#include "fonts/gfx_font_lvgl_priv.h"
#include "fonts/gfx_font_priv.h"

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_SIZES_H

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *   STATIC VARIABLES
 **********************/

static const char *const TAG = "font_freetype";
static FT_Library s_library = NULL;
static gfx_ft_lib_handle_t s_font_lib = NULL;

/**********************
 *   STATIC PROTOTYPES
 **********************/

static gfx_err_t gfx_font_ft_lib_create_internal(void);
static gfx_err_t gfx_font_ft_lib_cleanup_internal(void);
static gfx_err_t gfx_font_ft_new_internal(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font);
static gfx_err_t gfx_font_ft_delete_internal(gfx_font_t font);

static int gfx_font_ft_get_bitmap_size_px(const FT_Bitmap_Size *bitmap_size);
static int gfx_font_ft_select_fixed_size_index(FT_Face face, int requested_size);
static gfx_err_t gfx_font_ft_activate_size(gfx_font_ft_t *ft_font);
static gfx_err_t gfx_font_ft_update_metrics(gfx_font_ft_t *ft_font);
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

static gfx_err_t gfx_font_ft_lib_create_internal(void)
{
    FT_Error error;

    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)calloc(1, sizeof(gfx_ft_lib_t));
    GFX_RETURN_ON_FALSE(lib, GFX_ERR_NO_MEM, TAG, "no mem for FT library");

    lib->ft_face_head = NULL;

    error = FT_Init_FreeType(&s_library);
    if (error) {
        GFX_LOGE(TAG, "init freetype library: error=%d", error);
        free(lib);
        return GFX_ERR_INVALID_STATE;
    }

    lib->ft_library = s_library;
    lib->ft_face_head = NULL;
    s_font_lib = lib;
    return GFX_OK;
}

static gfx_err_t gfx_font_ft_lib_cleanup_internal(void)
{
    gfx_ft_lib_t *lib = (gfx_ft_lib_t *)s_font_lib;
    if (!lib) {
        return GFX_OK;
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

    return GFX_OK;
}

static int gfx_font_ft_get_bitmap_size_px(const FT_Bitmap_Size *bitmap_size)
{
    if (bitmap_size == NULL) {
        return 0;
    }

    if (bitmap_size->y_ppem > 0) {
        return (int)((bitmap_size->y_ppem + 32) >> 6);
    }

    return bitmap_size->height;
}

static int gfx_font_ft_select_fixed_size_index(FT_Face face, int requested_size)
{
    int best_index = -1;
    int best_delta = INT32_MAX;
    int best_size = 0;

    if (face == NULL || face->available_sizes == NULL || face->num_fixed_sizes <= 0) {
        return -1;
    }

    for (int i = 0; i < face->num_fixed_sizes; i++) {
        int strike_size = gfx_font_ft_get_bitmap_size_px(&face->available_sizes[i]);
        int delta = abs(strike_size - requested_size);

        if (best_index < 0 ||
                delta < best_delta ||
                (delta == best_delta && strike_size <= requested_size && strike_size > best_size)) {
            best_index = i;
            best_delta = delta;
            best_size = strike_size;
        }
    }

    return best_index;
}

static gfx_err_t gfx_font_ft_activate_size(gfx_font_ft_t *ft_font)
{
    GFX_RETURN_ON_FALSE(ft_font != NULL && ft_font->face != NULL && ft_font->ft_size != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "font size is invalid");

    FT_Error error = FT_Activate_Size(ft_font->ft_size);
    GFX_RETURN_ON_FALSE(!error, GFX_ERR_INVALID_STATE, TAG, "activate font size failed: error=%d", error);

    if (ft_font->fixed_size_index >= 0) {
        error = FT_Select_Size(ft_font->face, ft_font->fixed_size_index);
    } else {
        error = FT_Set_Pixel_Sizes(ft_font->face, 0, (FT_UInt)ft_font->size);
    }

    GFX_RETURN_ON_FALSE(!error, GFX_ERR_INVALID_STATE, TAG, "select font size failed: error=%d", error);
    return GFX_OK;
}

static gfx_err_t gfx_font_ft_update_metrics(gfx_font_ft_t *ft_font)
{
    GFX_RETURN_ON_ERROR(gfx_font_ft_activate_size(ft_font), TAG, "update metrics: activate size failed");

    ft_font->line_height = (int)(ft_font->face->size->metrics.height >> 6);
    ft_font->base_line = -(int)(ft_font->face->size->metrics.descender >> 6);

    FT_Fixed scale = ft_font->face->size->metrics.y_scale;
    int8_t thickness = FT_MulFix(scale, ft_font->face->underline_thickness) >> 6;
    ft_font->underline_position = FT_MulFix(scale, ft_font->face->underline_position) >> 6;
    ft_font->underline_thickness = thickness < 1 ? 1 : thickness;

    return GFX_OK;
}

static gfx_err_t gfx_font_ft_new_internal(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    GFX_RETURN_ON_FALSE(cfg != NULL && ret_font != NULL, GFX_ERR_INVALID_ARG, TAG, "invalid output");
    GFX_RETURN_ON_FALSE(cfg->mem && cfg->mem_size, GFX_ERR_INVALID_ARG, TAG, "invalid memory input");
    GFX_RETURN_ON_FALSE(cfg->font_size > 0, GFX_ERR_INVALID_ARG, TAG, "font size must be greater than zero");

    FT_Face face = NULL;
    FT_Error error;
    gfx_err_t ret = GFX_OK;

    gfx_ft_lib_t *lib = s_font_lib;
    GFX_RETURN_ON_FALSE(lib, GFX_ERR_INVALID_STATE, TAG, "font library is NULL");

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
        GFX_RETURN_ON_FALSE(!error, GFX_ERR_INVALID_ARG, TAG, "error loading font");

        gfx_ft_face_entry_t *new_face_entry = (gfx_ft_face_entry_t *)calloc(1, sizeof(gfx_ft_face_entry_t));
        if (new_face_entry == NULL) {
            FT_Done_Face(face);
            return GFX_ERR_NO_MEM;
        }

        new_face_entry->face = face;
        new_face_entry->mem = cfg->mem;
        new_face_entry->next = lib->ft_face_head;
        lib->ft_face_head = new_face_entry;
    }

    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)calloc(1, sizeof(gfx_font_ft_t));
    GFX_RETURN_ON_FALSE(ft_font, GFX_ERR_NO_MEM, TAG, "no mem for ft_font");

    ft_font->face = face;
    ft_font->size = cfg->font_size;
    ft_font->fixed_size_index = -1;

    FT_Size size;
    error = FT_New_Size(face, &size);
    if (error) {
        free(ft_font);
        GFX_LOGE(TAG, "new freetype size failed: error=%d", error);
        return GFX_ERR_INVALID_STATE;
    }
    ft_font->ft_size = size;
    FT_Reference_Face(face);

    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE) && face->num_fixed_sizes > 0) {
        ft_font->fixed_size_index = gfx_font_ft_select_fixed_size_index(face, cfg->font_size);
        GFX_GOTO_ON_FALSE(ft_font->fixed_size_index >= 0, GFX_ERR_NOT_FOUND, err, TAG,
                          "no usable fixed font size");
        ft_font->size = gfx_font_ft_get_bitmap_size_px(&face->available_sizes[ft_font->fixed_size_index]);
        GFX_LOGD(TAG, "fallback fixed font size: requested=%u selected=%d index=%d",
                 cfg->font_size, ft_font->size, ft_font->fixed_size_index);
    }

    GFX_GOTO_ON_ERROR(gfx_font_ft_update_metrics(ft_font), err, TAG, "set font size failed");

    *ret_font = (gfx_font_t)ft_font;

    return GFX_OK;

err:
    if (ft_font->ft_size != NULL) {
        FT_Done_Size(ft_font->ft_size);
    }
    FT_Done_Face(face);
    free(ft_font);
    return ret;
}

static gfx_err_t gfx_font_ft_delete_internal(gfx_font_t font)
{
    GFX_RETURN_ON_FALSE(font, GFX_ERR_INVALID_ARG, TAG, "font is NULL");

    if (gfx_is_lvgl_font(font)) {
        return GFX_OK;
    }

    gfx_font_ft_t *ft_font = (gfx_font_ft_t *)font;
    if (ft_font->ft_size != NULL) {
        FT_Done_Size(ft_font->ft_size);
    }
    if (ft_font->face != NULL) {
        FT_Done_Face(ft_font->face);
    }
    free(ft_font);

    return GFX_OK;
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

    if (gfx_font_ft_activate_size(ft_font) != GFX_OK) {
        return false;
    }

    FT_UInt glyph_index = FT_Get_Char_Index(face, unicode);
    if (glyph_index == 0) {
        return false;
    }

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

    if (gfx_font_ft_activate_size(ft_font) != GFX_OK) {
        return NULL;
    }

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

gfx_err_t gfx_ft_lib_create(void)
{
    return gfx_font_ft_lib_create_internal();
}

gfx_err_t gfx_ft_lib_cleanup(void)
{
    return gfx_font_ft_lib_cleanup_internal();
}

gfx_err_t gfx_label_font_create(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    return gfx_font_ft_new_internal(cfg, ret_font);
}

gfx_err_t gfx_label_font_delete(gfx_font_t font)
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

#else

gfx_err_t gfx_label_font_create(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    (void)cfg;
    (void)ret_font;
    return GFX_ERR_NOT_SUPPORTED;
}

gfx_err_t gfx_label_font_delete(gfx_font_t font)
{
    (void)font;
    return GFX_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_GFX_FONT_FREETYPE_SUPPORT */
