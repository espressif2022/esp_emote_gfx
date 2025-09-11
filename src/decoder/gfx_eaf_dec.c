/**
 * @file gfx_eaf_dec.c
 * @brief EAF (Emote Animation Format) decoder implementation
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "decoder/gfx_eaf_dec.h"
#include "esp_jpeg_dec.h"

static const char *TAG = "anim_decoder";

/**********************
 *  STATIC VARIABLES
 **********************/
static block_decoder_t eaf_decoder[GFX_EAF_ENCODING_MAX] = {0};

/**********************
 *  STATIC HELPER FUNCTIONS
 **********************/

static uint32_t gfx_eaf_format_calc_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

/* Huffman Tree Helper Functions */
static HuffmanNode* huffman_node_create()
{
    HuffmanNode* node = (HuffmanNode*)calloc(1, sizeof(HuffmanNode));
    return node;
}

static void huffman_tree_free(HuffmanNode* node)
{
    if (!node) {
        return;
    }
    huffman_tree_free(node->left);
    huffman_tree_free(node->right);
    free(node);
}

static esp_err_t huffman_decode_data(const uint8_t* encoded_data, size_t encoded_len,
                                     const uint8_t* dict_data, size_t dict_len,
                                     uint8_t* decoded_data, size_t* decoded_len)
{
    if (!encoded_data || !dict_data || encoded_len == 0 || dict_len == 0) {
        *decoded_len = 0;
        return ESP_OK;
    }

    // Get padding bits from dictionary
    uint8_t padding_bits = dict_data[0];
    size_t dict_pos = 1;

    // Reconstruct Huffman Tree
    HuffmanNode* root = huffman_node_create();
    HuffmanNode* current_node = NULL;

    while (dict_pos < dict_len) {
        uint8_t symbol = dict_data[dict_pos++];
        uint8_t code_len = dict_data[dict_pos++];

        size_t code_byte_len = (code_len + 7) / 8;
        uint64_t code = 0;
        for (size_t i = 0; i < code_byte_len; ++i) {
            code = (code << 8) | dict_data[dict_pos++];
        }

        // Insert symbol into tree
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

    // Calculate total bits to decode
    size_t total_bits = encoded_len * 8;
    if (padding_bits > 0) {
        total_bits -= padding_bits;
    }

    current_node = root;
    size_t decoded_pos = 0;

    // Process each bit in the encoded data
    for (size_t bit_index = 0; bit_index < total_bits; bit_index++) {
        size_t byte_idx = bit_index / 8;
        int bit_offset = 7 - (bit_index % 8);  // Most significant bit first
        int bit_val = (encoded_data[byte_idx] >> bit_offset) & 1;

        if (bit_val == 0) {
            current_node = current_node->left;
        } else {
            current_node = current_node->right;
        }

        if (current_node == NULL) {
            ESP_LOGE(TAG, "Invalid path in Huffman tree at bit %d", (int)bit_index);
            break;
        }

        if (current_node->is_leaf) {
            decoded_data[decoded_pos++] = current_node->symbol;
            current_node = root;
        }
    }

    *decoded_len = decoded_pos;
    huffman_tree_free(root);
    return ESP_OK;
}

/**********************
 *  HEADER FUNCTIONS
 **********************/

gfx_eaf_format_t gfx_eaf_header_parse(const uint8_t *file_data, size_t file_size, gfx_eaf_header_t *header)
{
    memset(header, 0, sizeof(gfx_eaf_header_t));

    memcpy(header->format, file_data, 2);
    header->format[2] = '\0';

    if (strncmp(header->format, "_S", 2) == 0) {
        memcpy(header->version, file_data + 3, 6);

        header->bit_depth = file_data[9];

        if (header->bit_depth != 4 && header->bit_depth != 8 && header->bit_depth != 24) {
            ESP_LOGE(TAG, "Invalid bit depth: %d", header->bit_depth);
            return GFX_EAF_FORMAT_INVALID;
        }

        header->width = *(uint16_t *)(file_data + 10);
        header->height = *(uint16_t *)(file_data + 12);
        header->blocks = *(uint16_t *)(file_data + 14);
        header->block_height = *(uint16_t *)(file_data + 16);

        header->block_len = (uint32_t *)malloc(header->blocks * sizeof(uint32_t));
        if (header->block_len == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for split lengths");
            return GFX_EAF_FORMAT_INVALID;
        }

        for (int i = 0; i < header->blocks; i++) {
            header->block_len[i] = *(uint32_t *)(file_data + 18 + i * 4);
        }

        header->num_colors = 1 << header->bit_depth;

        if (header->bit_depth == 24) {
            header->num_colors = 0;
            header->palette = NULL;
        } else {
            header->palette = (uint8_t *)malloc(header->num_colors * 4);
            if (header->palette == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for palette");
                free(header->block_len);
                header->block_len = NULL;
                return GFX_EAF_FORMAT_INVALID;
            }

            memcpy(header->palette, file_data + 18 + header->blocks * 4, header->num_colors * 4);
        }
        header->data_offset = 18 + header->blocks * 4 + header->num_colors * 4;
        return GFX_EAF_FORMAT_VALID;

    } else if (strncmp(header->format, "_R", 2) == 0) {
        uint8_t file_length = *(uint8_t *)(file_data + 2);

        header->palette = (uint8_t *)malloc(file_length + 1);
        if (header->palette == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for redirect filename");
            return GFX_EAF_FORMAT_INVALID;
        }

        memcpy(header->palette, file_data + 3, file_length);
        header->palette[file_length] = '\0';
        header->num_colors = file_length + 1;

        return GFX_EAF_FORMAT_REDIRECT;
    } else if (strncmp(header->format, "_C", 2) == 0) {
        return GFX_EAF_FORMAT_INVALID;
    } else {
        ESP_LOGE(TAG, "Invalid format: %s", header->format);
        printf("%02X %02X %02X\r\n", header->format[0], header->format[1], header->format[2]);
        return GFX_EAF_FORMAT_INVALID;
    }
}

void gfx_eaf_free_header(gfx_eaf_header_t *header)
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

void gfx_eaf_calculate_offsets(const gfx_eaf_header_t *header, uint32_t *offsets)
{
    offsets[0] = header->data_offset;
    for (int i = 1; i < header->blocks; i++) {
        offsets[i] = offsets[i - 1] + header->block_len[i - 1];
    }
}

/**********************
 *  PALETTE FUNCTIONS
 **********************/

gfx_color_t gfx_eaf_palette_get_color(const gfx_eaf_header_t *header, uint8_t color_index, bool swap_bytes)
{
    const uint8_t *color_data = &header->palette[color_index * 4];
    // RGB888: R=color[2], G=color[1], B=color[0]
    // RGB565:
    // - R: (color[2] & 0xF8) << 8
    // - G: (color[1] & 0xFC) << 3
    // - B: (color[0] & 0xF8) >> 3
    gfx_color_t result;
    uint16_t rgb565_value = swap_bytes ? __builtin_bswap16(((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3)) : \
                            ((color_data[2] & 0xF8) << 8) | ((color_data[1] & 0xFC) << 3) | ((color_data[0] & 0xF8) >> 3);
    result.full = rgb565_value;
    return result;
}

/**********************
 *  DECODING FUNCTIONS
 **********************/

static esp_err_t gfx_eaf_huffman_rle_decode(const uint8_t *input_data, size_t input_size,
                                            uint8_t *output_buffer, size_t *out_size,
                                            bool swap_color)
{
    if (out_size == NULL || *out_size == 0) {
        ESP_LOGE(TAG, "Output size is invalid");
        return ESP_FAIL;
    }

    uint8_t *huffman_buffer = malloc(*out_size);
    if (huffman_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for Huffman buffer");
        return ESP_FAIL;
    }

    size_t huffman_out_size = *out_size;
    esp_err_t ret = gfx_eaf_huffman_decode(input_data, input_size, huffman_buffer, &huffman_out_size, swap_color);
    if (ret == ESP_OK) {
        ret = gfx_eaf_rle_decode(huffman_buffer, huffman_out_size, output_buffer, out_size, swap_color);
        *out_size = huffman_out_size;
    }

    free(huffman_buffer);
    return ret;
}

static esp_err_t register_decoder(gfx_eaf_encoding_t type, block_decoder_t decoder)
{
    if (type >= GFX_EAF_ENCODING_MAX) {
        ESP_LOGE(TAG, "Invalid encoding type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (eaf_decoder[type] != NULL) {
        ESP_LOGW(TAG, "Decoder already registered for type: %d", type);
    }

    eaf_decoder[type] = decoder;
    return ESP_OK;
}

static esp_err_t init_decoders(void)
{
    esp_err_t ret = ESP_OK;

    ret |= register_decoder(GFX_EAF_ENCODING_RLE, gfx_eaf_rle_decode);
    ret |= register_decoder(GFX_EAF_ENCODING_HUFFMAN, gfx_eaf_huffman_rle_decode);
    ret |= register_decoder(GFX_EAF_ENCODING_JPEG, gfx_eaf_jpeg_decode);
    ret |= register_decoder(GFX_EAF_ENCODING_HUFFMAN_DIRECT, gfx_eaf_huffman_decode);

    return ret;
}

esp_err_t gfx_eaf_block_decode(const gfx_eaf_header_t *header, const uint8_t *frame_data,
                               int block_index, uint8_t *decode_buffer, bool swap_color)
{
    // Calculate block offsets
    uint32_t *offsets = (uint32_t *)malloc(header->blocks * sizeof(uint32_t));
    if (offsets == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for block offsets");
        return ESP_FAIL;
    }

    gfx_eaf_calculate_offsets(header, offsets);

    // Get block data
    const uint8_t *block_data = frame_data + offsets[block_index];
    int block_len = header->block_len[block_index];
    uint8_t encoding_type = block_data[0];
    int width = header->width;
    int block_height = header->block_height;

    esp_err_t decode_result = ESP_FAIL;

    if (encoding_type >= sizeof(eaf_decoder) / sizeof(eaf_decoder[0])) {
        ESP_LOGE(TAG, "Unknown encoding type: %02X", encoding_type);
        free(offsets);
        return ESP_FAIL;
    }

    block_decoder_t decoder = eaf_decoder[encoding_type];
    if (!decoder) {
        ESP_LOGE(TAG, "No decoder for encoding type: %02X", encoding_type);
        free(offsets);
        return ESP_FAIL;
    }

    size_t out_size;
    if (encoding_type == GFX_EAF_ENCODING_JPEG) {
        out_size = width * block_height * 2; // RGB565 = 2 bytes per pixel
    } else {
        out_size = width * block_height;
    }

    decode_result = decoder(block_data + 1, block_len - 1, decode_buffer, &out_size, swap_color);

    if (decode_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode block %d", block_index);
        free(offsets);
        return ESP_FAIL;
    }

    free(offsets);
    return ESP_OK;
}

esp_err_t gfx_eaf_rle_decode(const uint8_t *input_data, size_t input_size,
                             uint8_t *output_buffer, size_t *out_size,
                             bool swap_color)
{
    (void)swap_color; // Unused parameter

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 1 <= input_size) {
        uint8_t repeat_count = input_data[in_pos++];
        uint8_t repeat_value = input_data[in_pos++];

        if (out_pos + repeat_count > *out_size) {
            ESP_LOGE(TAG, "Decompressed buffer overflow, %d > %d", out_pos + repeat_count, *out_size);
            return ESP_FAIL;
        }

        uint32_t value_4bytes = repeat_value | (repeat_value << 8) | (repeat_value << 16) | (repeat_value << 24);
        while (repeat_count >= 4) {
            *((uint32_t*)(output_buffer + out_pos)) = value_4bytes;
            out_pos += 4;
            repeat_count -= 4;
        }

        while (repeat_count > 0) {
            output_buffer[out_pos++] = repeat_value;
            repeat_count--;
        }
    }

    *out_size = out_pos;
    return ESP_OK;
}

esp_err_t gfx_eaf_jpeg_decode(const uint8_t *jpeg_data, size_t jpeg_size,
                              uint8_t *decode_buffer, size_t *out_size, bool swap_color)
{
    uint32_t w, h;
    jpeg_dec_config_t config = {
        .output_type = swap_color ? JPEG_PIXEL_FORMAT_RGB565_BE : JPEG_PIXEL_FORMAT_RGB565_LE,
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_open(&config, &jpeg_dec);
    if (!jpeg_dec) {
        ESP_LOGE(TAG, "Failed to open jpeg decoder");
        return ESP_FAIL;
    }

    jpeg_dec_io_t *jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    jpeg_dec_header_info_t *out_info = malloc(sizeof(jpeg_dec_header_info_t));
    if (!jpeg_io || !out_info) {
        if (jpeg_io) {
            free(jpeg_io);
        }
        if (out_info) {
            free(out_info);
        }
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to allocate memory for jpeg decoder");
        return ESP_FAIL;
    }

    jpeg_io->inbuf = (unsigned char *)jpeg_data;
    jpeg_io->inbuf_len = jpeg_size;

    jpeg_error_t ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret == JPEG_ERR_OK) {
        w = out_info->width;
        h = out_info->height;

        size_t required_size = w * h * 2; // RGB565 = 2 bytes per pixel
        if (*out_size < required_size) {
            ESP_LOGE(TAG, "Output buffer too small: need %zu, got %zu", required_size, *out_size);
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            return ESP_ERR_INVALID_SIZE;
        }

        jpeg_io->outbuf = decode_buffer;
        ret = jpeg_dec_process(jpeg_dec, jpeg_io);
        if (ret != JPEG_ERR_OK) {
            free(jpeg_io);
            free(out_info);
            jpeg_dec_close(jpeg_dec);
            ESP_LOGE(TAG, "Failed to decode jpeg:[%d]", ret);
            return ESP_FAIL;
        }
        *out_size = required_size;
    } else {
        free(jpeg_io);
        free(out_info);
        jpeg_dec_close(jpeg_dec);
        ESP_LOGE(TAG, "Failed to parse jpeg header");
        return ESP_FAIL;
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return ESP_OK;
}

esp_err_t gfx_eaf_huffman_decode(const uint8_t *input_data, size_t input_size,
                                 uint8_t *output_buffer, size_t *out_size,
                                 bool swap_color)
{
    (void)swap_color; // Unused parameter
    size_t decoded_size = *out_size;

    if (!input_data || input_size < 3 || !output_buffer) {
        ESP_LOGE(TAG, "Invalid parameters: input_data=%p, input_size=%d, output_buffer=%p",
                 input_data, input_size, output_buffer);
        return ESP_FAIL;
    }

    uint16_t dict_size = (input_data[1] << 8) | input_data[0];
    if (input_size < 2 + dict_size) {
        ESP_LOGE(TAG, "Compressed data too short for dictionary");
        return ESP_FAIL;
    }

    size_t encoded_size = input_size - 2 - dict_size;
    if (encoded_size == 0) {
        ESP_LOGE(TAG, "No data to decode");
        return ESP_FAIL;
    }

    esp_err_t ret = huffman_decode_data(input_data + 2 + dict_size, encoded_size,
                                        input_data + 2, dict_size,
                                        output_buffer, &decoded_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Huffman decoding failed: %d", ret);
        return ESP_FAIL;
    }

    if (decoded_size > *out_size) {
        ESP_LOGE(TAG, "Decoded data too large: %d > %d", decoded_size, *out_size);
        return ESP_FAIL;
    }
    *out_size = decoded_size;

    return ESP_OK;
}

/**********************
 *  FORMAT FUNCTIONS
 **********************/

esp_err_t gfx_eaf_format_init(const uint8_t *data, size_t data_len, gfx_eaf_format_handle_t *ret_parser)
{
    static bool decoders_initialized = false;

    if (!decoders_initialized) {
        esp_err_t ret = init_decoders();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize eaf_decoder");
            return ret;
        }
        decoders_initialized = true;
    }

    esp_err_t ret = ESP_OK;
    frame_entry_t *entries = NULL;

    gfx_eaf_format_ctx_t *parser = (gfx_eaf_format_ctx_t *)calloc(1, sizeof(gfx_eaf_format_ctx_t));
    ESP_GOTO_ON_FALSE(parser, ESP_ERR_NO_MEM, err, TAG, "no mem for parser handle");

    // Check file format magic number: 0x89
    ESP_GOTO_ON_FALSE(data[GFX_EAF_FORMAT_OFFSET] == GFX_EAF_FORMAT_MAGIC, ESP_ERR_INVALID_CRC, err, TAG, "bad file format magic");

    // Check for EAF/AAF format string
    const char *format_str = (const char *)(data + GFX_EAF_STR_OFFSET);
    bool is_valid = (memcmp(format_str, "EAF", 3) == 0) || (memcmp(format_str, "AAF", 3) == 0);
    ESP_GOTO_ON_FALSE(is_valid, ESP_ERR_INVALID_CRC, err, TAG, "bad file format string (expected EAF or AAF)");

    int total_frames = *(int *)(data + GFX_EAF_NUM_OFFSET);
    uint32_t stored_chk = *(uint32_t *)(data + GFX_EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = *(uint32_t *)(data + GFX_EAF_TABLE_LEN);

    uint32_t calculated_chk = gfx_eaf_format_calc_checksum((uint8_t *)(data + GFX_EAF_TABLE_OFFSET), stored_len);
    ESP_GOTO_ON_FALSE(calculated_chk == stored_chk, ESP_ERR_INVALID_CRC, err, TAG, "bad full checksum");

    entries = (frame_entry_t *)malloc(sizeof(frame_entry_t) * total_frames);

    frame_table_entry_t *table = (frame_table_entry_t *)(data + GFX_EAF_TABLE_OFFSET);
    for (int i = 0; i < total_frames; i++) {
        (entries + i)->table = (table + i);
        (entries + i)->frame_mem = (void *)(data + GFX_EAF_TABLE_OFFSET + total_frames * sizeof(frame_table_entry_t) + table[i].frame_offset);

        uint16_t *magic_ptr = (uint16_t *)(entries + i)->frame_mem;
        ESP_GOTO_ON_FALSE(*magic_ptr == GFX_EAF_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG, "bad file magic header");
    }

    parser->entries = entries;
    parser->total_frames = total_frames;

    *ret_parser = (gfx_eaf_format_handle_t)parser;

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

esp_err_t gfx_eaf_format_deinit(gfx_eaf_format_handle_t handle)
{
    assert(handle && "handle is invalid");
    gfx_eaf_format_ctx_t *parser = (gfx_eaf_format_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser);
    }
    return ESP_OK;
}

int gfx_eaf_format_get_total_frames(gfx_eaf_format_handle_t handle)
{
    assert(handle && "handle is invalid");
    gfx_eaf_format_ctx_t *parser = (gfx_eaf_format_ctx_t *)(handle);

    return parser->total_frames;
}

const uint8_t *gfx_eaf_format_get_frame_data(gfx_eaf_format_handle_t handle, int index)
{
    assert(handle && "handle is invalid");

    gfx_eaf_format_ctx_t *parser = (gfx_eaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return (const uint8_t *)((parser->entries + index)->frame_mem + GFX_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return NULL;
    }
}

int gfx_eaf_format_get_frame_size(gfx_eaf_format_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    gfx_eaf_format_ctx_t *parser = (gfx_eaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return ((parser->entries + index)->table->frame_size - GFX_EAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return -1;
    }
}