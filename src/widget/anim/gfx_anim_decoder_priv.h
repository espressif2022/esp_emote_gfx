/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "widget/gfx_anim.h"

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
    int num_colors;
} gfx_anim_frame_desc_t;

typedef struct gfx_anim_decoder_ops {
    const char *name;
    bool (*can_open)(const gfx_anim_src_t *src_desc);
    esp_err_t (*open)(const gfx_anim_src_t *src_desc, void **out_handle);
    void (*close)(void *handle);
    int (*get_total_frames)(void *handle);
    esp_err_t (*get_frame_info)(void *handle, int frame_index, gfx_anim_frame_desc_t *frame_desc);
    void (*free_frame_info)(gfx_anim_frame_desc_t *frame_desc);
    const uint8_t *(*get_frame_data)(void *handle, int frame_index);
    int (*get_frame_size)(void *handle, int frame_index);
    /* decode_block writes RGB565 blocks in native framebuffer order when requested */
    esp_err_t (*decode_block)(const gfx_anim_frame_desc_t *frame_desc, const uint8_t *block_data,
                              int block_len, uint8_t *decode_buffer, bool swap_color);
    /* get_palette_color returns a semantic/native RGB565 color for the target draw path */
    bool (*get_palette_color)(const gfx_anim_frame_desc_t *frame_desc, uint8_t color_index,
                              bool swap_bytes, gfx_color_t *result);
} gfx_anim_decoder_ops_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

esp_err_t gfx_anim_decoder_registry_init(void);
const gfx_anim_decoder_ops_t *gfx_anim_decoder_find_for_source(const gfx_anim_src_t *src_desc);
const gfx_anim_decoder_ops_t *gfx_anim_decoder_get_eaf(void);

#ifdef __cplusplus
}
#endif
