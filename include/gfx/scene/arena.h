/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Arena scene package ABI (ARN1) and runtime helpers.
 *
 * Experimental runtime API. Versioned package layout rules:
 * - Little-endian, packed structs, no native pointers in the package.
 * - Runtime arena is a writable RAM copy (gfx_arena_load memcpy).
 * - Incompatible package layout changes require GFX_ARENA_VERSION bump.
 * - C structs used by the runtime are not ABI-stable until Arena graduates.
 *
 * Dual backend:
 * - Package UI  -> arena (this API)
 * - Hand-written UI -> gfx_*_create / gfx_object_t (unchanged)
 *
 * Runtime-only dynamics (list scroll/inertia) live in gfx_arena_scene_t
 * side tables — not in the package blob (no VERSION bump).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_ARENA_MAGIC   0x314E5241u /* 'ARN1' LE */
#define GFX_ARENA_VERSION 1u
#define GFX_ARENA_NO_NODE 0xFFFFFFFFu

#define GFX_ARENA_NODE_CONTAINER     1u
#define GFX_ARENA_NODE_LABEL         2u
#define GFX_ARENA_NODE_BUTTON        3u
#define GFX_ARENA_NODE_IMAGE         4u
#define GFX_ARENA_NODE_LIST          5u
#define GFX_ARENA_NODE_WHEEL         6u
#define GFX_ARENA_NODE_IMAGE_BUTTON  7u
#define GFX_ARENA_NODE_PROGRESS      8u
#define GFX_ARENA_NODE_COVERFLOW     9u
#define GFX_ARENA_NODE_PAGEFLOW      10u
#define GFX_ARENA_NODE_ANIM          11u  /**< Hosted gfx_anim; blob = gfx_arena_anim_hdr_t + path */
#define GFX_ARENA_NODE_MOTION        12u  /**< Hosted gfx_motion_player; blob = gfx_arena_motion_hdr_t + key */

#define GFX_ARENA_F_VISIBLE   (1u << 0)
#define GFX_ARENA_F_BG        (1u << 1)
#define GFX_ARENA_F_CLICKABLE (1u << 2)
#define GFX_ARENA_F_PRESSED   (1u << 3)

/** Anim blob format (gfx_arena_anim_hdr_t.format). */
#define GFX_ARENA_ANIM_FMT_FILE_PATH 0u

/** Anim blob flags (gfx_arena_anim_hdr_t.flags). */
#define GFX_ARENA_ANIM_F_LOOP (1u << 0)

/** Motion blob flags (gfx_arena_motion_hdr_t.flags). */
#define GFX_ARENA_MOTION_F_LOOP (1u << 0)

/** Image blob pixel format (gfx_arena_img_hdr_t.format). */
#define GFX_ARENA_IMG_FMT_RGB565    0u
/**
 * File-path image: after header, one NUL-terminated path (UTF-8).
 * If GFX_ARENA_IMG_F_HAS_PRESSED, a second NUL-terminated path follows.
 * w/h may be 0 in the package; runtime fills from decoder.
 */
#define GFX_ARENA_IMG_FMT_FILE_PATH 1u

/** Image blob flags (gfx_arena_img_hdr_t.pad). */
#define GFX_ARENA_IMG_F_HAS_PRESSED (1u << 0)

/** Sentinel for gfx_arena_scene_bind_* item_index: bind the whole IMAGE/IMAGE_BUTTON node. */
#define GFX_ARENA_IMG_ITEM_NONE 0xFFFFu

/** List/wheel items blob flags (gfx_arena_items_hdr_t.flags). */
#define GFX_ARENA_ITEMS_F_CYCLIC       (1u << 0)
#define GFX_ARENA_ITEMS_F_SNAP         (1u << 1) /* list: snap to item after fling */
#define GFX_ARENA_ITEMS_SELECTED_NONE  0xFFFFu

/** Progress value is permille 0..1000. */
#define GFX_ARENA_PROGRESS_MAX 1000u

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t node_count;
    uint32_t nodes_off;
    uint32_t str_off;
    uint32_t total_size;
    uint32_t root_off;
} gfx_arena_hdr_t;

/**
 * Fixed 32-byte node. Relative offsets only.
 *
 * Conventions (v1):
 * - LABEL: name_off = text; bg_rgb = text color
 * - BUTTON: name_off = caption; bg_rgb = fill; reserved = action name_off
 * - IMAGE / IMAGE_BUTTON: reserved = gfx_arena_img_hdr_t (+ payload);
 *   RGB565: pixels; FILE_PATH: NUL path(s). Runtime may also bind via
 *   gfx_arena_scene_bind_image_src() (gfx_image_resource / gfx_fs / JPEG).
 *   IMAGE_BUTTON name_off = caption; action matched by node name;
 *   IMAGE_BUTTON may append a pressed face (pixels or second path).
 * - LIST/WHEEL/COVERFLOW/PAGEFLOW: reserved = gfx_arena_items_hdr_t (+ items);
 *   PAGEFLOW uses the same flat items blob as COVERFLOW.
 * - PROGRESS: reserved = gfx_arena_progress_hdr_t; name_off = widget name;
 *   action matched by node name; bg_rgb unused
 * - ANIM: reserved = gfx_arena_anim_hdr_t (+ FILE_PATH); playback in scene side table
 * - MOTION: reserved = gfx_arena_motion_hdr_t (+ asset_key string); asset bound at runtime
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
} gfx_arena_node_t;

/**
 * Prefixed to image payload in the package; pad carries GFX_ARENA_IMG_F_* flags.
 * Payload depends on format (RGB565 pixels or FILE_PATH strings).
 */
typedef struct {
    uint16_t w;
    uint16_t h;
    uint16_t format; /* GFX_ARENA_IMG_FMT_* */
    uint16_t pad;
} gfx_arena_img_hdr_t;

/**
 * Prefixed to list/wheel item payload.
 * Items follow as: repeated (u16 byte_len + byte_len UTF-8 bytes), no NUL required.
 * selected is runtime-mutable in the RAM arena copy.
 */
typedef struct {
    uint16_t item_count;
    uint16_t selected;     /* GFX_ARENA_ITEMS_SELECTED_NONE or index */
    uint16_t item_height;  /* 0 = draw default (24) */
    uint16_t flags;        /* GFX_ARENA_ITEMS_F_* */
    uint32_t text_rgb;     /* RGB888 item text color */
} gfx_arena_items_hdr_t;

/** Progress bar blob (runtime-mutable value); pad is radius (0 = default), thumb drawn from fill. */
typedef struct {
    uint16_t value;      /* 0..GFX_ARENA_PROGRESS_MAX */
    uint16_t pad;        /* radius in px; 0 = draw default */
    uint32_t track_rgb;
    uint32_t fill_rgb;
} gfx_arena_progress_hdr_t;

/**
 * Prefixed to ANIM payload.
 * format=FILE_PATH → one NUL-terminated path after header.
 * Playback state lives in gfx_arena_scene_t side table.
 */
typedef struct {
    uint16_t format;      /* GFX_ARENA_ANIM_FMT_* */
    uint16_t fps;         /* 0 = default 50 */
    uint16_t flags;       /* GFX_ARENA_ANIM_F_* */
    uint16_t pad;
    uint32_t start_frame;
    uint32_t end_frame;   /* 0xFFFFFFFF = all frames */
} gfx_arena_anim_hdr_t;

/**
 * Prefixed to MOTION payload.
 * Followed by NUL-terminated asset_key (bind key for firmware asset).
 * action_idx is runtime-mutable in the RAM arena copy.
 */
typedef struct {
    uint16_t action_idx;
    uint16_t flags;       /* GFX_ARENA_MOTION_F_* */
    uint32_t stroke_rgb;  /* 0 = default player color */
} gfx_arena_motion_hdr_t;
#pragma pack(pop)

typedef struct {
    uint8_t *base;
    size_t   size;
} gfx_arena_t;

/** Host-side pack helpers (not in package). Type-specific payload in `u`. */
typedef struct {
    const uint16_t *rgb565;           /* RGB565 pack path; NULL if using file_path */
    const uint16_t *pressed_rgb565;   /* NULL = none; same w/h as rgb565 */
    const char     *file_path;        /* FILE_PATH pack; mutually exclusive with rgb565 */
    const char     *pressed_file_path;/* optional second path for IMAGE_BUTTON */
    uint16_t w;
    uint16_t h;
} gfx_arena_desc_image_t;

typedef struct {
    const char *const *items;
    uint16_t item_count;
    uint16_t selected;
    uint16_t item_height;
    uint16_t flags;      /* GFX_ARENA_ITEMS_F_* */
    uint32_t text_rgb;
} gfx_arena_desc_items_t;

typedef struct {
    uint16_t value;      /* 0..GFX_ARENA_PROGRESS_MAX */
    uint16_t radius;     /* 0 = draw default */
    uint32_t track_rgb;
    uint32_t fill_rgb;
} gfx_arena_desc_progress_t;

/** Pack descriptor for GFX_ARENA_NODE_ANIM (FILE_PATH). */
typedef struct {
    const char *file_path;
    uint16_t fps;         /* 0 = default 50 */
    uint16_t flags;       /* GFX_ARENA_ANIM_F_* */
    uint32_t start_frame;
    uint32_t end_frame;   /* 0xFFFFFFFF = all */
} gfx_arena_desc_anim_t;

/** Pack descriptor for GFX_ARENA_NODE_MOTION (asset_key + action). */
typedef struct {
    const char *asset_key; /* firmware bind key, e.g. "claw" */
    uint16_t action_idx;
    uint16_t flags;        /* GFX_ARENA_MOTION_F_* */
    uint32_t stroke_rgb;   /* 0 = default */
} gfx_arena_desc_motion_t;

typedef struct {
    uint16_t type;
    uint16_t flags;
    int16_t  x, y;
    uint16_t w, h;
    uint32_t bg_rgb;
    const char *name;
    int      parent;
    const char *action; /* BUTTON: action name string; else unused */
    union {
        gfx_arena_desc_image_t    image;    /* IMAGE / IMAGE_BUTTON */
        gfx_arena_desc_items_t    items;    /* LIST / WHEEL / COVERFLOW / PAGEFLOW */
        gfx_arena_desc_progress_t progress; /* PROGRESS */
        gfx_arena_desc_anim_t     anim;     /* ANIM */
        gfx_arena_desc_motion_t   motion;   /* MOTION */
    } u;
} gfx_arena_desc_t;

/**
 * @brief Pack host-side node descriptors into an ARN1 package buffer
 * @param descs Descriptor array
 * @param count Number of descriptors
 * @param out_size Receives packed byte size on success
 * @return Newly allocated package buffer, or NULL on failure; caller frees with free()
 */
uint8_t *gfx_arena_pack(const gfx_arena_desc_t *descs, uint16_t count, size_t *out_size);

/**
 * @brief Load an ARN1 package into a writable RAM arena copy
 * @param pkg Package bytes
 * @param pkg_size Package length in bytes
 * @param out Output arena handle
 * @return 0 on success, negative on error
 */
int gfx_arena_load(const uint8_t *pkg, size_t pkg_size, gfx_arena_t *out);

/**
 * @brief Free a loaded arena RAM copy
 * @param a Arena handle; NULL-safe
 */
void gfx_arena_free(gfx_arena_t *a);

/**
 * @brief Get the package header from a loaded arena
 * @param a Arena handle
 * @return Pointer to gfx_arena_hdr_t, or NULL if invalid
 */
gfx_arena_hdr_t *gfx_arena_hdr(gfx_arena_t *a);

/**
 * @brief Resolve a node by byte offset
 * @param a Arena handle
 * @param off Node byte offset, or GFX_ARENA_NO_NODE
 * @return Node pointer, or NULL if off is GFX_ARENA_NO_NODE / invalid
 */
gfx_arena_node_t *gfx_arena_node(gfx_arena_t *a, uint32_t off);

/**
 * @brief Resolve a NUL-terminated string by byte offset
 * @param a Arena handle
 * @param off String table offset; 0 means none
 * @return String pointer, or NULL if off is 0 / invalid
 */
const char *gfx_arena_str(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get an image blob header
 * @param a Arena handle
 * @param off Image blob offset (node->reserved)
 * @return Header pointer, or NULL if out of range
 */
const gfx_arena_img_hdr_t *gfx_arena_img(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get RGB565 pixel payload after an image header
 * @param a Arena handle
 * @param off Image blob offset (node->reserved)
 * @return Pixel pointer, or NULL if not RGB565 / invalid
 */
const uint16_t *gfx_arena_img_pixels(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a FILE_PATH string from an image blob
 * @param a Arena handle
 * @param off Image blob offset (node->reserved)
 * @param face 0 = normal path, 1 = pressed path
 * @return Path string, or NULL if not FILE_PATH / missing / OOB
 */
const char *gfx_arena_img_file_path(gfx_arena_t *a, uint32_t off, uint8_t face);

/**
 * @brief Get a list/wheel/coverflow/pageflow items header
 * @param a Arena handle
 * @param off Items blob offset (node->reserved)
 * @return Header pointer, or NULL if out of range
 */
const gfx_arena_items_hdr_t *gfx_arena_items(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a mutable items header (RAM arena copy)
 * @param a Arena handle
 * @param off Items blob offset (node->reserved)
 * @return Mutable header pointer, or NULL if out of range
 */
gfx_arena_items_hdr_t *gfx_arena_items_mut(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a progress blob header
 * @param a Arena handle
 * @param off Progress blob offset (node->reserved)
 * @return Header pointer, or NULL if out of range
 */
const gfx_arena_progress_hdr_t *gfx_arena_progress(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a mutable progress blob header (RAM arena copy)
 * @param a Arena handle
 * @param off Progress blob offset (node->reserved)
 * @return Mutable header pointer, or NULL if out of range
 */
gfx_arena_progress_hdr_t *gfx_arena_progress_mut(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get an anim blob header
 * @param a Arena handle
 * @param off Anim blob offset (node->reserved)
 * @return Header pointer, or NULL if out of range
 */
const gfx_arena_anim_hdr_t *gfx_arena_anim(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get the FILE_PATH string after an anim header
 * @param a Arena handle
 * @param off Anim blob offset (node->reserved)
 * @return Path string, or NULL if missing / not FILE_PATH
 */
const char *gfx_arena_anim_file_path(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a motion blob header
 * @param a Arena handle
 * @param off Motion blob offset (node->reserved)
 * @return Header pointer, or NULL if out of range
 */
const gfx_arena_motion_hdr_t *gfx_arena_motion(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get a mutable motion blob header (RAM arena copy)
 * @param a Arena handle
 * @param off Motion blob offset (node->reserved)
 * @return Mutable header pointer, or NULL if out of range
 */
gfx_arena_motion_hdr_t *gfx_arena_motion_mut(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get the asset key string after a motion header
 * @param a Arena handle
 * @param off Motion blob offset (node->reserved)
 * @return Asset key string, or NULL if missing
 */
const char *gfx_arena_motion_asset_key(gfx_arena_t *a, uint32_t off);

/**
 * @brief Get one item's UTF-8 text from an items blob
 * @param a Arena handle
 * @param items_off Items blob offset (node->reserved)
 * @param index Item index
 * @param out_len Receives byte length (not including NUL); may be NULL
 * @return Text bytes (not NUL-terminated), or NULL if bad index
 */
const char *gfx_arena_item_text(gfx_arena_t *a, uint32_t items_off, uint16_t index, uint16_t *out_len);

/**
 * @brief Find the first node whose name string equals @p name
 * @param a Arena handle
 * @param name Exact name to match
 * @return Node pointer, or NULL if not found
 */
gfx_arena_node_t *gfx_arena_find_by_name(gfx_arena_t *a, const char *name);

/**
 * @brief Get the byte offset of a node within the arena
 * @param a Arena handle
 * @param n Node pointer from this arena
 * @return Byte offset, or GFX_ARENA_NO_NODE if invalid
 */
uint32_t gfx_arena_node_offset(const gfx_arena_t *a, const gfx_arena_node_t *n);

/**
 * @brief Get the 0-based index of a node in the node table
 * @param a Arena handle
 * @param n Node pointer from this arena
 * @return Node index, or GFX_ARENA_NO_NODE if invalid
 */
uint32_t gfx_arena_node_index(const gfx_arena_t *a, const gfx_arena_node_t *n);

/**
 * @brief Dump a node subtree to stdout for debugging
 * @param a Arena handle
 * @param node_off Root of the subtree to dump
 * @param depth Indentation depth
 */
void gfx_arena_dump_tree(gfx_arena_t *a, uint32_t node_off, int depth);

/**
 * @brief Compute a node's absolute axis-aligned bounds
 * @param a Arena handle
 * @param node_off Node byte offset
 * @param x1 Receives left (inclusive)
 * @param y1 Receives top (inclusive)
 * @param x2 Receives right (exclusive)
 * @param y2 Receives bottom (exclusive)
 * @return 0 on success, negative on error
 */
int gfx_arena_node_abs_area(const gfx_arena_t *a, uint32_t node_off,
                        int16_t *x1, int16_t *y1, int16_t *x2, int16_t *y2);

#ifdef __cplusplus
}
#endif
