/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Arena scene package ABI (ARN1) and runtime helpers.
 *
 * Frozen layout rules:
 * - Little-endian, packed structs, no native pointers in the package.
 * - Runtime arena is a writable RAM copy (arena_load memcpy).
 * - Incompatible layout changes require ARENA_VERSION bump.
 *
 * Dual backend:
 * - Package UI  -> arena (this API)
 * - Hand-written UI -> gfx_*_create / gfx_object_t (unchanged)
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARENA_MAGIC   0x314E5241u /* 'ARN1' LE */
#define ARENA_VERSION 1u
#define ARENA_NO_NODE 0xFFFFFFFFu

#define ARENA_NODE_CONTAINER 1u
#define ARENA_NODE_LABEL     2u
#define ARENA_NODE_BUTTON    3u
#define ARENA_NODE_IMAGE     4u
#define ARENA_NODE_LIST      5u
#define ARENA_NODE_WHEEL     6u

#define ARENA_F_VISIBLE   (1u << 0)
#define ARENA_F_BG        (1u << 1)
#define ARENA_F_CLICKABLE (1u << 2)
#define ARENA_F_PRESSED   (1u << 3)
/*
 * Node text is runtime-bound (GSP `bind`): excluded from baked static
 * layers (ARENA_DRAW_MODE_BAKE) and drawn by the overlay pass instead.
 * Zero in v1 packages keeps text static — no ARENA_VERSION bump needed.
 */
#define ARENA_F_TEXT_DYN  (1u << 4)

/*
 * flags bits 8..15 carry an optional per-node corner radius in pixels.
 * 0 keeps the type default (BUTTON: 6, others: square), so v1 packages
 * with these bits zero render unchanged. No ARENA_VERSION bump needed.
 */
#define ARENA_F_RADIUS_SHIFT 8u
#define ARENA_F_RADIUS_MASK  (0xFFu << ARENA_F_RADIUS_SHIFT)

/** Image blob pixel format (arena_img_hdr_t.format). */
#define ARENA_IMG_FMT_RGB565 0u

/** List/wheel items blob flags (arena_items_hdr_t.flags). */
#define ARENA_ITEMS_F_CYCLIC       (1u << 0)
#define ARENA_ITEMS_SELECTED_NONE  0xFFFFu

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t node_count;
    uint32_t nodes_off;
    uint32_t str_off;
    uint32_t total_size;
    uint32_t root_off;
} arena_hdr_t;

/**
 * Fixed 32-byte node. Relative offsets only.
 *
 * Conventions (v1):
 * - LABEL: name_off = text; bg_rgb = text color
 * - BUTTON: name_off = caption; bg_rgb = fill; reserved = action name_off
 * - IMAGE: reserved = offset of arena_img_hdr_t (+ pixels); w/h = draw box
 * - LIST/WHEEL: name_off = widget name; bg_rgb = panel fill;
 *   reserved = offset of arena_items_hdr_t (+ length-prefixed UTF-8 items)
 */
typedef struct {
    uint16_t type;
    uint16_t flags;
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint32_t bg_rgb;
    uint32_t name_off;
    uint32_t first_child;
    uint32_t next_sibling;
    uint32_t reserved;
} arena_node_t;

/** Prefixed to RGB565 pixel payload in the package. */
typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t format; /* ARENA_IMG_FMT_* */
    uint16_t pad;
} arena_img_hdr_t;

/**
 * Prefixed to list/wheel item payload.
 * Items follow as: repeated (u16 byte_len + byte_len UTF-8 bytes), no NUL required.
 * selected is runtime-mutable in the RAM arena copy.
 */
typedef struct {
    uint16_t item_count;
    uint16_t selected;     /* ARENA_ITEMS_SELECTED_NONE or index */
    uint16_t item_height;  /* 0 = draw default (24) */
    uint16_t flags;        /* ARENA_ITEMS_F_* */
    uint32_t text_rgb;     /* RGB888 item text color */
} arena_items_hdr_t;
#pragma pack(pop)

/** Per-node corner radius from flags bits 8..15; 0 = type default. */
static inline uint8_t arena_node_radius(const arena_node_t *n)
{
    return (uint8_t)((n->flags & ARENA_F_RADIUS_MASK) >> ARENA_F_RADIUS_SHIFT);
}

typedef struct {
    uint8_t *base;
    size_t   size;
} arena_t;

typedef struct {
    uint16_t type;
    uint16_t flags;
    int16_t  x, y;
    uint16_t w, h;
    uint32_t bg_rgb;
    const char *name;
    int      parent;
    const char *action; /* BUTTON: action name; ignored for IMAGE/LIST/WHEEL */
    uint8_t  radius;    /* corner radius px; 0 = type default (BUTTON 6, others square) */
    /** IMAGE: optional RGB565 pixels (w*h*2 bytes). Packed after string table. */
    const uint16_t *img_rgb565;
    uint16_t img_w;
    uint16_t img_h;
    /** LIST/WHEEL: optional item strings (copied into package). */
    const char *const *items;
    uint16_t item_count;
    uint16_t selected;
    uint16_t item_height;
    uint16_t items_flags;
    uint32_t text_rgb;
} arena_desc_t;

uint8_t *arena_pack(const arena_desc_t *descs, uint16_t count, size_t *out_size);
int arena_load(const uint8_t *pkg, size_t pkg_size, arena_t *out);
void arena_free(arena_t *a);

arena_hdr_t *arena_hdr(arena_t *a);
arena_node_t *arena_node(arena_t *a, uint32_t off);
const char *arena_str(arena_t *a, uint32_t off);
const arena_img_hdr_t *arena_img(arena_t *a, uint32_t off);
const uint16_t *arena_img_pixels(arena_t *a, uint32_t off);
const arena_items_hdr_t *arena_items(arena_t *a, uint32_t off);
arena_items_hdr_t *arena_items_mut(arena_t *a, uint32_t off);
/** Item UTF-8 bytes; out_len set; returns NULL if bad index. Not NUL-terminated. */
const char *arena_item_text(arena_t *a, uint32_t items_off, uint16_t index, uint16_t *out_len);
arena_node_t *arena_find_by_name(arena_t *a, const char *name);
uint32_t arena_node_offset(const arena_t *a, const arena_node_t *n);
uint32_t arena_node_index(const arena_t *a, const arena_node_t *n);
void arena_dump_tree(arena_t *a, uint32_t node_off, int depth);

/** Inclusive abs rect; returns 0 on success. */
int arena_node_abs_area(const arena_t *a, uint32_t node_off,
                        int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2);

#ifdef __cplusplus
}
#endif
