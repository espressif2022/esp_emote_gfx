/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gsp_core.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t x;
    int32_t y;
    bool hidden;
} render_state_t;

typedef struct {
    bool hidden_set;
    bool hidden;
    bool bg_set;
    uint32_t bg_color;
    bool opacity_set;
    uint8_t opacity;
    bool selected_set;
    uint16_t selected;
    char *text;
} runtime_obj_state_t;

struct gsp_core_runtime {
    const uint8_t *buf;
    size_t size;
    gsp_core_header_t hdr;
    runtime_obj_state_t *objs;
    char last_call[96];
};

static void color_rgb(uint32_t rgb, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((rgb >> 16) & 0xffu);
    *g = (uint8_t)((rgb >> 8) & 0xffu);
    *b = (uint8_t)(rgb & 0xffu);
}

static void blend_pixel(uint8_t *px, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uint32_t inv = 255u - a;
    px[0] = (uint8_t)(((uint32_t)r * a + (uint32_t)px[0] * inv + 127u) / 255u);
    px[1] = (uint8_t)(((uint32_t)g * a + (uint32_t)px[1] * inv + 127u) / 255u);
    px[2] = (uint8_t)(((uint32_t)b * a + (uint32_t)px[2] * inv + 127u) / 255u);
    px[3] = 255u;
}

static void draw_rect(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                      int32_t x, int32_t y, int32_t w, int32_t h,
                      uint32_t rgb, uint8_t alpha)
{
    if (w <= 0 || h <= 0 || alpha == 0) {
        return;
    }
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + w > (int32_t)width ? (int32_t)width : x + w;
    int32_t y1 = y + h > (int32_t)height ? (int32_t)height : y + h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    uint8_t r, g, b;
    color_rgb(rgb, &r, &g, &b);
    for (int32_t yy = y0; yy < y1; yy++) {
        uint8_t *row = rgba + (size_t)yy * stride;
        for (int32_t xx = x0; xx < x1; xx++) {
            blend_pixel(row + (size_t)xx * 4u, r, g, b, alpha);
        }
    }
}

static void draw_border(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                        int32_t x, int32_t y, int32_t w, int32_t h,
                        uint32_t rgb, uint16_t border_width, uint8_t alpha)
{
    int32_t bw = border_width > 0 ? border_width : 1;
    draw_rect(rgba, width, height, stride, x, y, w, bw, rgb, alpha);
    draw_rect(rgba, width, height, stride, x, y + h - bw, w, bw, rgb, alpha);
    draw_rect(rgba, width, height, stride, x, y, bw, h, rgb, alpha);
    draw_rect(rgba, width, height, stride, x + w - bw, y, bw, h, rgb, alpha);
}

static const uint8_t *glyph5x7(char c)
{
    static const uint8_t blank[7] = {0};
    static const uint8_t unknown[7] = {0x1f, 0x11, 0x05, 0x02, 0x00, 0x02, 0x00};
    static const struct {
        char c;
        uint8_t rows[7];
    } glyphs[] = {
        {'0',{0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}}, {'1',{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}},
        {'2',{0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}}, {'3',{0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e}},
        {'4',{0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}}, {'5',{0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}},
        {'6',{0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}}, {'7',{0x1f,0x01,0x02,0x04,0x08,0x08,0x08}},
        {'8',{0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}}, {'9',{0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}},
        {'A',{0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}}, {'B',{0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}},
        {'C',{0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}}, {'D',{0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}},
        {'E',{0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}}, {'F',{0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}},
        {'G',{0x0e,0x11,0x10,0x17,0x11,0x11,0x0f}}, {'H',{0x11,0x11,0x11,0x1f,0x11,0x11,0x11}},
        {'I',{0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}}, {'J',{0x07,0x02,0x02,0x02,0x12,0x12,0x0c}},
        {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1f}},
        {'M',{0x11,0x1b,0x15,0x15,0x11,0x11,0x11}}, {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
        {'O',{0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}}, {'P',{0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}},
        {'Q',{0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}}, {'R',{0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}},
        {'S',{0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}}, {'T',{0x1f,0x04,0x04,0x04,0x04,0x04,0x04}},
        {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0e}}, {'V',{0x11,0x11,0x11,0x11,0x11,0x0a,0x04}},
        {'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0a}}, {'X',{0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}},
        {'Y',{0x11,0x11,0x0a,0x04,0x04,0x04,0x04}}, {'Z',{0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}},
        {'a',{0x00,0x00,0x0e,0x01,0x0f,0x11,0x0f}}, {'b',{0x10,0x10,0x16,0x19,0x11,0x11,0x1e}},
        {'c',{0x00,0x00,0x0e,0x10,0x10,0x11,0x0e}}, {'d',{0x01,0x01,0x0d,0x13,0x11,0x11,0x0f}},
        {'e',{0x00,0x00,0x0e,0x11,0x1f,0x10,0x0e}}, {'f',{0x06,0x09,0x08,0x1c,0x08,0x08,0x08}},
        {'g',{0x00,0x0f,0x11,0x11,0x0f,0x01,0x0e}}, {'h',{0x10,0x10,0x16,0x19,0x11,0x11,0x11}},
        {'i',{0x04,0x00,0x0c,0x04,0x04,0x04,0x0e}}, {'j',{0x02,0x00,0x06,0x02,0x02,0x12,0x0c}},
        {'k',{0x10,0x10,0x12,0x14,0x18,0x14,0x12}}, {'l',{0x0c,0x04,0x04,0x04,0x04,0x04,0x0e}},
        {'m',{0x00,0x00,0x1a,0x15,0x15,0x11,0x11}}, {'n',{0x00,0x00,0x16,0x19,0x11,0x11,0x11}},
        {'o',{0x00,0x00,0x0e,0x11,0x11,0x11,0x0e}}, {'p',{0x00,0x00,0x1e,0x11,0x1e,0x10,0x10}},
        {'q',{0x00,0x00,0x0d,0x13,0x0f,0x01,0x01}}, {'r',{0x00,0x00,0x16,0x19,0x10,0x10,0x10}},
        {'s',{0x00,0x00,0x0f,0x10,0x0e,0x01,0x1e}}, {'t',{0x08,0x08,0x1c,0x08,0x08,0x09,0x06}},
        {'u',{0x00,0x00,0x11,0x11,0x11,0x13,0x0d}}, {'v',{0x00,0x00,0x11,0x11,0x11,0x0a,0x04}},
        {'w',{0x00,0x00,0x11,0x11,0x15,0x15,0x0a}}, {'x',{0x00,0x00,0x11,0x0a,0x04,0x0a,0x11}},
        {'y',{0x00,0x00,0x11,0x11,0x0f,0x01,0x0e}}, {'z',{0x00,0x00,0x1f,0x02,0x04,0x08,0x1f}},
        {' ',{0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, {'.',{0x00,0x00,0x00,0x00,0x00,0x0c,0x0c}},
        {':',{0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00}}, {'-',{0x00,0x00,0x00,0x1f,0x00,0x00,0x00}},
        {'+',{0x00,0x04,0x04,0x1f,0x04,0x04,0x00}}, {'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
        {'_',{0x00,0x00,0x00,0x00,0x00,0x00,0x1f}}, {'%',{0x19,0x19,0x02,0x04,0x08,0x13,0x13}},
    };
    if (c == '\0') {
        return blank;
    }
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].c == c) {
            return glyphs[i].rows;
        }
    }
    return unknown;
}

static void draw_char(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                      int32_t x, int32_t y, char c, uint32_t rgb, uint8_t alpha)
{
    const uint8_t *rows = glyph5x7(c);
    uint8_t r, g, b;
    color_rgb(rgb, &r, &g, &b);
    for (int yy = 0; yy < 7; yy++) {
        for (int xx = 0; xx < 5; xx++) {
            if ((rows[yy] & (uint8_t)(1u << (4 - xx))) == 0) {
                continue;
            }
            int32_t px = x + xx;
            int32_t py = y + yy;
            if (px >= 0 && py >= 0 && px < (int32_t)width && py < (int32_t)height) {
                blend_pixel(rgba + (size_t)py * stride + (size_t)px * 4u, r, g, b, alpha);
            }
        }
    }
}

static void draw_text(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                      int32_t x, int32_t y, int32_t box_w, const char *text,
                      uint32_t rgb, uint8_t alpha)
{
    if (text == NULL || box_w <= 0) {
        return;
    }
    int32_t pen = x;
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++) {
        if (*p < 0x80u) {
            if (pen + 6 > x + box_w) {
                break;
            }
            draw_char(rgba, width, height, stride, pen, y, (char)*p, rgb, alpha);
            pen += 6;
        } else {
            if (pen + 8 > x + box_w) {
                break;
            }
            draw_rect(rgba, width, height, stride, pen, y + 1, 6, 6, rgb, alpha);
            pen += 8;
            while ((p[1] & 0xC0u) == 0x80u) {
                p++;
            }
        }
    }
}

static int decode_blob(const uint8_t *buf, const gsp_core_blob_t *blob, uint8_t **raw_out)
{
    uint8_t *raw = (uint8_t *)malloc(blob->raw_size);
    if (raw == NULL) {
        return GSP_CORE_ERR_ALLOC;
    }
    const uint8_t *comp = buf + blob->data_off;
    if (blob->codec == GSP_CORE_CODEC_STORE) {
        memcpy(raw, comp, blob->raw_size);
    } else if (blob->codec == GSP_CORE_CODEC_RLE16) {
        uint32_t ro = 0;
        for (uint32_t ci = 0; ci + 4u <= blob->comp_size; ci += 4u) {
            uint16_t count = gsp_core_rd_u16(comp + ci);
            uint8_t lo = comp[ci + 2];
            uint8_t hi = comp[ci + 3];
            while (count-- > 0 && ro + 2u <= blob->raw_size) {
                raw[ro++] = lo;
                raw[ro++] = hi;
            }
        }
        if (ro != blob->raw_size) {
            free(raw);
            return GSP_CORE_ERR_CODEC;
        }
    } else {
        free(raw);
        return GSP_CORE_ERR_CODEC;
    }
    *raw_out = raw;
    return GSP_CORE_OK;
}

static void blob_pixel(const uint8_t *raw, const gsp_core_blob_t *blob, uint32_t x, uint32_t y,
                       uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
    uint32_t stride = blob->stride != 0 ? blob->stride : (uint32_t)blob->w * 2u;
    const uint8_t *p = raw + (size_t)y * stride;
    *a = 255u;
    if (blob->cf == GSP_CORE_CF_RGB565 || blob->cf == GSP_CORE_CF_RGB565_SWAPPED) {
        p += (size_t)x * 2u;
        uint16_t v = blob->cf == GSP_CORE_CF_RGB565 ?
            (uint16_t)p[0] | ((uint16_t)p[1] << 8) :
            (uint16_t)p[1] | ((uint16_t)p[0] << 8);
        *r = (uint8_t)(((v >> 11) & 0x1fu) * 255u / 31u);
        *g = (uint8_t)(((v >> 5) & 0x3fu) * 255u / 63u);
        *b = (uint8_t)((v & 0x1fu) * 255u / 31u);
    } else if (blob->cf == GSP_CORE_CF_RGB888 || blob->cf == GSP_CORE_CF_BGR888) {
        p += (size_t)x * 3u;
        *r = blob->cf == GSP_CORE_CF_RGB888 ? p[0] : p[2];
        *g = p[1];
        *b = blob->cf == GSP_CORE_CF_RGB888 ? p[2] : p[0];
    } else {
        p += (size_t)x * 4u;
        if (blob->cf == GSP_CORE_CF_ARGB8888) {
            *a = p[0];
            *r = p[1];
            *g = p[2];
            *b = p[3];
        } else {
            *r = p[1];
            *g = p[2];
            *b = p[3];
        }
    }
}

static int draw_image(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                      const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                      const gsp_core_obj_t *obj, int32_t x, int32_t y, uint8_t alpha)
{
    (void)size;
    gsp_core_blob_t blob;
    int rc = gsp_core_get_blob(buf, hdr->total_size, hdr, obj->blob_idx, &blob);
    if (rc != GSP_CORE_OK) {
        return rc;
    }
    uint8_t *raw = NULL;
    rc = decode_blob(buf, &blob, &raw);
    if (rc != GSP_CORE_OK) {
        return rc;
    }
    for (int32_t dy = 0; dy < (int32_t)obj->h; dy++) {
        int32_t py = y + dy;
        if (py < 0 || py >= (int32_t)height) {
            continue;
        }
        uint32_t sy = (uint32_t)((uint64_t)dy * blob.h / (obj->h ? obj->h : 1u));
        for (int32_t dx = 0; dx < (int32_t)obj->w; dx++) {
            int32_t px = x + dx;
            if (px < 0 || px >= (int32_t)width) {
                continue;
            }
            uint32_t sx = (uint32_t)((uint64_t)dx * blob.w / (obj->w ? obj->w : 1u));
            uint8_t r, g, b, a;
            blob_pixel(raw, &blob, sx, sy, &r, &g, &b, &a);
            uint8_t aa = (uint8_t)((uint32_t)a * alpha / 255u);
            blend_pixel(rgba + (size_t)py * stride + (size_t)px * 4u, r, g, b, aa);
        }
    }
    free(raw);
    return GSP_CORE_OK;
}

static void draw_items(uint8_t *rgba, uint32_t width, uint32_t height, uint32_t stride,
                       const uint8_t *buf, size_t size, const gsp_core_obj_t *obj,
                       const gsp_core_runtime_t *rt, uint32_t obj_idx,
                       int32_t x, int32_t y, uint8_t alpha)
{
    if ((obj->flags & GSP_CORE_F_PARAMS) == 0) {
        return;
    }
    gsp_core_item_params_t params;
    if (gsp_core_parse_item_params(buf, size, obj->params_off, obj->params_len, &params) != GSP_CORE_OK) {
        return;
    }
    uint32_t item_h = params.item_height != 0 ? params.item_height : 24u;
    uint16_t selected = params.selected;
    if (rt != NULL && obj_idx < rt->hdr.obj_count && rt->objs[obj_idx].selected_set) {
        selected = rt->objs[obj_idx].selected;
    }
    size_t cursor = 0;
    for (uint16_t i = 0; i < params.item_count; i++) {
        if (cursor + 2u > params.items_len) {
            break;
        }
        uint16_t len = gsp_core_rd_u16(params.items + cursor);
        cursor += 2u;
        if (cursor + len > params.items_len) {
            break;
        }
        int32_t iy = y + (int32_t)i * (int32_t)item_h;
        if (iy >= y + (int32_t)obj->h) {
            break;
        }
        if (i == selected) {
            draw_rect(rgba, width, height, stride, x + 3, iy + 2, obj->w - 6, item_h - 4,
                      0xffffffu, (uint8_t)(alpha / 5u));
        }
        char tmp[96];
        size_t n = len < sizeof(tmp) - 1u ? len : sizeof(tmp) - 1u;
        memcpy(tmp, params.items + cursor, n);
        tmp[n] = '\0';
        draw_text(rgba, width, height, stride, x + 8, iy + 8, obj->w - 16,
                  tmp, obj->fg_color, alpha);
        cursor += len;
    }
}

static bool runtime_obj_hidden(const gsp_core_runtime_t *rt, uint32_t idx,
                               const gsp_core_obj_t *obj)
{
    if (rt != NULL && idx < rt->hdr.obj_count && rt->objs[idx].hidden_set) {
        return rt->objs[idx].hidden;
    }
    return (obj->flags & GSP_CORE_F_HIDDEN) != 0;
}

static uint32_t runtime_obj_bg(const gsp_core_runtime_t *rt, uint32_t idx,
                               const gsp_core_obj_t *obj)
{
    if (rt != NULL && idx < rt->hdr.obj_count && rt->objs[idx].bg_set) {
        return rt->objs[idx].bg_color;
    }
    return obj->bg_color;
}

static uint8_t runtime_obj_alpha(const gsp_core_runtime_t *rt, uint32_t idx,
                                 const gsp_core_obj_t *obj)
{
    if (rt != NULL && idx < rt->hdr.obj_count && rt->objs[idx].opacity_set) {
        return rt->objs[idx].opacity;
    }
    return (obj->flags & GSP_CORE_F_OPACITY) ? obj->opacity : 255u;
}

static const char *runtime_obj_text(const gsp_core_runtime_t *rt, uint32_t idx,
                                    const uint8_t *buf, const gsp_core_header_t *hdr,
                                    const gsp_core_obj_t *obj)
{
    if (rt != NULL && idx < rt->hdr.obj_count && rt->objs[idx].text != NULL) {
        return rt->objs[idx].text;
    }
    return gsp_core_get_cstr(buf, hdr->total_size, obj->text_off);
}

static int compute_render_state(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                                const gsp_core_runtime_t *rt, render_state_t *state)
{
    for (uint32_t i = 0; i < hdr->obj_count; i++) {
        gsp_core_obj_t obj;
        int rc = gsp_core_get_object(buf, size, hdr, i, &obj);
        if (rc != GSP_CORE_OK) {
            return rc;
        }
        int32_t abs_x = obj.x;
        int32_t abs_y = obj.y;
        bool hidden = runtime_obj_hidden(rt, i, &obj);
        if (obj.parent_idx != GSP_CORE_NO_PARENT) {
            abs_x += state[obj.parent_idx].x;
            abs_y += state[obj.parent_idx].y;
            hidden = hidden || state[obj.parent_idx].hidden;
        }
        state[i] = (render_state_t) {.x = abs_x, .y = abs_y, .hidden = hidden};
    }
    return GSP_CORE_OK;
}

static int render_rgba_internal(const uint8_t *buf, size_t size, const gsp_core_runtime_t *rt,
                                uint8_t *rgba, uint32_t width, uint32_t height,
                                uint32_t stride)
{
    if (buf == NULL || rgba == NULL || stride < width * 4u) {
        return GSP_CORE_ERR_ARG;
    }
    gsp_core_header_t hdr;
    int rc = gsp_core_parse_header(buf, size, &hdr);
    if (rc != GSP_CORE_OK) {
        return rc;
    }
    if (width != hdr.screen_w || height != hdr.screen_h) {
        return GSP_CORE_ERR_ARG;
    }
    rc = gsp_core_validate(buf, size);
    if (rc != GSP_CORE_OK) {
        return rc;
    }

    render_state_t *state = (render_state_t *)calloc(hdr.obj_count, sizeof(*state));
    if (state == NULL) {
        return GSP_CORE_ERR_ALLOC;
    }
    rc = compute_render_state(buf, size, &hdr, rt, state);
    if (rc != GSP_CORE_OK) {
        free(state);
        return rc;
    }

    uint8_t br, bg, bb;
    color_rgb(hdr.screen_bg, &br, &bg, &bb);
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = rgba + (size_t)y * stride;
        for (uint32_t x = 0; x < width; x++) {
            row[x * 4u + 0] = br;
            row[x * 4u + 1] = bg;
            row[x * 4u + 2] = bb;
            row[x * 4u + 3] = 255u;
        }
    }

    for (uint32_t i = 0; i < hdr.obj_count; i++) {
        gsp_core_obj_t obj;
        rc = gsp_core_get_object(buf, size, &hdr, i, &obj);
        if (rc != GSP_CORE_OK) {
            free(state);
            return rc;
        }
        int32_t abs_x = state[i].x;
        int32_t abs_y = state[i].y;
        if (state[i].hidden) {
            continue;
        }

        uint8_t alpha = runtime_obj_alpha(rt, i, &obj);
        if (obj.flags & GSP_CORE_F_BG_COLOR) {
            draw_rect(rgba, width, height, stride, abs_x, abs_y, obj.w, obj.h,
                      runtime_obj_bg(rt, i, &obj), alpha);
        } else if (obj.type == GSP_CORE_OBJ_BUTTON) {
            draw_rect(rgba, width, height, stride, abs_x, abs_y, obj.w, obj.h, 0x263241u, alpha);
        }
        if ((obj.flags & GSP_CORE_F_IMAGE) && obj.type == GSP_CORE_OBJ_IMAGE) {
            rc = draw_image(rgba, width, height, stride, buf, size, &hdr, &obj, abs_x, abs_y, alpha);
            if (rc != GSP_CORE_OK) {
                free(state);
                return rc;
            }
        }
        if (obj.type == GSP_CORE_OBJ_LIST || obj.type == GSP_CORE_OBJ_WHEEL) {
            draw_items(rgba, width, height, stride, buf, size, &obj, rt, i, abs_x, abs_y, alpha);
        }
        if (obj.flags & GSP_CORE_F_BORDER) {
            draw_border(rgba, width, height, stride, abs_x, abs_y, obj.w, obj.h,
                        obj.border_color, obj.border_width, alpha);
        }
        if (obj.flags & GSP_CORE_F_TEXT) {
            const char *text = runtime_obj_text(rt, i, buf, &hdr, &obj);
            int32_t ty = abs_y + ((obj.type == GSP_CORE_OBJ_BUTTON && obj.h > 12) ? ((int32_t)obj.h - 7) / 2 : 4);
            int32_t tx = abs_x + (obj.type == GSP_CORE_OBJ_BUTTON ? 10 : 0);
            int32_t tw = obj.type == GSP_CORE_OBJ_BUTTON ? (int32_t)obj.w - 20 : obj.w;
            draw_text(rgba, width, height, stride, tx, ty, tw, text, obj.fg_color, alpha);
        }
    }
    free(state);
    return GSP_CORE_OK;
}

int gsp_core_render_rgba(const uint8_t *buf, size_t size, uint8_t *rgba,
                         uint32_t width, uint32_t height, uint32_t stride)
{
    return render_rgba_internal(buf, size, NULL, rgba, width, height, stride);
}

gsp_core_runtime_t *gsp_core_runtime_create(const uint8_t *buf, size_t size)
{
    gsp_core_header_t hdr;
    if (gsp_core_parse_header(buf, size, &hdr) != GSP_CORE_OK ||
        gsp_core_validate(buf, size) != GSP_CORE_OK) {
        return NULL;
    }
    gsp_core_runtime_t *rt = (gsp_core_runtime_t *)calloc(1, sizeof(*rt));
    if (rt == NULL) {
        return NULL;
    }
    rt->objs = (runtime_obj_state_t *)calloc(hdr.obj_count, sizeof(*rt->objs));
    if (rt->objs == NULL) {
        free(rt);
        return NULL;
    }
    rt->buf = buf;
    rt->size = size;
    rt->hdr = hdr;
    return rt;
}

void gsp_core_runtime_destroy(gsp_core_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }
    if (rt->objs != NULL) {
        for (uint32_t i = 0; i < rt->hdr.obj_count; i++) {
            free(rt->objs[i].text);
        }
    }
    free(rt->objs);
    free(rt);
}

int gsp_core_runtime_render_rgba(gsp_core_runtime_t *rt, uint8_t *rgba,
                                 uint32_t width, uint32_t height, uint32_t stride)
{
    if (rt == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    return render_rgba_internal(rt->buf, rt->size, rt, rgba, width, height, stride);
}

static int object_index_by_name(gsp_core_runtime_t *rt, const char *name)
{
    if (rt == NULL || name == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < rt->hdr.obj_count; i++) {
        gsp_core_obj_t obj;
        if (gsp_core_get_object(rt->buf, rt->size, &rt->hdr, i, &obj) != GSP_CORE_OK) {
            return -1;
        }
        const char *obj_name = gsp_core_get_cstr(rt->buf, rt->hdr.total_size, obj.name_off);
        if (obj_name != NULL && strcmp(obj_name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int action_target(gsp_core_runtime_t *rt, const gsp_core_action_t *action)
{
    if (action->target_name_off != 0) {
        const char *name = gsp_core_get_cstr(rt->buf, rt->hdr.total_size, action->target_name_off);
        return object_index_by_name(rt, name);
    }
    return action->target_idx == GSP_CORE_ACT_NO_TARGET ? -1 : (int)action->target_idx;
}

static char *dup_cstr(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s) + 1u;
    char *out = (char *)malloc(len);
    if (out != NULL) {
        memcpy(out, s, len);
    }
    return out;
}

static int runtime_show_layer(gsp_core_runtime_t *rt, int target)
{
    gsp_core_obj_t layer;
    if (target < 0 || gsp_core_get_object(rt->buf, rt->size, &rt->hdr, (uint32_t)target, &layer) != GSP_CORE_OK ||
        layer.type != GSP_CORE_OBJ_LAYER) {
        return GSP_CORE_ERR_ACTION;
    }
    for (uint32_t i = 0; i < rt->hdr.obj_count; i++) {
        gsp_core_obj_t obj;
        if (gsp_core_get_object(rt->buf, rt->size, &rt->hdr, i, &obj) != GSP_CORE_OK) {
            return GSP_CORE_ERR_ACTION;
        }
        if (obj.type == GSP_CORE_OBJ_LAYER && obj.parent_idx == layer.parent_idx) {
            rt->objs[i].hidden_set = true;
            rt->objs[i].hidden = ((int)i != target);
        }
    }
    return GSP_CORE_OK;
}

static int runtime_execute_action(gsp_core_runtime_t *rt, const gsp_core_action_t *action)
{
    int target = action_target(rt, action);
    switch (action->action) {
    case GSP_CORE_ACT_SHOW:
        if (target >= 0) {
            rt->objs[target].hidden_set = true;
            rt->objs[target].hidden = false;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_HIDE:
        if (target >= 0) {
            rt->objs[target].hidden_set = true;
            rt->objs[target].hidden = true;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_TOGGLE:
        if (target >= 0) {
            gsp_core_obj_t obj;
            if (gsp_core_get_object(rt->buf, rt->size, &rt->hdr, (uint32_t)target, &obj) != GSP_CORE_OK) {
                return GSP_CORE_ERR_ACTION;
            }
            bool cur = rt->objs[target].hidden_set ? rt->objs[target].hidden :
                ((obj.flags & GSP_CORE_F_HIDDEN) != 0);
            rt->objs[target].hidden_set = true;
            rt->objs[target].hidden = !cur;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_SET_TEXT:
        if (target >= 0 && action->param_off != 0) {
            const char *param = gsp_core_get_cstr(rt->buf, rt->hdr.total_size, action->param_off);
            char *copy = dup_cstr(param);
            if (copy == NULL) {
                return GSP_CORE_ERR_ALLOC;
            }
            free(rt->objs[target].text);
            rt->objs[target].text = copy;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_SET_BG_COLOR:
        if (target >= 0) {
            rt->objs[target].bg_set = true;
            rt->objs[target].bg_color = action->arg & 0x00ffffffu;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_SET_OPACITY:
        if (target >= 0) {
            rt->objs[target].opacity_set = true;
            rt->objs[target].opacity = action->arg > 255u ? 255u : (uint8_t)action->arg;
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_CALL:
        if (action->target_name_off != 0) {
            const char *name = gsp_core_get_cstr(rt->buf, rt->hdr.total_size, action->target_name_off);
            if (name != NULL) {
                snprintf(rt->last_call, sizeof(rt->last_call), "%s", name);
            }
        }
        return GSP_CORE_OK;
    case GSP_CORE_ACT_GOTO:
        return runtime_show_layer(rt, target);
    default:
        return GSP_CORE_OK;
    }
}

static int runtime_update_value_object(gsp_core_runtime_t *rt, int idx, int32_t x, int32_t y)
{
    gsp_core_obj_t obj;
    if (idx < 0 || gsp_core_get_object(rt->buf, rt->size, &rt->hdr, (uint32_t)idx, &obj) != GSP_CORE_OK) {
        return GSP_CORE_ERR_BOUNDS;
    }
    if (obj.type != GSP_CORE_OBJ_LIST && obj.type != GSP_CORE_OBJ_WHEEL) {
        return GSP_CORE_OK;
    }
    gsp_core_item_params_t params;
    if (gsp_core_parse_item_params(rt->buf, rt->size, obj.params_off, obj.params_len, &params) != GSP_CORE_OK ||
        params.item_count == 0) {
        return GSP_CORE_OK;
    }
    render_state_t *state = (render_state_t *)calloc(rt->hdr.obj_count, sizeof(*state));
    if (state == NULL) {
        return GSP_CORE_ERR_ALLOC;
    }
    int rc = compute_render_state(rt->buf, rt->size, &rt->hdr, rt, state);
    if (rc != GSP_CORE_OK) {
        free(state);
        return rc;
    }
    uint32_t item_h = params.item_height != 0 ? params.item_height : 24u;
    int32_t rel_y = y - state[idx].y;
    uint16_t selected = 0;
    if (rel_y > 0) {
        selected = (uint16_t)((uint32_t)rel_y / item_h);
    }
    if (selected >= params.item_count) {
        selected = params.item_count - 1u;
    }
    rt->objs[idx].selected_set = true;
    rt->objs[idx].selected = selected;
    free(state);
    (void)x;
    return GSP_CORE_OK;
}

int gsp_core_runtime_hit_test(gsp_core_runtime_t *rt, int32_t x, int32_t y)
{
    if (rt == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    render_state_t *state = (render_state_t *)calloc(rt->hdr.obj_count, sizeof(*state));
    if (state == NULL) {
        return GSP_CORE_ERR_ALLOC;
    }
    int rc = compute_render_state(rt->buf, rt->size, &rt->hdr, rt, state);
    if (rc != GSP_CORE_OK) {
        free(state);
        return rc;
    }
    for (int32_t i = (int32_t)rt->hdr.obj_count - 1; i >= 0; i--) {
        gsp_core_obj_t obj;
        if (gsp_core_get_object(rt->buf, rt->size, &rt->hdr, (uint32_t)i, &obj) != GSP_CORE_OK) {
            free(state);
            return GSP_CORE_ERR_BOUNDS;
        }
        if (state[i].hidden) {
            continue;
        }
        if (x >= state[i].x && y >= state[i].y &&
            x < state[i].x + (int32_t)obj.w && y < state[i].y + (int32_t)obj.h) {
            free(state);
            return i;
        }
    }
    free(state);
    return -1;
}

int gsp_core_runtime_click(gsp_core_runtime_t *rt, int32_t x, int32_t y)
{
    if (rt == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    int hit = gsp_core_runtime_hit_test(rt, x, y);
    if (hit < 0) {
        return hit;
    }
    int rc = runtime_update_value_object(rt, hit, x, y);
    if (rc != GSP_CORE_OK) {
        return rc;
    }
    gsp_core_obj_t hit_obj;
    if (gsp_core_get_object(rt->buf, rt->size, &rt->hdr, (uint32_t)hit, &hit_obj) != GSP_CORE_OK) {
        return GSP_CORE_ERR_BOUNDS;
    }
    if (hit_obj.flags & GSP_CORE_F_CALLBACK) {
        const char *cb = gsp_core_get_cstr(rt->buf, rt->hdr.total_size, hit_obj.callback_off);
        if (cb != NULL) {
            snprintf(rt->last_call, sizeof(rt->last_call), "%s", cb);
        }
    }
    for (uint32_t i = 0; i < rt->hdr.action_count; i++) {
        gsp_core_action_t action;
        rc = gsp_core_get_action(rt->buf, rt->size, &rt->hdr, i, &action);
        if (rc != GSP_CORE_OK) {
            return rc;
        }
        if (action.src_idx == (uint16_t)hit &&
            (action.event == GSP_CORE_EV_CLICK || action.event == GSP_CORE_EV_RELEASE ||
             action.event == GSP_CORE_EV_VALUE)) {
            rc = runtime_execute_action(rt, &action);
            if (rc != GSP_CORE_OK) {
                return rc;
            }
        }
    }
    return hit;
}

const char *gsp_core_runtime_last_call(const gsp_core_runtime_t *rt)
{
    return rt != NULL && rt->last_call[0] != '\0' ? rt->last_call : NULL;
}

void gsp_core_runtime_clear_last_call(gsp_core_runtime_t *rt)
{
    if (rt != NULL) {
        rt->last_call[0] = '\0';
    }
}
