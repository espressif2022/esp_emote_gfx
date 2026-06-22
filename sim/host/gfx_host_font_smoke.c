/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include "fonts/gfx_font_priv.h"
#include "gfx/widgets/font_lvgl.h"

static int load_file(const char *path, uint8_t **out_data)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint8_t *data;

    if (fp == NULL) {
        fprintf(stderr, "open font failed: %s\n", path);
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    size = ftell(fp);
    if (size <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 1;
    }

    data = malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        return 1;
    }

    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    *out_data = data;
    return 0;
}

static int check_glyph(gfx_font_handle_t adapter, uint32_t unicode)
{
    gfx_glyph_dsc_t dsc;
    const uint8_t *bitmap;
    uint32_t nonzero = 0;

    if (!adapter->get_glyph_dsc(adapter, &dsc, unicode, 0)) {
        fprintf(stderr, "glyph missing: U+%04X\n", (unsigned)unicode);
        return 1;
    }

    bitmap = adapter->get_glyph_bitmap(adapter, unicode, &dsc);
    if (bitmap == NULL || dsc.box_w == 0U || dsc.box_h == 0U) {
        fprintf(stderr, "glyph bitmap invalid: U+%04X box=%ux%u\n",
                (unsigned)unicode, (unsigned)dsc.box_w, (unsigned)dsc.box_h);
        return 1;
    }

    for (uint16_t y = 0; y < dsc.box_h; y++) {
        for (uint16_t x = 0; x < dsc.box_w; x++) {
            if (adapter->get_pixel_value(adapter, bitmap, x, y, dsc.box_w) != 0U) {
                nonzero++;
            }
        }
    }

    if (nonzero == 0U) {
        fprintf(stderr, "glyph bitmap blank: U+%04X\n", (unsigned)unicode);
        return 1;
    }

    return 0;
}

int main(void)
{
    static const char *font_path = "examples/expression/assets/font_puhui_common_20_4.bin";
    static const uint32_t glyphs[] = {
        'A',
        '3',
        0x4F60U, /* 你 */
        0x597DU, /* 好 */
    };
    uint8_t *font_data = NULL;
    lv_font_t *font;
    gfx_font_adapter_t adapter = {0};
    int ret = 0;

    if (load_file(font_path, &font_data) != 0) {
        return 1;
    }

    font = gfx_font_lv_load_from_binary(font_data);
    if (font == NULL) {
        fprintf(stderr, "load lvgl binary font failed\n");
        free(font_data);
        return 1;
    }

    if (gfx_font_init_adapter(&adapter, font) != GFX_OK) {
        fprintf(stderr, "init font adapter failed\n");
        gfx_font_lv_delete(font);
        free(font_data);
        return 1;
    }

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (check_glyph(&adapter, glyphs[i]) != 0) {
            ret = 1;
        }
    }

    gfx_font_lv_delete(font);
    free(font_data);
    return ret;
}
