/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gsp_pack — host 侧“编译器”：scene 描述 -> u32-offset 字节包。
 * 演示用，不追求极致压缩；只保证：位置无关、字符串去重、逐字节确定。
 *
 * v2：图片像素也烘焙进包（blob 表）。每个 blob 按 ITE 思路带压缩头
 *     （codec + raw_size + comp_size），这里内置一个无依赖的 RLE16
 *     （16bpp 游程编码）+ STORE 回退；对 UI 常见的平色/单向渐变很有效。
 */

#include <stdlib.h>
#include <string.h>

#include "gsp_format.h"

/* 简单字符串内表：把字符串追加进 str 区并去重，返回相对 str 区起点的偏移。*/
typedef struct {
    uint8_t *bytes;
    size_t   len;
    size_t   cap;
} gsp_strtab_t;

static uint32_t strtab_intern(gsp_strtab_t *st, const char *s)
{
    if (s == NULL) {
        return 0; /* 调用方用 flags 判定是否有效；0 视为“无” */
    }

    /* 去重：扫描已追加的 NUL 结尾字符串 */
    size_t i = 0;
    while (i < st->len) {
        const char *cur = (const char *)(st->bytes + i);
        if (strcmp(cur, s) == 0) {
            return (uint32_t)i;
        }
        i += strlen(cur) + 1;
    }

    size_t need = strlen(s) + 1;
    memcpy(st->bytes + st->len, s, need);
    uint32_t off = (uint32_t)st->len;
    st->len += need;
    return off;
}

/* ---------- RLE16：16bpp 像素游程编码 ---------- */
/* token = u16 count + u16 pixel（均小端）。返回 malloc 缓冲，*out 为字节数。*/
static uint8_t *rle16_encode(const uint8_t *raw, uint32_t raw_size, uint32_t *out_size)
{
    const uint32_t npix = raw_size / 2u;
    /* 最坏情况：每像素各成一段 -> npix 个 4 字节 token */
    uint8_t *out = (uint8_t *)malloc((size_t)npix * 4u + 4u);
    if (out == NULL) {
        return NULL;
    }
    uint32_t o = 0, i = 0;
    while (i < npix) {
        const uint16_t px = gsp_rd_u16(raw + (size_t)i * 2u);
        uint32_t run = 1;
        while (i + run < npix && run < 0xFFFFu &&
               gsp_rd_u16(raw + (size_t)(i + run) * 2u) == px) {
            run++;
        }
        gsp_wr_u16(out + o, (uint16_t)run);
        o += 2;
        gsp_wr_u16(out + o, px);
        o += 2;
        i += run;
    }
    *out_size = o;
    return out;
}

/* ---------- blob（烘焙位图）构建 ---------- */
typedef struct {
    const gfx_image_dsc_t *src;   /* 去重键 */
    uint16_t w, h, stride;
    uint8_t  cf, codec;
    uint32_t raw_size, comp_size;
    uint8_t *comp;                /* malloc 的压缩字节 */
} blob_build_t;

/* 压缩一个源位图到 blob_build_t，成功返回 0。*/
static int blob_compress(const gfx_image_dsc_t *d, blob_build_t *b)
{
    const uint16_t w = (uint16_t)d->header.w;
    const uint16_t h = (uint16_t)d->header.h;
    const uint8_t  cf = (uint8_t)d->header.cf;
    if (d->data == NULL) {
        return -1;
    }
    /* raw_size 优先用 data_size（对 planar 的 RGB565A8 等也正确）；
     * 否则按 stride*h 推算，此时需要 bpp 求默认 stride。*/
    uint16_t stride = (uint16_t)d->header.stride;
    uint32_t raw_size = d->data_size;
    if (raw_size == 0) {
        const uint8_t bpp = gsp_bpp(cf);
        if (bpp == 0) {
            return -1;
        }
        if (stride == 0) {
            stride = (uint16_t)(w * bpp);
        }
        raw_size = (uint32_t)stride * h;
    }

    uint8_t  codec = GSP_CODEC_STORE;
    uint8_t *comp = NULL;
    uint32_t comp_size = raw_size;

    /* RLE16 只用于连续存放的 16bpp（RGB565）；planar/带 alpha 一律 STORE */
    const bool rle_ok = (cf == GFX_COLOR_FORMAT_RGB565 || cf == GFX_COLOR_FORMAT_RGB565_SWAPPED) &&
                        (stride == 0 || stride == (uint16_t)(w * 2)) &&
                        (raw_size == (uint32_t)w * h * 2u);
    if (rle_ok) {
        uint32_t rs = 0;
        uint8_t *r = rle16_encode(d->data, raw_size, &rs);
        if (r != NULL && rs < raw_size) {
            codec = GSP_CODEC_RLE16;
            comp = r;
            comp_size = rs;
        } else {
            free(r);
        }
    }
    if (codec == GSP_CODEC_STORE) {
        comp = (uint8_t *)malloc(raw_size ? raw_size : 1u);
        if (comp == NULL) {
            return -1;
        }
        memcpy(comp, d->data, raw_size);
        comp_size = raw_size;
    }

    b->src = d;
    b->w = w;
    b->h = h;
    b->stride = stride;
    b->cf = cf;
    b->codec = codec;
    b->raw_size = raw_size;
    b->comp_size = comp_size;
    b->comp = comp;
    return 0;
}

uint8_t *gsp_pack(const gsp_scene_desc_t *scene, size_t *out_size)
{
    if (scene == NULL || out_size == NULL || scene->objs == NULL || scene->obj_count == 0) {
        return NULL;
    }

    const uint16_t n = scene->obj_count;

    /* ---- 收集 / 压缩 blob（按 image_src 指针去重）---- */
    blob_build_t *blobs = (blob_build_t *)calloc(n, sizeof(*blobs));
    uint32_t     *obj_blob = (uint32_t *)calloc(n, sizeof(*obj_blob)); /* 每对象 blob 索引 */
    if (blobs == NULL || obj_blob == NULL) {
        free(blobs);
        free(obj_blob);
        return NULL;
    }
    uint16_t blob_count = 0;
    for (uint16_t i = 0; i < n; i++) {
        const gsp_desc_t *d = &scene->objs[i];
        if (!(d->flags & GSP_F_IMAGE) || d->image_src == NULL) {
            continue;
        }
        int found = -1;
        for (uint16_t k = 0; k < blob_count; k++) {
            if (blobs[k].src == d->image_src) {
                found = (int)k;
                break;
            }
        }
        if (found < 0) {
            if (blob_compress(d->image_src, &blobs[blob_count]) != 0) {
                for (uint16_t k = 0; k < blob_count; k++) {
                    free(blobs[k].comp);
                }
                free(blobs);
                free(obj_blob);
                return NULL;
            }
            found = (int)blob_count++;
        }
        obj_blob[i] = (uint32_t)found;
    }

    /* string 区容量上界 + params 区总长（先扫一遍拿到上界）*/
    size_t strcap = 1; /* 至少 1，避免 malloc(0) */
    uint32_t params_total = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (scene->objs[i].text) {
            strcap += strlen(scene->objs[i].text) + 1;
        }
        if (scene->objs[i].callback) {
            strcap += strlen(scene->objs[i].callback) + 1;
        }
        if (scene->objs[i].name) {
            strcap += strlen(scene->objs[i].name) + 1;
        }
        if ((scene->objs[i].flags & GSP_F_PARAMS) && scene->objs[i].params != NULL) {
            params_total += scene->objs[i].params_len;
        }
    }
    const uint16_t action_count = scene->actions ? scene->action_count : 0u;
    for (uint16_t i = 0; i < action_count; i++) {
        if (scene->actions[i].param) {
            strcap += strlen(scene->actions[i].param) + 1;
        }
        if (scene->actions[i].target_name) {
            strcap += strlen(scene->actions[i].target_name) + 1;
        }
    }

    /* ---- 布局：各表基址均可提前算出 ----
     *   [header][obj 表][blob 表][action 表][params 区][string 区][blob 数据]
     */
    const uint32_t obj_table_off = GSP_HEADER_SIZE;
    const uint32_t blob_table_off = obj_table_off + (uint32_t)n * GSP_OBJ_SIZE;
    const uint32_t action_table_off = blob_table_off + (uint32_t)blob_count * GSP_BLOB_SIZE;
    const uint32_t params_base = action_table_off + (uint32_t)action_count * GSP_ACTION_SIZE;
    const uint32_t str_table_off = params_base + params_total;

    gsp_strtab_t st = { .bytes = (uint8_t *)malloc(strcap), .len = 0, .cap = strcap };
    uint8_t *obj_bytes = (uint8_t *)calloc(n, GSP_OBJ_SIZE);
    uint8_t *params_bytes = (uint8_t *)malloc(params_total ? params_total : 1u);
    uint32_t params_cursor = 0;
    if (st.bytes == NULL || obj_bytes == NULL || params_bytes == NULL) {
        free(st.bytes);
        free(obj_bytes);
        free(params_bytes);
        for (uint16_t k = 0; k < blob_count; k++) {
            free(blobs[k].comp);
        }
        free(blobs);
        free(obj_blob);
        return NULL;
    }

    for (uint16_t i = 0; i < n; i++) {
        const gsp_desc_t *d = &scene->objs[i];
        uint8_t *e = obj_bytes + (size_t)i * GSP_OBJ_SIZE;

        uint32_t text_off = 0;
        if (d->flags & GSP_F_TEXT) {
            text_off = str_table_off + strtab_intern(&st, d->text);
        }
        uint32_t cb_off = 0;
        if (d->flags & GSP_F_CALLBACK) {
            cb_off = str_table_off + strtab_intern(&st, d->callback);
        }
        uint32_t name_off = 0;
        if (d->flags & GSP_F_NAME) {
            name_off = str_table_off + strtab_intern(&st, d->name);
        }
        uint32_t blob_idx = 0;
        if ((d->flags & GSP_F_IMAGE) && d->image_src != NULL) {
            blob_idx = obj_blob[i];
        }
        uint32_t params_off = 0;
        uint16_t params_len = 0;
        if ((d->flags & GSP_F_PARAMS) && d->params != NULL && d->params_len > 0) {
            params_off = params_base + params_cursor;
            params_len = d->params_len;
            memcpy(params_bytes + params_cursor, d->params, params_len);
            params_cursor += params_len;
        }

        gsp_wr_u16(e + 0, d->type);
        gsp_wr_u16(e + 2, d->parent_idx);
        gsp_wr_u16(e + 4, (uint16_t)d->x);
        gsp_wr_u16(e + 6, (uint16_t)d->y);
        gsp_wr_u16(e + 8, d->w);
        gsp_wr_u16(e + 10, d->h);
        gsp_wr_u32(e + 12, d->flags);
        gsp_wr_u32(e + 16, d->fg_color);
        gsp_wr_u32(e + 20, d->bg_color);
        gsp_wr_u32(e + 24, d->border_color);
        gsp_wr_u16(e + 28, d->border_width);
        gsp_wr_u16(e + 30, d->radius);
        gsp_wr_u32(e + 32, text_off);
        gsp_wr_u32(e + 36, cb_off);
        gsp_wr_u32(e + 40, name_off);
        gsp_wr_u32(e + 44, blob_idx);
        gsp_wr_u32(e + 48, params_off);
        gsp_wr_u16(e + 52, params_len);
        e[54] = d->opacity;
        e[55] = d->text_align;
        gsp_wr_u16(e + 56, d->font_id);
        gsp_wr_u16(e + 58, d->bind_id);
        gsp_wr_u32(e + 60, 0u);
    }

    /* ---- action 表：event -> action，全部用 u16 索引 / u32 偏移 ---- */
    uint8_t *action_bytes = (uint8_t *)calloc(action_count ? action_count : 1, GSP_ACTION_SIZE);
    if (action_bytes == NULL) {
        free(st.bytes);
        free(obj_bytes);
        free(params_bytes);
        for (uint16_t k = 0; k < blob_count; k++) {
            free(blobs[k].comp);
        }
        free(blobs);
        free(obj_blob);
        return NULL;
    }
    for (uint16_t i = 0; i < action_count; i++) {
        const gsp_action_desc_t *a = &scene->actions[i];
        uint8_t *e = action_bytes + (size_t)i * GSP_ACTION_SIZE;

        uint32_t name_off = 0;
        if (a->target_name) {
            name_off = str_table_off + strtab_intern(&st, a->target_name);
        }
        uint32_t param_off = 0;
        uint16_t param_len = 0;
        if (a->param) {
            param_off = str_table_off + strtab_intern(&st, a->param);
            param_len = (uint16_t)(strlen(a->param) + 1u);
        }

        gsp_wr_u16(e + 0, a->src_idx);
        gsp_wr_u16(e + 2, a->event);
        gsp_wr_u16(e + 4, a->action);
        gsp_wr_u16(e + 6, a->target_name ? GSP_ACT_NO_TARGET : a->target_idx);
        gsp_wr_u32(e + 8, name_off);
        gsp_wr_u32(e + 12, param_off);
        gsp_wr_u16(e + 16, param_len);
        gsp_wr_u16(e + 18, 0u);        /* flags 预留 */
        gsp_wr_u32(e + 20, a->arg);
    }

    /* blob data 紧跟 string 区之后 */
    const uint32_t blob_data_off = str_table_off + (uint32_t)st.len;
    uint32_t blob_data_total = 0;
    for (uint16_t k = 0; k < blob_count; k++) {
        blob_data_total += blobs[k].comp_size;
    }

    /* blob table 字节（data_off 用绝对偏移）*/
    uint8_t *blob_bytes = (uint8_t *)calloc(blob_count ? blob_count : 1, GSP_BLOB_SIZE);
    if (blob_bytes == NULL) {
        free(action_bytes);
        free(st.bytes);
        free(obj_bytes);
        free(params_bytes);
        for (uint16_t k = 0; k < blob_count; k++) {
            free(blobs[k].comp);
        }
        free(blobs);
        free(obj_blob);
        return NULL;
    }
    uint32_t cursor = blob_data_off;
    for (uint16_t k = 0; k < blob_count; k++) {
        uint8_t *e = blob_bytes + (size_t)k * GSP_BLOB_SIZE;
        gsp_wr_u16(e + 0, blobs[k].w);
        gsp_wr_u16(e + 2, blobs[k].h);
        e[4] = blobs[k].cf;
        e[5] = blobs[k].codec;
        gsp_wr_u16(e + 6, blobs[k].stride);
        gsp_wr_u32(e + 8, blobs[k].raw_size);
        gsp_wr_u32(e + 12, blobs[k].comp_size);
        gsp_wr_u32(e + 16, cursor);
        cursor += blobs[k].comp_size;
    }

    const uint32_t total = blob_data_off + blob_data_total;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (buf == NULL) {
        free(action_bytes);
        free(blob_bytes);
        free(st.bytes);
        free(obj_bytes);
        free(params_bytes);
        for (uint16_t k = 0; k < blob_count; k++) {
            free(blobs[k].comp);
        }
        free(blobs);
        free(obj_blob);
        return NULL;
    }

    /* header */
    gsp_wr_u32(buf + 0, GSP_MAGIC);
    gsp_wr_u32(buf + 4, GSP_VERSION);
    gsp_wr_u16(buf + 8, scene->screen_w);
    gsp_wr_u16(buf + 10, scene->screen_h);
    gsp_wr_u32(buf + 12, scene->screen_bg);
    gsp_wr_u32(buf + 16, n);
    gsp_wr_u32(buf + 20, obj_table_off);
    gsp_wr_u32(buf + 24, str_table_off);
    gsp_wr_u32(buf + 28, blob_count);
    gsp_wr_u32(buf + 32, blob_table_off);
    gsp_wr_u32(buf + 36, total);
    gsp_wr_u32(buf + 40, 0u);   /* crc32 占位，稍后回填 */
    gsp_wr_u32(buf + 44, action_count);
    gsp_wr_u32(buf + 48, action_count ? action_table_off : 0u);
    gsp_wr_u32(buf + 52, 0u);   /* reserved */

    memcpy(buf + obj_table_off, obj_bytes, (size_t)n * GSP_OBJ_SIZE);
    memcpy(buf + blob_table_off, blob_bytes, (size_t)blob_count * GSP_BLOB_SIZE);
    memcpy(buf + action_table_off, action_bytes, (size_t)action_count * GSP_ACTION_SIZE);
    memcpy(buf + params_base, params_bytes, params_total);
    memcpy(buf + str_table_off, st.bytes, st.len);
    cursor = blob_data_off;
    for (uint16_t k = 0; k < blob_count; k++) {
        memcpy(buf + cursor, blobs[k].comp, blobs[k].comp_size);
        cursor += blobs[k].comp_size;
    }

    /* 全包装配完成后回填 crc32 */
    gsp_wr_u32(buf + 40, gsp_crc32_scene(buf, total));

    free(action_bytes);
    free(blob_bytes);
    free(obj_bytes);
    free(st.bytes);
    free(params_bytes);
    for (uint16_t k = 0; k < blob_count; k++) {
        free(blobs[k].comp);
    }
    free(blobs);
    free(obj_blob);

    *out_size = total;
    return buf;
}
