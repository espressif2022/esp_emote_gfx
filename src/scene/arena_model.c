/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(arena_hdr_t) == 24, "arena header size drift");
_Static_assert(sizeof(arena_node_t) == 32, "arena node size drift");

static void *arena_at(arena_t *a, uint32_t off)
{
    assert(a != NULL && a->base != NULL);
    assert((size_t)off < a->size);
    return a->base + off;
}

arena_hdr_t *arena_hdr(arena_t *a)
{
    return (arena_hdr_t *)arena_at(a, 0);
}

arena_node_t *arena_node(arena_t *a, uint32_t off)
{
    if (off == ARENA_NO_NODE) {
        return NULL;
    }
    return (arena_node_t *)arena_at(a, off);
}

const char *arena_str(arena_t *a, uint32_t off)
{
    if (off == 0) {
        return NULL;
    }
    return (const char *)arena_at(a, off);
}

const arena_img_hdr_t *arena_img(arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(arena_img_hdr_t) > a->size) {
        return NULL;
    }
    return (const arena_img_hdr_t *)(a->base + off);
}

const uint16_t *arena_img_pixels(arena_t *a, uint32_t off)
{
    const arena_img_hdr_t *ih = arena_img(a, off);
    if (ih == NULL || ih->format != ARENA_IMG_FMT_RGB565 || ih->w == 0 || ih->h == 0) {
        return NULL;
    }
    const size_t need = (size_t)off + sizeof(arena_img_hdr_t) +
                        (size_t)ih->w * (size_t)ih->h * sizeof(uint16_t);
    if (need > a->size) {
        return NULL;
    }
    return (const uint16_t *)(a->base + off + sizeof(arena_img_hdr_t));
}

const arena_items_hdr_t *arena_items(arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(arena_items_hdr_t) > a->size) {
        return NULL;
    }
    return (const arena_items_hdr_t *)(a->base + off);
}

arena_items_hdr_t *arena_items_mut(arena_t *a, uint32_t off)
{
    return (arena_items_hdr_t *)arena_items(a, off);
}

const char *arena_item_text(arena_t *a, uint32_t items_off, uint16_t index, uint16_t *out_len)
{
    const arena_items_hdr_t *ih = arena_items(a, items_off);
    if (ih == NULL || index >= ih->item_count) {
        return NULL;
    }
    size_t cursor = (size_t)items_off + sizeof(arena_items_hdr_t);
    for (uint16_t i = 0; i <= index; i++) {
        if (cursor + 2u > a->size) {
            return NULL;
        }
        const uint16_t len = (uint16_t)(a->base[cursor] | ((uint16_t)a->base[cursor + 1] << 8));
        cursor += 2u;
        if (cursor + (size_t)len > a->size) {
            return NULL;
        }
        if (i == index) {
            if (out_len != NULL) {
                *out_len = len;
            }
            return (const char *)(a->base + cursor);
        }
        cursor += (size_t)len;
    }
    return NULL;
}

uint32_t arena_node_offset(const arena_t *a, const arena_node_t *n)
{
    if (a == NULL || a->base == NULL || n == NULL) {
        return ARENA_NO_NODE;
    }
    const uintptr_t base = (uintptr_t)a->base;
    const uintptr_t p = (uintptr_t)n;
    if (p < base || (p - base) >= a->size) {
        return ARENA_NO_NODE;
    }
    return (uint32_t)(p - base);
}

uint32_t arena_node_index(const arena_t *a, const arena_node_t *n)
{
    if (a == NULL || a->base == NULL || n == NULL) {
        return ARENA_NO_NODE;
    }
    const arena_hdr_t *hdr = (const arena_hdr_t *)a->base;
    const uintptr_t base = (uintptr_t)(a->base + hdr->nodes_off);
    const uintptr_t p = (uintptr_t)n;
    if (p < base) {
        return ARENA_NO_NODE;
    }
    const uintptr_t delta = p - base;
    if ((delta % sizeof(arena_node_t)) != 0) {
        return ARENA_NO_NODE;
    }
    const uint32_t idx = (uint32_t)(delta / sizeof(arena_node_t));
    if (idx >= hdr->node_count) {
        return ARENA_NO_NODE;
    }
    return idx;
}

static size_t items_payload_bytes(const arena_desc_t *d)
{
    if (d == NULL || d->items == NULL || d->item_count == 0) {
        return 0;
    }
    size_t n = sizeof(arena_items_hdr_t);
    for (uint16_t k = 0; k < d->item_count; k++) {
        const char *s = d->items[k];
        const size_t len = (s != NULL) ? strlen(s) : 0u;
        n += 2u + len;
    }
    return n;
}

uint8_t *arena_pack(const arena_desc_t *descs, uint16_t count, size_t *out_size)
{
    size_t str_bytes = 0;
    size_t blob_bytes = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (descs[i].name != NULL) {
            str_bytes += strlen(descs[i].name) + 1u;
        }
        if (descs[i].action != NULL) {
            str_bytes += strlen(descs[i].action) + 1u;
        }
        if (descs[i].type == ARENA_NODE_IMAGE && descs[i].img_rgb565 != NULL &&
                descs[i].img_w > 0 && descs[i].img_h > 0) {
            blob_bytes += sizeof(arena_img_hdr_t) +
                          (size_t)descs[i].img_w * (size_t)descs[i].img_h * sizeof(uint16_t);
        }
        if (descs[i].type == ARENA_NODE_LIST || descs[i].type == ARENA_NODE_WHEEL) {
            blob_bytes += items_payload_bytes(&descs[i]);
        }
    }

    const size_t nodes_off = sizeof(arena_hdr_t);
    const size_t str_off = nodes_off + (size_t)count * sizeof(arena_node_t);
    const size_t blob_off = str_off + str_bytes;
    const size_t total = blob_off + blob_bytes;

    uint8_t *buf = (uint8_t *)calloc(1, total);
    if (buf == NULL) {
        return NULL;
    }

    arena_hdr_t *hdr = (arena_hdr_t *)buf;
    hdr->magic = ARENA_MAGIC;
    hdr->version = ARENA_VERSION;
    hdr->node_count = count;
    hdr->nodes_off = (uint32_t)nodes_off;
    hdr->str_off = (uint32_t)str_off;
    hdr->total_size = (uint32_t)total;
    hdr->root_off = ARENA_NO_NODE;

    uint32_t *node_offs = (uint32_t *)calloc(count, sizeof(uint32_t));
    uint32_t *first_child = (uint32_t *)calloc(count, sizeof(uint32_t));
    if (node_offs == NULL || first_child == NULL) {
        free(buf);
        free(node_offs);
        free(first_child);
        return NULL;
    }

    for (uint16_t i = 0; i < count; i++) {
        node_offs[i] = (uint32_t)(nodes_off + (size_t)i * sizeof(arena_node_t));
        first_child[i] = ARENA_NO_NODE;
    }

    for (uint16_t i = 0; i < count; i++) {
        const int p = descs[i].parent;
        if (p < 0) {
            if (hdr->root_off == ARENA_NO_NODE) {
                hdr->root_off = node_offs[i];
            }
            continue;
        }
        assert((uint16_t)p < i);
        if (first_child[p] == ARENA_NO_NODE) {
            first_child[p] = node_offs[i];
        }
    }

    size_t str_cursor = str_off;
    size_t blob_cursor = blob_off;
    for (uint16_t i = 0; i < count; i++) {
        arena_node_t *n = (arena_node_t *)(buf + node_offs[i]);
        n->type = descs[i].type;
        n->flags = descs[i].flags;
        n->x = descs[i].x;
        n->y = descs[i].y;
        n->w = descs[i].w;
        n->h = descs[i].h;
        n->bg_rgb = descs[i].bg_rgb;
        n->first_child = first_child[i];
        n->next_sibling = ARENA_NO_NODE;
        n->reserved = 0;
        n->name_off = 0;
        if (descs[i].name != NULL) {
            const size_t len = strlen(descs[i].name) + 1u;
            memcpy(buf + str_cursor, descs[i].name, len);
            n->name_off = (uint32_t)str_cursor;
            str_cursor += len;
        }
        if (descs[i].type == ARENA_NODE_IMAGE && descs[i].img_rgb565 != NULL &&
                descs[i].img_w > 0 && descs[i].img_h > 0) {
            arena_img_hdr_t *ih = (arena_img_hdr_t *)(buf + blob_cursor);
            ih->w = descs[i].img_w;
            ih->h = descs[i].img_h;
            ih->format = ARENA_IMG_FMT_RGB565;
            ih->pad = 0;
            const size_t px_bytes =
                (size_t)descs[i].img_w * (size_t)descs[i].img_h * sizeof(uint16_t);
            memcpy(buf + blob_cursor + sizeof(arena_img_hdr_t), descs[i].img_rgb565, px_bytes);
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(arena_img_hdr_t) + px_bytes;
        } else if ((descs[i].type == ARENA_NODE_LIST || descs[i].type == ARENA_NODE_WHEEL) &&
                   descs[i].items != NULL && descs[i].item_count > 0) {
            arena_items_hdr_t *ih = (arena_items_hdr_t *)(buf + blob_cursor);
            ih->item_count = descs[i].item_count;
            ih->selected = descs[i].selected;
            ih->item_height = descs[i].item_height;
            ih->flags = descs[i].items_flags;
            ih->text_rgb = (descs[i].text_rgb != 0u) ? descs[i].text_rgb : 0xF3F7FAu;
            size_t cur = blob_cursor + sizeof(arena_items_hdr_t);
            for (uint16_t k = 0; k < descs[i].item_count; k++) {
                const char *s = descs[i].items[k];
                const uint16_t len = (uint16_t)((s != NULL) ? strlen(s) : 0u);
                buf[cur] = (uint8_t)(len & 0xFFu);
                buf[cur + 1] = (uint8_t)((len >> 8) & 0xFFu);
                cur += 2u;
                if (len > 0 && s != NULL) {
                    memcpy(buf + cur, s, len);
                    cur += len;
                }
            }
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor = cur;
        } else if (descs[i].action != NULL) {
            const size_t len = strlen(descs[i].action) + 1u;
            memcpy(buf + str_cursor, descs[i].action, len);
            n->reserved = (uint32_t)str_cursor;
            str_cursor += len;
        }
    }

    for (uint16_t i = 0; i < count; i++) {
        const int p = descs[i].parent;
        if (p < 0) {
            continue;
        }
        for (uint16_t j = (uint16_t)(i + 1u); j < count; j++) {
            if (descs[j].parent == p) {
                arena_node_t *n = (arena_node_t *)(buf + node_offs[i]);
                n->next_sibling = node_offs[j];
                break;
            }
        }
    }

    uint32_t prev_root = ARENA_NO_NODE;
    for (uint16_t i = 0; i < count; i++) {
        if (descs[i].parent >= 0) {
            continue;
        }
        if (prev_root != ARENA_NO_NODE) {
            arena_node_t *prev = (arena_node_t *)(buf + prev_root);
            prev->next_sibling = node_offs[i];
        }
        prev_root = node_offs[i];
    }

    free(node_offs);
    free(first_child);

    if (out_size != NULL) {
        *out_size = total;
    }
    return buf;
}

int arena_load(const uint8_t *pkg, size_t pkg_size, arena_t *out)
{
    if (pkg == NULL || out == NULL || pkg_size < sizeof(arena_hdr_t)) {
        return -1;
    }

    const arena_hdr_t *hdr = (const arena_hdr_t *)pkg;
    if (hdr->magic != ARENA_MAGIC || hdr->version != ARENA_VERSION) {
        return -2;
    }
    if (hdr->total_size != pkg_size || hdr->nodes_off >= pkg_size || hdr->str_off > pkg_size) {
        return -3;
    }
    if ((size_t)hdr->nodes_off + (size_t)hdr->node_count * sizeof(arena_node_t) > pkg_size) {
        return -4;
    }

    uint8_t *base = (uint8_t *)malloc(pkg_size);
    if (base == NULL) {
        return -5;
    }
    memcpy(base, pkg, pkg_size);
    out->base = base;
    out->size = pkg_size;
    return 0;
}

void arena_free(arena_t *a)
{
    if (a == NULL) {
        return;
    }
    free(a->base);
    a->base = NULL;
    a->size = 0;
}

arena_node_t *arena_find_by_name(arena_t *a, const char *name)
{
    if (a == NULL || name == NULL) {
        return NULL;
    }
    const arena_hdr_t *hdr = arena_hdr(a);
    const uint32_t nodes_off = hdr->nodes_off;
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        uint32_t off = nodes_off + (uint32_t)i * (uint32_t)sizeof(arena_node_t);
        arena_node_t *n = arena_node(a, off);
        const char *s = arena_str(a, n->name_off);
        if (s != NULL && strcmp(s, name) == 0) {
            return n;
        }
    }
    return NULL;
}

static int abs_area_rec(const arena_t *a, uint32_t want_off, uint32_t cur_off,
                        int ox, int oy, int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2)
{
    for (uint32_t off = cur_off; off != ARENA_NO_NODE; ) {
        arena_node_t *n = arena_node((arena_t *)a, off);
        if (n == NULL) {
            return -1;
        }
        const int ax = ox + (int)n->x;
        const int ay = oy + (int)n->y;
        if (off == want_off) {
            *x1 = (int16_t)ax;
            *y1 = (int16_t)ay;
            *x2 = (int16_t)(ax + (int)n->w - 1);
            *y2 = (int16_t)(ay + (int)n->h - 1);
            return 0;
        }
        if (n->first_child != ARENA_NO_NODE) {
            if (abs_area_rec(a, want_off, n->first_child, ax, ay, x1, y1, x2, y2) == 0) {
                return 0;
            }
        }
        off = n->next_sibling;
    }
    return -1;
}

int arena_node_abs_area(const arena_t *a, uint32_t node_off,
                        int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2)
{
    if (a == NULL || a->base == NULL || node_off == ARENA_NO_NODE ||
            x1 == NULL || y1 == NULL || x2 == NULL || y2 == NULL) {
        return -1;
    }
    const arena_hdr_t *hdr = (const arena_hdr_t *)a->base;
    return abs_area_rec(a, node_off, hdr->root_off, 0, 0, x1, y1, x2, y2);
}

void arena_dump_tree(arena_t *a, uint32_t node_off, int depth)
{
    for (uint32_t off = node_off; off != ARENA_NO_NODE; ) {
        arena_node_t *n = arena_node(a, off);
        const char *name = arena_str(a, n->name_off);
        for (int i = 0; i < depth; i++) {
            fputs("  ", stdout);
        }
        printf("off=%u type=%u flags=0x%x rect=(%d,%d %ux%u) bg=#%06x name=%s\n",
               (unsigned)off, (unsigned)n->type, (unsigned)n->flags,
               (int)n->x, (int)n->y, (unsigned)n->w, (unsigned)n->h,
               (unsigned)(n->bg_rgb & 0xFFFFFFu),
               name != NULL ? name : "-");
        if (n->first_child != ARENA_NO_NODE) {
            arena_dump_tree(a, n->first_child, depth + 1);
        }
        off = n->next_sibling;
    }
}
