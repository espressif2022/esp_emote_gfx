/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gsp_core.h"

#include <stdbool.h>
#include <string.h>

uint16_t gsp_core_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int16_t gsp_core_rd_i16(const uint8_t *p)
{
    return (int16_t)gsp_core_rd_u16(p);
}

uint32_t gsp_core_rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t crc32_feed(uint32_t crc, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc;
}

uint32_t gsp_core_crc32(const uint8_t *buf, uint32_t total)
{
    static const uint8_t zero4[4] = {0, 0, 0, 0};
    uint32_t c = 0xFFFFFFFFu;

    c = crc32_feed(c, buf, 40);
    c = crc32_feed(c, zero4, sizeof(zero4));
    if (total > 44u) {
        c = crc32_feed(c, buf + 44, total - 44u);
    }
    return c ^ 0xFFFFFFFFu;
}

int gsp_core_parse_header(const uint8_t *buf, size_t size, gsp_core_header_t *out)
{
    if (buf == NULL || out == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    if (size < GSP_CORE_HEADER_SIZE) {
        return GSP_CORE_ERR_SIZE;
    }
    if (gsp_core_rd_u32(buf + 0) != GSP_CORE_MAGIC) {
        return GSP_CORE_ERR_MAGIC;
    }
    if (gsp_core_rd_u32(buf + 4) != GSP_CORE_VERSION) {
        return GSP_CORE_ERR_VERSION;
    }

    gsp_core_header_t h = {
        .version = gsp_core_rd_u32(buf + 4),
        .screen_w = gsp_core_rd_u16(buf + 8),
        .screen_h = gsp_core_rd_u16(buf + 10),
        .screen_bg = gsp_core_rd_u32(buf + 12),
        .obj_count = gsp_core_rd_u32(buf + 16),
        .obj_table_off = gsp_core_rd_u32(buf + 20),
        .str_table_off = gsp_core_rd_u32(buf + 24),
        .blob_count = gsp_core_rd_u32(buf + 28),
        .blob_table_off = gsp_core_rd_u32(buf + 32),
        .total_size = gsp_core_rd_u32(buf + 36),
        .crc32 = gsp_core_rd_u32(buf + 40),
        .action_count = gsp_core_rd_u32(buf + 44),
        .action_table_off = gsp_core_rd_u32(buf + 48),
    };

    if (h.total_size < GSP_CORE_HEADER_SIZE || h.total_size > size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    *out = h;
    return GSP_CORE_OK;
}

int gsp_core_get_object(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                        uint32_t index, gsp_core_obj_t *out)
{
    if (buf == NULL || hdr == NULL || out == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    if (index >= hdr->obj_count) {
        return GSP_CORE_ERR_BOUNDS;
    }
    uint64_t off64 = (uint64_t)hdr->obj_table_off + (uint64_t)index * GSP_CORE_OBJ_SIZE;
    if (off64 + GSP_CORE_OBJ_SIZE > size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    const uint8_t *e = buf + (size_t)off64;
    *out = (gsp_core_obj_t) {
        .type = gsp_core_rd_u16(e + 0),
        .parent_idx = gsp_core_rd_u16(e + 2),
        .x = gsp_core_rd_i16(e + 4),
        .y = gsp_core_rd_i16(e + 6),
        .w = gsp_core_rd_u16(e + 8),
        .h = gsp_core_rd_u16(e + 10),
        .flags = gsp_core_rd_u32(e + 12),
        .fg_color = gsp_core_rd_u32(e + 16),
        .bg_color = gsp_core_rd_u32(e + 20),
        .border_color = gsp_core_rd_u32(e + 24),
        .border_width = gsp_core_rd_u16(e + 28),
        .radius = gsp_core_rd_u16(e + 30),
        .text_off = gsp_core_rd_u32(e + 32),
        .callback_off = gsp_core_rd_u32(e + 36),
        .name_off = gsp_core_rd_u32(e + 40),
        .blob_idx = gsp_core_rd_u32(e + 44),
        .params_off = gsp_core_rd_u32(e + 48),
        .params_len = gsp_core_rd_u16(e + 52),
        .opacity = e[54],
        .text_align = e[55],
        .font_id = gsp_core_rd_u16(e + 56),
        .bind_id = gsp_core_rd_u16(e + 58),
    };
    if (gsp_core_rd_u32(e + 60) != 0) {
        return GSP_CORE_ERR_BOUNDS;
    }
    return GSP_CORE_OK;
}

int gsp_core_get_blob(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                      uint32_t index, gsp_core_blob_t *out)
{
    if (buf == NULL || hdr == NULL || out == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    if (index >= hdr->blob_count) {
        return GSP_CORE_ERR_BOUNDS;
    }
    uint64_t off64 = (uint64_t)hdr->blob_table_off + (uint64_t)index * GSP_CORE_BLOB_SIZE;
    if (off64 + GSP_CORE_BLOB_SIZE > size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    const uint8_t *b = buf + (size_t)off64;
    *out = (gsp_core_blob_t) {
        .w = gsp_core_rd_u16(b + 0),
        .h = gsp_core_rd_u16(b + 2),
        .cf = b[4],
        .codec = b[5],
        .stride = gsp_core_rd_u16(b + 6),
        .raw_size = gsp_core_rd_u32(b + 8),
        .comp_size = gsp_core_rd_u32(b + 12),
        .data_off = gsp_core_rd_u32(b + 16),
    };
    return GSP_CORE_OK;
}

int gsp_core_get_action(const uint8_t *buf, size_t size, const gsp_core_header_t *hdr,
                        uint32_t index, gsp_core_action_t *out)
{
    if (buf == NULL || hdr == NULL || out == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    if (index >= hdr->action_count || hdr->action_table_off == 0) {
        return GSP_CORE_ERR_BOUNDS;
    }
    uint64_t off64 = (uint64_t)hdr->action_table_off + (uint64_t)index * GSP_CORE_ACTION_SIZE;
    if (off64 + GSP_CORE_ACTION_SIZE > size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    const uint8_t *e = buf + (size_t)off64;
    *out = (gsp_core_action_t) {
        .src_idx = gsp_core_rd_u16(e + 0),
        .event = gsp_core_rd_u16(e + 2),
        .action = gsp_core_rd_u16(e + 4),
        .target_idx = gsp_core_rd_u16(e + 6),
        .target_name_off = gsp_core_rd_u32(e + 8),
        .param_off = gsp_core_rd_u32(e + 12),
        .param_len = gsp_core_rd_u16(e + 16),
        .flags = gsp_core_rd_u16(e + 18),
        .arg = gsp_core_rd_u32(e + 20),
    };
    return GSP_CORE_OK;
}

const char *gsp_core_get_cstr(const uint8_t *buf, size_t size, uint32_t off)
{
    if (buf == NULL || off == 0 || (size_t)off >= size) {
        return NULL;
    }
    for (size_t i = off; i < size; i++) {
        if (buf[i] == '\0') {
            return (const char *)(buf + off);
        }
    }
    return NULL;
}

int gsp_core_parse_item_params(const uint8_t *buf, size_t size, uint32_t off,
                               uint16_t len, gsp_core_item_params_t *out)
{
    if (buf == NULL || out == NULL) {
        return GSP_CORE_ERR_ARG;
    }
    if (len < 12u || (uint64_t)off + len > size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    const uint8_t *p = buf + off;
    *out = (gsp_core_item_params_t) {
        .item_count = gsp_core_rd_u16(p + 0),
        .selected = gsp_core_rd_u16(p + 2),
        .item_height = gsp_core_rd_u16(p + 4),
        .rows_or_page = gsp_core_rd_u16(p + 6),
        .flags = gsp_core_rd_u16(p + 8),
        .items = p + 12,
        .items_len = len - 12u,
    };
    return GSP_CORE_OK;
}

static bool valid_type(uint16_t type)
{
    return type >= GSP_CORE_OBJ_CONTAINER && type <= GSP_CORE_OBJ_LAYER;
}

static uint8_t bpp_for_cf(uint8_t cf)
{
    switch (cf) {
    case GSP_CORE_CF_RGB565:
    case GSP_CORE_CF_RGB565_SWAPPED:
        return 2;
    case GSP_CORE_CF_RGB888:
    case GSP_CORE_CF_BGR888:
        return 3;
    case GSP_CORE_CF_XRGB8888:
    case GSP_CORE_CF_ARGB8888:
        return 4;
    default:
        return 0;
    }
}

int gsp_core_validate(const uint8_t *buf, size_t size)
{
    gsp_core_header_t h;
    int rc = gsp_core_parse_header(buf, size, &h);
    if (rc != GSP_CORE_OK) {
        return rc;
    }
    if (gsp_core_rd_u32(buf + 52) != 0) {
        return GSP_CORE_ERR_BOUNDS;
    }
    if (gsp_core_crc32(buf, h.total_size) != h.crc32) {
        return GSP_CORE_ERR_CRC;
    }
    if (h.obj_count == 0 || h.obj_count > 0xFFFFu || h.blob_count > 0xFFFFu ||
        h.action_count > 0xFFFFu) {
        return GSP_CORE_ERR_COUNT;
    }

    uint64_t obj_end = (uint64_t)h.obj_table_off + (uint64_t)h.obj_count * GSP_CORE_OBJ_SIZE;
    uint64_t blob_end = (uint64_t)h.blob_table_off + (uint64_t)h.blob_count * GSP_CORE_BLOB_SIZE;
    uint64_t action_end = h.action_count ?
        (uint64_t)h.action_table_off + (uint64_t)h.action_count * GSP_CORE_ACTION_SIZE : blob_end;
    if (h.obj_table_off != GSP_CORE_HEADER_SIZE || obj_end > h.total_size ||
        h.blob_table_off != obj_end || blob_end > h.total_size) {
        return GSP_CORE_ERR_BOUNDS;
    }
    if ((h.action_count != 0 && h.action_table_off != blob_end) ||
        (h.action_count == 0 && h.action_table_off != 0) ||
        action_end > h.total_size || h.str_table_off < action_end ||
        h.str_table_off > h.total_size) {
        return GSP_CORE_ERR_BOUNDS;
    }

    for (uint32_t i = 0; i < h.obj_count; i++) {
        gsp_core_obj_t o;
        rc = gsp_core_get_object(buf, size, &h, i, &o);
        if (rc != GSP_CORE_OK) {
            return rc;
        }
        if (!valid_type(o.type)) {
            return GSP_CORE_ERR_TYPE;
        }
        if (o.parent_idx != GSP_CORE_NO_PARENT && o.parent_idx >= i) {
            return GSP_CORE_ERR_PARENT;
        }
        if ((o.flags & GSP_CORE_F_TEXT) && gsp_core_get_cstr(buf, h.total_size, o.text_off) == NULL) {
            return GSP_CORE_ERR_STRING;
        }
        if ((o.flags & GSP_CORE_F_CALLBACK) && gsp_core_get_cstr(buf, h.total_size, o.callback_off) == NULL) {
            return GSP_CORE_ERR_STRING;
        }
        if ((o.flags & GSP_CORE_F_NAME) && gsp_core_get_cstr(buf, h.total_size, o.name_off) == NULL) {
            return GSP_CORE_ERR_STRING;
        }
        if ((o.flags & GSP_CORE_F_IMAGE) && o.blob_idx >= h.blob_count) {
            return GSP_CORE_ERR_BLOB;
        }
        if (o.flags & GSP_CORE_F_PARAMS) {
            if ((uint64_t)o.params_off + o.params_len > h.total_size ||
                o.params_off < GSP_CORE_HEADER_SIZE) {
                return GSP_CORE_ERR_BOUNDS;
            }
        }
        if ((o.flags & GSP_CORE_F_ALIGN) && o.text_align > 3u) {
            return GSP_CORE_ERR_BOUNDS;
        }
    }

    for (uint32_t i = 0; i < h.blob_count; i++) {
        gsp_core_blob_t b;
        rc = gsp_core_get_blob(buf, size, &h, i, &b);
        if (rc != GSP_CORE_OK) {
            return rc;
        }
        uint8_t bpp = bpp_for_cf(b.cf);
        if (bpp == 0 || b.raw_size == 0 ||
            (uint64_t)b.data_off + b.comp_size > h.total_size) {
            return GSP_CORE_ERR_BLOB;
        }
        if (b.codec == GSP_CORE_CODEC_STORE) {
            if (b.comp_size != b.raw_size) {
                return GSP_CORE_ERR_CODEC;
            }
        } else if (b.codec == GSP_CORE_CODEC_RLE16) {
            if (b.cf != GSP_CORE_CF_RGB565 && b.cf != GSP_CORE_CF_RGB565_SWAPPED) {
                return GSP_CORE_ERR_CODEC;
            }
            if ((b.comp_size % 4u) != 0) {
                return GSP_CORE_ERR_CODEC;
            }
            uint32_t decoded = 0;
            const uint8_t *comp = buf + b.data_off;
            for (uint32_t pos = 0; pos < b.comp_size; pos += 4u) {
                decoded += (uint32_t)gsp_core_rd_u16(comp + pos) * 2u;
            }
            if (decoded != b.raw_size) {
                return GSP_CORE_ERR_CODEC;
            }
        } else {
            return GSP_CORE_ERR_CODEC;
        }
    }

    for (uint32_t i = 0; i < h.action_count; i++) {
        gsp_core_action_t a;
        rc = gsp_core_get_action(buf, size, &h, i, &a);
        if (rc != GSP_CORE_OK) {
            return rc;
        }
        if (a.src_idx >= h.obj_count ||
            (a.target_idx != GSP_CORE_ACT_NO_TARGET && a.target_idx >= h.obj_count) ||
            a.event > GSP_CORE_EV_VALUE || a.action > GSP_CORE_ACT_BACK || a.flags != 0) {
            return GSP_CORE_ERR_ACTION;
        }
        if (a.target_name_off != 0 &&
            gsp_core_get_cstr(buf, h.total_size, a.target_name_off) == NULL) {
            return GSP_CORE_ERR_STRING;
        }
        if (a.param_off != 0 &&
            ((uint64_t)a.param_off + a.param_len > h.total_size ||
             gsp_core_get_cstr(buf, h.total_size, a.param_off) == NULL)) {
            return GSP_CORE_ERR_STRING;
        }
    }
    return GSP_CORE_OK;
}

const char *gsp_core_err_name(int err)
{
    switch (err) {
    case GSP_CORE_OK: return "GSP_CORE_OK";
    case GSP_CORE_ERR_ARG: return "GSP_CORE_ERR_ARG";
    case GSP_CORE_ERR_SIZE: return "GSP_CORE_ERR_SIZE";
    case GSP_CORE_ERR_MAGIC: return "GSP_CORE_ERR_MAGIC";
    case GSP_CORE_ERR_VERSION: return "GSP_CORE_ERR_VERSION";
    case GSP_CORE_ERR_BOUNDS: return "GSP_CORE_ERR_BOUNDS";
    case GSP_CORE_ERR_CRC: return "GSP_CORE_ERR_CRC";
    case GSP_CORE_ERR_COUNT: return "GSP_CORE_ERR_COUNT";
    case GSP_CORE_ERR_PARENT: return "GSP_CORE_ERR_PARENT";
    case GSP_CORE_ERR_STRING: return "GSP_CORE_ERR_STRING";
    case GSP_CORE_ERR_TYPE: return "GSP_CORE_ERR_TYPE";
    case GSP_CORE_ERR_BLOB: return "GSP_CORE_ERR_BLOB";
    case GSP_CORE_ERR_CODEC: return "GSP_CORE_ERR_CODEC";
    case GSP_CORE_ERR_ACTION: return "GSP_CORE_ERR_ACTION";
    case GSP_CORE_ERR_ALLOC: return "GSP_CORE_ERR_ALLOC";
    default: return "GSP_CORE_ERR_UNKNOWN";
    }
}
