/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_EAF_DEC
#include "common/gfx_log_priv.h"

#include "common/gfx_types_priv.h"
#include "gfx_eaf_dec.h"
#include "platform/gfx_platform_jpeg_priv.h"

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
 *  STATIC INLINE HELPERS
 **********************/

/* EAF tables and frame headers are byte-packed and not guaranteed to be
 * naturally aligned, so reads go through memcpy to avoid unaligned-access
 * faults/UB. Values are stored little-endian (matches the encoder and the
 * Xtensa/RISC-V targets), so the bytes are consumed in LE order. */
static inline uint16_t eaf_rd_u16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline uint32_t eaf_rd_u32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline int32_t eaf_rd_i32(const uint8_t *p)
{
    int32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "eaf_dec";
static eaf_dec_block_decoder_cb_t s_eaf_decoders[EAF_DEC_ENCODING_MAX] = {0};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static uint32_t dec_calculate_checksum(const uint8_t *data, uint32_t length);
static void huffman_tree_free(eaf_dec_huffman_node_t *node);
static gfx_err_t huffman_decode_data(eaf_dec_ctx_t *ctx,
                                     const uint8_t *in_data, size_t in_size,
                                     const uint8_t *dict_data, size_t dict_len,
                                     uint8_t *out_data, size_t *out_size);
static gfx_err_t eaf_dec_decode_huffman_ctx(eaf_dec_ctx_t *ctx, const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size);
static gfx_err_t decode_huffman_rle_ctx(eaf_dec_ctx_t *ctx, const uint8_t *in_data, size_t in_size,
                                        uint8_t *out_data, size_t *out_size);
static gfx_err_t eaf_dec_decode_jpeg_block_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size);

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

static void huffman_tree_free(eaf_dec_huffman_node_t *node)
{
    if (!node) {
        return;
    }
    huffman_tree_free(node->left);
    huffman_tree_free(node->right);
    free(node);
}

/* Tree-node allocator. With a parser ctx, nodes are bump-allocated from a
 * reusable arena (reset every call, freed only at deinit); without one, each
 * node is calloc'd and the tree is freed after use. Either way the resulting
 * tree structure and decoded output are identical. */
typedef struct {
    eaf_dec_ctx_t *ctx; /*!< Arena owner, or NULL for transient malloc mode */
    size_t used;        /*!< Bump index into ctx->huff_nodes (arena mode) */
} huff_builder_t;

/* Upper bound on tree nodes for a dictionary: root + sum(code_len), since each
 * code adds at most code_len nodes along its root-to-leaf path. */
static size_t huff_count_nodes(const uint8_t *dict_data, size_t dict_len)
{
    size_t dict_pos = 1; /* skip padding byte */
    size_t nodes = 1;    /* root */
    while (dict_pos + 1 < dict_len) {
        dict_pos++; /* symbol */
        uint8_t code_len = dict_data[dict_pos++];
        size_t code_byte_len = (code_len + 7) / 8;
        if (dict_pos + code_byte_len > dict_len) {
            break;
        }
        dict_pos += code_byte_len;
        nodes += code_len;
    }
    return nodes;
}

static gfx_err_t huff_arena_reserve(eaf_dec_ctx_t *ctx, size_t nodes)
{
    if (ctx->huff_node_cap >= nodes) {
        return GFX_OK;
    }
    eaf_dec_huffman_node_t *p = (eaf_dec_huffman_node_t *)realloc(ctx->huff_nodes,
                                nodes * sizeof(eaf_dec_huffman_node_t));
    if (p == NULL) {
        return GFX_ERR_NO_MEM;
    }
    ctx->huff_nodes = p;
    ctx->huff_node_cap = nodes;
    return GFX_OK;
}

static eaf_dec_huffman_node_t *huff_node_get(huff_builder_t *b)
{
    eaf_dec_huffman_node_t *node;
    if (b->ctx != NULL) {
        if (b->used >= b->ctx->huff_node_cap) {
            return NULL; /* arena was pre-sized to the upper bound; should not happen */
        }
        node = &b->ctx->huff_nodes[b->used++];
        memset(node, 0, sizeof(*node));
    } else {
        node = (eaf_dec_huffman_node_t *)calloc(1, sizeof(*node));
    }
    return node;
}

static void huff_builder_cleanup(huff_builder_t *b, eaf_dec_huffman_node_t *root)
{
    if (b->ctx == NULL) {
        huffman_tree_free(root); /* transient mode owns the tree */
    }
    /* Arena mode: nodes persist in ctx and are recycled on the next call. */
}

static gfx_err_t huffman_decode_data(eaf_dec_ctx_t *ctx,
                                     const uint8_t *in_data, size_t in_size,
                                     const uint8_t *dict_data, size_t dict_len,
                                     uint8_t *out_data, size_t *out_size)
{
    if (!in_data || !dict_data || in_size == 0 || dict_len == 0) {
        *out_size = 0;
        return GFX_OK;
    }

    /* On entry *out_size carries the destination capacity (the caller sizes
     * out_data to the expected decoded length). Treat it as a hard bound so a
     * crafted bitstream/dictionary cannot write past the buffer. */
    const size_t out_cap = *out_size;

    huff_builder_t builder = { .ctx = ctx, .used = 0 };
    if (ctx != NULL) {
        gfx_err_t res = huff_arena_reserve(ctx, huff_count_nodes(dict_data, dict_len));
        if (res != GFX_OK) {
            GFX_LOGE(TAG, "No mem for Huffman node arena");
            return GFX_FAIL;
        }
    }

    uint8_t padding_bits = dict_data[0];
    size_t dict_pos = 1;

    eaf_dec_huffman_node_t *root = huff_node_get(&builder);
    if (root == NULL) {
        GFX_LOGE(TAG, "No mem for Huffman root");
        return GFX_FAIL;
    }
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
                    current_node->left = huff_node_get(&builder);
                }
                current_node = current_node->left;
            } else {
                if (!current_node->right) {
                    current_node->right = huff_node_get(&builder);
                }
                current_node = current_node->right;
            }
            if (current_node == NULL) {
                GFX_LOGE(TAG, "No mem for Huffman node");
                huff_builder_cleanup(&builder, root);
                return GFX_FAIL;
            }
        }
        current_node->is_leaf = 1;
        current_node->symbol = symbol;
    }

    size_t total_bits = in_size * 8;
    if (padding_bits >= total_bits) {
        /* Malformed dictionary header: padding cannot consume the whole stream. */
        GFX_LOGE(TAG, "Invalid Huffman padding bits %u (stream bits %zu)", padding_bits, total_bits);
        huff_builder_cleanup(&builder, root);
        *out_size = 0;
        return GFX_FAIL;
    }
    total_bits -= padding_bits;

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
            if (out_pos >= out_cap) {
                GFX_LOGE(TAG, "Huffman output exceeds capacity %zu", out_cap);
                *out_size = out_pos;
                huff_builder_cleanup(&builder, root);
                return GFX_FAIL;
            }
            out_data[out_pos++] = current_node->symbol;
            current_node = root;
        }
    }

    *out_size = out_pos;
    huff_builder_cleanup(&builder, root);
    return GFX_OK;
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

        /* Fixed sub-header spans bytes [0, EAF_FRAME_BLOCK_LEN_TABLE_OFFSET). */
        if (file_size < EAF_FRAME_BLOCK_LEN_TABLE_OFFSET) {
            GFX_LOGE(TAG, "Frame %d too small for sub-header", frame_index);
            return EAF_DEC_TYPE_INVALID;
        }

        memcpy(header.version, file_data + EAF_FRAME_VERSION_OFFSET, 6);

        header.bit_depth = file_data[EAF_FRAME_BIT_DEPTH_OFFSET];

        /* 4-bit is not implemented in the block decoders, so do not advertise it
         * as a valid frame; only 8-bit (palette) and 24-bit (RGB565) decode. */
        if (header.bit_depth != EAF_COLOR_DEPTH_8BIT && header.bit_depth != EAF_COLOR_DEPTH_24BIT) {
            GFX_LOGE(TAG, "Unsupported bit depth: %d (only 8/24-bit decode is implemented)", header.bit_depth);
            return EAF_DEC_TYPE_INVALID;
        }

        header.width = eaf_rd_u16(file_data + EAF_FRAME_WIDTH_OFFSET);
        header.height = eaf_rd_u16(file_data + EAF_FRAME_HEIGHT_OFFSET);
        header.blocks = eaf_rd_u16(file_data + EAF_FRAME_BLOCKS_OFFSET);
        header.block_height = eaf_rd_u16(file_data + EAF_FRAME_BLOCK_HEIGHT_OFFSET);

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
        /* Fixed sub-header spans bytes [0, EAF_FRAME_BLOCK_LEN_TABLE_OFFSET). */
        if (file_size < EAF_FRAME_BLOCK_LEN_TABLE_OFFSET) {
            GFX_LOGE(TAG, "Frame %d too small for sub-header", frame_index);
            return EAF_DEC_TYPE_INVALID;
        }

        memcpy(header->version, file_data + EAF_FRAME_VERSION_OFFSET, 6);

        header->bit_depth = file_data[EAF_FRAME_BIT_DEPTH_OFFSET];

        /* 4-bit is not implemented in the block decoders, so do not advertise it
         * as a valid frame; only 8-bit (palette) and 24-bit (RGB565) decode. */
        if (header->bit_depth != EAF_COLOR_DEPTH_8BIT && header->bit_depth != EAF_COLOR_DEPTH_24BIT) {
            GFX_LOGE(TAG, "Unsupported bit depth: %d (only 8/24-bit decode is implemented)", header->bit_depth);
            return EAF_DEC_TYPE_INVALID;
        }

        header->width = eaf_rd_u16(file_data + EAF_FRAME_WIDTH_OFFSET);
        header->height = eaf_rd_u16(file_data + EAF_FRAME_HEIGHT_OFFSET);
        header->blocks = eaf_rd_u16(file_data + EAF_FRAME_BLOCKS_OFFSET);
        header->block_height = eaf_rd_u16(file_data + EAF_FRAME_BLOCK_HEIGHT_OFFSET);

        header->num_colors = (header->bit_depth == EAF_COLOR_DEPTH_24BIT) ? 0 : (1 << header->bit_depth);

        /* The block-length table and palette must lie inside the frame payload
         * before we read them. */
        const size_t block_len_bytes = (size_t)header->blocks * EAF_FRAME_BLOCK_LEN_SIZE;
        const size_t palette_bytes = (size_t)header->num_colors * EAF_FRAME_PALETTE_ENTRY_SIZE;
        const size_t header_bytes = (size_t)EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + block_len_bytes + palette_bytes;
        if ((size_t)file_size < header_bytes) {
            GFX_LOGE(TAG, "Frame %d header (%zu) exceeds payload (%zu)", frame_index, header_bytes, (size_t)file_size);
            return EAF_DEC_TYPE_INVALID;
        }

        header->block_len = (uint32_t *)malloc(block_len_bytes);
        if (header->block_len == NULL) {
            GFX_LOGE(TAG, "No mem for block_len");
            return EAF_DEC_TYPE_INVALID;
        }

        for (int i = 0; i < header->blocks; i++) {
            header->block_len[i] = eaf_rd_u32(file_data + EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + i * EAF_FRAME_BLOCK_LEN_SIZE);
        }

        if (header->bit_depth == EAF_COLOR_DEPTH_24BIT) {
            header->palette = NULL;
        } else {
            header->palette = (uint8_t *)malloc(palette_bytes);
            if (header->palette == NULL) {
                GFX_LOGE(TAG, "No mem for palette");
                free(header->block_len);
                header->block_len = NULL;
                return EAF_DEC_TYPE_INVALID;
            }

            memcpy(header->palette, file_data + EAF_FRAME_BLOCK_LEN_TABLE_OFFSET + block_len_bytes, palette_bytes);
        }
        header->data_offset = (uint16_t)header_bytes;
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

bool eaf_dec_get_palette_color(const eaf_dec_header_t *header, uint8_t color_index, gfx_color_t *result)
{
    const uint8_t *color_data = &header->palette[color_index * 4];

    if (color_data[0] == 0 && color_data[1] == 0 && color_data[2] == 0) {
        /* Transparent/empty entry. Still publish a defined color so callers that
         * ignore the return value do not read an uninitialised gfx_color_t. */
        result->full = 0;
        return true;
    }

    gfx_color_t color = {
        .full = (uint16_t)(((color_data[2] & 0xF8) << 8) |
                           ((color_data[1] & 0xFC) << 3) |
                           ((color_data[0] & 0xF8) >> 3)),
    };

    result->full = color.full;
    return false;
}

/**********************
 *  DECODING FUNCTIONS
 **********************/

static gfx_err_t decode_huffman_rle_ctx(eaf_dec_ctx_t *ctx, const uint8_t *in_data, size_t in_size,
                                        uint8_t *out_data, size_t *out_size)
{
    if (out_size == NULL || *out_size == 0) {
        GFX_LOGE(TAG, "Output size is invalid");
        return GFX_FAIL;
    }

    /* tmp holds the Huffman output, which is the RLE-encoded stream that later
     * expands into *out_size decoded bytes. Each RLE (count,value) pair is 2
     * input bytes and emits >=1 byte (encoders never emit count==0), so the RLE
     * input is at most 2 * decoded_size: that is the worst-case bound used here. */
    size_t tmp_size = *out_size * 2;

    /* Reuse the handle-owned scratch when available to avoid a per-block malloc;
     * fall back to a transient buffer otherwise. */
    uint8_t *tmp_data;
    bool tmp_owned = false;
    if (ctx != NULL) {
        if (ctx->huff_tmp_cap < tmp_size) {
            uint8_t *p = (uint8_t *)realloc(ctx->huff_tmp, tmp_size);
            if (p == NULL) {
                GFX_LOGE(TAG, "No mem for tmp buffer");
                return GFX_FAIL;
            }
            ctx->huff_tmp = p;
            ctx->huff_tmp_cap = tmp_size;
        }
        tmp_data = ctx->huff_tmp;
    } else {
        tmp_data = (uint8_t *)malloc(tmp_size);
        if (tmp_data == NULL) {
            GFX_LOGE(TAG, "No mem for tmp buffer");
            return GFX_FAIL;
        }
        tmp_owned = true;
    }

    size_t tmp_len = tmp_size;
    gfx_err_t ret = eaf_dec_decode_huffman_ctx(ctx, in_data, in_size, tmp_data, &tmp_len);
    if (ret == GFX_OK) {
        ret = eaf_dec_decode_rle(tmp_data, tmp_len, out_data, out_size);
    }

    if (tmp_owned) {
        free(tmp_data);
    }
    return ret;
}

static gfx_err_t decode_huffman_rle(const uint8_t *in_data, size_t in_size,
                                    uint8_t *out_data, size_t *out_size)
{
    return decode_huffman_rle_ctx(NULL, in_data, in_size, out_data, out_size);
}

static gfx_err_t register_decoder(eaf_dec_encoding_type_t type, eaf_dec_block_decoder_cb_t decoder)
{
    if (type >= EAF_DEC_ENCODING_MAX) {
        GFX_LOGE(TAG, "Invalid encoding type: %d", type);
        return GFX_ERR_INVALID_ARG;
    }

    if (s_eaf_decoders[type] != NULL) {
        GFX_LOGW(TAG, "Decoder already registered for type: %d", type);
    }

    s_eaf_decoders[type] = decoder;
    return GFX_OK;
}

static gfx_err_t init_decoders(void)
{
    gfx_err_t ret = GFX_OK;

    ret |= register_decoder(EAF_DEC_ENCODING_RLE, eaf_dec_decode_rle);
    ret |= register_decoder(EAF_DEC_ENCODING_HUFFMAN, decode_huffman_rle);
    ret |= register_decoder(EAF_DEC_ENCODING_HUFFMAN_DIRECT, eaf_dec_decode_huffman);
#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
#endif
#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
    ret |= register_decoder(EAF_DEC_ENCODING_HEATSHRINK, eaf_dec_decode_heatshrink);
#endif
    ret |= register_decoder(EAF_DEC_ENCODING_RAW, eaf_dec_decode_raw);

    return ret;
}

gfx_err_t eaf_dec_decode_block(eaf_dec_handle_t handle, const eaf_dec_header_t *header,
                               const uint8_t *block_data, int block_len, uint8_t *out_data)
{
    eaf_dec_ctx_t *ctx = (eaf_dec_ctx_t *)handle;
    if (header == NULL || block_data == NULL || out_data == NULL || block_len < 1) {
        GFX_LOGE(TAG, "Invalid block decode args");
        return GFX_FAIL;
    }
    uint8_t encoding_type = block_data[0];
    int width = header->width;
    int block_height = header->block_height;

    gfx_err_t decode_result = GFX_FAIL;

    if (encoding_type >= EAF_DEC_ENCODING_MAX) {
        GFX_LOGE(TAG, "Unknown encoding type: %02X", encoding_type);
        return GFX_FAIL;
    }

    size_t out_size;
#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
    if (encoding_type == EAF_DEC_ENCODING_JPEG) {
        out_size = width * block_height * 2;
        decode_result = eaf_dec_decode_jpeg_block_with_hint(block_data + 1, block_len - 1,
                        (uint32_t)width, (uint32_t)block_height,
                        out_data, &out_size);
        if (decode_result != GFX_OK) {
            return GFX_FAIL;
        }
        return GFX_OK;
    } else {
        out_size = width * block_height;
    }
#else
    out_size = width * block_height;
#endif

    /* Huffman variants take the handle scratch (reusable Huffman tmp + node
     * arena); the remaining encoders are stateless and use the registry. */
    if (encoding_type == EAF_DEC_ENCODING_HUFFMAN) {
        decode_result = decode_huffman_rle_ctx(ctx, block_data + 1, block_len - 1, out_data, &out_size);
    } else if (encoding_type == EAF_DEC_ENCODING_HUFFMAN_DIRECT) {
        decode_result = eaf_dec_decode_huffman_ctx(ctx, block_data + 1, block_len - 1, out_data, &out_size);
    } else {
        eaf_dec_block_decoder_cb_t decoder = s_eaf_decoders[encoding_type];
        if (!decoder) {
            GFX_LOGE(TAG, "No decoder for encoding type: %02X", encoding_type);
            return GFX_FAIL;
        }
        decode_result = decoder(block_data + 1, block_len - 1, out_data, &out_size);
    }

    if (decode_result != GFX_OK) {
        return GFX_FAIL;
    }

    /* A short decode (Huffman/RLE that emits fewer bytes than the block holds)
     * must leave a deterministic, zero-filled tail rather than uninitialised
     * scratch, so independent decoder instances produce identical output. */
    const size_t expected_size = (size_t)width * (size_t)block_height;
    if (out_size < expected_size) {
        memset(out_data + out_size, 0, expected_size - out_size);
    }

    return GFX_OK;
}

gfx_err_t eaf_dec_decode_rle(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size)
{
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 1 < in_size) {
        uint8_t repeat_count = in_data[in_pos++];
        uint8_t repeat_value = in_data[in_pos++];

        if (out_pos + repeat_count > *out_size) {
            GFX_LOGE(TAG, "Decompressed buffer overflow, %zu > %zu", out_pos + repeat_count, *out_size);
            return GFX_FAIL;
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
    return GFX_OK;
}

gfx_err_t eaf_dec_decode_raw(const uint8_t *in_data, size_t in_size,
                             uint8_t *out_data, size_t *out_size)
{
    if (!in_data || !out_data || !out_size) {
        GFX_LOGE(TAG, "Invalid parameters");
        return GFX_FAIL;
    }

    if (*out_size < in_size) {
        GFX_LOGE(TAG, "Output buffer too small: need %zu, got %zu", in_size, *out_size);
        return GFX_ERR_INVALID_SIZE;
    }

    if (in_size > 0) {
        memcpy(out_data, in_data, in_size);
    }
    *out_size = in_size;
    return GFX_OK;
}

#ifdef CONFIG_GFX_EAF_HEATSHRINK_SUPPORT
gfx_err_t eaf_dec_decode_heatshrink(const uint8_t *in_data, size_t in_size,
                                    uint8_t *out_data, size_t *out_size)
{
    if (!in_data || !out_data || !out_size) {
        GFX_LOGE(TAG, "Invalid parameters");
        return GFX_FAIL;
    }

    size_t out_capacity = *out_size;
    if (out_capacity == 0) {
        return GFX_OK;
    }

#if CONFIG_HEATSHRINK_DYNAMIC_ALLOC
    heatshrink_decoder *hsd = heatshrink_decoder_alloc(32, 8, 4);
    if (!hsd) {
        GFX_LOGE(TAG, "No mem for heatshrink decoder");
        return GFX_ERR_NO_MEM;
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
    return GFX_OK;

hs_fail:
#if CONFIG_HEATSHRINK_DYNAMIC_ALLOC
    heatshrink_decoder_free(hsd);
#endif
    return GFX_FAIL;
}
#endif // CONFIG_GFX_EAF_HEATSHRINK_SUPPORT

#if CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT
static gfx_err_t eaf_dec_decode_jpeg_block_with_hint(const uint8_t *in_data, size_t in_size,
        uint32_t width, uint32_t height, uint8_t *out_data, size_t *out_size)
{
    if (!gfx_platform_jpeg_is_available()) {
        GFX_LOGE(TAG, "JPEG decoder unavailable");
        return GFX_ERR_NOT_SUPPORTED;
    }

    return gfx_platform_jpeg_decode_rgb565_with_hint(in_data, in_size,
            width, height,
            out_data, out_size);
}
#endif // CONFIG_GFX_EAF_JPEG_DECODE_SUPPORT

static gfx_err_t eaf_dec_decode_huffman_ctx(eaf_dec_ctx_t *ctx, const uint8_t *in_data, size_t in_size,
        uint8_t *out_data, size_t *out_size)
{
    size_t out_len = *out_size;

    if (!in_data || in_size < 3 || !out_data) {
        GFX_LOGE(TAG, "Invalid parameters");
        return GFX_FAIL;
    }

    uint16_t dict_size = (in_data[1] << 8) | in_data[0];
    if (in_size < 2 + dict_size) {
        GFX_LOGE(TAG, "Compressed data too short for dictionary");
        return GFX_FAIL;
    }

    size_t encoded_size = in_size - 2 - dict_size;
    gfx_err_t ret = GFX_OK;

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
        } else {
            /* No single fill symbol: there is nothing decodable, so report a
             * zero-length result and let the caller zero-fill the block. This
             * keeps output deterministic instead of exposing the uninitialised
             * scratch buffer. */
            out_len = 0;
        }
    } else {
        ret = huffman_decode_data(ctx, in_data + 2 + dict_size, encoded_size,
                                  in_data + 2, dict_size,
                                  out_data, &out_len);
    }

    if (ret != GFX_OK) {
        GFX_LOGE(TAG, "Huffman decoding failed: %d", ret);
        return GFX_FAIL;
    }

    if (out_len > *out_size) {
        GFX_LOGE(TAG, "Decoded data too large: %zu > %zu", out_len, *out_size);
        return GFX_FAIL;
    }
    *out_size = out_len;

    return GFX_OK;
}

gfx_err_t eaf_dec_decode_huffman(const uint8_t *in_data, size_t in_size,
                                 uint8_t *out_data, size_t *out_size)
{
    return eaf_dec_decode_huffman_ctx(NULL, in_data, in_size, out_data, out_size);
}

/**********************
 *  FORMAT FUNCTIONS
 **********************/

static gfx_err_t eaf_dec_ensure_decoders(void)
{
    static bool decoders_initialized = false;

    if (!decoders_initialized) {
        gfx_err_t ret = init_decoders();
        if (ret != GFX_OK) {
            GFX_LOGE(TAG, "Decoder init failed");
            return ret;
        }
        decoders_initialized = true;
    }
    return GFX_OK;
}

gfx_err_t eaf_dec_init(const uint8_t *data, size_t data_len, eaf_dec_handle_t *ret_parser)
{
    gfx_err_t init_ret = eaf_dec_ensure_decoders();
    if (init_ret != GFX_OK) {
        return init_ret;
    }

    gfx_err_t ret = GFX_OK;
    eaf_dec_frame_entry_t *entries = NULL;
    eaf_dec_ctx_t *parser = NULL;

    if (data == NULL || ret_parser == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *ret_parser = NULL;

    /* Need at least the fixed file header (magic, format string, frame count,
     * checksum, table length) before dereferencing any of those fields. */
    if (data_len < EAF_TABLE_OFFSET) {
        GFX_LOGE(TAG, "data too small for header: %zu", data_len);
        return GFX_ERR_INVALID_SIZE;
    }

    parser = (eaf_dec_ctx_t *)calloc(1, sizeof(eaf_dec_ctx_t));
    GFX_GOTO_ON_FALSE(parser, GFX_ERR_NO_MEM, err, TAG, "no mem for parser handle");

    GFX_GOTO_ON_FALSE(data[EAF_FORMAT_OFFSET] == EAF_FORMAT_MAGIC, GFX_ERR_INVALID_CRC, err, TAG, "bad file format magic");

    const char *format_str = (const char *)(data + EAF_STR_OFFSET);
    bool is_valid = (memcmp(format_str, EAF_FORMAT_STR, 3) == 0) || (memcmp(format_str, AAF_FORMAT_STR, 3) == 0);
    GFX_GOTO_ON_FALSE(is_valid, GFX_ERR_INVALID_CRC, err, TAG, "bad file format string (expected EAF or AAF)");

    int total_frames = eaf_rd_i32(data + EAF_NUM_OFFSET);
    uint32_t stored_chk = eaf_rd_u32(data + EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = eaf_rd_u32(data + EAF_TABLE_LEN);

    /* The checksum spans data[EAF_TABLE_OFFSET .. +stored_len); reject lengths
     * that would read past the supplied buffer before hashing. */
    GFX_GOTO_ON_FALSE(stored_len <= data_len - EAF_TABLE_OFFSET, GFX_ERR_INVALID_SIZE, err, TAG,
                      "table length %" PRIu32 " exceeds buffer", stored_len);

    /* Frame table must fit: EAF_TABLE_OFFSET + total_frames * entry_size. The
     * division form avoids size_t overflow on a corrupt frame count. */
    GFX_GOTO_ON_FALSE(total_frames > 0, GFX_ERR_INVALID_SIZE, err, TAG, "non-positive frame count");
    size_t entry_size = sizeof(eaf_dec_frame_table_entry_t);
    GFX_GOTO_ON_FALSE((size_t)total_frames <= (data_len - EAF_TABLE_OFFSET) / entry_size,
                      GFX_ERR_INVALID_SIZE, err, TAG, "frame table %d exceeds buffer", total_frames);

    uint32_t calculated_chk = dec_calculate_checksum((uint8_t *)(data + EAF_TABLE_OFFSET), stored_len);
    GFX_GOTO_ON_FALSE(calculated_chk == stored_chk, GFX_ERR_INVALID_CRC, err, TAG, "bad full checksum");

    entries = (eaf_dec_frame_entry_t *)malloc(sizeof(eaf_dec_frame_entry_t) * (size_t)total_frames);
    GFX_GOTO_ON_FALSE(entries, GFX_ERR_NO_MEM, err, TAG, "no mem for frame entries");

    eaf_dec_frame_table_entry_t *table = (eaf_dec_frame_table_entry_t *)(data + EAF_TABLE_OFFSET);
    const size_t frame_base = (size_t)EAF_TABLE_OFFSET + (size_t)total_frames * entry_size;
    for (int i = 0; i < total_frames; i++) {
        /* Validate frame_base + frame_offset + frame_size <= data_len without
         * overflowing, so frame_mem and its magic stay inside the buffer. */
        uint32_t frame_offset = table[i].frame_offset;
        uint32_t frame_size = table[i].frame_size;
        GFX_GOTO_ON_FALSE(frame_size >= EAF_MAGIC_LEN, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d size %" PRIu32 " too small", i, frame_size);
        GFX_GOTO_ON_FALSE(frame_offset <= data_len - frame_base, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d offset out of range", i);
        GFX_GOTO_ON_FALSE(frame_size <= data_len - frame_base - frame_offset, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d extends past buffer", i);

        (entries + i)->table = (table + i);
        (entries + i)->frame_mem = (const char *)(data + frame_base + frame_offset);

        uint16_t magic = eaf_rd_u16((const uint8_t *)(entries + i)->frame_mem);
        GFX_GOTO_ON_FALSE(magic == EAF_MAGIC_HEAD, GFX_ERR_INVALID_CRC, err, TAG, "bad file magic header");
    }

    parser->entries = entries;
    parser->total_frames = total_frames;

    *ret_parser = (eaf_dec_handle_t)parser;

    return GFX_OK;

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

gfx_err_t eaf_dec_init_reader(const eaf_dec_reader_t *reader, eaf_dec_handle_t *ret_parser)
{
    gfx_err_t init_ret = eaf_dec_ensure_decoders();
    if (init_ret != GFX_OK) {
        return init_ret;
    }

    if (reader == NULL || reader->read == NULL || ret_parser == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    *ret_parser = NULL;

    gfx_err_t ret = GFX_OK;
    eaf_dec_ctx_t *parser = NULL;
    eaf_dec_frame_entry_t *entries = NULL;
    eaf_dec_frame_table_entry_t *table_copy = NULL;
    uint8_t *chunk = NULL;

    const size_t size = reader->size;
    if (size < EAF_TABLE_OFFSET) {
        GFX_LOGE(TAG, "stream too small for header: %zu", size);
        return GFX_ERR_INVALID_SIZE;
    }

    /* Fixed file header (magic, format string, frame count, checksum, table len). */
    uint8_t head[EAF_TABLE_OFFSET];
    GFX_RETURN_ON_ERROR(reader->read(reader->io_ctx, 0, sizeof(head), head), TAG, "read header");

    GFX_RETURN_ON_FALSE(head[EAF_FORMAT_OFFSET] == EAF_FORMAT_MAGIC, GFX_ERR_INVALID_CRC, TAG,
                        "bad file format magic");
    bool is_valid = (memcmp(head + EAF_STR_OFFSET, EAF_FORMAT_STR, 3) == 0) ||
                    (memcmp(head + EAF_STR_OFFSET, AAF_FORMAT_STR, 3) == 0);
    GFX_RETURN_ON_FALSE(is_valid, GFX_ERR_INVALID_CRC, TAG, "bad file format string (expected EAF or AAF)");

    int total_frames = eaf_rd_i32(head + EAF_NUM_OFFSET);
    uint32_t stored_chk = eaf_rd_u32(head + EAF_CHECKSUM_OFFSET);
    uint32_t stored_len = eaf_rd_u32(head + EAF_TABLE_LEN);

    GFX_RETURN_ON_FALSE(stored_len <= size - EAF_TABLE_OFFSET, GFX_ERR_INVALID_SIZE, TAG,
                        "table length %" PRIu32 " exceeds stream", stored_len);
    GFX_RETURN_ON_FALSE(total_frames > 0, GFX_ERR_INVALID_SIZE, TAG, "non-positive frame count");
    const size_t entry_size = sizeof(eaf_dec_frame_table_entry_t);
    GFX_RETURN_ON_FALSE((size_t)total_frames <= (size - EAF_TABLE_OFFSET) / entry_size,
                        GFX_ERR_INVALID_SIZE, TAG, "frame table %d exceeds stream", total_frames);

    parser = (eaf_dec_ctx_t *)calloc(1, sizeof(eaf_dec_ctx_t));
    GFX_GOTO_ON_FALSE(parser, GFX_ERR_NO_MEM, err, TAG, "no mem for parser handle");

    /* Checksum spans data[EAF_TABLE_OFFSET .. +stored_len). Hash it in fixed
     * chunks so peak RAM stays at the chunk size instead of the whole region. */
    const size_t chunk_cap = 2048;
    chunk = (uint8_t *)malloc(chunk_cap);
    GFX_GOTO_ON_FALSE(chunk, GFX_ERR_NO_MEM, err, TAG, "no mem for checksum chunk");
    uint32_t calc = 0;
    size_t remaining = stored_len;
    size_t off = EAF_TABLE_OFFSET;
    while (remaining > 0) {
        size_t n = remaining < chunk_cap ? remaining : chunk_cap;
        GFX_GOTO_ON_ERROR(reader->read(reader->io_ctx, off, n, chunk), err, TAG, "read checksum region");
        for (size_t i = 0; i < n; i++) {
            calc += chunk[i];
        }
        off += n;
        remaining -= n;
    }
    GFX_GOTO_ON_FALSE(calc == stored_chk, GFX_ERR_INVALID_CRC, err, TAG, "bad full checksum");
    free(chunk);
    chunk = NULL;

    /* Copy and integerize the frame table (small metadata kept in memory). */
    table_copy = (eaf_dec_frame_table_entry_t *)malloc(entry_size * (size_t)total_frames);
    GFX_GOTO_ON_FALSE(table_copy, GFX_ERR_NO_MEM, err, TAG, "no mem for frame table copy");
    GFX_GOTO_ON_ERROR(reader->read(reader->io_ctx, EAF_TABLE_OFFSET, entry_size * (size_t)total_frames,
                                   (uint8_t *)table_copy), err, TAG, "read frame table");

    entries = (eaf_dec_frame_entry_t *)malloc(sizeof(eaf_dec_frame_entry_t) * (size_t)total_frames);
    GFX_GOTO_ON_FALSE(entries, GFX_ERR_NO_MEM, err, TAG, "no mem for frame entries");

    const size_t frame_base = (size_t)EAF_TABLE_OFFSET + (size_t)total_frames * entry_size;
    for (int i = 0; i < total_frames; i++) {
        uint32_t frame_offset = table_copy[i].frame_offset;
        uint32_t frame_size = table_copy[i].frame_size;
        GFX_GOTO_ON_FALSE(frame_size >= EAF_MAGIC_LEN, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d size %" PRIu32 " too small", i, frame_size);
        GFX_GOTO_ON_FALSE(frame_offset <= size - frame_base, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d offset out of range", i);
        GFX_GOTO_ON_FALSE(frame_size <= size - frame_base - frame_offset, GFX_ERR_INVALID_SIZE, err, TAG,
                          "frame %d extends past stream", i);

        /* Validate the frame magic header with a small targeted read. */
        uint8_t magic_bytes[EAF_MAGIC_LEN];
        GFX_GOTO_ON_ERROR(reader->read(reader->io_ctx, frame_base + frame_offset, EAF_MAGIC_LEN, magic_bytes),
                          err, TAG, "read frame %d magic", i);
        GFX_GOTO_ON_FALSE(eaf_rd_u16(magic_bytes) == EAF_MAGIC_HEAD, GFX_ERR_INVALID_CRC, err, TAG,
                          "frame %d bad magic header", i);

        entries[i].table = &table_copy[i];
        entries[i].frame_mem = NULL; /* streaming: payload pulled on demand */
    }

    parser->entries = entries;
    parser->total_frames = total_frames;
    parser->streaming = true;
    parser->reader = *reader;
    parser->frame_base = frame_base;
    parser->table_copy = table_copy;
    parser->loaded_frame = -1;

    *ret_parser = (eaf_dec_handle_t)parser;
    return GFX_OK;

err:
    free(chunk);
    free(entries);
    free(table_copy);
    free(parser);
    *ret_parser = NULL;
    return ret;
}

gfx_err_t eaf_dec_deinit(eaf_dec_handle_t handle)
{
    if (handle == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    eaf_dec_ctx_t *parser = (eaf_dec_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser->huff_tmp);
        free(parser->huff_nodes);
        /* Streaming-only resources; NULL in copy/direct mode. The reader's backend
         * is owned by the caller and is not closed here. */
        free(parser->table_copy);
        free(parser->frame_buf);
        free(parser);
    }
    return GFX_OK;
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

    if (parser->streaming) {
        if (index < 0 || index >= parser->total_frames) {
            GFX_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
            return NULL;
        }
        const eaf_dec_frame_table_entry_t *t = &parser->table_copy[index];
        if (t->frame_size < EAF_MAGIC_LEN) {
            return NULL;
        }
        /* Payload starts after the 2-byte frame magic, matching the copy/direct
         * path that returns frame_mem + EAF_MAGIC_LEN. */
        size_t payload_off = parser->frame_base + t->frame_offset + EAF_MAGIC_LEN;
        size_t payload_len = (size_t)t->frame_size - EAF_MAGIC_LEN;

        /* Read on demand into the reused per-frame buffer; same frame is a
         * cache hit so repeated calls within one frame do not re-read. */
        if (parser->loaded_frame != index) {
            if (parser->frame_buf_cap < payload_len) {
                uint8_t *p = (uint8_t *)realloc(parser->frame_buf, payload_len);
                if (p == NULL) {
                    GFX_LOGE(TAG, "No mem for streaming frame buffer (%zu)", payload_len);
                    return NULL;
                }
                parser->frame_buf = p;
                parser->frame_buf_cap = payload_len;
            }
            if (parser->reader.read(parser->reader.io_ctx, payload_off, payload_len, parser->frame_buf) != GFX_OK) {
                parser->loaded_frame = -1;
                GFX_LOGE(TAG, "Stream read failed for frame %d", index);
                return NULL;
            }
            parser->loaded_frame = index;
        }
        return parser->frame_buf;
    }

    if (index >= 0 && index < parser->total_frames) {
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

    if (index >= 0 && index < parser->total_frames) {
        return ((parser->entries + index)->table->frame_size - EAF_MAGIC_LEN);
    } else {
        GFX_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return -1;
    }
}

gfx_err_t eaf_dec_decode_frame(eaf_dec_handle_t handle, int frame_index,
                               uint8_t *out_data, size_t out_size)
{
    if (!handle || !out_data) {
        return GFX_ERR_INVALID_STATE;
    }

    const uint8_t *frame_data = eaf_dec_get_frame_data(handle, frame_index);
    if (!frame_data) {
        GFX_LOGE(TAG, "Frame %d data unavailable", frame_index);
        return GFX_FAIL;
    }

    eaf_dec_header_t header;
    eaf_dec_type_t format = eaf_dec_get_frame_info(handle, frame_index, &header);
    if (format != EAF_DEC_TYPE_VALID) {
        GFX_LOGE(TAG, "Frame %d header parse failed", frame_index);
        return GFX_FAIL;
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
        return GFX_ERR_NO_MEM;
    }
    eaf_dec_calculate_offsets(&header, offsets);

    uint8_t *tmp_data = calloc(1, block_size);
    if (!tmp_data) {
        GFX_LOGE(TAG, "No mem for block buffer");
        free(offsets);
        eaf_dec_free_header(&header);
        return GFX_ERR_NO_MEM;
    }

    uint32_t palette_cache[256];
    memset(palette_cache, 0xFF, sizeof(palette_cache));

    for (int block = 0; block < header.blocks; block++) {
        const uint8_t *block_data = frame_data + offsets[block];
        int block_len = header.block_len[block];
        gfx_err_t ret = eaf_dec_decode_block(handle, &header, block_data, block_len, tmp_data);

        if (ret != GFX_OK) {
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
                    eaf_dec_get_palette_color(&header, index, &eaf_color);
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

    return GFX_OK;
}
