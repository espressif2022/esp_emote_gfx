/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "core/gfx_types.h"
#include "decoder/gfx_aaf_format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GFX_AAF_FORMAT_SBMP = 0,      // Split BMP format
    GFX_AAF_FORMAT_REDIRECT = 1,  // Redirect format
    GFX_AAF_FORMAT_INVALID = 2
} gfx_aaf_format_t;

typedef enum {
    GFX_AAF_ENCODING_RLE = 0,
    GFX_AAF_ENCODING_HUFFMAN = 1,
    GFX_AAF_ENCODING_JPEG = 2,
    GFX_AAF_ENCODING_HUFFMAN_DIRECT = 3
} gfx_aaf_encoding_t;

// Image header structure
typedef struct {
    char format[3];        // Format identifier (e.g., "_S")
    char version[6];       // Version string
    uint8_t bit_depth;     // Bit depth (4 or 8)
    uint16_t width;        // Image width
    uint16_t height;       // Image height
    uint16_t blocks;       // Number of blocks
    uint16_t block_height; // Height of each block
    uint32_t *block_len;   // Data length of each block (changed from uint16_t to uint32_t)
    uint16_t data_offset;  // Offset to data segment
    uint8_t *palette;      // Color palette (dynamically allocated)
    int num_colors;        // Number of colors in palette
} gfx_aaf_header_t;

// Huffman tree node structure
typedef struct Node {
    uint8_t is_leaf;
    uint8_t value;
    struct Node* left;
    struct Node* right;
} Node;

/**
 * @brief Parse the header of an image file
 * @param data Pointer to the image data
 * @param data_len Length of the image data
 * @param header Pointer to store the parsed header information
 * @return Image format type (SBMP, REDIRECT, or INVALID)
 */
gfx_aaf_format_t gfx_aaf_parse_header(const uint8_t *data, size_t data_len, gfx_aaf_header_t *header);

gfx_color_t gfx_aaf_parse_palette(const gfx_aaf_header_t *header, uint8_t index, bool swap);

void gfx_aaf_calculate_offsets(const gfx_aaf_header_t *header, uint32_t *offsets);

void gfx_aaf_free_header(gfx_aaf_header_t *header);

esp_err_t gfx_aaf_huffman_decode(const uint8_t* buffer, size_t buflen, uint8_t* output, size_t* output_len);

esp_err_t gfx_aaf_rle_decode(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_len);

#ifdef __cplusplus
}
#endif