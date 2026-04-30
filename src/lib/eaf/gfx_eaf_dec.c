/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_EAF_DEC
#include "common/gfx_log_priv.h"

#include "gfx_eaf_dec.h"
#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
#include "esp_jpeg_dec.h"
#endif

#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
#include "heatshrink_decoder.h"
#endif // CONFIG_GFX_EAF_HEATSHRINK_SUPPORT

/*********************
 *      DEFINES
 *********************/
#define EAF_FRAME_VERSION_OFFSET         (3)
#define EAF_FRAME_BIT_DEPTH_OFFSET       (9)
#define EAF_FRAME_WIDTH_OFFSET           (10)
#define EAF_FRAME_HEIGHT_OFFSET          (12)
#define EAF_FRAME_BLOCKS_OFFSET          (14)
#define EAF_FRAME_BLOCK_HEIGHT_OFFSET    (16)
#define EAF_FRAME_BLOCK_LEN_TABLE_OFFSET (18)
#define EAF_FRAME_BLOCK_LEN_SIZE         (4)
#define EAF_FRAME_PALETTE_ENTRY_SIZE     (4)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "eaf_dec";
static eaf_dec_block_decoder_cb_t s_eaf_decoders[EAF_DEC_ENCODING_MAX] = {0};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static uint32_t dec_calculate_checksum(const uint8_t *data, uint32_t length);
static eaf_dec_huffman_node_t *huffman_node_create(void);
static void huffman_tree_free(eaf_dec_huffman_node_t *node);
static esp_err_t huffman_decode_data(const uint8_t *in_data, size_t in_size,
                                     const uint8_t *dict_data, size_t dict_len,
                                     uint8_t *out_data, size_t *out_size);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint32_t dec_calculate_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

static eaf_dec_huffman_node_t *huffman_node_create(void)
{
    eaf_dec_huffman_node_t *node = (eaf_dec_huffman_node_t *)calloc(1, sizeof(eaf_dec_huffman_node_t));
    return node;
}

static void huffman_tree_free(eaf_dec_huffman_node_t *node)
{
    if (!node) {
        return;
    }
    huffman_tree_free(node->left);
    huffman_tree_free(node->right);
    free(node);
}

static esp_err_t huffman_decode_data(const uint8_t *in_data, size_t in_size,
                                     const uint8_t *dict_data, size_t dict_len,
                                     uint8_t *out_data, size_t *out_size)
{
    if (!in_data || !dict_data || in_size == 0 || dict_len == 0) {
        *out_size = 0;
        return ESP_OK;
    }

    uint8_t padding_bits = dict_data[0];
    size_t dict_pos = 1;

    eaf_dec_huffman_node_t *root = huffman_node_create();
    eaf_dec_huffman_node_t *current_node = NULL;

    while (dict_pos < dict_len) {
        uint8_t symbol = dict_data[dict_pos++];
        uint8_t code_len = dict_data[dict_pos++];

        size_t code_byte_len = (code_len + 7) / 8;
        uint64_t code = 0;
        for (size_t i = 0; i < code_byte_len; ++i) {
            code = (code << 8) | dict_data[dict_pos++];
        }

        current_node = root;
        for (int bit_pos = code_len - 1; bit_pos >= 0; --bit_pos) {
            int bit_val = (code >> bit_pos) & 1;
            if (bit_val == 0) {
                if (!current_node->left) {
                    current_node->left = huffman_node_create();
                }
                current_node = current_node->left;
            } else {
                if (!current_node->right) {
                    current_node->right = huffman_node_create();
                }
                current_node = current_node->right;
            }
        }
        current_node->is_leaf = 1;
        current_node->symbol = symbol;
    }

    size_t total_bits = in_size * 8;
    if (padding_bits > 0) {
        total_bits -= padding_bits;
    }

    current_node = root;
    size_t out_pos = 0;

    for (size_t bit_index = 0; bit_index < total_bits; bit_index++) {
        size_t byte_idx = bit_index / 8;
        int bit_offset = 7 - (bit_index % 8);
        int bit_val = (in_data[byte_idx] >> bit_offset) & 1;

        if (bit_val == 0) {
            current_node = current_node->left;
        } else {
            current_node = current_node->right;
        }

        if (current_node == NULL) {
            GFX_LOGE(TAG, "Invalid Huffman path at bit %d", (int)bit_index);
            break;
        }

        if (current_node->is_leaf) {
            out_data[out_pos++] = current_node->symbol;
            current_node = root;
        }
    }

    *out_size = out_pos;
    huffman_tree_free(root);
    return ESP_OK;
}

eaf_dec_type_t eaf_dec_probe_frame_info(eaf_dec_handle_t handle, int frame_index)
{
    if (!handle) {
        GFX_LOGE(TAG, "Invalid handle");
        return EAF_DEC_TYPE_INVALID;
    }

    const uint8_t *file_data = eaf_dec_get_frame_data(handle, frame_index);
    if (!file_data) {
        GFX_LOGE(TAG, "Frame %d data unavailable", frame_index);
        return EAF_DEC_TYPE_INVALID;
    }

    size_t file_size = eaf_dec_get_frame_size(handle, frame_index);
    if (file_size <= 0) {
        GFX_LOGE(TAG, "Frame %d invalid size", frame_index);
        return EAF_DEC_TYPE_INVALID;
    }
    eaf_dec_header_t header;

    memset(&header, 0, sizeof(eaf_dec_header_t));
    memcpy(header.format, file_data, 2);
    header.format[2] = '\0';

    if (strncmp(header.format, "_S", 2) == 0) {

        memcpy(header.version, file_data + EAF_FRAME_VERSION_OFFSET, 6);

        header.bit_depth = file_data[EAF_FRAME_BIT_DEPTH_OFFSET];

        if (header.bit_depth != EAF_COLOR_DEPTH_4BIT && header.bit_depth != EAF_COLOR_DEPTH_8BIT && header.bit_depth != EAF_COLOR_DEPTH_24BIT) {
            GFX_LOGE(TAG, "Invalid bit depth: %d", header.bit_depth);
            return EAF_DEC_TYPE_INVALID;
        }

        header.width = *(uint16_t *)(file_data + EAF_FRAME_WIDTH_OFFSET);
        header.height = *(uint16_t *)(file_data + EAF_FRAME_HEIGHT_OFFSET);
        header.blocks = *(uint16_t *)(file_data + EAF_FRAME_BLOCKS_OFFSET);
        header.block_height = *(uint16_t *)(file_data + EAF_FRAME_BLOCK_HEIGHT_OFFSET);

        if (header.width == 0 || header.height == 0 || header.blocks == 0 || header.block_height == 0) {
            return EAF_DEC_TYPE_INVALID;
        }
    } else if (strncmp(header.format, "_C", 2) == 0) {
        return EAF_DEC_TYPE_FLAG;
    } else {
        return EAF_DEC_TYPE_INVALID;
    }

    return EAF_DEC_TYPE_VALID;
}

eaf_dec_type_t eaf_dec_get_frame_info(eaf_dec_handle_t handle, int frame_index, eaf_dec_header_t *header)
{
    if (!handle) {
        GFX_LOGE(TAG, "Invalid handle");
        return EAF_DEC_TYPE_INVALID;
    }

    const uint8_t *file_data = eaf_dec_get_frame_data(handle, frame_index);
    if (!file_data) {
        GFX_LOGE(TAG, "Frame %d data unavailable", frame_index);
        return EAF_DEC_TYPE_INVALID;
    }

    size_t file_size = eaf_dec_get_frame_size(handle, frame_index);
    if (file_size <= 0) {
        GFX_LOGE(TAG, "Frame %d invalid size", frame_index);
        return EAF_DEC_TYPE_INVALID;
    }

    memset(header, 0, sizeof(eaf_dec_header_t));

    memcpy(header->format, file_data, 2);
    header->format[2] = '\0';

    if (strncmp(header->format, "_S", 2) == 0) {
        memcpy(header->version, file_data + EAF_FRAME_VERSION_OFFSET, 6);

        header->bit_depth = file_data[EAF_FRAME_BIT_DEPTH_OFFSET];

        if (header->bit_depth != EAF_COLOR_DEPTH_4BIT && header->bit_depth != EAF_COLOR_DEPTH_8BIT && header->bit_depth != EAF_COLOR_DEPTH_24BIT) {
            GFX_LOGE(TAG, "Invalid bit depth: %d", header->bit_depth);
            return EAF_DEC_TYPE_INVALID;
        }

        header->width = *(uint16_t *)(file_data + EAF_FRAME_WIDTH_OFFSET);
        header->height = *(uint16_t *)(file_data + EAF_FRAME_HEIGHT_OFFSET);
        header->blocks = *(uint16_t *)(file_data + EAF_FRAME_BLOCKS_OFFSET);
        header->block_height = *(uint16_t *)(file_data + EAF_FRAME_BLOCK_HEIGHT_OFFSET);

        header->block_len = (uint32_t *)malloc(header->blocks * sizeof(uint32_t));
        if (header->block_len == NULL) {
            GFX_LOGE(TAG, "No mem for block_len");
            return EAF_DEC_TYPE_INVALID;
        }

        for (int i = 0; i < header->blocks; i++) {
            header->block_len[i] = *(uint32_t *)(file_data + EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + i * EAF_FRAME_BLOCK_LEN_SIZE);
        }

        header->num_colors = 1 << header->bit_depth;

        if (header->bit_depth == EAF_COLOR_DEPTH_24BIT) {
            header->num_colors = 0;
            header->palette = NULL;
        } else {
            header->palette = (uint8_t *)malloc(header->num_colors * EAF_FRAME_PALETTE_ENTRY_SIZE);
            if (header->palette == NULL) {
                GFX_LOGE(TAG, "No mem for palette");
                free(header->block_len);
                header->block_len = NULL;
                return EAF_DEC_TYPE_INVALID;
            }

            memcpy(header->palette, file_data + EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + header->blocks * EAF_FRAME_BLOCK_LEN_SIZE, header->num_colors * EAF_FRAME_PALETTE_ENTRY_SIZE);
        }
        header->data_offset = EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + header->blocks * EAF_FRAME_BLOCK_LEN_SIZE + header->num_colors * EAF_FRAME_PALETTE_ENTRY_SIZE;
        return EAF_DEC_TYPE_VALID;

    } else if (strncmp(header->format, "_C", 2) == 0) {
        return EAF_DEC_TYPE_FLAG;
    } else {
        GFX_LOGE(TAG, "Invalid format: %s", header->format);
        return EAF_DEC_TYPE_INVALID;
    }
}

void eaf_dec_free_header(eaf_dec_header_t *header)
{
    if (header->block_len != NULL) {
        free(header->block_len);
        header->block_len = NULL;
    }
    if (header->palette != NULL) {
        free(header->palette);
        header->palette = NULL;
    }
}

void eaf_dec_calculate_offsets(const eaf_dec_header_t *header, uint32_t *offsets)
{
    offsets[0] = header->data_offset;
    for (int i = 1; i < header->blocks; i++) {
        offsets[i] = offsets[i - 1] + header->block_len[i - 1];
    }
}

/**********************
 *  PALETTE FUNCTIONS
 **********************/

bool eaf_dec_get_palette_color(const eaf_dec_header_t *header, uint8_t color_index, bool swap_bytes, gfx_color_t *result)
{
    const uint8_t *color_data = &header->palette[color_index * 4];

    if (color_data[0] == 0 && color_data[1] == 0 && color_data[2] == 0 && color_data[3] == 0) {
        return true;
    }

    gfx_color_t color = {
        .full = (uint16_t)(((color_data[2] & 0xF8) << 8) |
                           ((color_data[1] & 0xFC) << 3) |
                           ((color_data[0] & 0xF8) >> 3)),
    };

    result->full = gfx_color_to_native_u16(color, swap_bytes);
    return false;
}

/**********************
 *  DECODING FUNCTIONS
 **********************/

static esp_err_t decode_huffman_rle(const uint8_t *in_data, size_t in_size,
                                    uint8_t *out_data, size_t *out_size,
                                    bool swap_color)
{
    if (out_size == NULL || *out_size == 0) {
        GFX_LOGE(TAG, "Output size is invalid");
        return ESP_FAIL;
    }

    size_t tmp_size = *out_size * 2;
    uint8_t *tmp_data = malloc(tmp_size);
    if (tmp_data == NULL) {
        GFX_LOGE(TAG, "No mem for tmp buffer");
        return ESP_FAIL;
    }

    size_t tmp_len = tmp_size;
    esp_err_t ret = eaf_dec_decode_huffman(in_data, in_size, tmp_data, &tmp_len, swap_color);
    if (ret == ESP_OK) {
        ret = eaf_dec_decode_rle(tmp_data, tmp_len, out_data, out_size, swap_color);
    }

    free(tmp_data);
    return ret;
}

static esp_err_t register_decoder(eaf_dec_encoding_type_t type, eaf_dec_block_decoder_cb_t decoder)
{
    if (type >= EAF_DEC_ENCODING_MAX) {
        GFX_LOGE(TAG, "Invalid encoding type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_eaf_decoders[type] != NULL) {
        GFX_LOGW(TAG, "Decoder already registered for type: %d", type);
    }

    s_eaf_decoders[type] = decoder;
    return ESP_OK;
}

static esp_err_t init_decoders(void)
{
    esp_err_t ret = ESP_OK;

    ret |= register_decoder(EAF_DEC_ENCODING_RLE, eaf_dec_decode_rle);
    ret |= register_decoder(EAF_DEC_ENCODING_HUFFMAN, decode_huffman_rle);
    ret |= register_decoder(EAF_DEC_ENCODING_HUFFMAN_DIRECT, eaf_dec_decode_huffman);
#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
    ret |= register_decoder(EAF_DEC_ENCODING_JPEG, eaf_dec_decode_jpeg);
#endif
#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
    ret |= register_decoder(EAF_DEC_ENCODING_HEATSHRINK, eaf_dec_decode_heatshrink);
#endif
    ret |= register_decoder(EAF_DEC_ENCODING_RAW, eaf_dec_decode_raw);

    return ret;
}

esp_err_t eaf_dec_decode_block(const eaf_dec_header_t *header, const uint8_t *block_data,
                               int block_len, uint8_t *out_data, bool swap_color)
{
    uint8_t encoding_type = block_data[0];
    int width = header->width;
    int block_height = header->block_height;

    esp_err_t decode_result = ESP_FAIL;

    if (encoding_type >= EAF_DEC_ENCODING_MAX) {
        GFX_LOGE(TAG, "Unknown encoding type: %02X", encoding_type);
        return ESP_FAIL;
    }

    eaf_dec_block_decoder_cb_t decoder = s_eaf_decoders[encoding_type];
    if (!decoder) {
        GFX_LOGE(TAG, "No decoder for encoding type: %02X", encoding_type);
        return ESP_FAIL;
    }

    size_t out_size;
#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
    if (encoding_type == EAF_DEC_ENCODING_JPEG) {
        out_size = width * block_height * 2;
    } else {
        out_size = width * block_height;
    }
#else
    out_size = width * block_height;
#endif

    decode_result = decoder(block_data + 1, block_len - 1, out_data, &out_size, swap_color);

    if (decode_result != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t eaf_dec_decode_rle(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size,
                             bool swap_color)
{
    (void)swap_color;

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 1 < in_size) {
        uint8_t repeat_count = in_data[in_pos++];
        uint8_t repeat_value = in_data[in_pos++];

        if (out_pos + repeat_count > *out_size) {
            GFX_LOGE(TAG, "Decompressed buffer overflow, %zu > %zu", out_pos + repeat_count, *out_size);
            return ESP_FAIL;
        }

        uint32_t value_4bytes = repeat_value | (repeat_value << 8) | (repeat_value << 16) | (repeat_value << 24);
        while (repeat_count >= 4) {
            *((uint32_t *)(out_data + out_pos)) = value_4bytes;
            out_pos += 4;
            repeat_count -= 4;
        }

        while (repeat_count > 0) {
            out_data[out_pos++] = repeat_value;
            repeat_count--;
        }
    }

    *out_size = out_pos;
    return ESP_OK;
}

esp_err_t eaf_dec_decode_raw(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size,
                             bool swap_color)
{
    (void)swap_color;

    if (!in_data || !out_data || !out_size) {
        GFX_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }

    if (*out_size < in_size) {
        GFX_LOGE(TAG, "Output buffer too small: need %zu, got %zu", in_size, *out_size);
        return ESP_ERR_INVALID_SIZE;
    }

    if (in_size > 0) {
        memcpy(out_data, in_data, in_size);
    }
    *out_size = in_size;
    return ESP_OK;
}

#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
esp_err_t eaf_dec_decode_heatshrink(const uint8_t *in_data, size_t in_size,
                                    uint8_t *out_data, size_t *out_size,
                                    bool swap_color)
{
    (void)swap_color;

    if (!in_data || !out_data || !out_size) {
        GFX_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }

    size_t out_capacity = *out_size;
    if (out_capacity == 0) {
        return ESP_OK;
    }

#if CONFIG_HEATSHRINK_DYNAMIC_ALLOC
    heatshrink_decoder *hsd = heatshrink_decoder_alloc(32, 8, 4);
    if (!hsd) {
        GFX_LOGE(TAG, "No mem for heatshrink decoder");
        return ESP_ERR_NO_MEM;
    }
#else
    heatshrink_decoder hsd_stack;
    heatshrink_decoder *hsd = &hsd_stack;
#endif

    heatshrink_decoder_reset(hsd);

    size_t in_pos = 0;
    size_t out_pos = 0;
    while (in_pos < in_size) {
        size_t sunk = 0;
        HSD_sink_res sres = heatshrink_decoder_sink(hsd, (uint8_t *)(in_data + in_pos),
                            in_size - in_pos, &sunk);
        if (sres < 0) {
            GFX_LOGE(TAG, "Heatshrink sink error: %d", sres);
            goto hs_fail;
        }
        in_pos += sunk;

        while (true) {
            size_t produced = 0;
            size_t remain = out_capacity - out_pos;
            if (remain == 0) {
                GFX_LOGE(TAG, "Heatshrink output overflow");
                goto hs_fail;
            }
            HSD_poll_res press = heatshrink_decoder_poll(hsd, out_data + out_pos, remain, &produced);
            if (press < 0) {
                GFX_LOGE(TAG, "Heatshrink poll error: %d", press);
                goto hs_fail;
            }
            out_pos += produced;
            if (press == HSDR_POLL_EMPTY) {
                break;
            }
        }
    }

    while (true) {
        HSD_finish_res fres = heatshrink_decoder_finish(hsd);
        if (fres < 0) {
            GFX_LOGE(TAG, "Heatshrink finish error: %d", fres);
            goto hs_fail;
        }

        while (true) {
            size_t produced = 0;
            size_t remain = out_capacity - out_pos;
            if (remain == 0) {
                GFX_LOGE(TAG, "Heatshrink output overflow");
                goto hs_fail;
            }
            HSD_poll_res press = heatshrink_decoder_poll(hsd, out_data + out_pos, remain, &produced);
            if (press < 0) {
                GFX_LOGE(TAG, "Heatshrink poll error: %d", press);
                goto hs_fail;
            }
            out_pos += produced;
            if (press == HSDR_POLL_EMPTY) {
                break;
            }
        }

        if (fres == HSDR_FINISH_DONE) {
            break;
        }
    }

    *out_size = out_pos;
#if CONFIG_HEATSHRINK_DYNAMIC_ALLOC
    heatshrink_decoder_free(hsd);
#endif
    return ESP_OK;

hs_fail:
#if CONFIG_HEATSHRINK_DYNAMIC_ALLOC
    heatshrink_decoder_free(hsd);
#endif
    return ESP_FAIL;
}
#endif // CONFIG_GFX_EAF_HEATSHRINK_SUPPORT

#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
esp_err_t eaf_dec_decode_jpeg(const uint8_t *in_data, size_t in_size,
                              uint8_t *out_data, size_t *out_size, bool swap_color)
{
    esp_err_t ret = ESP_OK;
    uint32_t w, h;
    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    jpeg_dec_config_t config = {
        .output_type = swap_color ? JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    ESP_GOTO_ON_ERROR(jpeg_dec_open(&config, &jpeg_dec), err, TAG, "JPEG decoder open failed");

    jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    ESP_GOTO_ON_FALSE(jpeg_io, ESP_ERR_NO_MEM, err, TAG, "No mem for jpeg_io");

    out_info = malloc(sizeof(jpeg_dec_header_info_t));
    ESP_GOTO_ON_FALSE(out_info, ESP_ERR_NO_MEM, err, TAG, "No mem for out_info");

    jpeg_io->inbuf = (unsigned char *)in_data;
    jpeg_io->inbuf_len = in_size;

    jpeg_error_t jpeg_ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG header parse failed");

    w = out_info->width;
    h = out_info->height;

    size_t required_size = w * h * 2;
    ESP_GOTO_ON_FALSE(*out_size >= required_size, ESP_ERR_INVALID_SIZE, err, TAG,
                      "Buffer too small: need %zu, got %zu", required_size, *out_size);

    jpeg_io->outbuf = out_data;
    jpeg_ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    ESP_GOTO_ON_FALSE(jpeg_ret == JPEG_ERR_OK, ESP_FAIL, err, TAG, "JPEG decode failed: %d", jpeg_ret);

    *out_size = required_size;

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return ESP_OK;

err:
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (out_info) {
        free(out_info);
    }
    if (jpeg_dec) {
        jpeg_dec_close(jpeg_dec);
    }
    return ret;
}
#endif // CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT

esp_err_t eaf_dec_decode_huffman(const uint8_t *in_data, size_t in_size,
                                 uint8_t *out_data, size_t *out_size,
                                 bool swap_color)
{
    (void)swap_color;
    size_t out_len = *out_size;

    if (!in_data || in_size < 3 || !out_data) {
        GFX_LOGE(TAG, "Invalid parameters");
        return ESP_FAIL;
    }

    uint16_t dict_size = (in_data[1] << 8) | in_data[0];
    if (in_size < 2 + dict_size) {
        GFX_LOGE(TAG, "Compressed data too short for dictionary");
        return ESP_FAIL;
    }

    size_t encoded_size = in_size - 2 - dict_size;
    esp_err_t ret = ESP_OK;

    // Special case: when the block is single color, the dictionary may contain only one symbol and the data length is 0
    if (encoded_size == 0) {

        size_t dict_pos = 1; // dict_bytes[0] is padding
        int symbol_count = 0;
        uint8_t single_symbol = 0;
        const uint8_t *dict_bytes = in_data + 2;

        while (dict_pos < dict_size) {
            uint8_t byte_val = dict_bytes[dict_pos++];
            uint8_t code_len = dict_bytes[dict_pos++];
            size_t code_byte_len = (size_t)((code_len + 7) / 8);
            if (dict_pos + code_byte_len > dict_size) {
                break;
            }
            dict_pos += code_byte_len;
            symbol_count++;
            single_symbol = byte_val;
            if (symbol_count > 1) {
                break;
            }
        }

        if (symbol_count == 1) {
            memset(out_data, single_symbol, out_len);
        }
    } else {
        ret = huffman_decode_data(in_data + 2 + dict_size, encoded_size,
                                  in_data + 2, dict_size,
                                  out_data, &out_len);
    }

    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "Huffman decoding failed: %d", ret);
        return ESP_FAIL;
    }

    if (out_len > *out_size) {
        GFX_LOGE(TAG, "Decoded data too large: %zu > %zu", out_len, *out_size);
        return ESP_FAIL;
    }
    *out_size = out_len;

    return ESP_OK;
}

/**********************
 *  FORMAT FUNCTIONS
 **********************/

esp_err_t eaf_dec_init(const uint8_t *data, size_t data_len, eaf_dec_handle_t *ret_parser)
{
    static bool decoders_initialized = false;

    if (!decoders_initialized) {
        esp_err_t ret = init_decoders();
        if (ret != ESP_OK) {
            GFX_LOGE(TAG, "Decoder init failed");
            return ret;
        }
        decoders_initialized = true;
    }

    esp_err_t ret = ESP_OK;
    eaf_dec_frame_entry_t *entries = NULL;

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)calloc(1, sizeof(eaf_dec_ctx_t));
    ESP_GOTO_ON_FALSE(parser, ESP_ERR_NO_MEM, err, TAG, "no mem for parser handle");

    ESP_GOTO_ON_FALSE(data[EAF_FORMAT_OFFSET] == EAF_FORMAT_MAGIC, ESP_ERR_INVALID_CRC, err, TAG, "bad file format magic");

    const char *format_str = (const char *)(data + EAF_STR_OFFSET);
    bool is_valid = (memcmp(format_str, EAF_FORMAT_STR, 3) == 0) || (memcmp(format_str, AAF_FORMAT_STR, 3) == 0);
    ESP_GOTO_ON_FALSE(is_valid, ESP_ERR_INVALID_CRC, err, TAG, "bad file format string (expected EAF or AAF)");

    int total_frames = *(int *)(data + EAF_NUM_OFFSET);
    uint32_t stored_chk = *(uint32_t *)(data + EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = *(uint32_t *)(data + EAF_TABLE_LEN);

    uint32_t calculated_chk = dec_calculate_checksum((uint8_t *)(data + EAF_TABLE_OFFSET), stored_len);
    ESP_GOTO_ON_FALSE(calculated_chk == stored_chk, ESP_ERR_INVALID_CRC, err, TAG, "bad full checksum");

    entries = (eaf_dec_frame_entry_t *)malloc(sizeof(eaf_dec_frame_entry_t) * total_frames);

    eaf_dec_frame_table_entry_t *table = (eaf_dec_frame_table_entry_t *)(data + EAF_TABLE_OFFSET);
    for (int i = 0; i < total_frames; i++) {
        (entries + i)->table = (table + i);
        (entries + i)->frame_mem = (void *)(data + EAF_TABLE_OFFSET + total_frames * sizeof(eaf_dec_frame_table_entry_t) + table[i].frame_offset);

        uint16_t *magic_ptr = (uint16_t *)(entries + i)->frame_mem;
        ESP_GOTO_ON_FALSE(*magic_ptr == EAF_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG, "bad file magic header");
    }

    parser->entries = entries;
    parser->total_frames = total_frames;

    *ret_parser = (eaf_dec_handle_t)parser;

    return ESP_OK;

err:
    if (entries) {
        free(entries);
    }
    if (parser) {
        free(parser);
    }
    *ret_parser = NULL;

    return ret;
}

esp_err_t eaf_dec_deinit(eaf_dec_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser);
    }
    return ESP_OK;
}

int eaf_dec_get_total_frames(eaf_dec_handle_t handle)
{
    if (handle == NULL) {
        GFX_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)(handle);
    return parser->total_frames;
}

const uint8_t *eaf_dec_get_frame_data(eaf_dec_handle_t handle, int index)
{
    if (handle == NULL) {
        GFX_LOGE(TAG, "Handle is invalid");
        return NULL;
    }

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return (const uint8_t *)((parser->entries + index)->frame_mem + EAF_MAGIC_LEN);
    } else {
        GFX_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return NULL;
    }
}

int eaf_dec_get_frame_size(eaf_dec_handle_t handle, int index)
{
    if (handle == NULL) {
        GFX_LOGE(TAG, "Handle is invalid");
        return -1;
    }

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return ((parser->entries + index)->table->frame_size - EAF_MAGIC_LEN);
    } else {
        GFX_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return -1;
    }
}

esp_err_t eaf_dec_decode_frame(eaf_dec_handle_t handle, int frame_index,
                               uint8_t *out_data, size_t out_size,
                               bool swap_bytes)
{
    if (!handle || !out_data) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t *frame_data = eaf_dec_get_frame_data(handle, frame_index);
    if (!frame_data) {
        GFX_LOGE(TAG, "Frame %d data unavailable", frame_index);
        return ESP_FAIL;
    }

    eaf_dec_header_t header;
    eaf_dec_type_t format = eaf_dec_get_frame_info(handle, frame_index, &header);
    if (format != EAF_DEC_TYPE_VALID) {
        GFX_LOGE(TAG, "Frame %d header parse failed", frame_index);
        return ESP_FAIL;
    }

    size_t block_height = header.block_height;
    size_t width = header.width;
    size_t height = header.height;
    uint8_t bit_depth = header.bit_depth;

    size_t block_size = width * block_height;
    block_size = (bit_depth == EAF_COLOR_DEPTH_24BIT) ? block_size * 2 : block_size;

    uint32_t *offsets = (uint32_t *)malloc(header.blocks * sizeof(uint32_t));
    if (offsets == NULL) {
        GFX_LOGE(TAG, "No mem for block offsets");
        eaf_dec_free_header(&header);
        return ESP_ERR_NO_MEM;
    }
    eaf_dec_calculate_offsets(&header, offsets);

    uint8_t *tmp_data = malloc(block_size);
    if (!tmp_data) {
        GFX_LOGE(TAG, "No mem for block buffer");
        free(offsets);
        eaf_dec_free_header(&header);
        return ESP_ERR_NO_MEM;
    }

    uint32_t palette_cache[256];
    memset(palette_cache, 0xFF, sizeof(palette_cache));

    for (int block = 0; block < header.blocks; block++) {
        const uint8_t *block_data = frame_data + offsets[block];
        int block_len = header.block_len[block];
        esp_err_t ret = eaf_dec_decode_block(&header, block_data, block_len, tmp_data, swap_bytes);

        if (ret != ESP_OK) {
            GFX_LOGD(TAG, "Block %d decode failed", block);
            continue;
        }

        uint16_t *block_buffer = (uint16_t *)out_data + (block * block_height * width);

        size_t valid_size;
        if ((block + 1) * block_height > height) {
            valid_size = (height - block * block_height) * width;
            valid_size = (bit_depth == EAF_COLOR_DEPTH_24BIT) ? valid_size * 2 : valid_size;
        } else {
            valid_size = block_size;
        }

        if (bit_depth == EAF_COLOR_DEPTH_8BIT) {
            for (size_t i = 0; i < valid_size; i++) {
                uint8_t index = tmp_data[i];
                uint16_t color;

                if (palette_cache[index] == 0xFFFFFFFF) {
                    gfx_color_t eaf_color;
                    eaf_dec_get_palette_color(&header, index, swap_bytes, &eaf_color);
                    palette_cache[index] = eaf_color.full;
                    color = eaf_color.full;
                } else {
                    color = palette_cache[index];
                }
                block_buffer[i] = color;
            }
        } else if (bit_depth == EAF_COLOR_DEPTH_4BIT) {
            GFX_LOGW(TAG, "%d-bit depth not supported", EAF_COLOR_DEPTH_4BIT);
        } else if (bit_depth == EAF_COLOR_DEPTH_24BIT) {
            memcpy(block_buffer, tmp_data, valid_size);
        }
    }

    free(tmp_data);
    free(offsets);
    eaf_dec_free_header(&header);

    return ESP_OK;
}
