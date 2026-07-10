/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * GSP1 (v4) → ARN1 materializer.
 *
 * Converts container / layer / label / button / image / list / wheel.
 * Image: RGB565 STORE/RLE16 blobs only (other cf/codec skipped, node kept empty).
 * List/wheel: GSP item params → gfx_arena_items_hdr_t (static draw + tap select).
 * Button callback_off → arena action name (reserved).
 * Label text_off → arena name_off; fg_color → bg_rgb (text color).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GFX_GSP_TO_ARENA_OK = 0,
    GFX_GSP_TO_ARENA_ERR_ARG = -1,
    GFX_GSP_TO_ARENA_ERR_MAGIC = -2,
    GFX_GSP_TO_ARENA_ERR_VERSION = -3,
    GFX_GSP_TO_ARENA_ERR_SIZE = -4,
    GFX_GSP_TO_ARENA_ERR_EMPTY = -5, /* no convertible objects */
    GFX_GSP_TO_ARENA_ERR_ALLOC = -6,
    GFX_GSP_TO_ARENA_ERR_PACK = -7,
};

typedef struct {
    uint16_t screen_w;
    uint16_t screen_h;
    uint32_t screen_bg;     /* RGB888 */
    uint16_t src_obj_count; /* GSP objects */
    uint16_t out_node_count;/* arena nodes emitted */
    uint16_t skipped;       /* unsupported GSP objects */
} gfx_gsp_to_arena_info_t;

/**
 * @brief Materialize GSP bytes into a newly allocated ARN1 package
 * @param gsp GSP package bytes
 * @param gfx_gsp_size GSP length in bytes
 * @param out_pkg Receives allocated ARN package; caller frees with free()
 * @param out_pkg_size Receives ARN package size
 * @param info Optional conversion stats; may be NULL
 * @return GFX_GSP_TO_ARENA_OK on success, GFX_GSP_TO_ARENA_ERR_* otherwise
 */
int gfx_gsp_to_arena(const uint8_t *gsp, size_t gfx_gsp_size,
                 uint8_t **out_pkg, size_t *out_pkg_size,
                 gfx_gsp_to_arena_info_t *info);

#ifdef __cplusplus
}
#endif
