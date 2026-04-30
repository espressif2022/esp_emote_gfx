/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "core/gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      DEFINES
 **********************/

#define EAF_MAGIC_HEAD          0x5A5A
#define EAF_MAGIC_LEN           2
#define EAF_FORMAT_MAGIC        0x89
#define EAF_FORMAT_STR          "EAF"
#define AAF_FORMAT_STR          "AAF"

#define EAF_FORMAT_OFFSET       0
#define EAF_STR_OFFSET          1
#define EAF_NUM_OFFSET          4
#define EAF_CHECKSUM_OFFSET     8
#define EAF_TABLE_LEN           12
#define EAF_TABLE_OFFSET        16

#define EAF_COLOR_DEPTH_4BIT    4
#define EAF_COLOR_DEPTH_8BIT    8
#define EAF_COLOR_DEPTH_24BIT   24

/**********************
 *      TYPEDEFS
 **********************/

#pragma pack(1)
typedef struct {
    uint32_t frame_size;          /*!< Size of the frame */
    uint32_t frame_offset;        /*!< Offset of the frame */
} eaf_dec_frame_table_entry_t;
#pragma pack()

typedef struct {
    const char *frame_mem;
    const eaf_dec_frame_table_entry_t *table;
} eaf_dec_frame_entry_t;

typedef struct {
    eaf_dec_frame_entry_t *entries;
    int total_frames;
} eaf_dec_ctx_t;

typedef enum {
    EAF_DEC_TYPE_VALID = 0,      /*!< Valid EAF format with split BMP data */
    EAF_DEC_TYPE_REDIRECT = 1,    /*!< Redirect format pointing to another file */
    EAF_DEC_TYPE_INVALID = 2,      /*!< Invalid or unsupported format */
    EAF_DEC_TYPE_FLAG = 3         /*!< Invalid format */
} eaf_dec_type_t;

typedef enum {
    EAF_DEC_ENCODING_RLE = 0,           /*!< Run-Length Encoding */
    EAF_DEC_ENCODING_HUFFMAN = 1,       /*!< Huffman encoding with RLE */
    EAF_DEC_ENCODING_JPEG = 2,          /*!< JPEG encoding */
    EAF_DEC_ENCODING_HUFFMAN_DIRECT = 3, /*!< Direct Huffman encoding without RLE */
    EAF_DEC_ENCODING_HEATSHRINK = 4,    /*!< Heatshrink encoding */
    EAF_DEC_ENCODING_RAW = 5,           /*!< Raw (uncompressed) */
    EAF_DEC_ENCODING_MAX                /*!< Maximum number of encoding types */
} eaf_dec_encoding_type_t;

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
} eaf_dec_header_t;

typedef struct eaf_dec_huffman_node {
    uint8_t is_leaf;              /*!< Whether this node is a leaf node */
    uint8_t symbol;               /*!< Symbol value for leaf nodes */
    struct eaf_dec_huffman_node *left;     /*!< Left child node */
    struct eaf_dec_huffman_node *right;    /*!< Right child node */
} eaf_dec_huffman_node_t;

/**
 * @brief EAF format parser handle
 */
typedef void *eaf_dec_handle_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Probe the header of an EAF file
 * @param handle Parser handle
 * @param frame_index Frame index
 * @return Image format type (VALID, FLAG, or INVALID)
 */
eaf_dec_type_t eaf_dec_probe_frame_info(eaf_dec_handle_t handle, int frame_index);

/**
 * @brief Parse the header of an EAF file
 * @param handle Parser handle
 * @param frame_index Frame index
 * @param header Pointer to store the parsed header information
 * @return Image format type (VALID, REDIRECT, or INVALID)
 */
eaf_dec_type_t eaf_dec_get_frame_info(eaf_dec_handle_t handle, int frame_index, eaf_dec_header_t *header);

/**
 * @brief Free resources allocated for EAF header
 * @param header Pointer to the header structure
 */
void eaf_dec_free_header(eaf_dec_header_t *header);

/**
 * @brief Calculate block offsets from header information
 * @param header Pointer to the header structure
 * @param offsets Array to store calculated offsets
 */
void eaf_dec_calculate_offsets(const eaf_dec_header_t *header, uint32_t *offsets);

/**********************
 *  COLOR OPERATIONS
 **********************/

/**
 * @brief Get color from palette at specified index
 * @param header Pointer to the header structure containing palette
 * @param color_index Index in the palette
 * @param swap_bytes Whether the returned RGB565 value should be converted to
 *        native framebuffer byte order for the current draw target
 * @param result Output parameter for semantic/native RGB565 color value
 * @return true if color is fully transparent (00 00 00 00), false otherwise
 */
bool eaf_dec_get_palette_color(const eaf_dec_header_t *header, uint8_t color_index, bool swap_bytes, gfx_color_t *result);

/**********************
 *  COMPRESSION OPERATIONS
 **********************/

typedef esp_err_t (*eaf_dec_block_decoder_cb_t)(const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size,
        bool swap_color);

/**
 * @brief Decode RLE compressed data
 * @param in_data Input compressed data
 * @param in_size Size of compressed data
 * @param out_data Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_rle(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size,
                             bool swap_color);

/**
 * @brief Decode Huffman compressed data
 * @param in_data Input compressed data
 * @param in_size Size of input data
 * @param out_data Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether decoded RGB565 data should be written in native
 *        framebuffer byte order for the current draw target (unused here)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_huffman(const uint8_t *in_data, size_t in_size,
                                 uint8_t *out_data, size_t *out_size,
                                 bool swap_color);

#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
/**
 * @brief Decode heatshrink compressed data
 * @param in_data Input compressed data
 * @param in_size Size of input data
 * @param out_data Output buffer for decompressed data
 * @param out_size Size of output buffer
 * @param swap_color Whether decoded RGB565 data should be written in native
 *        framebuffer byte order for the current draw target (unused here)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_heatshrink(const uint8_t *in_data, size_t in_size,
                                    uint8_t *out_data, size_t *out_size,
                                    bool swap_color);
#endif // CONFIG_GFX_EAF_HEATSHRINK_SUPPORT

/**
 * @brief Decode raw (uncompressed) data
 * @param in_data Input data
 * @param in_size Size of input data
 * @param out_data Output buffer for data
 * @param out_size Size of output buffer
 * @param swap_color Whether decoded RGB565 data should be written in native
 *        framebuffer byte order for the current draw target (unused here)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_raw(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size,
                             bool swap_color);

#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
/**
 * @brief Decode JPEG compressed data
 * @param in_data Input JPEG data
 * @param in_size Size of input data
 * @param out_data Output buffer for decoded data
 * @param out_size Size of output buffer
 * @param swap_color Whether decoded RGB565 data should be written in native
 *        framebuffer byte order for the current draw target
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_jpeg(const uint8_t *in_data, size_t in_size,
                              uint8_t *out_data, size_t *out_size, bool swap_color);
#endif // CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT

/**********************
 *  FRAME OPERATIONS
 **********************/

/**
 * @brief Decode a block of EAF data
 * @param header EAF header information
 * @param block_data Pointer to the block data
 * @param block_len Length of the block
 * @param out_data Buffer to store decoded data
 * @param swap_color Whether decoded RGB565 data should be written in native
 *        framebuffer byte order for the current draw target
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_block(const eaf_dec_header_t *header, const uint8_t *block_data,
                               int block_len, uint8_t *out_data, bool swap_color);

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
esp_err_t eaf_dec_init(const uint8_t *data, size_t data_len, eaf_dec_handle_t *ret_parser);

/**
 * @brief Deinitialize EAF format parser
 * @param handle Parser handle
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_deinit(eaf_dec_handle_t handle);

/**
 * @brief Get total number of frames in EAF file
 * @param handle Parser handle
 * @return Total number of frames
 */
int eaf_dec_get_total_frames(eaf_dec_handle_t handle);

/**
 * @brief Get frame data at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Pointer to frame data, NULL on failure
 */
const uint8_t *eaf_dec_get_frame_data(eaf_dec_handle_t handle, int index);

/**
 * @brief Get frame size at specified index
 * @param handle Parser handle
 * @param index Frame index
 * @return Frame size in bytes, -1 on failure
 */
int eaf_dec_get_frame_size(eaf_dec_handle_t handle, int index);

/**
 * @brief Decode a full EAF frame
 * @param handle Format handle
 * @param frame_index Index of the frame to decode
 * @param out_data Output buffer for decoded frame
 * @param out_size Size of output buffer
 * @param swap_bytes Whether to swap color bytes
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t eaf_dec_decode_frame(eaf_dec_handle_t handle, int frame_index,
                               uint8_t *out_data, size_t out_size,
                               bool swap_bytes);

#ifdef __cplusplus
}
#endif
