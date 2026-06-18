/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "esp_check.h"
#include "esp_err.h"
#include "core/gfx_asset.h"
#include "core/base/gfx_asset_source.h"
#include "lib/eaf/gfx_eaf_dec.h"
#include "codecs/anim/gfx_anim_decoder_priv.h"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    eaf_dec_handle_t eaf;
    gfx_asset_source_t source;
} gfx_anim_eaf_decoder_ctx_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_anim_eaf_read_frame_desc(void *ctx, uint32_t frame_index, gfx_anim_frame_desc_t *frame_desc);
static void gfx_anim_eaf_free_frame_desc(gfx_anim_frame_desc_t *frame_desc);

static bool gfx_anim_eaf_probe(const gfx_anim_src_t *src)
{
    const uint8_t *data = NULL;
    gfx_asset_source_t source = {0};
    bool matched = false;

    if (src == NULL || src->data == NULL) {
        return false;
    }

    if (src->type == GFX_ANIM_SRC_TYPE_FILE) {
        if (gfx_asset_source_load((const char *)src->data, &source) != ESP_OK ||
                source.size < sizeof(eaf_dec_header_t)) {
            gfx_asset_source_release(&source);
            return false;
        }
        data = source.data;
    } else if (src->type == GFX_ANIM_SRC_TYPE_MEMORY && src->data_len >= sizeof(eaf_dec_header_t)) {
        data = (const uint8_t *)src->data;
    } else {
        return false;
    }

    if (data[EAF_FORMAT_OFFSET] == EAF_FORMAT_MAGIC) {
        matched = (memcmp(data + EAF_STR_OFFSET, EAF_FORMAT_STR, 3) == 0) ||
                  (memcmp(data + EAF_STR_OFFSET, AAF_FORMAT_STR, 3) == 0);
    }

    gfx_asset_source_release(&source);
    return matched;
}

static esp_err_t gfx_anim_eaf_open(const gfx_anim_src_t *src, void **out_ctx)
{
    gfx_anim_eaf_decoder_ctx_t *ctx = NULL;
    const uint8_t *data = NULL;
    size_t data_len = 0;

    if (src == NULL || out_ctx == NULL || src->data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* Load the whole eaf/aaf once into a resident range. All frames are decoded
     * from this buffer at runtime; no per-frame file reads. Direct backends
     * (mmap-assets / raw partition mmap) stay zero-copy, while VFS/SPIFFS and
     * the fread fallback own a heap copy released in close(). */
    if (src->type == GFX_ANIM_SRC_TYPE_FILE) {
        esp_err_t ret = gfx_asset_source_load((const char *)src->data, &ctx->source);
        if (ret != ESP_OK) {
            free(ctx);
            return ret;
        }
        data = ctx->source.data;
        data_len = ctx->source.size;
    } else if (src->type == GFX_ANIM_SRC_TYPE_MEMORY && src->data_len > 0U) {
        data = (const uint8_t *)src->data;
        data_len = src->data_len;
    } else {
        free(ctx);
        return ESP_ERR_INVALID_SIZE;
    }

    if (eaf_dec_init(data, data_len, &ctx->eaf) != ESP_OK) {
        gfx_asset_source_release(&ctx->source);
        free(ctx);
        return ESP_FAIL;
    }

    *out_ctx = ctx;
    return ESP_OK;
}

static void gfx_anim_eaf_close(void *ctx)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;

    if (decoder_ctx == NULL) {
        return;
    }

    if (decoder_ctx->eaf != NULL) {
        eaf_dec_deinit(decoder_ctx->eaf);
    }
    gfx_asset_source_release(&decoder_ctx->source);

    free(decoder_ctx);
}

static uint32_t gfx_anim_eaf_get_frame_count(void *ctx)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;
    int eaf_frame_count;

    if (decoder_ctx == NULL) {
        return 0;
    }

    eaf_frame_count = eaf_dec_get_total_frames(decoder_ctx->eaf);
    return eaf_frame_count > 0 ? (uint32_t)(eaf_frame_count - 1) : 0;
}

static esp_err_t gfx_anim_eaf_get_info(void *ctx, gfx_anim_info_t *info)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;
    gfx_anim_frame_desc_t frame_desc;
    uint32_t frame_count;
    esp_err_t ret;

    if (decoder_ctx == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame_count = gfx_anim_eaf_get_frame_count(ctx);
    if (frame_count == 0U) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(&frame_desc, 0, sizeof(frame_desc));
    ret = gfx_anim_eaf_read_frame_desc(ctx, 0, &frame_desc);
    if (ret != ESP_OK) {
        return ret;
    }

    info->width = frame_desc.width;
    info->height = frame_desc.height;
    info->frame_count = frame_count;
    gfx_anim_eaf_free_frame_desc(&frame_desc);
    return (info->width > 0U && info->height > 0U) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static void gfx_anim_eaf_import_frame_desc(gfx_anim_frame_desc_t *frame_desc, eaf_dec_header_t *header)
{
    memset(frame_desc, 0, sizeof(*frame_desc));
    frame_desc->bit_depth = header->bit_depth;
    frame_desc->width = header->width;
    frame_desc->height = header->height;
    frame_desc->blocks = header->blocks;
    frame_desc->block_height = header->block_height;
    frame_desc->block_len = header->block_len;
    frame_desc->data_offset = header->data_offset;
    frame_desc->palette = header->palette;
    frame_desc->num_colors = (uint16_t)header->num_colors;

    header->block_len = NULL;
    header->palette = NULL;
}

static void gfx_anim_eaf_export_frame_desc(const gfx_anim_frame_desc_t *frame_desc, eaf_dec_header_t *header)
{
    memset(header, 0, sizeof(*header));
    header->bit_depth = frame_desc->bit_depth;
    header->width = frame_desc->width;
    header->height = frame_desc->height;
    header->blocks = frame_desc->blocks;
    header->block_height = frame_desc->block_height;
    header->block_len = frame_desc->block_len;
    header->data_offset = frame_desc->data_offset;
    header->palette = frame_desc->palette;
    header->num_colors = frame_desc->num_colors;
}

static esp_err_t gfx_anim_eaf_read_frame_desc(void *ctx, uint32_t frame_index, gfx_anim_frame_desc_t *frame_desc)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;
    eaf_dec_header_t header;
    eaf_dec_type_t format;

    if (decoder_ctx == NULL || frame_desc == NULL || frame_index > INT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&header, 0, sizeof(header));
    format = eaf_dec_get_frame_info(decoder_ctx->eaf, (int)frame_index, &header);
    if (format != EAF_DEC_TYPE_VALID) {
        eaf_dec_free_header(&header);
        return ESP_ERR_INVALID_RESPONSE;
    }

    gfx_anim_eaf_import_frame_desc(frame_desc, &header);
    return ESP_OK;
}

static void gfx_anim_eaf_free_frame_desc(gfx_anim_frame_desc_t *frame_desc)
{
    if (frame_desc == NULL) {
        return;
    }

    free(frame_desc->block_len);
    free(frame_desc->palette);
    memset(frame_desc, 0, sizeof(*frame_desc));
}

static const uint8_t *gfx_anim_eaf_get_frame_payload(void *ctx, uint32_t frame_index)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;
    return decoder_ctx != NULL && frame_index <= INT_MAX ?
           eaf_dec_get_frame_data(decoder_ctx->eaf, (int)frame_index) : NULL;
}

static size_t gfx_anim_eaf_get_frame_payload_size(void *ctx, uint32_t frame_index)
{
    gfx_anim_eaf_decoder_ctx_t *decoder_ctx = (gfx_anim_eaf_decoder_ctx_t *)ctx;
    int payload_size;

    if (decoder_ctx == NULL || frame_index > INT_MAX) {
        return 0;
    }

    payload_size = eaf_dec_get_frame_size(decoder_ctx->eaf, (int)frame_index);
    return payload_size > 0 ? (size_t)payload_size : 0;
}

static esp_err_t gfx_anim_eaf_decode_frame_block(const gfx_anim_frame_desc_t *frame_desc,
        const uint8_t *block_payload, size_t block_payload_size, uint8_t *out_pixels)
{
    eaf_dec_header_t header;

    if (frame_desc == NULL || block_payload_size > INT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_anim_eaf_export_frame_desc(frame_desc, &header);
    return eaf_dec_decode_block(&header, block_payload, (int)block_payload_size, out_pixels);
}

static bool gfx_anim_eaf_read_palette_color(const gfx_anim_frame_desc_t *frame_desc, uint8_t color_index,
        gfx_color_t *result)
{
    eaf_dec_header_t header;

    if (frame_desc == NULL || result == NULL) {
        return true;
    }

    gfx_anim_eaf_export_frame_desc(frame_desc, &header);
    return eaf_dec_get_palette_color(&header, color_index, result);
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

const gfx_anim_decoder_t *gfx_anim_eaf_decoder_get(void)
{
    static const gfx_anim_decoder_t s_eaf_decoder = {
        .name = "eaf",
        .probe = gfx_anim_eaf_probe,
        .open = gfx_anim_eaf_open,
        .close = gfx_anim_eaf_close,
        .get_info = gfx_anim_eaf_get_info,
        .get_frame_count = gfx_anim_eaf_get_frame_count,
        .read_frame_desc = gfx_anim_eaf_read_frame_desc,
        .free_frame_desc = gfx_anim_eaf_free_frame_desc,
        .get_frame_payload = gfx_anim_eaf_get_frame_payload,
        .get_frame_payload_size = gfx_anim_eaf_get_frame_payload_size,
        .decode_frame_block = gfx_anim_eaf_decode_frame_block,
        .read_palette_color = gfx_anim_eaf_read_palette_color,
    };

    return &s_eaf_decoder;
}
