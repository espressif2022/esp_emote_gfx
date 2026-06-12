/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/host/gfx_font_host_priv.h"
#include "gfx/widgets/label.h"

typedef struct {
    uint32_t magic;
} gfx_host_bitmap_font_t;

#define GFX_HOST_FONT_MAGIC 0x48464E54U

static const gfx_host_bitmap_font_t s_host_font = {
    .magic = GFX_HOST_FONT_MAGIC,
};

static const uint8_t s_host_font_digits[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
};

static const uint8_t s_host_font_letters[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};

static const uint8_t s_host_font_question[7] = {
    0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04,
};

static const uint8_t s_host_font_dash[7] = {
    0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00,
};

static const uint8_t s_host_font_dot[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C,
};

static const uint8_t *gfx_host_font_glyph_rows(uint32_t unicode)
{
    if (unicode >= 'a' && unicode <= 'z') {
        unicode = (uint32_t)toupper((int)unicode);
    }
    if (unicode >= 'A' && unicode <= 'Z') {
        return s_host_font_letters[unicode - 'A'];
    }
    if (unicode >= '0' && unicode <= '9') {
        return s_host_font_digits[unicode - '0'];
    }
    if (unicode == '-') {
        return s_host_font_dash;
    }
    if (unicode == '.') {
        return s_host_font_dot;
    }
    if (unicode == '?') {
        return s_host_font_question;
    }
    return NULL;
}

static bool gfx_host_font_get_glyph_dsc(gfx_font_handle_t font_adapter, void *glyph_dsc,
                                        uint32_t unicode, uint32_t unicode_next)
{
    (void)font_adapter;
    (void)unicode_next;

    gfx_glyph_dsc_t *out = (gfx_glyph_dsc_t *)glyph_dsc;
    if (out == NULL) {
        return false;
    }

    if (unicode == ' ') {
        out->bitmap_index = 0;
        out->adv_w = 4U << 8;
        out->box_w = 0;
        out->box_h = 0;
        out->ofs_x = 0;
        out->ofs_y = 0;
        return true;
    }

    if (gfx_host_font_glyph_rows(unicode) == NULL) {
        return false;
    }

    out->bitmap_index = unicode;
    out->adv_w = 6U << 8;
    out->box_w = 5;
    out->box_h = 7;
    out->ofs_x = 0;
    out->ofs_y = 1;
    return true;
}

static const uint8_t *gfx_host_font_get_glyph_bitmap(gfx_font_handle_t font_adapter,
        uint32_t unicode, void *glyph_dsc)
{
    (void)font_adapter;
    (void)glyph_dsc;
    return gfx_host_font_glyph_rows(unicode);
}

static int gfx_host_font_get_glyph_width(gfx_font_handle_t font_adapter, uint32_t unicode)
{
    (void)font_adapter;
    return unicode == ' ' ? 4 : 6;
}

static int gfx_host_font_get_line_height(gfx_font_handle_t font_adapter)
{
    (void)font_adapter;
    return 9;
}

static int gfx_host_font_get_base_line(gfx_font_handle_t font_adapter)
{
    (void)font_adapter;
    return 2;
}

static uint8_t gfx_host_font_get_pixel_value(gfx_font_handle_t font_adapter, const uint8_t *bitmap,
        int32_t x, int32_t y, int32_t box_w)
{
    (void)font_adapter;
    (void)box_w;
    if (bitmap == NULL || x < 0 || x >= 5 || y < 0 || y >= 7) {
        return 0;
    }
    return (bitmap[y] & (uint8_t)(1U << (4 - x))) ? 0xFF : 0x00;
}

static int gfx_host_font_adjust_baseline_offset(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    (void)font_adapter;
    const gfx_glyph_dsc_t *dsc = (const gfx_glyph_dsc_t *)glyph_dsc;
    return dsc != NULL ? dsc->ofs_y : 0;
}

static int gfx_host_font_get_advance_width(gfx_font_handle_t font_adapter, void *glyph_dsc)
{
    (void)font_adapter;
    const gfx_glyph_dsc_t *dsc = (const gfx_glyph_dsc_t *)glyph_dsc;
    return dsc != NULL ? (int)(dsc->adv_w >> 8) : 0;
}

gfx_font_t gfx_host_font_default(void)
{
    return (gfx_font_t)&s_host_font;
}

bool gfx_host_font_is_builtin(const void *font)
{
    const gfx_host_bitmap_font_t *host_font = (const gfx_host_bitmap_font_t *)font;
    return host_font != NULL && host_font->magic == GFX_HOST_FONT_MAGIC;
}

void gfx_host_font_init_adapter(gfx_font_handle_t font_adapter, const void *font)
{
    font_adapter->font = (void *)font;
    font_adapter->get_glyph_dsc = gfx_host_font_get_glyph_dsc;
    font_adapter->get_glyph_bitmap = gfx_host_font_get_glyph_bitmap;
    font_adapter->get_glyph_width = gfx_host_font_get_glyph_width;
    font_adapter->get_line_height = gfx_host_font_get_line_height;
    font_adapter->get_base_line = gfx_host_font_get_base_line;
    font_adapter->get_pixel_value = gfx_host_font_get_pixel_value;
    font_adapter->adjust_baseline_offset = gfx_host_font_adjust_baseline_offset;
    font_adapter->get_advance_width = gfx_host_font_get_advance_width;
}

esp_err_t gfx_label_font_create(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font)
{
    (void)cfg;
    if (ret_font == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *ret_font = gfx_host_font_default();
    return ESP_OK;
}

esp_err_t gfx_label_font_delete(gfx_font_t font)
{
    (void)font;
    return ESP_OK;
}
