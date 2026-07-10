/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/gsp_to_arena.h"

#include <stdlib.h>
#include <string.h>

#include "gfx/scene/arena.h"
#include "gfx/scene/gsp.h"
#include "gfx/types.h"

typedef struct {
    uint8_t *mem;
    size_t size;
    size_t used;
} scratch_t;

static void *scratch_alloc(scratch_t *s, size_t n)
{
    if (s == NULL || n == 0) {
        return NULL;
    }
    n = (n + 7u) & ~((size_t)7u);
    if (s->used + n > s->size) {
        return NULL;
    }
    void *p = s->mem + s->used;
    s->used += n;
    return p;
}

static const char *safe_str(const uint8_t *buf, size_t size, uint32_t off)
{
    if (off == 0 || (size_t)off >= size) {
        return NULL;
    }
    size_t i = (size_t)off;
    while (i < size && buf[i] != '\0') {
        i++;
    }
    if (i >= size) {
        return NULL;
    }
    return (const char *)(buf + off);
}

static int is_convertible(uint16_t type)
{
    return type == GSP_OBJ_CONTAINER || type == GSP_OBJ_LAYER ||
           type == GSP_OBJ_LABEL || type == GSP_OBJ_BUTTON ||
           type == GSP_OBJ_IMAGE || type == GSP_OBJ_LIST ||
           type == GSP_OBJ_WHEEL;
}

static uint16_t *decode_blob_rgb565(const uint8_t *gsp, size_t gsp_size,
                                    uint32_t blob_off, uint32_t blob_count,
                                    uint32_t blob_idx, uint16_t *out_w, uint16_t *out_h,
                                    scratch_t *scratch)
{
    if (blob_idx >= blob_count || out_w == NULL || out_h == NULL) {
        return NULL;
    }
    const uint8_t *b = gsp + blob_off + (size_t)blob_idx * GSP_BLOB_SIZE;
    const uint16_t w = gsp_rd_u16(b + 0);
    const uint16_t h = gsp_rd_u16(b + 2);
    const uint8_t cf = b[4];
    const uint8_t codec = b[5];
    const uint32_t raw_size = gsp_rd_u32(b + 8);
    const uint32_t comp_size = gsp_rd_u32(b + 12);
    const uint32_t data_off = gsp_rd_u32(b + 16);

    if (w == 0 || h == 0 || cf != GFX_COLOR_FORMAT_RGB565) {
        return NULL;
    }
    if ((uint64_t)data_off + comp_size > gsp_size || raw_size == 0) {
        return NULL;
    }
    if (raw_size != (uint32_t)w * (uint32_t)h * 2u) {
        return NULL;
    }

    uint8_t *raw = (uint8_t *)scratch_alloc(scratch, raw_size);
    if (raw == NULL) {
        return NULL;
    }

    const uint8_t *comp = gsp + data_off;
    if (codec == GSP_CODEC_STORE) {
        if (comp_size != raw_size) {
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
        if (ro != raw_size) {
            return NULL;
        }
    } else {
        return NULL;
    }

    *out_w = w;
    *out_h = h;
    return (uint16_t *)raw;
}

static int parse_items(const uint8_t *params, uint16_t params_len,
                       scratch_t *scratch, const char ***out_items,
                       uint16_t *out_count, uint16_t *out_selected,
                       uint16_t *out_item_h, uint16_t *out_flags)
{
    if (params == NULL || params_len < 12u || out_items == NULL) {
        return -1;
    }
    const uint16_t item_count = gsp_rd_u16(params + 0);
    const uint16_t selected = gsp_rd_u16(params + 2);
    const uint16_t item_height = gsp_rd_u16(params + 4);
    const uint16_t flags = gsp_rd_u16(params + 8);
    if (item_count == 0 || item_count > 256u) {
        return -1;
    }

    const char **items = (const char **)scratch_alloc(scratch, (size_t)item_count * sizeof(char *));
    if (items == NULL) {
        return -1;
    }

    uint32_t cursor = 12u;
    for (uint16_t i = 0; i < item_count; i++) {
        if (cursor + 2u > params_len) {
            return -1;
        }
        const uint16_t len = gsp_rd_u16(params + cursor);
        cursor += 2u;
        if (cursor + len > params_len) {
            return -1;
        }
        char *copy = (char *)scratch_alloc(scratch, (size_t)len + 1u);
        if (copy == NULL) {
            return -1;
        }
        if (len > 0) {
            memcpy(copy, params + cursor, len);
        }
        copy[len] = '\0';
        items[i] = copy;
        cursor += len;
    }

    *out_items = items;
    *out_count = item_count;
    *out_selected = selected;
    *out_item_h = item_height;
    *out_flags = flags;
    return 0;
}

int gsp_to_arena(const uint8_t *gsp, size_t gsp_size,
                 uint8_t **out_pkg, size_t *out_pkg_size,
                 gsp_to_arena_info_t *info)
{
    if (gsp == NULL || out_pkg == NULL || out_pkg_size == NULL ||
            gsp_size < GSP_HEADER_SIZE) {
        return GSP_TO_ARENA_ERR_ARG;
    }

    const uint32_t magic = gsp_rd_u32(gsp + 0);
    const uint32_t version = gsp_rd_u32(gsp + 4);
    if (magic != GSP_MAGIC) {
        return GSP_TO_ARENA_ERR_MAGIC;
    }
    if (version != GSP_VERSION) {
        return GSP_TO_ARENA_ERR_VERSION;
    }

    const uint16_t screen_w = gsp_rd_u16(gsp + 8);
    const uint16_t screen_h = gsp_rd_u16(gsp + 10);
    const uint32_t screen_bg = gsp_rd_u32(gsp + 12);
    const uint32_t obj_count = gsp_rd_u32(gsp + 16);
    const uint32_t obj_off = gsp_rd_u32(gsp + 20);
    const uint32_t blob_count = gsp_rd_u32(gsp + 28);
    const uint32_t blob_off = gsp_rd_u32(gsp + 32);
    const uint32_t total_size = gsp_rd_u32(gsp + 36);

    if (total_size != gsp_size || obj_count == 0 || obj_count > 4096u) {
        return GSP_TO_ARENA_ERR_SIZE;
    }
    if ((uint64_t)obj_off + (uint64_t)obj_count * GSP_OBJ_SIZE > gsp_size) {
        return GSP_TO_ARENA_ERR_SIZE;
    }
    if (blob_count > 0 &&
            (uint64_t)blob_off + (uint64_t)blob_count * GSP_BLOB_SIZE > gsp_size) {
        return GSP_TO_ARENA_ERR_SIZE;
    }

    uint16_t *map = (uint16_t *)calloc(obj_count, sizeof(uint16_t));
    if (map == NULL) {
        return GSP_TO_ARENA_ERR_ALLOC;
    }

    uint16_t out_n = 0;
    uint16_t skipped = 0;
    size_t scratch_need = 64u * 1024u;
    for (uint32_t i = 0; i < obj_count; i++) {
        const uint8_t *e = gsp + obj_off + (size_t)i * GSP_OBJ_SIZE;
        const uint16_t type = gsp_rd_u16(e + 0);
        if (is_convertible(type)) {
            map[i] = out_n++;
            if (type == GSP_OBJ_IMAGE) {
                const uint32_t blob_idx = gsp_rd_u32(e + 44);
                if (blob_idx < blob_count) {
                    const uint8_t *b = gsp + blob_off + (size_t)blob_idx * GSP_BLOB_SIZE;
                    scratch_need += (size_t)gsp_rd_u32(b + 8) + 64u;
                }
            } else if (type == GSP_OBJ_LIST || type == GSP_OBJ_WHEEL) {
                const uint16_t plen = gsp_rd_u16(e + 52);
                scratch_need += (size_t)plen + 256u;
            }
        } else {
            map[i] = 0xFFFFu;
            skipped++;
        }
    }

    if (out_n == 0) {
        free(map);
        return GSP_TO_ARENA_ERR_EMPTY;
    }

    scratch_t scratch = {0};
    scratch.size = scratch_need;
    scratch.mem = (uint8_t *)malloc(scratch.size);
    arena_desc_t *descs = (arena_desc_t *)calloc(out_n, sizeof(arena_desc_t));
    if (scratch.mem == NULL || descs == NULL) {
        free(map);
        free(scratch.mem);
        free(descs);
        return GSP_TO_ARENA_ERR_ALLOC;
    }

    uint16_t oi = 0;
    for (uint32_t i = 0; i < obj_count; i++) {
        if (map[i] == 0xFFFFu) {
            continue;
        }
        const uint8_t *e = gsp + obj_off + (size_t)i * GSP_OBJ_SIZE;
        const uint16_t type = gsp_rd_u16(e + 0);
        const uint16_t parent_idx = gsp_rd_u16(e + 2);
        const int16_t x = gsp_rd_i16(e + 4);
        const int16_t y = gsp_rd_i16(e + 6);
        const uint16_t w = gsp_rd_u16(e + 8);
        const uint16_t h = gsp_rd_u16(e + 10);
        const uint32_t flags = gsp_rd_u32(e + 12);
        const uint32_t fg = gsp_rd_u32(e + 16);
        const uint32_t bg = gsp_rd_u32(e + 20);
        const uint32_t text_off = gsp_rd_u32(e + 32);
        const uint32_t cb_off = gsp_rd_u32(e + 36);
        const uint32_t name_off = gsp_rd_u32(e + 40);
        const uint32_t blob_idx = gsp_rd_u32(e + 44);
        const uint32_t params_off = gsp_rd_u32(e + 48);
        const uint16_t params_len = gsp_rd_u16(e + 52);

        const char *text = (flags & GSP_F_TEXT) ? safe_str(gsp, gsp_size, text_off) : NULL;
        const char *cbname = (flags & GSP_F_CALLBACK) ? safe_str(gsp, gsp_size, cb_off) : NULL;
        const char *wname = (flags & GSP_F_NAME) ? safe_str(gsp, gsp_size, name_off) : NULL;
        const uint8_t *params = NULL;
        if ((flags & GSP_F_PARAMS) && params_off != 0 &&
                (size_t)params_off + params_len <= gsp_size) {
            params = gsp + params_off;
        }

        arena_desc_t *d = &descs[oi];
        memset(d, 0, sizeof(*d));
        d->x = x;
        d->y = y;
        d->w = w;
        d->h = h;
        d->flags = ARENA_F_VISIBLE;
        if (flags & GSP_F_HIDDEN) {
            d->flags = (uint16_t)(d->flags & ~ARENA_F_VISIBLE);
        }
        if (flags & GSP_F_RADIUS) {
            const uint16_t radius = gsp_rd_u16(e + 30);
            d->radius = (radius > 0xFFu) ? 0xFFu : (uint8_t)radius;
        }

        if (parent_idx == GSP_NO_PARENT) {
            d->parent = -1;
        } else if (parent_idx < obj_count && map[parent_idx] != 0xFFFFu) {
            d->parent = (int)map[parent_idx];
        } else {
            d->parent = -1;
        }

        if (type == GSP_OBJ_CONTAINER || type == GSP_OBJ_LAYER) {
            d->type = ARENA_NODE_CONTAINER;
            d->name = wname;
            if (flags & GSP_F_BG_COLOR) {
                d->flags |= ARENA_F_BG;
                d->bg_rgb = bg;
            }
        } else if (type == GSP_OBJ_LABEL) {
            d->type = ARENA_NODE_LABEL;
            d->name = (text != NULL) ? text : wname;
            d->bg_rgb = (flags & GSP_F_FG_COLOR) ? fg : 0xF3F7FAu;
        } else if (type == GSP_OBJ_BUTTON) {
            d->type = ARENA_NODE_BUTTON;
            d->name = (text != NULL) ? text : wname;
            d->action = cbname;
            d->flags |= ARENA_F_CLICKABLE;
            if (flags & GSP_F_BG_COLOR) {
                d->flags |= ARENA_F_BG;
                d->bg_rgb = bg;
            } else {
                d->flags |= ARENA_F_BG;
                d->bg_rgb = 0x2f8cffu;
            }
        } else if (type == GSP_OBJ_IMAGE) {
            d->type = ARENA_NODE_IMAGE;
            d->name = wname;
            if (flags & GSP_F_IMAGE) {
                uint16_t iw = 0, ih = 0;
                uint16_t *px = decode_blob_rgb565(gsp, gsp_size, blob_off, blob_count,
                                                  blob_idx, &iw, &ih, &scratch);
                if (px != NULL) {
                    d->img_rgb565 = px;
                    d->img_w = iw;
                    d->img_h = ih;
                }
            }
        } else { /* LIST / WHEEL */
            d->type = (type == GSP_OBJ_LIST) ? ARENA_NODE_LIST : ARENA_NODE_WHEEL;
            d->name = wname;
            d->flags |= ARENA_F_CLICKABLE | ARENA_F_BG;
            d->bg_rgb = (flags & GSP_F_BG_COLOR) ? bg : 0x1a1f2eu;
            d->text_rgb = (flags & GSP_F_FG_COLOR) ? fg : 0xF3F7FAu;
            if (params != NULL) {
                const char **items = NULL;
                uint16_t ic = 0, sel = 0, ih = 0, iflags = 0;
                if (parse_items(params, params_len, &scratch, &items, &ic, &sel, &ih, &iflags) == 0) {
                    d->items = items;
                    d->item_count = ic;
                    d->selected = sel;
                    d->item_height = ih;
                    d->items_flags = 0;
                    if (type == GSP_OBJ_WHEEL && (iflags & GSP_ITEM_PARAMS_F_CYCLIC)) {
                        d->items_flags |= ARENA_ITEMS_F_CYCLIC;
                    }
                }
            }
        }

        oi++;
    }

    size_t pkg_size = 0;
    uint8_t *pkg = arena_pack(descs, out_n, &pkg_size);
    free(descs);
    free(map);
    free(scratch.mem);

    if (pkg == NULL) {
        return GSP_TO_ARENA_ERR_PACK;
    }

    *out_pkg = pkg;
    *out_pkg_size = pkg_size;
    if (info != NULL) {
        info->screen_w = screen_w;
        info->screen_h = screen_h;
        info->screen_bg = screen_bg;
        info->src_obj_count = (uint16_t)obj_count;
        info->out_node_count = out_n;
        info->skipped = skipped;
    }
    return GSP_TO_ARENA_OK;
}
