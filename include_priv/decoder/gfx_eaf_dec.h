/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gfx_eaf_dec.h
 * @brief EAF (Emote Animation Format) Decoder
 * 
 * This module provides functionality for decoding EAF format files, including:
 * - File format parsing and validation
 * - Frame data extraction and management
 * - Multiple encoding format support (RLE, Huffman, JPEG)
 * - Color palette handling
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "core/gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *  FILE FORMAT DEFINITIONS
 **********************/

/*
 * EAF File Format Structure:
 *
 * Offset  Size    Description
 * 0       1       Magic number (0x89)
 * 1       3       Format string ("EAF")
 * 4       4       Total number of frames
 * 8       4       Checksum of table + data
 * 12      4       Length of table + data
* 16      N       Frame table (N = total_frames * 8)
* 16+N    M       Frame data (M = sum of all frame sizes)
 */

/* Magic numbers and identifiers */
#define GFX_EAF_MAGIC_HEAD          0x5A5A
#define GFX_EAF_MAGIC_LEN           2
#define GFX_EAF_FORMAT_MAGIC        0x89
#define GFX_EAF_FORMAT_STR          "EAF"
#define GFX_AAF_FORMAT_STR          "AAF"

/* File structure offsets */
#define GFX_EAF_FORMAT_OFFSET       0
#define GFX_EAF_STR_OFFSET          1
#define GFX_EAF_NUM_OFFSET          4
#define GFX_EAF_CHECKSUM_OFFSET     8
#define GFX_EAF_TABLE_LEN           12
#define GFX_EAF_TABLE_OFFSET        16

/**********************
 *  INTERNAL STRUCTURES
 **********************/

/**
 * @brief Frame table entry structure
 */
#pragma pack(1)
typedef struct {
    uint32_t frame_size;          /*!< Size of the frame */
    uint32_t frame_offset;        /*!< Offset of the frame */
} frame_table_entry_t;
#pragma pack()

/**
 * @brief Frame entry with memory and table information
 */
typedef struct {
    const char *frame_mem;
    const frame_table_entry_t *table;
} frame_entry_t;

/**
 * @brief EAF format context structure
 */
typedef struct {
    frame_entry_t *entries;
    int total_frames;
} gfx_eaf_format_ctx_t;

/**********************
 *  PUBLIC TYPES
 **********************/

/**
 * @brief EAF format type enumeration
 */
typedef enum {
    GFX_EAF_FORMAT_VALID = 0,      /*!< Valid EAF format with split BMP data */
    GFX_EAF_FORMAT_REDIRECT = 1,    /*!< Redirect format pointing to another file */
    GFX_EAF_FORMAT_INVALID = 2      /*!< Invalid or unsupported format */
} gfx_eaf_format_t;

/**
 * @brief EAF encoding type enumeration
 */
typedef enum {
    GFX_EAF_ENCODING_RLE = 0,           /*!< Run-Length Encoding */
    GFX_EAF_ENCODING_HUFFMAN = 1,       /*!< Huffman encoding with RLE */
    GFX_EAF_ENCODING_JPEG = 2,          /*!< JPEG encoding */
    GFX_EAF_ENCODING_HUFFMAN_DIRECT = 3, /*!< Direct Huffman encoding without RLE */
    GFX_EAF_ENCODING_MAX                /*!< Maximum number of encoding types */
} gfx_eaf_encoding_t;

/**
 * @brief EAF image header structure
 */
typedef struct {
    char format[3];        /*!< Format identifier (e.g., "_S") */
    char version[6];       /*!< Version string */
    uint8_t bit_depth;     /*!< Bit depth (4, 8, or 24) */
    uint16_t width;        /*!< Image width in pixels */
    uint16_t height;       /*!< Image height in pixels */
    uint16_t blocks;       /*!< Number of blocks */
    uint16_t block_height; /*!< Height of each block */
    uint32_t *block_len;   /*!< Data length of each block */
    uint16_t data_offset;  /*!< Offset to data segment */
    uint8_t *palette;      /*!< Color palette (dynamically allocated) */
    int num_colors;        /*!< Number of colors in palette */
} gfx_eaf_header_t;

/**
 * @brief Huffman tree node structure
 */
typedef struct HuffmanNode {
    uint8_t is_leaf;              /*!< Whether this node is a leaf node */
    uint8_t symbol;               /*!< Symbol value for leaf nodes */
    struct HuffmanNode* left;     /*!< Left child node */
    struct HuffmanNode* right;    /*!< Right child node */
} HuffmanNode;

/**
 * @brief EAF format parser handle
 */
typedef void *gfx_eaf_format_handle_t;

/**********************
 *  HEADER OPERATIONS
 **********************/

/**
 * @brief Parse the header of an EAF file
 * @param file_data Pointer to the image file data
 * @param file_size Size of the image file data
 * @param header Pointer to store the parsed header information
 * @return Image format type (VALID, REDIRECT, or INVALID)
 */
gfx_eaf_format_t gfx_eaf_header_parse(const uint8_t *file_data, size_t file_size, gfx_eaf_header_t *header);

/**
 * @brief Free resources allocated for EAF header
 * @param header Pointer to the header structure
 */
void gfx_eaf_free_header(gfx_eaf_header_t *header);

/**
 * @brief Calculate block offsets from header information
 * @param header Pointer to the header structure
 * @param offsets Array to store calculated offsets
 */
void gfx_eaf_calculate_offsets(const gfx_eaf_header_t *header, uint32_t *offsets);

/**********************
 *  COLOR OPERATIONS
 **********************/

/**
 * @brief Get color from palette at specified index
 * @param header Pointer to the header structure containing palette
 * @param color_index Index in the palette
 * @param swap_bytes Whether to swap color bytes
 * @return Color value in RGB565 format
 */
gfx_color_t gfx_eaf_palette_get_color(const gfx_eaf_header_t *header, uint8_t color_index, bool swap_bytes);

/**********************
 *  COMPRESSION OPERATIONS
 **********************/

/**
 * @brief Function pointer type for block decoders
 * @param input_data Input compressed data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes (only used by JPEG decoder)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
typedef esp_err_t (*block_decoder_t)(const uint8_t *input_data, size_t input_size,
                                    uint8_t *output_buffer, size_t *out_size,
                                    bool swap_color);

/**
 * @brief Decode RLE compressed data
 * @param compressed_data Input compressed data
 * @param compressed_size Size of compressed data
 * @param decompressed_data Output buffer for decompressed data
 * @param decompressed_size Size of output buffer
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_rle_decode(const uint8_t *input_data, size_t input_size,
                            uint8_t *output_buffer, size_t *out_size,
                            bool swap_color);

/**
 * @brief Decode Huffman compressed data
 * @param input_data Input compressed data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes (unused)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
                                uint8_t *output_buffer, size_t *out_size,
                                bool swap_color);

/**
 * @brief Decode JPEG compressed data
 * @param input_data Input JPEG data
 * @param input_size Size of input data
 * @param output_buffer Output buffer for decoded data
 * @param out_size Size of output buffer
 * @param swap_color Whether to swap color bytes
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_jpeg_decode(const uint8_t *input_data, size_t input_size,
                             uint8_t *output_buffer, size_t *out_size,
                             bool swap_color);

/**********************
 *  FRAME OPERATIONS
 **********************/

/**
 * @brief Decode a block of EAF data
 * @param header EAF header information
 * @param frame_data Pointer to the frame data
 * @param block_index Index of the block to decode
 * @param decode_buffer Buffer to store decoded data
 * @param swap_color Whether to swap color bytes
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_block_decode(const gfx_eaf_header_t *header, const uint8_t *frame_data, 
                              int block_index, uint8_t *decode_buffer, bool swap_color);

/**********************
 *  FORMAT OPERATIONS
 **********************/

/**
 * @brief Initialize EAF format parser
 * @param data Pointer to EAF file data
 * @param data_len Length of EAF file data
 * @param ret_parser Pointer to store the parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_format_init(const uint8_t *data, size_t data_len, gfx_eaf_format_handle_t *ret_parser);

/**
 * @brief Deinitialize EAF format parser
 * @param handle Parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t gfx_eaf_format_deinit(gfx_eaf_format_handle_t handle);

/**
 * @brief Get total number of frames in EAF file
 * @param handle Parser handle
 * @return Total number of frames
 */
int gfx_eaf_format_get_total_frames(gfx_eaf_format_handle_t handle);

/**
 * @brief Get frame data at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Pointer to frame data, NULL on failure
 */
const uint8_t *gfx_eaf_format_get_frame_data(gfx_eaf_format_handle_t handle, int index);

/**
 * @brief Get frame size at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Frame size in bytes, -1 on failure
 */
int gfx_eaf_format_get_frame_size(gfx_eaf_format_handle_t handle, int index);

#ifdef __cplusplus
}
#endif