/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Host exporter for the demo home scene.
 *
 * This is intentionally small and boring: build an author-side scene, call the
 * same gsp_pack() compiler, then emit both the binary .inc and its readable
 * manifest. The manifest is part of the artifact contract.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsp_format.h"

#define HOME_SCREEN_W 480
#define HOME_SCREEN_H 480
#define HOME_OBJ_COUNT 12
#define HOME_FONT_COUNT 4
#define HOME_ACTION_COUNT 3
#define HOME_IMG_W 96
#define HOME_IMG_H 72

typedef struct {
    const char *role;
    const char *note;
} obj_meta_t;

typedef struct {
    uint8_t *data;
    uint16_t len;
} item_params_t;

static uint16_t rgb565(uint32_t rgb)
{
    uint8_t r = (uint8_t)((rgb >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((rgb >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(rgb & 0xFFu);

    return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                      ((uint16_t)(g & 0xFCu) << 3) |
                      ((uint16_t)b >> 3));
}

static void put_px(uint8_t *pixels, uint16_t x, uint16_t y, uint16_t color)
{
    size_t off = ((size_t)y * HOME_IMG_W + x) * 2u;

    pixels[off + 0] = (uint8_t)(color & 0xFFu);
    pixels[off + 1] = (uint8_t)((color >> 8) & 0xFFu);
}

static void build_preview_image(uint8_t *pixels)
{
    const uint16_t bg = rgb565(0x102033);
    const uint16_t cyan = rgb565(0x67D8F6);
    const uint16_t blue = rgb565(0x2F8CFF);
    const uint16_t green = rgb565(0x6FE0A8);
    const uint16_t amber = rgb565(0xFFD166);
    const uint16_t ink = rgb565(0x18202A);
    const uint16_t white = rgb565(0xF3F7FA);

    for (uint16_t y = 0; y < HOME_IMG_H; y++) {
        for (uint16_t x = 0; x < HOME_IMG_W; x++) {
            uint16_t color = bg;

            if (x < 2 || y < 2 || x >= HOME_IMG_W - 2 || y >= HOME_IMG_H - 2) {
                color = cyan;
            } else if (y < 18) {
                color = blue;
            } else if (x < 34 && y < 52) {
                color = green;
            } else if (x >= 62 && y >= 30) {
                color = amber;
            } else if (((x + y) % 17) < 3) {
                color = rgb565(0x1C3C55);
            }
            put_px(pixels, x, y, color);
        }
    }

    /* Tiny symbolic "UI card" marks; no font dependency in the baked image. */
    for (uint16_t y = 24; y < 30; y++) {
        for (uint16_t x = 42; x < 82; x++) {
            put_px(pixels, x, y, white);
        }
    }
    for (uint16_t y = 36; y < 42; y++) {
        for (uint16_t x = 42; x < 72; x++) {
            put_px(pixels, x, y, rgb565(0x8AA0B4));
        }
    }
    for (uint16_t y = 50; y < 60; y++) {
        for (uint16_t x = 12; x < 84; x++) {
            if (x < 14 || x >= 82 || y < 52 || y >= 58) {
                put_px(pixels, x, y, ink);
            }
        }
    }
}

static void free_item_params(item_params_t *p)
{
    if (p != NULL) {
        free(p->data);
        p->data = NULL;
        p->len = 0;
    }
}

static int build_item_params(const char *const *items, uint16_t item_count,
                             uint16_t selected, uint16_t item_height,
                             uint16_t rows_or_page, uint16_t flags,
                             item_params_t *out)
{
    uint32_t total = 12u;

    if (items == NULL || out == NULL) {
        return 1;
    }
    for (uint16_t i = 0; i < item_count; i++) {
        const char *item = items[i] != NULL ? items[i] : "";
        size_t len = strlen(item);
        if (len > 0xFFFFu || total + 2u + len > 0xFFFFu) {
            return 1;
        }
        total += 2u + (uint32_t)len;
    }

    uint8_t *buf = (uint8_t *)malloc(total);
    if (buf == NULL) {
        return 1;
    }

    gsp_wr_u16(buf + 0, item_count);
    gsp_wr_u16(buf + 2, selected);
    gsp_wr_u16(buf + 4, item_height);
    gsp_wr_u16(buf + 6, rows_or_page);
    gsp_wr_u16(buf + 8, flags);
    gsp_wr_u16(buf + 10, 0u);

    uint32_t cursor = 12u;
    for (uint16_t i = 0; i < item_count; i++) {
        const char *item = items[i] != NULL ? items[i] : "";
        uint16_t len = (uint16_t)strlen(item);
        gsp_wr_u16(buf + cursor, len);
        cursor += 2u;
        memcpy(buf + cursor, item, len);
        cursor += len;
    }

    out->data = buf;
    out->len = (uint16_t)total;
    return 0;
}

static int write_preview_bmp(const char *path, const uint8_t *pixels)
{
    const uint32_t row_bytes = HOME_IMG_W * 3u;
    const uint32_t row_padded = (row_bytes + 3u) & ~3u;
    const uint32_t pixel_bytes = row_padded * HOME_IMG_H;
    const uint32_t file_size = 14u + 40u + pixel_bytes;
    uint8_t pad[3] = {0};
    uint8_t le[4] = {0};
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return 1;
    }

    fputc('B', fp);
    fputc('M', fp);
    gsp_wr_u32(le, file_size);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 0);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 54u);
    fwrite(le, 1, 4, fp);

    gsp_wr_u32(le, 40u);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, HOME_IMG_W);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, HOME_IMG_H);
    fwrite(le, 1, 4, fp);
    gsp_wr_u16(le, 1u);
    fwrite(le, 1, 2, fp);
    gsp_wr_u16(le, 24u);
    fwrite(le, 1, 2, fp);
    gsp_wr_u32(le, 0u);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, pixel_bytes);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 2835u);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 2835u);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 0u);
    fwrite(le, 1, 4, fp);
    gsp_wr_u32(le, 0u);
    fwrite(le, 1, 4, fp);

    for (int y = HOME_IMG_H - 1; y >= 0; y--) {
        for (uint16_t x = 0; x < HOME_IMG_W; x++) {
            const uint8_t *p = pixels + ((size_t)y * HOME_IMG_W + x) * 2u;
            uint16_t c = gsp_rd_u16(p);
            uint8_t r = (uint8_t)(((c >> 11) & 0x1Fu) * 255u / 31u);
            uint8_t g = (uint8_t)(((c >> 5) & 0x3Fu) * 255u / 63u);
            uint8_t b = (uint8_t)((c & 0x1Fu) * 255u / 31u);
            fputc(b, fp);
            fputc(g, fp);
            fputc(r, fp);
        }
        fwrite(pad, 1, row_padded - row_bytes, fp);
    }

    fclose(fp);
    return 0;
}

static const char *type_name(uint16_t type)
{
    switch (type) {
    case GSP_OBJ_CONTAINER: return "container";
    case GSP_OBJ_LABEL: return "label";
    case GSP_OBJ_BUTTON: return "button";
    case GSP_OBJ_IMAGE: return "image";
    case GSP_OBJ_LIST: return "list";
    case GSP_OBJ_WHEEL: return "wheel";
    case GSP_OBJ_LAYER: return "layer";
    default: return "?";
    }
}

static const char *codec_name(uint8_t codec)
{
    switch (codec) {
    case GSP_CODEC_STORE: return "store";
    case GSP_CODEC_RLE16: return "rle16";
    default: return "?";
    }
}

static const char *event_name(uint16_t ev)
{
    switch (ev) {
    case GSP_EV_CLICK: return "click";
    case GSP_EV_PRESS: return "press";
    case GSP_EV_RELEASE: return "release";
    case GSP_EV_LONG: return "long";
    case GSP_EV_VALUE: return "value";
    default: return "none";
    }
}

static const char *action_name(uint16_t act)
{
    switch (act) {
    case GSP_ACT_SHOW: return "show";
    case GSP_ACT_HIDE: return "hide";
    case GSP_ACT_TOGGLE: return "toggle";
    case GSP_ACT_SET_TEXT: return "set_text";
    case GSP_ACT_SET_BG_COLOR: return "set_bg_color";
    case GSP_ACT_SET_OPACITY: return "set_opacity";
    case GSP_ACT_CALL: return "call";
    case GSP_ACT_GOTO: return "goto";
    case GSP_ACT_BACK: return "back";
    default: return "none";
    }
}

static const char *safe_str(const uint8_t *pkg, size_t size, uint32_t off)
{
    if (off == 0 || (size_t)off >= size) {
        return NULL;
    }
    return (const char *)(pkg + off);
}

static int write_inc(const char *path, const uint8_t *pkg, size_t pkg_size,
                     const gsp_font_desc_t *fonts, size_t font_count)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return 1;
    }

    fprintf(fp,
            "/* Auto-generated by gsp_export_home - do not edit.\n"
            " * ITE-style GSP scene package include:\n"
            " *   - home_fonts[]: runtime resource bindings for font_id\n"
            " *   - home_scene_pkg[]: complete GSP1 binary scene package\n"
            " *\n"
            " * home_scene_pkg contains no native pointers. Object references are u16 indexes;\n"
            " * strings and blob data are u32 byte offsets from the package base.\n"
            " */\n"
            "#include \"gsp_format.h\"\n\n"
            "#define HOME_SCREEN_W       %u\n"
            "#define HOME_SCREEN_H       %u\n"
            "#define HOME_COLOR_FMT      0\n"
            "#define HOME_OBJ_COUNT      %u\n"
            "#define HOME_FONT_COUNT     %u\n"
            "#define HOME_ACTION_COUNT   %u\n"
            "#define HOME_FONT_CJK_REGULAR_PATH \"fonts/NotoSansCJK-Regular.ttc\"\n\n",
            HOME_SCREEN_W, HOME_SCREEN_H, HOME_OBJ_COUNT, HOME_FONT_COUNT, HOME_ACTION_COUNT);

    fprintf(fp, "static const gsp_font_desc_t home_fonts[HOME_FONT_COUNT] = {\n");
    for (size_t i = 0; i < font_count; i++) {
        fprintf(fp,
                "    { .id = %u, .family = \"%s\", .path = HOME_FONT_CJK_REGULAR_PATH, "
                ".size_px = %u, .weight = %u, .style = %u },\n",
                fonts[i].id, fonts[i].family, fonts[i].size_px, fonts[i].weight,
                fonts[i].style);
    }
    fprintf(fp, "};\n\n");

    fprintf(fp, "static const uint8_t home_scene_pkg[] = {\n");
    for (size_t i = 0; i < pkg_size; i++) {
        if ((i % 12u) == 0) {
            fprintf(fp, "    ");
        }
        fprintf(fp, "0x%02X,%s", pkg[i], (i + 1u == pkg_size) ? "" : " ");
        if ((i % 12u) == 11u || i + 1u == pkg_size) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, "};\n\nstatic const size_t home_scene_pkg_len = sizeof(home_scene_pkg);\n");
    fclose(fp);
    return 0;
}

static void write_flags(FILE *fp, uint32_t flags)
{
    bool any = false;

    struct {
        uint32_t bit;
        const char *name;
    } names[] = {
        { GSP_F_TEXT, "TEXT" },
        { GSP_F_FG_COLOR, "FG_COLOR" },
        { GSP_F_BG_COLOR, "BG_COLOR" },
        { GSP_F_BORDER, "BORDER" },
        { GSP_F_RADIUS, "RADIUS" },
        { GSP_F_CALLBACK, "CALLBACK" },
        { GSP_F_IMAGE, "IMAGE" },
        { GSP_F_NAME, "NAME" },
        { GSP_F_HIDDEN, "HIDDEN" },
        { GSP_F_OPACITY, "OPACITY" },
        { GSP_F_ALIGN, "ALIGN" },
        { GSP_F_PARAMS, "PARAMS" },
    };

    if (flags == 0) {
        fprintf(fp, "none");
        return;
    }

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (flags & names[i].bit) {
            fprintf(fp, "%s%s", any ? "|" : "", names[i].name);
            any = true;
        }
    }
}

static int write_manifest(const char *path, const uint8_t *pkg, size_t pkg_size,
                          const gsp_font_desc_t *fonts, size_t font_count,
                          const obj_meta_t *meta)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return 1;
    }

    const uint32_t version = gsp_rd_u32(pkg + 4);
    const uint16_t sw = gsp_rd_u16(pkg + 8);
    const uint16_t sh = gsp_rd_u16(pkg + 10);
    const uint32_t bg = gsp_rd_u32(pkg + 12);
    const uint32_t obj_count = gsp_rd_u32(pkg + 16);
    const uint32_t obj_off = gsp_rd_u32(pkg + 20);
    const uint32_t str_off = gsp_rd_u32(pkg + 24);
    const uint32_t blob_count = gsp_rd_u32(pkg + 28);
    const uint32_t blob_off = gsp_rd_u32(pkg + 32);
    const uint32_t total = gsp_rd_u32(pkg + 36);
    const uint32_t crc = gsp_rd_u32(pkg + 40);
    const uint32_t action_count = gsp_rd_u32(pkg + 44);
    const uint32_t action_off = gsp_rd_u32(pkg + 48);
    const uint32_t params_off = action_off + action_count * GSP_ACTION_SIZE;
    uint32_t first_blob_data = total;

    for (uint32_t i = 0; i < blob_count; i++) {
        const uint8_t *b = pkg + blob_off + (size_t)i * GSP_BLOB_SIZE;
        uint32_t data_off = gsp_rd_u32(b + 16);
        if (data_off < first_blob_data) {
            first_blob_data = data_off;
        }
    }

    fprintf(fp,
            "# `home.inc` GSP Scene Package Manifest\n\n"
            "This file explains the binary bytes in `home.inc`. Keep it synchronized whenever `home_scene_pkg[]` changes.\n\n"
            "## Export Summary\n\n"
            "- Package include: `home.inc`\n"
            "- Runtime package symbol: `home_scene_pkg[]`\n"
            "- Package length symbol: `home_scene_pkg_len`\n"
            "- Runtime font binding symbol: `home_fonts[]`\n"
            "- Source preview image: `home_preview.bmp`\n"
            "- Format: GSP1 v%u, little-endian, pointer-free runtime package\n"
            "- Loader: `gsp_load_with_fonts(home_scene_pkg, home_scene_pkg_len, ...)`\n"
            "- Current change: enlarged the main container and replaced the placeholder with a baked image blob.\n\n",
            version);

    fprintf(fp,
            "## Header\n\n"
            "| Field | Value |\n"
            "|---|---:|\n"
            "| magic | `GSP1` |\n"
            "| version | `%u` |\n"
            "| screen | `%u x %u` |\n"
            "| screen_bg | `0x%06X` |\n"
            "| obj_count | `%u` |\n"
            "| obj_table_off | `%u` |\n"
            "| str_table_off | `%u` |\n"
            "| blob_count | `%u` |\n"
            "| blob_table_off | `%u` |\n"
            "| action_count | `%u` |\n"
            "| action_table_off | `%u` |\n"
            "| total_size | `%u` |\n"
            "| crc32 | `0x%08X` |\n\n",
            version, sw, sh, bg & 0xFFFFFFu, obj_count, obj_off, str_off,
            blob_count, blob_off, action_count, action_off, total, crc);

    fprintf(fp,
            "## Binary Layout\n\n"
            "| Region | Byte Range | Size | Notes |\n"
            "|---|---:|---:|---|\n"
            "| Header | `0..55` | `56` | Fixed GSP header (v4) |\n"
            "| Object table | `%u..%u` | `%u` | `%u * 64B` object entries |\n"
            "| Blob table | `%u..%u` | `%u` | `%u * 20B` blob entries |\n"
            "| Action table | `%u..%u` | `%u` | `%u * 24B` action entries |\n"
            "| Params area | `%u..%u` | `%u` | Widget-private params blocks |\n"
            "| String table | `%u..%u` | `%u` | NUL-terminated UTF-8 strings |\n"
            "| Blob data | `%u..%u` | `%u` | Compressed image payloads |\n\n",
            obj_off, obj_off + obj_count * GSP_OBJ_SIZE - 1u, obj_count * GSP_OBJ_SIZE,
            obj_count,
            blob_off, blob_off + blob_count * GSP_BLOB_SIZE - 1u,
            blob_count * GSP_BLOB_SIZE, blob_count,
            action_off, action_off + action_count * GSP_ACTION_SIZE - 1u,
            action_count * GSP_ACTION_SIZE, action_count,
            params_off, str_off - 1u, str_off - params_off,
            str_off, first_blob_data - 1u, first_blob_data - str_off,
            first_blob_data, total - 1u, total - first_blob_data);

    fprintf(fp,
            "## Component Rules Used\n\n"
            "| ID | Type | Runtime create path |\n"
            "|---:|---|---|\n"
            "| `1` | container | `gfx_container_create()` |\n"
            "| `2` | label | `gfx_label_create()` |\n"
            "| `3` | button | `gfx_button_create()` |\n"
            "| `4` | image | `gfx_image_create()` + package blob source |\n"
            "| `5` | list | `gfx_list_create()` + params-v1 items |\n"
            "| `6` | wheel | `gfx_wheel_create()` + params-v1 items |\n"
            "| `7` | layer | `gfx_container_create()` + layer switching |\n\n"
            "Parent rule: entries are preorder; every non-root object must reference an earlier object index. `0xFFFF` means root.\n\n");

    fprintf(fp,
            "## Object Table\n\n"
            "| Idx | Role | Type | Parent | Rect | Flags | Name / Text / Callback / Blob | Font | Bind | Notes |\n"
            "|---:|---|---|---:|---|---:|---|---:|---:|---|\n");

    for (uint32_t i = 0; i < obj_count; i++) {
        const uint8_t *e = pkg + obj_off + (size_t)i * GSP_OBJ_SIZE;
        uint16_t type = gsp_rd_u16(e + 0);
        uint16_t parent = gsp_rd_u16(e + 2);
        int16_t x = gsp_rd_i16(e + 4);
        int16_t y = gsp_rd_i16(e + 6);
        uint16_t w = gsp_rd_u16(e + 8);
        uint16_t h = gsp_rd_u16(e + 10);
        uint32_t flags = gsp_rd_u32(e + 12);
        uint32_t text_off = gsp_rd_u32(e + 32);
        uint32_t cb_off = gsp_rd_u32(e + 36);
        uint32_t name_off = gsp_rd_u32(e + 40);
        uint32_t blob_idx = gsp_rd_u32(e + 44);
        uint16_t font_id = gsp_rd_u16(e + 56);
        uint16_t bind_id = gsp_rd_u16(e + 58);
        const char *text = safe_str(pkg, pkg_size, text_off);
        const char *cb = safe_str(pkg, pkg_size, cb_off);
        const char *name = safe_str(pkg, pkg_size, name_off);

        fprintf(fp, "| `%u` | %s | %s | ", i, meta[i].role, type_name(type));
        if (parent == GSP_NO_PARENT) {
            fprintf(fp, "root");
        } else {
            fprintf(fp, "`%u`", parent);
        }
        fprintf(fp, " | `(%d,%d %ux%u)` | `0x%03X` | ", x, y, w, h, flags);
        bool any_ref = false;
        if (name != NULL) {
            fprintf(fp, "name@`%u` `%s`", name_off, name);
            any_ref = true;
        }
        if (text != NULL) {
            fprintf(fp, "%stext@`%u` `%s`", any_ref ? ", " : "", text_off, text);
            any_ref = true;
        }
        if (flags & GSP_F_IMAGE) {
            fprintf(fp, "%sblob `%u`", any_ref ? ", " : "", blob_idx);
            any_ref = true;
        }
        if (cb != NULL) {
            fprintf(fp, "%scb@`%u` `%s`", any_ref ? ", " : "", cb_off, cb);
            any_ref = true;
        }
        if (!any_ref) {
            fprintf(fp, "none");
        }
        fprintf(fp, " | `%u` | `%u` | %s; flags: ", font_id, bind_id, meta[i].note);
        write_flags(fp, flags);
        fprintf(fp, " |\n");
    }

    fprintf(fp,
            "\n## Action Table (v4)\n\n"
            "Position-independent `event -> action` records (24B each). No inline structs, no pointers: "
            "targets are u16 object indexes or name offsets, params are u32 string offsets, colors are scalar `arg`.\n\n"
            "| Idx | Src Obj | Event | Action | Target | Param / Arg |\n"
            "|---:|---:|---|---|---|---|\n");
    for (uint32_t i = 0; i < action_count; i++) {
        const uint8_t *e = pkg + action_off + (size_t)i * GSP_ACTION_SIZE;
        uint16_t src = gsp_rd_u16(e + 0);
        uint16_t ev = gsp_rd_u16(e + 2);
        uint16_t act = gsp_rd_u16(e + 4);
        uint16_t tgt = gsp_rd_u16(e + 6);
        uint32_t name_off = gsp_rd_u32(e + 8);
        uint32_t param_off = gsp_rd_u32(e + 12);
        uint32_t arg = gsp_rd_u32(e + 20);
        const char *tname = safe_str(pkg, pkg_size, name_off);
        const char *param = safe_str(pkg, pkg_size, param_off);

        fprintf(fp, "| `%u` | `%u` | %s | %s | ", i, src, event_name(ev), action_name(act));
        if (tname != NULL) {
            fprintf(fp, "name@`%u` `%s`", name_off, tname);
        } else if (tgt != GSP_ACT_NO_TARGET) {
            fprintf(fp, "obj `%u`", tgt);
        } else {
            fprintf(fp, "none");
        }
        fprintf(fp, " | ");
        if (param != NULL) {
            fprintf(fp, "param@`%u` `%s`", param_off, param);
        } else if (arg != 0) {
            fprintf(fp, "arg `0x%06X`", arg & 0xFFFFFFu);
        } else {
            fprintf(fp, "none");
        }
        fprintf(fp, " |\n");
    }

    fprintf(fp,
            "\n## String Table\n\n"
            "| Offset | String |\n"
            "|---:|---|\n");
    for (uint32_t off = str_off; off < first_blob_data;) {
        const char *s = (const char *)(pkg + off);
        size_t len = strlen(s);
        fprintf(fp, "| `%u` | `%s` |\n", off, s);
        off += (uint32_t)len + 1u;
    }

    fprintf(fp,
            "\n## Resource Bindings\n\n"
            "Fonts are currently outside the binary package as a C binding table in the same `.inc`. Objects refer to these by `font_id`.\n\n"
            "| Font Id | Family | Path | Size | Weight | Style | Used By |\n"
            "|---:|---|---|---:|---:|---:|---|\n");
    for (size_t i = 0; i < font_count; i++) {
        fprintf(fp, "| `%u` | `%s` | `%s` | `%u` | `%u` | `%u` | ",
                fonts[i].id, fonts[i].family, fonts[i].path, fonts[i].size_px,
                fonts[i].weight, fonts[i].style);
        bool any = false;
        for (uint32_t j = 0; j < obj_count; j++) {
            const uint8_t *e = pkg + obj_off + (size_t)j * GSP_OBJ_SIZE;
            uint16_t type = gsp_rd_u16(e + 0);
            uint16_t font_id = gsp_rd_u16(e + 56);
            if ((type == GSP_OBJ_LABEL || type == GSP_OBJ_BUTTON) && font_id == fonts[i].id) {
                fprintf(fp, "%sobject `%u`", any ? ", " : "", j);
                any = true;
            }
        }
        fprintf(fp, "%s |\n", any ? "" : "none");
    }

    fprintf(fp,
            "\nImages/blobs:\n\n"
            "| Blob Id | Size | Format | Codec | Raw | Compressed | Data Offset | Used By |\n"
            "|---:|---|---:|---|---:|---:|---:|---|\n");
    for (uint32_t i = 0; i < blob_count; i++) {
        const uint8_t *b = pkg + blob_off + (size_t)i * GSP_BLOB_SIZE;
        uint16_t w = gsp_rd_u16(b + 0);
        uint16_t h = gsp_rd_u16(b + 2);
        uint8_t cf = b[4];
        uint8_t codec = b[5];
        uint32_t raw_size = gsp_rd_u32(b + 8);
        uint32_t comp_size = gsp_rd_u32(b + 12);
        uint32_t data_off = gsp_rd_u32(b + 16);
        fprintf(fp, "| `%u` | `%ux%u` | `0x%02X` | `%s` | `%u` | `%u` | `%u` | object `5` |\n",
                i, w, h, cf, codec_name(codec), raw_size, comp_size, data_off);
    }

    fprintf(fp,
            "\n## Runtime Path\n\n"
            "```text\n"
            "home.inc\n"
            "  home_fonts[]      -> host/device font creation or binding\n"
            "  home_scene_pkg[]  -> gsp_load_with_fonts()\n"
            "      header/CRC validation\n"
            "      object table scan\n"
            "      string offset resolution\n"
            "      runtime refs: object name / bind_id -> gfx_object_t\n"
            "      list/wheel params-v1 decode -> widget items\n"
            "      blob table resolution -> image blob decode/cache\n"
            "      gfx_*_create + setter\n"
            "      callback name binding: \"on_ok\" -> on_ok_cb\n"
            "      action table dispatch: Next buttons -> layer GOTO\n"
            "      render through normal GFX object tree\n"
            "```\n\n"
            "## Validation\n\n"
            "Run after export:\n\n"
            "```bash\n"
            "SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy GSP_HEADLESS=1 ./build-host-sdl/gfx_ai_scene_pkg_demo\n"
            "ctest --test-dir build-host-sdl -R ai_scene_pkg --output-on-failure\n"
            "```\n\n"
            "Latest result:\n\n"
            "- Package size: `%u` bytes\n"
            "- CRC: `0x%08X`\n"
            "- Loaded objects: `%u`\n"
            "- Image blobs: `%u`\n"
            "- Self-check: expected `PASS (%u objects from home.inc)`\n\n"
            "## Known Limits\n\n"
            "- Font descriptions are still C-side binding metadata, not binary package records.\n"
            "- Image blob is baked into the package, but the source authoring image is generated by this exporter for now.\n"
            "- List/wheel params-v1 is currently a compact demo format; future widgets should define their own params profile.\n",
            total, crc, obj_count, blob_count, obj_count);

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "examples/ai_scene_pkg";
    char inc_path[512];
    char manifest_path[512];
    char preview_path[512];

    snprintf(inc_path, sizeof(inc_path), "%s/home.inc", out_dir);
    snprintf(manifest_path, sizeof(manifest_path), "%s/home.manifest.md", out_dir);
    snprintf(preview_path, sizeof(preview_path), "%s/home_preview.bmp", out_dir);

    uint8_t *image_pixels = (uint8_t *)malloc((size_t)HOME_IMG_W * HOME_IMG_H * 2u);
    if (image_pixels == NULL) {
        return 1;
    }
    build_preview_image(image_pixels);

    const gfx_image_dsc_t preview_image = {
        .header = {
            .magic = GFX_IMAGE_HEADER_MAGIC,
            .cf = GFX_COLOR_FORMAT_RGB565,
            .w = HOME_IMG_W,
            .h = HOME_IMG_H,
            .stride = HOME_IMG_W * 2u,
        },
        .data_size = (uint32_t)HOME_IMG_W * HOME_IMG_H * 2u,
        .data = image_pixels,
    };

    const gsp_font_desc_t fonts[HOME_FONT_COUNT] = {
        { .id = 0, .family = "NotoSansCJK", .path = "fonts/NotoSansCJK-Regular.ttc", .size_px = 22, .weight = 400 },
        { .id = 1, .family = "NotoSansCJK", .path = "fonts/NotoSansCJK-Regular.ttc", .size_px = 17, .weight = 400 },
        { .id = 2, .family = "NotoSansCJK", .path = "fonts/NotoSansCJK-Regular.ttc", .size_px = 20, .weight = 400 },
        { .id = 3, .family = "NotoSansCJK", .path = "fonts/NotoSansCJK-Regular.ttc", .size_px = 21, .weight = 700 },
    };

    static const char *const list_items[] = {
        "Layer model",
        "Wheel widget",
        "List widget",
        "Binary params",
    };
    static const char *const wheel_items[] = {
        "RGB565",
        "RGB888",
        "BGR888",
        "ARGB8888",
    };
    item_params_t list_params = {0};
    item_params_t wheel_params = {0};
    if (build_item_params(list_items, 4, 1, 34, 4, GSP_ITEM_PARAMS_F_SNAP_TO_ITEM,
                          &list_params) != 0 ||
        build_item_params(wheel_items, 4, 0, 30, 5, GSP_ITEM_PARAMS_F_CYCLIC,
                          &wheel_params) != 0) {
        free_item_params(&list_params);
        free_item_params(&wheel_params);
        free(image_pixels);
        return 1;
    }

    const gsp_desc_t descs[HOME_OBJ_COUNT] = {
        { .type = GSP_OBJ_CONTAINER, .parent_idx = GSP_NO_PARENT,
          .flags = GSP_F_BG_COLOR, .x = 0, .y = 0, .w = 480, .h = 480,
          .bg_color = 0x0E1116 },
        { .type = GSP_OBJ_LAYER, .parent_idx = 0,
          .flags = GSP_F_BG_COLOR | GSP_F_BORDER | GSP_F_RADIUS | GSP_F_NAME,
          .x = 36, .y = 68, .w = 408, .h = 344, .name = "homeLayer",
          .bg_color = 0x171C24, .border_color = 0x2E3A47, .border_width = 2,
          .radius = 18 },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT | GSP_F_NAME,
          .x = 30, .y = 30, .w = 340, .h = 32,
          .fg_color = 0xFFFFFF, .font_id = 0,
          .text = "AI Scene · u32-offset pkg", .name = "title" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT | GSP_F_NAME,
          .x = 30, .y = 72, .w = 340, .h = 26,
          .fg_color = 0x8AA0B4, .font_id = 1,
          .text = "page 1: image + callback + action", .name = "subtitle" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT,
          .x = 30, .y = 106, .w = 340, .h = 30,
          .fg_color = 0x6FE0A8, .font_id = 2,
          .text = "温度 23.5°C 数据绑定", .bind_id = 1 },
        { .type = GSP_OBJ_IMAGE, .parent_idx = 1,
          .flags = GSP_F_IMAGE,
          .x = 30, .y = 150, .w = HOME_IMG_W, .h = HOME_IMG_H,
          .image_src = &preview_image },
        { .type = GSP_OBJ_BUTTON, .parent_idx = 1,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER |
                   GSP_F_RADIUS | GSP_F_TEXT | GSP_F_CALLBACK | GSP_F_NAME,
          .x = 238, .y = 246, .w = 140, .h = 60,
          .bg_color = 0x2F8CFF, .fg_color = 0xFFFFFF, .border_color = 0x76B7E8,
          .border_width = 2, .radius = 12, .font_id = 3,
          .text = "Next", .callback = "on_ok", .name = "homeNext" },
        { .type = GSP_OBJ_LAYER, .parent_idx = 0,
          .flags = GSP_F_BG_COLOR | GSP_F_BORDER | GSP_F_RADIUS | GSP_F_NAME | GSP_F_HIDDEN,
          .x = 36, .y = 68, .w = 408, .h = 344,
          .bg_color = 0x171C24, .border_color = 0x2E3A47, .border_width = 2,
          .radius = 18, .name = "selectorLayer" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 7,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT,
          .x = 30, .y = 28, .w = 340, .h = 32,
          .fg_color = 0xFFFFFF, .font_id = 0,
          .text = "Page 2 · List + Wheel" },
        { .type = GSP_OBJ_LIST, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER | GSP_F_NAME | GSP_F_PARAMS,
          .x = 30, .y = 78, .w = 168, .h = 170,
          .bg_color = 0x101923, .fg_color = 0xF3F7FA, .border_color = 0x2F8CFF,
          .border_width = 1, .font_id = 1, .name = "featureList",
          .params = list_params.data, .params_len = list_params.len },
        { .type = GSP_OBJ_WHEEL, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER | GSP_F_NAME | GSP_F_PARAMS,
          .x = 220, .y = 78, .w = 158, .h = 170,
          .bg_color = 0x101923, .fg_color = 0xF3F7FA, .border_color = 0x6FE0A8,
          .border_width = 1, .font_id = 1, .name = "formatWheel",
          .params = wheel_params.data, .params_len = wheel_params.len },
        { .type = GSP_OBJ_BUTTON, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER |
                   GSP_F_RADIUS | GSP_F_TEXT | GSP_F_NAME,
          .x = 238, .y = 246, .w = 140, .h = 60,
          .bg_color = 0x6FE0A8, .fg_color = 0x102033, .border_color = 0xA9F0C8,
          .border_width = 2, .radius = 12, .font_id = 3,
          .text = "Next", .name = "selectorNext" },
    };

    /* v4 动作表：两个底部 Next 纯数据驱动 layer 跳转；homeNext 同时保留 C 回调。*/
    const gsp_action_desc_t actions[HOME_ACTION_COUNT] = {
        { .src_idx = 6, .event = GSP_EV_CLICK, .action = GSP_ACT_SET_BG_COLOR,
          .target_idx = 1, .arg = 0x1E3A5F },
        { .src_idx = 6, .event = GSP_EV_CLICK, .action = GSP_ACT_GOTO,
          .target_name = "selectorLayer" },
        { .src_idx = 11, .event = GSP_EV_CLICK, .action = GSP_ACT_GOTO,
          .target_name = "homeLayer" },
    };

    const obj_meta_t meta[HOME_OBJ_COUNT] = {
        { "screen root", "Background 0x0E1116" },
        { "home layer", "First page layer" },
        { "title", "Main scene title" },
        { "subtitle", "Package invariance note" },
        { "data label", "Demo bind_id 1" },
        { "baked image", "RGB565 preview image baked into blob table" },
        { "home Next", "Callback on_ok + GOTO selectorLayer" },
        { "selector layer", "Second page layer, initially hidden" },
        { "selector title", "Second page title" },
        { "feature list", "List widget with params-v1 items" },
        { "format wheel", "Wheel widget with params-v1 items" },
        { "selector Next", "GOTO homeLayer" },
    };

    const gsp_scene_desc_t scene = {
        .screen_w = HOME_SCREEN_W,
        .screen_h = HOME_SCREEN_H,
        .screen_bg = 0x0E1116,
        .objs = descs,
        .obj_count = HOME_OBJ_COUNT,
        .fonts = fonts,
        .font_count = HOME_FONT_COUNT,
        .actions = actions,
        .action_count = HOME_ACTION_COUNT,
    };

    size_t pkg_size = 0;
    uint8_t *pkg = gsp_pack(&scene, &pkg_size);
    if (pkg == NULL) {
        free_item_params(&list_params);
        free_item_params(&wheel_params);
        free(image_pixels);
        return 1;
    }

    int rc = write_inc(inc_path, pkg, pkg_size, fonts, HOME_FONT_COUNT);
    if (rc == 0) {
        rc = write_manifest(manifest_path, pkg, pkg_size, fonts, HOME_FONT_COUNT, meta);
    }
    if (rc == 0) {
        rc = write_preview_bmp(preview_path, image_pixels);
    }

    printf("exported %s, %s, and %s (%zu bytes)\n",
           inc_path, manifest_path, preview_path, pkg_size);

    free(pkg);
    free_item_params(&list_params);
    free_item_params(&wheel_params);
    free(image_pixels);
    return rc;
}
