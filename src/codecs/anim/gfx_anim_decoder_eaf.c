/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "common/gfx_check.h"
#include "core/fs/gfx_fs_priv.h"
#include "gfx/fs.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_ANIM_DECODER
#include "common/gfx_log_priv.h"
#include "lib/eaf/gfx_eaf_dec.h"
#include "codecs/anim/gfx_anim_decoder_priv.h"

static const char *const TAG = "anim_decoder";

typedef struct {
    uint8_t header[sizeof(eaf_dec_header_t)];
} gfx_anim_eaf_probe_buf_t;

typedef enum {
    GFX_ANIM_EAF_SOURCE_NONE = 0,
    GFX_ANIM_EAF_SOURCE_MEMORY,
    GFX_ANIM_EAF_SOURCE_FILE_DIRECT,
    GFX_ANIM_EAF_SOURCE_FILE_STREAM,
    GFX_ANIM_EAF_SOURCE_FILE_COPY,
} gfx_anim_eaf_source_mode_t;

typedef struct {
    gfx_anim_eaf_source_mode_t mode;
    gfx_fs_file_t *file;
    gfx_fs_blob_t blob;
    const uint8_t *bytes;
    size_t size;
} gfx_anim_eaf_source_t;

typedef struct {
    eaf_dec_handle_t eaf;
    gfx_anim_eaf_source_t source;
} gfx_anim_eaf_decoder_t;

/* Keep the EAF core file-system agnostic: streaming mode sees only this
 * random-access reader, much like a small Linux file_operations bridge. */
static gfx_err_t gfx_anim_eaf_source_read(void *io_ctx, size_t offset, size_t len, uint8_t *dst)
{
    gfx_fs_file_t *file = (gfx_fs_file_t *)io_ctx;

    if (file == NULL || dst == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    if (gfx_fs_fseek(file, (long)offset, SEEK_SET) != 0) {
        return GFX_ERR_INVALID_SIZE;
    }
    return gfx_fs_fread(file, dst, len) == len ? GFX_OK : GFX_FAIL;
}

/**********************
 *  STATIC PROTOTYPES
 **********************/

static gfx_err_t gfx_anim_eaf_read_frame_desc(void *ctx, uint32_t frame_index, gfx_anim_frame_desc_t *frame_desc);
static void gfx_anim_eaf_free_frame_desc(gfx_anim_frame_desc_t *frame_desc);

static const char *gfx_anim_eaf_source_mode_name(gfx_anim_eaf_source_mode_t mode)
{
    switch (mode) {
    case GFX_ANIM_EAF_SOURCE_MEMORY:
        return "memory";
    case GFX_ANIM_EAF_SOURCE_FILE_DIRECT:
        return "file-direct";
    case GFX_ANIM_EAF_SOURCE_FILE_STREAM:
        return "file-stream";
    case GFX_ANIM_EAF_SOURCE_FILE_COPY:
        return "file-copy";
    default:
        return "none";
    }
}

static void gfx_anim_eaf_source_release(gfx_anim_eaf_source_t *source)
{
    if (source == NULL) {
        return;
    }

    switch (source->mode) {
    case GFX_ANIM_EAF_SOURCE_FILE_DIRECT:
    case GFX_ANIM_EAF_SOURCE_FILE_STREAM:
        GFX_LOGD(TAG, "eaf close: mode=%s bytes=%zu",
                 gfx_anim_eaf_source_mode_name(source->mode), source->size);
        gfx_fs_fclose(source->file);
        break;
    case GFX_ANIM_EAF_SOURCE_FILE_COPY:
        GFX_LOGD(TAG, "eaf close: mode=%s bytes=%zu",
                 gfx_anim_eaf_source_mode_name(source->mode), source->blob.size);
        gfx_fs_unload(&source->blob);
        break;
    default:
        break;
    }

    memset(source, 0, sizeof(*source));
}

static gfx_err_t gfx_anim_eaf_bind_direct(gfx_anim_eaf_decoder_t *decoder,
        const void *bytes, size_t size, gfx_anim_eaf_source_mode_t mode)
{
    if (decoder == NULL || bytes == NULL || size == 0U) {
        return GFX_ERR_INVALID_ARG;
    }

    if (eaf_dec_init(bytes, size, &decoder->eaf) != GFX_OK) {
        return GFX_FAIL;
    }
    decoder->source.bytes = (const uint8_t *)bytes;
    decoder->source.size = size;
    decoder->source.mode = mode;
    return GFX_OK;
}

static gfx_err_t gfx_anim_eaf_bind_stream(gfx_anim_eaf_decoder_t *decoder, gfx_fs_file_t *file, size_t size)
{
    eaf_dec_reader_t reader = {
        .io_ctx = file,
        .size = size,
        .read = gfx_anim_eaf_source_read,
    };

    if (decoder == NULL || file == NULL || size == 0U) {
        return GFX_ERR_INVALID_ARG;
    }

    if (eaf_dec_init_reader(&reader, &decoder->eaf) != GFX_OK) {
        return GFX_FAIL;
    }

    decoder->source.file = file;
    decoder->source.size = size;
    decoder->source.mode = GFX_ANIM_EAF_SOURCE_FILE_STREAM;
    return GFX_OK;
}

static gfx_err_t gfx_anim_eaf_source_open_file(gfx_anim_eaf_decoder_t *decoder, const gfx_anim_src_t *src)
{
    gfx_fs_file_t *file;
    const void *direct;
    size_t size;
    gfx_err_t err;

    if (decoder == NULL || src == NULL || src->data == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    file = gfx_fs_fopen((const char *)src->data);
    if (file == NULL) {
        return GFX_ERR_NOT_FOUND;
    }

    direct = gfx_fs_fdata(file);
    size = gfx_fs_fsize(file);

    if (direct != NULL) {
        err = gfx_anim_eaf_bind_direct(decoder, direct, size, GFX_ANIM_EAF_SOURCE_FILE_DIRECT);
        if (err == GFX_OK) {
            decoder->source.file = file;
            GFX_LOGD(TAG, "eaf open: src=file payload=%p mode=%s bytes=%zu",
                     src->data, gfx_anim_eaf_source_mode_name(decoder->source.mode), size);
            return GFX_OK;
        }
        gfx_fs_fclose(file);
        return err;
    }

    if ((src->flags & GFX_ANIM_SRC_FLAG_STREAMING) != 0U) {
        err = gfx_anim_eaf_bind_stream(decoder, file, size);
        if (err == GFX_OK) {
            GFX_LOGD(TAG, "eaf open: src=file payload=%p mode=%s bytes=%zu",
                     src->data, gfx_anim_eaf_source_mode_name(decoder->source.mode), size);
            return GFX_OK;
        }
        GFX_LOGW(TAG, "eaf open: streaming setup failed; fallback to copy");
    }

    err = gfx_fs_blob_take_file(file, &decoder->source.blob);
    if (err != GFX_OK) {
        gfx_fs_fclose(file);
        return err;
    }

    err = gfx_anim_eaf_bind_direct(decoder, decoder->source.blob.data, decoder->source.blob.size,
                                   GFX_ANIM_EAF_SOURCE_FILE_COPY);
    if (err != GFX_OK) {
        gfx_fs_unload(&decoder->source.blob);
        memset(&decoder->source, 0, sizeof(decoder->source));
        return err;
    }

    GFX_LOGD(TAG, "eaf open: src=file payload=%p mode=%s bytes=%zu",
             src->data, gfx_anim_eaf_source_mode_name(decoder->source.mode), decoder->source.size);
    return GFX_OK;
}

static gfx_err_t gfx_anim_eaf_source_open_memory(gfx_anim_eaf_decoder_t *decoder, const gfx_anim_src_t *src)
{
    gfx_err_t err;

    if (decoder == NULL || src == NULL || src->data == NULL || src->data_len == 0U) {
        return GFX_ERR_INVALID_SIZE;
    }

    err = gfx_anim_eaf_bind_direct(decoder, src->data, src->data_len, GFX_ANIM_EAF_SOURCE_MEMORY);
    if (err == GFX_OK) {
        GFX_LOGD(TAG, "eaf open: src=memory payload=%p mode=%s bytes=%zu",
                 src->data, gfx_anim_eaf_source_mode_name(decoder->source.mode), src->data_len);
    }
    return err;
}

static bool gfx_anim_eaf_probe(const gfx_anim_src_t *src)
{
    const uint8_t *data = NULL;
    gfx_fs_file_t *file = NULL;
    gfx_anim_eaf_probe_buf_t probe_buf;
    bool matched = false;

    if (src == NULL || src->data == NULL) {
        return false;
    }

    if (src->type == GFX_ANIM_SRC_TYPE_FILE) {
        file = gfx_fs_fopen((const char *)src->data);
        if (file == NULL || gfx_fs_fsize(file) < sizeof(probe_buf.header)) {
            gfx_fs_fclose(file);
            return false;
        }
        data = gfx_fs_fdata(file);
        if (data == NULL) {
            if (gfx_fs_fread(file, probe_buf.header, sizeof(probe_buf.header)) != sizeof(probe_buf.header)) {
                gfx_fs_fclose(file);
                return false;
            }
            data = probe_buf.header;
        }
    } else if (src->type == GFX_ANIM_SRC_TYPE_MEMORY && src->data_len >= sizeof(eaf_dec_header_t)) {
        data = (const uint8_t *)src->data;
    } else {
        return false;
    }

    if (data[EAF_FORMAT_OFFSET] == EAF_FORMAT_MAGIC) {
        matched = (memcmp(data + EAF_STR_OFFSET, EAF_FORMAT_STR, 3) == 0) ||
                  (memcmp(data + EAF_STR_OFFSET, AAF_FORMAT_STR, 3) == 0);
    }

    gfx_fs_fclose(file);
    return matched;
}

static gfx_err_t gfx_anim_eaf_open(const gfx_anim_src_t *src, void **out_ctx)
{
    gfx_anim_eaf_decoder_t *decoder = NULL;
    gfx_err_t err;

    if (src == NULL || out_ctx == NULL || src->data == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    decoder = calloc(1, sizeof(*decoder));
    if (decoder == NULL) {
        return GFX_ERR_NO_MEM;
    }

    if (src->type == GFX_ANIM_SRC_TYPE_FILE) {
        err = gfx_anim_eaf_source_open_file(decoder, src);
    } else if (src->type == GFX_ANIM_SRC_TYPE_MEMORY && src->data_len > 0U) {
        err = gfx_anim_eaf_source_open_memory(decoder, src);
    } else {
        free(decoder);
        return GFX_ERR_INVALID_SIZE;
    }

    if (err != GFX_OK) {
        gfx_anim_eaf_source_release(&decoder->source);
        free(decoder);
        return err;
    }

    *out_ctx = decoder;
    return GFX_OK;
}

static void gfx_anim_eaf_close(void *ctx)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;

    if (decoder == NULL) {
        return;
    }

    if (decoder->eaf != NULL) {
        eaf_dec_deinit(decoder->eaf);
    }
    gfx_anim_eaf_source_release(&decoder->source);

    free(decoder);
}

static uint32_t gfx_anim_eaf_get_frame_count(void *ctx)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    int eaf_frame_count;

    if (decoder == NULL) {
        return 0;
    }

    eaf_frame_count = eaf_dec_get_total_frames(decoder->eaf);
    return eaf_frame_count > 0 ? (uint32_t)(eaf_frame_count - 1) : 0;
}

static gfx_err_t gfx_anim_eaf_get_info(void *ctx, gfx_anim_info_t *info)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    gfx_anim_frame_desc_t frame_desc;
    uint32_t frame_count;
    gfx_err_t ret;

    if (decoder == NULL || info == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    frame_count = gfx_anim_eaf_get_frame_count(ctx);
    if (frame_count == 0U) {
        return GFX_ERR_INVALID_SIZE;
    }

    memset(&frame_desc, 0, sizeof(frame_desc));
    ret = gfx_anim_eaf_read_frame_desc(ctx, 0, &frame_desc);
    if (ret != GFX_OK) {
        return ret;
    }

    info->width = frame_desc.width;
    info->height = frame_desc.height;
    info->frame_count = frame_count;
    gfx_anim_eaf_free_frame_desc(&frame_desc);
    return (info->width > 0U && info->height > 0U) ? GFX_OK : GFX_ERR_INVALID_SIZE;
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

static gfx_err_t gfx_anim_eaf_read_frame_desc(void *ctx, uint32_t frame_index, gfx_anim_frame_desc_t *frame_desc)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    eaf_dec_header_t header;
    eaf_dec_type_t format;

    if (decoder == NULL || frame_desc == NULL || frame_index > INT_MAX) {
        return GFX_ERR_INVALID_ARG;
    }

    memset(&header, 0, sizeof(header));
    format = eaf_dec_get_frame_info(decoder->eaf, (int)frame_index, &header);
    if (format != EAF_DEC_TYPE_VALID) {
        eaf_dec_free_header(&header);
        return GFX_ERR_INVALID_RESPONSE;
    }

    gfx_anim_eaf_import_frame_desc(frame_desc, &header);
    return GFX_OK;
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
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    return decoder != NULL && frame_index <= INT_MAX ?
           eaf_dec_get_frame_data(decoder->eaf, (int)frame_index) : NULL;
}

static size_t gfx_anim_eaf_get_frame_payload_size(void *ctx, uint32_t frame_index)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    int payload_size;

    if (decoder == NULL || frame_index > INT_MAX) {
        return 0;
    }

    payload_size = eaf_dec_get_frame_size(decoder->eaf, (int)frame_index);
    return payload_size > 0 ? (size_t)payload_size : 0;
}

static gfx_err_t gfx_anim_eaf_decode_frame_block(void *ctx, const gfx_anim_frame_desc_t *frame_desc,
        const uint8_t *block_payload, size_t block_payload_size, uint8_t *out_pixels)
{
    gfx_anim_eaf_decoder_t *decoder = (gfx_anim_eaf_decoder_t *)ctx;
    eaf_dec_header_t header;

    if (decoder == NULL || frame_desc == NULL || block_payload_size > INT_MAX) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_anim_eaf_export_frame_desc(frame_desc, &header);
    /* Pass the parser handle so the EAF decoder reuses its per-handle Huffman
     * scratch (tmp buffer + node arena) instead of allocating per block. */
    return eaf_dec_decode_block(decoder->eaf, &header, block_payload, (int)block_payload_size, out_pixels);
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
