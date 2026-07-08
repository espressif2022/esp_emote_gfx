/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gsp_load — host/device 共用 loader：u32-offset 字节包 -> gfx object 树。
 *
 * 关键：全程只把包字段当“u32 偏移 / u16 索引”解释，不做指针重定位，
 * 用 gfx_*_create + setter 工厂建树。因此不依赖 struct 布局、不区分
 * 32/64 位，同一份 buf 在两端解析一致。全程边界校验，坏包只返回错误码。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsp_format.h"

#include "gfx/types.h"
#include "gfx/widgets/button.h"
#include "gfx/widgets/container.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/label.h"

/* 校验 off 指向的字符串在 [off,size) 内 NUL 结尾，返回指针或 NULL。*/
static const char *safe_str(const uint8_t *buf, size_t size, uint32_t off)
{
    if (off == 0 || (size_t)off >= size) {
        return NULL;
    }
    for (size_t i = off; i < size; i++) {
        if (buf[i] == 0) {
            return (const char *)(buf + off);
        }
    }
    return NULL;
}

static gfx_font_t resolve_font(uint16_t font_id, const gsp_font_binding_t *fonts,
                               size_t font_count, gfx_font_t default_font)
{
    for (size_t i = 0; i < font_count; i++) {
        if (fonts != NULL && fonts[i].id == font_id && fonts[i].font != NULL) {
            return fonts[i].font;
        }
    }
    return default_font;
}

/* 安全销毁：先把所有节点从父节点脱开（消除级联关系），再逐个删除。*/
static void destroy_all(gfx_object_t **objs, uint32_t count)
{
    if (objs == NULL) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        gfx_object_t *o = objs[i];
        if (o != NULL) {
            gfx_object_t *p = gfx_object_get_parent(o);
            if (p != NULL) {
                (void)gfx_object_remove_child(p, o);
            }
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        if (objs[i] != NULL) {
            (void)gfx_object_delete(objs[i]);
            objs[i] = NULL;
        }
    }
}

/*
 * 解压 blob[idx] 到 out->img_bufs[idx] 并填 out->img_dscs[idx]（幂等缓存）。
 * 成功返回指向 img_dscs[idx] 的指针，失败返回 NULL 并置 *err。
 */
static const gfx_image_dsc_t *blob_get(const uint8_t *buf, size_t size, uint32_t blob_off,
                                       uint32_t blob_count, uint32_t idx,
                                       gsp_scene_t *out, int *err)
{
    if (idx >= blob_count) {
        *err = GSP_ERR_BLOB;
        return NULL;
    }
    if (out->img_bufs[idx] != NULL) {
        return &out->img_dscs[idx];   /* 已解压过，直接复用 */
    }

    const uint8_t *b = buf + blob_off + (size_t)idx * GSP_BLOB_SIZE;
    const uint16_t w = gsp_rd_u16(b + 0);
    const uint16_t h = gsp_rd_u16(b + 2);
    const uint8_t  cf = b[4];
    const uint8_t  codec = b[5];
    const uint16_t stride = gsp_rd_u16(b + 6);
    const uint32_t raw_size = gsp_rd_u32(b + 8);
    const uint32_t comp_size = gsp_rd_u32(b + 12);
    const uint32_t data_off = gsp_rd_u32(b + 16);

    if ((uint64_t)data_off + comp_size > size || raw_size == 0) {
        *err = GSP_ERR_BOUNDS;
        return NULL;
    }
    const uint8_t *comp = buf + data_off;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (raw == NULL) {
        *err = GSP_ERR_ALLOC;
        return NULL;
    }

    if (codec == GSP_CODEC_STORE) {
        if (comp_size != raw_size) {
            free(raw);
            *err = GSP_ERR_CODEC;
            return NULL;
        }
        memcpy(raw, comp, raw_size);
    } else if (codec == GSP_CODEC_RLE16) {
        uint32_t ro = 0, ci = 0;
        while (ci + 4u <= comp_size && ro + 2u <= raw_size) {
            uint32_t run = gsp_rd_u16(comp + ci);
            const uint16_t px = gsp_rd_u16(comp + ci + 2);
            ci += 4;
            while (run-- > 0 && ro + 2u <= raw_size) {
                gsp_wr_u16(raw + ro, px);
                ro += 2;
            }
        }
        if (ro != raw_size) {   /* 解出的字节数必须恰好填满 */
            free(raw);
            *err = GSP_ERR_CODEC;
            return NULL;
        }
    } else {
        free(raw);
        *err = GSP_ERR_CODEC;
        return NULL;
    }

    out->img_bufs[idx] = raw;
    gfx_image_dsc_t *dsc = &out->img_dscs[idx];
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic = GFX_IMAGE_HEADER_MAGIC;
    dsc->header.cf = cf;
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.stride = stride;
    dsc->data_size = raw_size;
    dsc->data = raw;
    return dsc;
}

int gsp_load_with_fonts(const uint8_t *buf, size_t size, gfx_display_t *disp,
                        const gsp_font_binding_t *fonts, size_t font_count,
                        gfx_font_t default_font,
                        const gsp_cb_binding_t *cbs, size_t cb_count,
                        gsp_scene_t *out)
{
    if (buf == NULL || out == NULL || disp == NULL) {
        return GSP_ERR_BOUNDS;
    }
    memset(out, 0, sizeof(*out));

    if (size < GSP_HEADER_SIZE) {
        return GSP_ERR_SIZE;
    }
    if (gsp_rd_u32(buf + 0) != GSP_MAGIC) {
        return GSP_ERR_MAGIC;
    }
    if (gsp_rd_u32(buf + 4) != GSP_VERSION) {
        return GSP_ERR_VERSION;
    }

    const uint32_t obj_count = gsp_rd_u32(buf + 16);
    const uint32_t obj_off = gsp_rd_u32(buf + 20);
    const uint32_t blob_count = gsp_rd_u32(buf + 28);
    const uint32_t blob_off = gsp_rd_u32(buf + 32);
    const uint32_t total = gsp_rd_u32(buf + 36);
    const uint32_t crc = gsp_rd_u32(buf + 40);

    if (total > size || total < GSP_HEADER_SIZE) {
        return GSP_ERR_BOUNDS;
    }
    if (gsp_crc32_scene(buf, total) != crc) {
        return GSP_ERR_CRC;
    }
    if (obj_count == 0 || obj_count > 0xFFFFu) {
        return GSP_ERR_COUNT;
    }
    const uint64_t obj_end = (uint64_t)obj_off + (uint64_t)obj_count * GSP_OBJ_SIZE;
    if (obj_off < GSP_HEADER_SIZE || obj_end > total) {
        return GSP_ERR_BOUNDS;
    }
    if (blob_count > 0xFFFFu) {
        return GSP_ERR_COUNT;
    }
    if (blob_count > 0) {
        const uint64_t blob_end = (uint64_t)blob_off + (uint64_t)blob_count * GSP_BLOB_SIZE;
        if (blob_off < obj_end || blob_end > total) {
            return GSP_ERR_BOUNDS;
        }
    }

    gfx_object_t **objs = (gfx_object_t **)calloc(obj_count, sizeof(*objs));
    if (objs == NULL) {
        return GSP_ERR_ALLOC;
    }
    /* blob 解压缓存（可能为空）*/
    if (blob_count > 0) {
        out->img_dscs = (gfx_image_dsc_t *)calloc(blob_count, sizeof(*out->img_dscs));
        out->img_bufs = (uint8_t **)calloc(blob_count, sizeof(*out->img_bufs));
        if (out->img_dscs == NULL || out->img_bufs == NULL) {
            free(objs);
            free(out->img_dscs);
            free(out->img_bufs);
            memset(out, 0, sizeof(*out));
            return GSP_ERR_ALLOC;
        }
    }
    out->blob_count = (uint16_t)blob_count;

    int rc = GSP_OK;
    for (uint32_t i = 0; i < obj_count; i++) {
        const uint8_t *e = buf + obj_off + (size_t)i * GSP_OBJ_SIZE;

        const uint16_t type = gsp_rd_u16(e + 0);
        const uint16_t parent = gsp_rd_u16(e + 2);
        const int16_t x = gsp_rd_i16(e + 4);
        const int16_t y = gsp_rd_i16(e + 6);
        const uint16_t w = gsp_rd_u16(e + 8);
        const uint16_t h = gsp_rd_u16(e + 10);
        const uint32_t flags = gsp_rd_u32(e + 12);
        const uint32_t fg = gsp_rd_u32(e + 16);
        const uint32_t bg = gsp_rd_u32(e + 20);
        const uint32_t bc = gsp_rd_u32(e + 24);
        const uint16_t bw = gsp_rd_u16(e + 28);
        const uint16_t radius = gsp_rd_u16(e + 30);
        const uint32_t text_off = gsp_rd_u32(e + 32);
        const uint32_t cb_off = gsp_rd_u32(e + 36);
        const uint32_t name_off = gsp_rd_u32(e + 40);
        const uint32_t blob_idx = gsp_rd_u32(e + 44);
        const uint32_t params_off = gsp_rd_u32(e + 48);
        const uint16_t params_len = gsp_rd_u16(e + 52);
        const uint8_t  text_align = e[55];
        const uint16_t font_id = gsp_rd_u16(e + 56);

        /* 先序约束：父必须是更早的对象 */
        if (parent != GSP_NO_PARENT && parent >= i) {
            rc = GSP_ERR_PARENT;
            break;
        }

        const char *text = NULL;
        if (flags & GSP_F_TEXT) {
            text = safe_str(buf, size, text_off);
            if (text == NULL) {
                rc = GSP_ERR_STRING;
                break;
            }
        }
        const char *cbname = NULL;
        if (flags & GSP_F_CALLBACK) {
            cbname = safe_str(buf, size, cb_off);
            if (cbname == NULL) {
                rc = GSP_ERR_STRING;
                break;
            }
        }
        const char *name = NULL;
        if (flags & GSP_F_NAME) {
            name = safe_str(buf, size, name_off);
            if (name == NULL) {
                rc = GSP_ERR_STRING;
                break;
            }
        }
        (void)name;   /* 引擎暂无 set_name API：格式已带，留待接入 */
        if (flags & GSP_F_PARAMS) {
            /* 私有参数块只做边界校验，具体解析交给各 widget（扩展点）*/
            if ((uint64_t)params_off + params_len > total || params_off < GSP_HEADER_SIZE) {
                rc = GSP_ERR_BOUNDS;
                break;
            }
        }

        gfx_object_t *o = NULL;
        gfx_font_t obj_font = resolve_font(font_id, fonts, font_count, default_font);
        switch (type) {
        case GSP_OBJ_CONTAINER:
            o = gfx_container_create(disp);
            if (o != NULL) {
                if (flags & GSP_F_BG_COLOR) {
                    (void)gfx_container_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GSP_F_BORDER) {
                    (void)gfx_container_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_container_set_border_width(o, bw);
                }
                if (flags & GSP_F_RADIUS) {
                    (void)gfx_container_set_radius(o, radius);
                }
            }
            break;

        case GSP_OBJ_LABEL:
            o = gfx_label_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_label_set_font(o, obj_font);
                }
                if (text != NULL) {
                    (void)gfx_label_set_text(o, text);
                }
                if (flags & GSP_F_FG_COLOR) {
                    (void)gfx_label_set_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GSP_F_ALIGN) {
                    (void)gfx_label_set_text_align(o, (gfx_text_align_t)text_align);
                }
            }
            break;

        case GSP_OBJ_BUTTON:
            o = gfx_button_create(disp);
            if (o != NULL) {
                if (obj_font != NULL) {
                    (void)gfx_button_set_font(o, obj_font);
                }
                if (text != NULL) {
                    (void)gfx_button_set_text(o, text);
                }
                if (flags & GSP_F_FG_COLOR) {
                    (void)gfx_button_set_text_color(o, GFX_COLOR_HEX(fg));
                }
                if (flags & GSP_F_BG_COLOR) {
                    (void)gfx_button_set_bg_color(o, GFX_COLOR_HEX(bg));
                }
                if (flags & GSP_F_BORDER) {
                    (void)gfx_button_set_border_color(o, GFX_COLOR_HEX(bc));
                    (void)gfx_button_set_border_width(o, bw);
                }
                if (flags & GSP_F_RADIUS) {
                    (void)gfx_button_set_radius(o, radius);
                }
                if (cbname != NULL) {
                    for (size_t k = 0; k < cb_count; k++) {
                        if (cbs != NULL && cbs[k].name != NULL &&
                            strcmp(cbs[k].name, cbname) == 0) {
                            (void)gfx_object_set_touch_cb(o, cbs[k].cb, cbs[k].user_data);
                            break;
                        }
                    }
                }
            }
            break;

        case GSP_OBJ_IMAGE:
            o = gfx_image_create(disp);
            if (o != NULL && (flags & GSP_F_IMAGE)) {
                int berr = GSP_OK;
                const gfx_image_dsc_t *dsc =
                    blob_get(buf, size, blob_off, blob_count, blob_idx, out, &berr);
                if (dsc == NULL) {
                    (void)gfx_object_delete(o);   /* 尚未入 objs[]，先自行销毁避免泄漏 */
                    o = NULL;
                    rc = berr;                     /* blob 越界 / 坏 codec */
                    break;
                }
                gfx_image_src_t src = {
                    .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
                    .data = dsc,
                };
                (void)gfx_image_set_source_desc(o, &src);
            }
            break;

        default:
            rc = GSP_ERR_TYPE;
            break;
        }

        if (rc != GSP_OK) {
            break;
        }
        if (o == NULL) {
            rc = GSP_ERR_CREATE;
            break;
        }

        (void)gfx_object_set_pos(o, x, y);
        (void)gfx_object_set_size(o, w, h);
        if (flags & GSP_F_HIDDEN) {
            (void)gfx_object_set_visible(o, false);
        }
        if (parent != GSP_NO_PARENT) {
            (void)gfx_object_add_child(objs[parent], o);
        }
        objs[i] = o;
    }

    if (rc != GSP_OK) {
        destroy_all(objs, obj_count);
        free(objs);
        for (uint32_t k = 0; k < blob_count; k++) {
            free(out->img_bufs ? out->img_bufs[k] : NULL);
        }
        free(out->img_bufs);
        free(out->img_dscs);
        memset(out, 0, sizeof(*out));
        return rc;
    }

    out->objs = objs;
    out->obj_count = (uint16_t)obj_count;
    out->root = objs[0];
    return GSP_OK;
}

int gsp_load(const uint8_t *buf, size_t size, gfx_display_t *disp, gfx_font_t font,
             const gsp_cb_binding_t *cbs, size_t cb_count, gsp_scene_t *out)
{
    const gsp_font_binding_t default_binding = {
        .id = 0,
        .font = font,
    };

    return gsp_load_with_fonts(buf, size, disp, &default_binding, font != NULL ? 1U : 0U,
                               font, cbs, cb_count, out);
}

void gsp_scene_free(gsp_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }
    if (scene->objs != NULL) {
        destroy_all(scene->objs, scene->obj_count);
        free(scene->objs);
    }
    for (uint32_t k = 0; k < scene->blob_count; k++) {
        free(scene->img_bufs ? scene->img_bufs[k] : NULL);
    }
    free(scene->img_bufs);
    free(scene->img_dscs);
    memset(scene, 0, sizeof(*scene));
}

/* ------------------------------------------------------------------ */
/* gsp_dump：把包内容打印到 stdout，直观展示“里面含啥 + 全是 offset”。 */
/* ------------------------------------------------------------------ */

static const char *type_name(uint16_t t)
{
    switch (t) {
    case GSP_OBJ_CONTAINER: return "container";
    case GSP_OBJ_LABEL:     return "label";
    case GSP_OBJ_BUTTON:    return "button";
    case GSP_OBJ_IMAGE:     return "image";
    default:                return "?";
    }
}

void gsp_dump(const uint8_t *buf, size_t size)
{
    if (buf == NULL || size < GSP_HEADER_SIZE) {
        printf("gsp_dump: invalid buffer\n");
        return;
    }

    const uint32_t magic = gsp_rd_u32(buf + 0);
    const uint32_t version = gsp_rd_u32(buf + 4);
    const uint16_t sw = gsp_rd_u16(buf + 8);
    const uint16_t sh = gsp_rd_u16(buf + 10);
    const uint32_t sbg = gsp_rd_u32(buf + 12);
    const uint32_t n = gsp_rd_u32(buf + 16);
    const uint32_t obj_off = gsp_rd_u32(buf + 20);
    const uint32_t str_off = gsp_rd_u32(buf + 24);
    const uint32_t blob_count = gsp_rd_u32(buf + 28);
    const uint32_t blob_off = gsp_rd_u32(buf + 32);
    const uint32_t total = gsp_rd_u32(buf + 36);
    const uint32_t crc = gsp_rd_u32(buf + 40);
    const uint32_t crc_calc = (total <= size) ? gsp_crc32_scene(buf, total) : 0u;

    printf("==== GSP package (%zu bytes) ====\n", size);
    printf("header: magic=%c%c%c%c version=%u screen=%ux%u bg=#%06X\n",
           (char)(magic & 0xFF), (char)((magic >> 8) & 0xFF),
           (char)((magic >> 16) & 0xFF), (char)((magic >> 24) & 0xFF),
           version, sw, sh, sbg & 0xFFFFFFu);
    printf("        obj_count=%u obj_table_off=%u str_table_off=%u\n", n, obj_off, str_off);
    printf("        blob_count=%u blob_table_off=%u total=%u\n", blob_count, blob_off, total);
    printf("        crc32=0x%08X (%s)\n", crc, crc == crc_calc ? "ok" : "MISMATCH");

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *e = buf + obj_off + (size_t)i * GSP_OBJ_SIZE;
        const uint16_t type = gsp_rd_u16(e + 0);
        const uint16_t parent = gsp_rd_u16(e + 2);
        const int16_t x = gsp_rd_i16(e + 4);
        const int16_t y = gsp_rd_i16(e + 6);
        const uint16_t w = gsp_rd_u16(e + 8);
        const uint16_t h = gsp_rd_u16(e + 10);
        const uint32_t flags = gsp_rd_u32(e + 12);
        const uint32_t text_off = gsp_rd_u32(e + 32);
        const uint32_t cb_off = gsp_rd_u32(e + 36);
        const uint32_t name_off = gsp_rd_u32(e + 40);
        const uint32_t blob_idx = gsp_rd_u32(e + 44);
        const uint16_t font_id = gsp_rd_u16(e + 56);

        char parent_buf[8];
        if (parent == GSP_NO_PARENT) {
            snprintf(parent_buf, sizeof(parent_buf), "root");
        } else {
            snprintf(parent_buf, sizeof(parent_buf), "%u", parent);
        }
        printf("obj[%u] %-9s parent=%-4s rect=(%d,%d %ux%u) flags=0x%03X",
               i, type_name(type), parent_buf, x, y, w, h, flags);
        if ((flags & GSP_F_TEXT) && text_off < size) {
            printf(" text@%u=\"%s\"", text_off, (const char *)(buf + text_off));
        }
        if ((flags & GSP_F_CALLBACK) && cb_off < size) {
            printf(" cb@%u=\"%s\"", cb_off, (const char *)(buf + cb_off));
        }
        if ((flags & GSP_F_NAME) && name_off < size) {
            printf(" name@%u=\"%s\"", name_off, (const char *)(buf + name_off));
        }
        if (flags & GSP_F_IMAGE) {
            printf(" blob=%u", blob_idx);
        }
        if (type == GSP_OBJ_LABEL || type == GSP_OBJ_BUTTON) {
            printf(" font=%u", font_id);
        }
        if (flags & GSP_F_HIDDEN) {
            printf(" hidden");
        }
        printf("\n");
    }

    for (uint32_t k = 0; k < blob_count; k++) {
        const uint8_t *b = buf + blob_off + (size_t)k * GSP_BLOB_SIZE;
        const uint16_t w = gsp_rd_u16(b + 0);
        const uint16_t h = gsp_rd_u16(b + 2);
        const uint8_t  cf = b[4];
        const uint8_t  codec = b[5];
        const uint32_t raw_size = gsp_rd_u32(b + 8);
        const uint32_t comp_size = gsp_rd_u32(b + 12);
        const uint32_t data_off = gsp_rd_u32(b + 16);
        const char *codec_name = (codec == GSP_CODEC_RLE16) ? "rle16" :
                                 (codec == GSP_CODEC_STORE) ? "store" : "?";
        double ratio = raw_size ? (100.0 * comp_size / raw_size) : 0.0;
        printf("blob[%u] %ux%u cf=0x%02X codec=%s raw=%uB comp=%uB (%.1f%%) data@%u\n",
               k, w, h, cf, codec_name, raw_size, comp_size, ratio, data_off);
    }
    printf("note: 结构引用全是 u32 偏移 / u16 索引；图片像素已烘焙进包并压缩，\n");
    printf("      带 codec+raw/comp 头，加载期解压 —— 同一份字节 32/64 位解析一致。\n");
    printf("=================================\n");
}
