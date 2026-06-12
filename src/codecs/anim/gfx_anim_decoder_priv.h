/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "gfx/widgets/anim.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint8_t bit_depth;
    uint16_t width;
    uint16_t height;
    uint16_t blocks;
    uint16_t block_height;
    uint32_t *block_len;
    uint16_t data_offset;
    uint8_t *palette;
    uint16_t num_colors;
} gfx_anim_frame_desc_t;

typedef struct gfx_anim_decoder {
    const char *name;
    bool (*probe)(const gfx_anim_src_t *src);
    esp_err_t (*open)(const gfx_anim_src_t *src, void **out_ctx);
    void (*close)(void *ctx);
    uint32_t (*get_frame_count)(void *ctx);
    esp_err_t (*read_frame_desc)(void *ctx, uint32_t frame_index, gfx_anim_frame_desc_t *frame_desc);
    void (*free_frame_desc)(gfx_anim_frame_desc_t *frame_desc);
    const uint8_t *(*get_frame_payload)(void *ctx, uint32_t frame_index);
    size_t (*get_frame_payload_size)(void *ctx, uint32_t frame_index);
    /* decode_frame_block writes RGB565 blocks in native framebuffer order when requested */
    esp_err_t (*decode_frame_block)(const gfx_anim_frame_desc_t *frame_desc, const uint8_t *block_payload,
                                    size_t block_payload_size, uint8_t *decode_buffer, bool swap_color);
    /* read_palette_color returns a semantic/native RGB565 color for the target draw path */
    bool (*read_palette_color)(const gfx_anim_frame_desc_t *frame_desc, uint8_t color_index,
                               bool swap_bytes, gfx_color_t *result);
} gfx_anim_decoder_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

esp_err_t gfx_anim_decoder_registry_init(void);
const gfx_anim_decoder_t *gfx_anim_decoder_select(const gfx_anim_src_t *src);
const gfx_anim_decoder_t *gfx_anim_eaf_decoder_get(void);

#ifdef __cplusplus
}
#endif
