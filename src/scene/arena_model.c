/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(gfx_arena_hdr_t) == 24, "arena header size drift");
_Static_assert(sizeof(gfx_arena_node_t) == 32, "arena node size drift");
_Static_assert(sizeof(gfx_arena_progress_hdr_t) == 12, "arena progress hdr size drift");
_Static_assert(sizeof(gfx_arena_anim_hdr_t) == 16, "arena anim hdr size drift");
_Static_assert(sizeof(gfx_arena_motion_hdr_t) == 8, "arena motion hdr size drift");

static void *gfx_arena_at(gfx_arena_t *a, uint32_t off)
{
    assert(a != NULL && a->base != NULL);
    assert((size_t)off < a->size);
    return a->base + off;
}

gfx_arena_hdr_t *gfx_arena_hdr(gfx_arena_t *a)
{
    return (gfx_arena_hdr_t *)gfx_arena_at(a, 0);
}

gfx_arena_node_t *gfx_arena_node(gfx_arena_t *a, uint32_t off)
{
    if (off == GFX_ARENA_NO_NODE) {
        return NULL;
    }
    return (gfx_arena_node_t *)gfx_arena_at(a, off);
}

const char *gfx_arena_str(gfx_arena_t *a, uint32_t off)
{
    if (off == 0) {
        return NULL;
    }
    return (const char *)gfx_arena_at(a, off);
}

const gfx_arena_img_hdr_t *gfx_arena_img(gfx_arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(gfx_arena_img_hdr_t) > a->size) {
        return NULL;
    }
    return (const gfx_arena_img_hdr_t *)(a->base + off);
}

const uint16_t *gfx_arena_img_pixels(gfx_arena_t *a, uint32_t off)
{
    const gfx_arena_img_hdr_t *ih = gfx_arena_img(a, off);
    if (ih == NULL || ih->format != GFX_ARENA_IMG_FMT_RGB565 || ih->w == 0 || ih->h == 0) {
        return NULL;
    }
    const size_t need = (size_t)off + sizeof(gfx_arena_img_hdr_t) +
                        (size_t)ih->w * (size_t)ih->h * sizeof(uint16_t);
    if (need > a->size) {
        return NULL;
    }
    return (const uint16_t *)(a->base + off + sizeof(gfx_arena_img_hdr_t));
}

const char *gfx_arena_img_file_path(gfx_arena_t *a, uint32_t off, uint8_t face)
{
    const gfx_arena_img_hdr_t *ih = gfx_arena_img(a, off);
    const char *p;
    size_t cur;
    size_t end;
    uint8_t i;

    if (ih == NULL || ih->format != GFX_ARENA_IMG_FMT_FILE_PATH) {
        return NULL;
    }
    if (face > 0U && (ih->pad & GFX_ARENA_IMG_F_HAS_PRESSED) == 0) {
        return NULL;
    }

    cur = (size_t)off + sizeof(gfx_arena_img_hdr_t);
    end = a->size;
    for (i = 0; i <= face; i++) {
        if (cur >= end) {
            return NULL;
        }
        p = (const char *)(a->base + cur);
        /* Require a terminating NUL within the package. */
        {
            size_t rem = end - cur;
            size_t n = 0;
            while (n < rem && p[n] != '\0') {
                n++;
            }
            if (n >= rem) {
                return NULL;
            }
            if (i == face) {
                return p;
            }
            cur += n + 1u;
        }
    }
    return NULL;
}

const gfx_arena_items_hdr_t *gfx_arena_items(gfx_arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(gfx_arena_items_hdr_t) > a->size) {
        return NULL;
    }
    return (const gfx_arena_items_hdr_t *)(a->base + off);
}

gfx_arena_items_hdr_t *gfx_arena_items_mut(gfx_arena_t *a, uint32_t off)
{
    return (gfx_arena_items_hdr_t *)gfx_arena_items(a, off);
}

const gfx_arena_progress_hdr_t *gfx_arena_progress(gfx_arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(gfx_arena_progress_hdr_t) > a->size) {
        return NULL;
    }
    return (const gfx_arena_progress_hdr_t *)(a->base + off);
}

gfx_arena_progress_hdr_t *gfx_arena_progress_mut(gfx_arena_t *a, uint32_t off)
{
    return (gfx_arena_progress_hdr_t *)gfx_arena_progress(a, off);
}

const gfx_arena_anim_hdr_t *gfx_arena_anim(gfx_arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(gfx_arena_anim_hdr_t) > a->size) {
        return NULL;
    }
    return (const gfx_arena_anim_hdr_t *)(a->base + off);
}

const char *gfx_arena_anim_file_path(gfx_arena_t *a, uint32_t off)
{
    const gfx_arena_anim_hdr_t *ah = gfx_arena_anim(a, off);
    if (ah == NULL || ah->format != GFX_ARENA_ANIM_FMT_FILE_PATH) {
        return NULL;
    }
    if ((size_t)off + sizeof(gfx_arena_anim_hdr_t) >= a->size) {
        return NULL;
    }
    return (const char *)(a->base + off + sizeof(gfx_arena_anim_hdr_t));
}

const gfx_arena_motion_hdr_t *gfx_arena_motion(gfx_arena_t *a, uint32_t off)
{
    if (a == NULL || a->base == NULL || off == 0 ||
            (size_t)off + sizeof(gfx_arena_motion_hdr_t) > a->size) {
        return NULL;
    }
    return (const gfx_arena_motion_hdr_t *)(a->base + off);
}

gfx_arena_motion_hdr_t *gfx_arena_motion_mut(gfx_arena_t *a, uint32_t off)
{
    return (gfx_arena_motion_hdr_t *)gfx_arena_motion(a, off);
}

const char *gfx_arena_motion_asset_key(gfx_arena_t *a, uint32_t off)
{
    if (gfx_arena_motion(a, off) == NULL) {
        return NULL;
    }
    if ((size_t)off + sizeof(gfx_arena_motion_hdr_t) >= a->size) {
        return NULL;
    }
    return (const char *)(a->base + off + sizeof(gfx_arena_motion_hdr_t));
}

const char *gfx_arena_item_text(gfx_arena_t *a, uint32_t items_off, uint16_t index, uint16_t *out_len)
{
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(a, items_off);
    if (ih == NULL || index >= ih->item_count) {
        return NULL;
    }
    size_t cursor = (size_t)items_off + sizeof(gfx_arena_items_hdr_t);
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

uint32_t gfx_arena_node_offset(const gfx_arena_t *a, const gfx_arena_node_t *n)
{
    if (a == NULL || a->base == NULL || n == NULL) {
        return GFX_ARENA_NO_NODE;
    }
    const uintptr_t base = (uintptr_t)a->base;
    const uintptr_t p = (uintptr_t)n;
    if (p < base || (p - base) >= a->size) {
        return GFX_ARENA_NO_NODE;
    }
    return (uint32_t)(p - base);
}

uint32_t gfx_arena_node_index(const gfx_arena_t *a, const gfx_arena_node_t *n)
{
    if (a == NULL || a->base == NULL || n == NULL) {
        return GFX_ARENA_NO_NODE;
    }
    const gfx_arena_hdr_t *hdr = (const gfx_arena_hdr_t *)a->base;
    const uintptr_t base = (uintptr_t)(a->base + hdr->nodes_off);
    const uintptr_t p = (uintptr_t)n;
    if (p < base) {
        return GFX_ARENA_NO_NODE;
    }
    const uintptr_t delta = p - base;
    if ((delta % sizeof(gfx_arena_node_t)) != 0) {
        return GFX_ARENA_NO_NODE;
    }
    const uint32_t idx = (uint32_t)(delta / sizeof(gfx_arena_node_t));
    if (idx >= hdr->node_count) {
        return GFX_ARENA_NO_NODE;
    }
    return idx;
}

static size_t items_payload_bytes(const gfx_arena_desc_items_t *it)
{
    if (it == NULL || it->items == NULL || it->item_count == 0) {
        return 0;
    }
    size_t n = sizeof(gfx_arena_items_hdr_t);
    for (uint16_t k = 0; k < it->item_count; k++) {
        const char *s = it->items[k];
        const size_t len = (s != NULL) ? strlen(s) : 0u;
        n += 2u + len;
    }
    return n;
}

uint8_t *gfx_arena_pack(const gfx_arena_desc_t *descs, uint16_t count, size_t *out_size)
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
        if (descs[i].type == GFX_ARENA_NODE_IMAGE || descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON) {
            if (descs[i].u.image.file_path != NULL) {
                blob_bytes += sizeof(gfx_arena_img_hdr_t) +
                              strlen(descs[i].u.image.file_path) + 1u;
                if (descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON &&
                        descs[i].u.image.pressed_file_path != NULL) {
                    blob_bytes += strlen(descs[i].u.image.pressed_file_path) + 1u;
                }
            } else if (descs[i].u.image.rgb565 != NULL &&
                       descs[i].u.image.w > 0 && descs[i].u.image.h > 0) {
                const size_t px_bytes =
                    (size_t)descs[i].u.image.w * (size_t)descs[i].u.image.h * sizeof(uint16_t);
                blob_bytes += sizeof(gfx_arena_img_hdr_t) + px_bytes;
                if (descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON &&
                        descs[i].u.image.pressed_rgb565 != NULL) {
                    blob_bytes += px_bytes;
                }
            }
        }
        if (descs[i].type == GFX_ARENA_NODE_LIST || descs[i].type == GFX_ARENA_NODE_WHEEL ||
                descs[i].type == GFX_ARENA_NODE_COVERFLOW ||
                descs[i].type == GFX_ARENA_NODE_PAGEFLOW) {
            blob_bytes += items_payload_bytes(&descs[i].u.items);
        }
        if (descs[i].type == GFX_ARENA_NODE_PROGRESS) {
            blob_bytes += sizeof(gfx_arena_progress_hdr_t);
        }
        if (descs[i].type == GFX_ARENA_NODE_ANIM && descs[i].u.anim.file_path != NULL) {
            blob_bytes += sizeof(gfx_arena_anim_hdr_t) + strlen(descs[i].u.anim.file_path) + 1u;
        }
        if (descs[i].type == GFX_ARENA_NODE_MOTION && descs[i].u.motion.asset_key != NULL) {
            blob_bytes += sizeof(gfx_arena_motion_hdr_t) + strlen(descs[i].u.motion.asset_key) + 1u;
        }
    }

    const size_t nodes_off = sizeof(gfx_arena_hdr_t);
    const size_t str_off = nodes_off + (size_t)count * sizeof(gfx_arena_node_t);
    const size_t blob_off = str_off + str_bytes;
    const size_t total = blob_off + blob_bytes;

    uint8_t *buf = (uint8_t *)calloc(1, total);
    if (buf == NULL) {
        return NULL;
    }

    gfx_arena_hdr_t *hdr = (gfx_arena_hdr_t *)buf;
    hdr->magic = GFX_ARENA_MAGIC;
    hdr->version = GFX_ARENA_VERSION;
    hdr->node_count = count;
    hdr->nodes_off = (uint32_t)nodes_off;
    hdr->str_off = (uint32_t)str_off;
    hdr->total_size = (uint32_t)total;
    hdr->root_off = GFX_ARENA_NO_NODE;

    uint32_t *node_offs = (uint32_t *)calloc(count, sizeof(uint32_t));
    uint32_t *first_child = (uint32_t *)calloc(count, sizeof(uint32_t));
    if (node_offs == NULL || first_child == NULL) {
        free(buf);
        free(node_offs);
        free(first_child);
        return NULL;
    }

    for (uint16_t i = 0; i < count; i++) {
        node_offs[i] = (uint32_t)(nodes_off + (size_t)i * sizeof(gfx_arena_node_t));
        first_child[i] = GFX_ARENA_NO_NODE;
    }

    for (uint16_t i = 0; i < count; i++) {
        const int p = descs[i].parent;
        if (p < 0) {
            if (hdr->root_off == GFX_ARENA_NO_NODE) {
                hdr->root_off = node_offs[i];
            }
            continue;
        }
        assert((uint16_t)p < i);
        if (first_child[p] == GFX_ARENA_NO_NODE) {
            first_child[p] = node_offs[i];
        }
    }

    size_t str_cursor = str_off;
    size_t blob_cursor = blob_off;
    for (uint16_t i = 0; i < count; i++) {
        gfx_arena_node_t *n = (gfx_arena_node_t *)(buf + node_offs[i]);
        n->type = descs[i].type;
        n->flags = descs[i].flags;
        n->x = descs[i].x;
        n->y = descs[i].y;
        n->w = descs[i].w;
        n->h = descs[i].h;
        n->bg_rgb = descs[i].bg_rgb;
        n->first_child = first_child[i];
        n->next_sibling = GFX_ARENA_NO_NODE;
        n->reserved = 0;
        n->name_off = 0;
        if (descs[i].name != NULL) {
            const size_t len = strlen(descs[i].name) + 1u;
            memcpy(buf + str_cursor, descs[i].name, len);
            n->name_off = (uint32_t)str_cursor;
            str_cursor += len;
        }
        if ((descs[i].type == GFX_ARENA_NODE_IMAGE || descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON) &&
                descs[i].u.image.file_path != NULL) {
            gfx_arena_img_hdr_t *ih = (gfx_arena_img_hdr_t *)(buf + blob_cursor);
            size_t path_len = strlen(descs[i].u.image.file_path) + 1u;
            size_t pressed_len = 0;
            ih->w = descs[i].u.image.w;
            ih->h = descs[i].u.image.h;
            ih->format = GFX_ARENA_IMG_FMT_FILE_PATH;
            ih->pad = 0;
            memcpy(buf + blob_cursor + sizeof(gfx_arena_img_hdr_t),
                   descs[i].u.image.file_path, path_len);
            if (descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON &&
                    descs[i].u.image.pressed_file_path != NULL) {
                ih->pad = (uint16_t)(ih->pad | GFX_ARENA_IMG_F_HAS_PRESSED);
                pressed_len = strlen(descs[i].u.image.pressed_file_path) + 1u;
                memcpy(buf + blob_cursor + sizeof(gfx_arena_img_hdr_t) + path_len,
                       descs[i].u.image.pressed_file_path, pressed_len);
            }
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(gfx_arena_img_hdr_t) + path_len + pressed_len;
        } else if ((descs[i].type == GFX_ARENA_NODE_IMAGE || descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON) &&
                   descs[i].u.image.rgb565 != NULL &&
                   descs[i].u.image.w > 0 && descs[i].u.image.h > 0) {
            gfx_arena_img_hdr_t *ih = (gfx_arena_img_hdr_t *)(buf + blob_cursor);
            ih->w = descs[i].u.image.w;
            ih->h = descs[i].u.image.h;
            ih->format = GFX_ARENA_IMG_FMT_RGB565;
            ih->pad = 0;
            const size_t px_bytes =
                (size_t)descs[i].u.image.w * (size_t)descs[i].u.image.h * sizeof(uint16_t);
            memcpy(buf + blob_cursor + sizeof(gfx_arena_img_hdr_t), descs[i].u.image.rgb565, px_bytes);
            if (descs[i].type == GFX_ARENA_NODE_IMAGE_BUTTON &&
                    descs[i].u.image.pressed_rgb565 != NULL) {
                ih->pad = (uint16_t)(ih->pad | GFX_ARENA_IMG_F_HAS_PRESSED);
                memcpy(buf + blob_cursor + sizeof(gfx_arena_img_hdr_t) + px_bytes,
                       descs[i].u.image.pressed_rgb565, px_bytes);
            }
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(gfx_arena_img_hdr_t) + px_bytes +
                           (((ih->pad & GFX_ARENA_IMG_F_HAS_PRESSED) != 0) ? px_bytes : 0u);
        } else if ((descs[i].type == GFX_ARENA_NODE_LIST || descs[i].type == GFX_ARENA_NODE_WHEEL ||
                    descs[i].type == GFX_ARENA_NODE_COVERFLOW ||
                    descs[i].type == GFX_ARENA_NODE_PAGEFLOW) &&
                   descs[i].u.items.items != NULL && descs[i].u.items.item_count > 0) {
            const gfx_arena_desc_items_t *it = &descs[i].u.items;
            gfx_arena_items_hdr_t *ih = (gfx_arena_items_hdr_t *)(buf + blob_cursor);
            ih->item_count = it->item_count;
            ih->selected = it->selected;
            ih->item_height = it->item_height;
            ih->flags = it->flags;
            ih->text_rgb = (it->text_rgb != 0u) ? it->text_rgb : 0xF3F7FAu;
            size_t cur = blob_cursor + sizeof(gfx_arena_items_hdr_t);
            for (uint16_t k = 0; k < it->item_count; k++) {
                const char *s = it->items[k];
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
        } else if (descs[i].type == GFX_ARENA_NODE_PROGRESS) {
            gfx_arena_progress_hdr_t *ph = (gfx_arena_progress_hdr_t *)(buf + blob_cursor);
            uint16_t v = descs[i].u.progress.value;
            if (v > GFX_ARENA_PROGRESS_MAX) {
                v = GFX_ARENA_PROGRESS_MAX;
            }
            ph->value = v;
            ph->pad = descs[i].u.progress.radius;
            ph->track_rgb = (descs[i].u.progress.track_rgb != 0u)
                ? descs[i].u.progress.track_rgb : 0x26313Bu;
            ph->fill_rgb = (descs[i].u.progress.fill_rgb != 0u)
                ? descs[i].u.progress.fill_rgb : 0x2F8CFFu;
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(gfx_arena_progress_hdr_t);
        } else if (descs[i].type == GFX_ARENA_NODE_ANIM && descs[i].u.anim.file_path != NULL) {
            gfx_arena_anim_hdr_t *ah = (gfx_arena_anim_hdr_t *)(buf + blob_cursor);
            size_t path_len = strlen(descs[i].u.anim.file_path) + 1u;
            ah->format = GFX_ARENA_ANIM_FMT_FILE_PATH;
            ah->fps = descs[i].u.anim.fps;
            ah->flags = descs[i].u.anim.flags;
            ah->pad = 0;
            ah->start_frame = descs[i].u.anim.start_frame;
            ah->end_frame = descs[i].u.anim.end_frame;
            memcpy(buf + blob_cursor + sizeof(gfx_arena_anim_hdr_t),
                   descs[i].u.anim.file_path, path_len);
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(gfx_arena_anim_hdr_t) + path_len;
        } else if (descs[i].type == GFX_ARENA_NODE_MOTION && descs[i].u.motion.asset_key != NULL) {
            gfx_arena_motion_hdr_t *mh = (gfx_arena_motion_hdr_t *)(buf + blob_cursor);
            size_t key_len = strlen(descs[i].u.motion.asset_key) + 1u;
            mh->action_idx = descs[i].u.motion.action_idx;
            mh->flags = descs[i].u.motion.flags;
            mh->stroke_rgb = descs[i].u.motion.stroke_rgb;
            memcpy(buf + blob_cursor + sizeof(gfx_arena_motion_hdr_t),
                   descs[i].u.motion.asset_key, key_len);
            n->reserved = (uint32_t)blob_cursor;
            blob_cursor += sizeof(gfx_arena_motion_hdr_t) + key_len;
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
                gfx_arena_node_t *n = (gfx_arena_node_t *)(buf + node_offs[i]);
                n->next_sibling = node_offs[j];
                break;
            }
        }
    }

    uint32_t prev_root = GFX_ARENA_NO_NODE;
    for (uint16_t i = 0; i < count; i++) {
        if (descs[i].parent >= 0) {
            continue;
        }
        if (prev_root != GFX_ARENA_NO_NODE) {
            gfx_arena_node_t *prev = (gfx_arena_node_t *)(buf + prev_root);
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

static bool gfx_arena_pkg_node_off_valid(const gfx_arena_hdr_t *hdr, uint32_t off)
{
    size_t relative;

    if (off == GFX_ARENA_NO_NODE) {
        return true;
    }
    if (off < hdr->nodes_off) {
        return false;
    }
    relative = (size_t)off - hdr->nodes_off;
    return relative % sizeof(gfx_arena_node_t) == 0U &&
           relative / sizeof(gfx_arena_node_t) < hdr->node_count;
}

static bool gfx_arena_pkg_string_valid(const uint8_t *pkg, const gfx_arena_hdr_t *hdr, uint32_t off)
{
    if (off == 0U) {
        return true;
    }
    if (off < hdr->str_off || off >= hdr->total_size) {
        return false;
    }
    return memchr(pkg + off, '\0', (size_t)hdr->total_size - off) != NULL;
}

static bool gfx_arena_pkg_refs_valid(const uint8_t *pkg, const gfx_arena_hdr_t *hdr)
{
    size_t nodes_end = (size_t)hdr->nodes_off +
                       (size_t)hdr->node_count * sizeof(gfx_arena_node_t);

    if (hdr->str_off < nodes_end || !gfx_arena_pkg_node_off_valid(hdr, hdr->root_off)) {
        return false;
    }
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        const gfx_arena_node_t *node = (const gfx_arena_node_t *)(
            pkg + hdr->nodes_off + (size_t)i * sizeof(gfx_arena_node_t));
        if (!gfx_arena_pkg_node_off_valid(hdr, node->first_child) ||
                !gfx_arena_pkg_node_off_valid(hdr, node->next_sibling) ||
                !gfx_arena_pkg_string_valid(pkg, hdr, node->name_off)) {
            return false;
        }
    }
    return true;
}

int gfx_arena_load(const uint8_t *pkg, size_t pkg_size, gfx_arena_t *out)
{
    if (pkg == NULL || out == NULL || pkg_size < sizeof(gfx_arena_hdr_t)) {
        return -1;
    }

    const gfx_arena_hdr_t *hdr = (const gfx_arena_hdr_t *)pkg;
    if (hdr->magic != GFX_ARENA_MAGIC || hdr->version != GFX_ARENA_VERSION) {
        return -2;
    }
    if (hdr->total_size != pkg_size || hdr->nodes_off >= pkg_size || hdr->str_off > pkg_size) {
        return -3;
    }
    if ((size_t)hdr->nodes_off + (size_t)hdr->node_count * sizeof(gfx_arena_node_t) > pkg_size) {
        return -4;
    }
    if (!gfx_arena_pkg_refs_valid(pkg, hdr)) {
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

void gfx_arena_free(gfx_arena_t *a)
{
    if (a == NULL) {
        return;
    }
    free(a->base);
    a->base = NULL;
    a->size = 0;
}

gfx_arena_node_t *gfx_arena_find_by_name(gfx_arena_t *a, const char *name)
{
    if (a == NULL || name == NULL) {
        return NULL;
    }
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(a);
    const uint32_t nodes_off = hdr->nodes_off;
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        uint32_t off = nodes_off + (uint32_t)i * (uint32_t)sizeof(gfx_arena_node_t);
        gfx_arena_node_t *n = gfx_arena_node(a, off);
        const char *s = gfx_arena_str(a, n->name_off);
        if (s != NULL && strcmp(s, name) == 0) {
            return n;
        }
    }
    return NULL;
}

static int abs_area_rec(const gfx_arena_t *a, uint32_t want_off, uint32_t cur_off,
                        int ox, int oy, int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2)
{
    for (uint32_t off = cur_off; off != GFX_ARENA_NO_NODE; ) {
        gfx_arena_node_t *n = gfx_arena_node((gfx_arena_t *)a, off);
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
        if (n->first_child != GFX_ARENA_NO_NODE) {
            if (abs_area_rec(a, want_off, n->first_child, ax, ay, x1, y1, x2, y2) == 0) {
                return 0;
            }
        }
        off = n->next_sibling;
    }
    return -1;
}

int gfx_arena_node_abs_area(const gfx_arena_t *a, uint32_t node_off,
                        int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2)
{
    if (a == NULL || a->base == NULL || node_off == GFX_ARENA_NO_NODE ||
            x1 == NULL || y1 == NULL || x2 == NULL || y2 == NULL) {
        return -1;
    }
    const gfx_arena_hdr_t *hdr = (const gfx_arena_hdr_t *)a->base;
    return abs_area_rec(a, node_off, hdr->root_off, 0, 0, x1, y1, x2, y2);
}

void gfx_arena_dump_tree(gfx_arena_t *a, uint32_t node_off, int depth)
{
    for (uint32_t off = node_off; off != GFX_ARENA_NO_NODE; ) {
        gfx_arena_node_t *n = gfx_arena_node(a, off);
        const char *name = gfx_arena_str(a, n->name_off);
        for (int i = 0; i < depth; i++) {
            fputs("  ", stdout);
        }
        printf("off=%u type=%u flags=0x%x rect=(%d,%d %ux%u) bg=#%06x name=%s\n",
               (unsigned)off, (unsigned)n->type, (unsigned)n->flags,
               (int)n->x, (int)n->y, (unsigned)n->w, (unsigned)n->h,
               (unsigned)(n->bg_rgb & 0xFFFFFFu),
               name != NULL ? name : "-");
        if (n->first_child != GFX_ARENA_NO_NODE) {
            gfx_arena_dump_tree(a, n->first_child, depth + 1);
        }
        off = n->next_sibling;
    }
}
