/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "lib/eaf/gfx_eaf_dec.h"
#include "widget/anim/gfx_anim_decoder_priv.h"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    eaf_dec_handle_t eaf_handle;
} gfx_anim_eaf_handle_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_anim_src_get_size(const gfx_anim_src_t *src_desc, size_t *out_size)
{
    if (src_desc == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (src_desc->type == GFX_ANIM_SRC_TYPE_MEMORY && src_desc->data != NULL && src_desc->data_len > 0) {
        *out_size = src_desc->data_len;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t gfx_anim_src_peek(const gfx_anim_src_t *src_desc, size_t offset, size_t len,
                                   const uint8_t **out_data)
{
    size_t total_size = 0;

    if (src_desc == NULL || out_data == NULL || len == 0 || src_desc->data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (gfx_anim_src_get_size(src_desc, &total_size) != ESP_OK || (offset + len) > total_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    *out_data = (const uint8_t *)src_desc->data + offset;
    return ESP_OK;
}

static bool gfx_anim_eaf_can_open(const gfx_anim_src_t *src_desc)
{
    const uint8_t *data = NULL;

    if (src_desc == NULL || src_desc->data == NULL || src_desc->data_len < sizeof(eaf_dec_header_t)) {
        return false;
    }

    if (gfx_anim_src_peek(src_desc, 0, EAF_TABLE_OFFSET, &data) != ESP_OK) {
        return false;
    }

    if (data[EAF_FORMAT_OFFSET] != EAF_FORMAT_MAGIC) {
        return false;
    }

    return (memcmp(data + EAF_STR_OFFSET, EAF_FORMAT_STR, 3) == 0) ||
           (memcmp(data + EAF_STR_OFFSET, AAF_FORMAT_STR, 3) == 0);
}

static esp_err_t gfx_anim_eaf_open(const gfx_anim_src_t *src_desc, void **out_handle)
{
    gfx_anim_eaf_handle_t *handle = NULL;
    size_t data_len = 0;

    if (src_desc == NULL || out_handle == NULL || src_desc->data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (gfx_anim_src_get_size(src_desc, &data_len) != ESP_OK) {
        free(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    if (eaf_dec_init(src_desc->data, data_len, &handle->eaf_handle) != ESP_OK) {
        free(handle);
        return ESP_FAIL;
    }

    *out_handle = handle;
    return ESP_OK;
}

static void gfx_anim_eaf_close(void *handle)
{
    gfx_anim_eaf_handle_t *eaf_handle = (gfx_anim_eaf_handle_t *)handle;

    if (eaf_handle == NULL) {
        return;
    }

    if (eaf_handle->eaf_handle != NULL) {
        eaf_dec_deinit(eaf_handle->eaf_handle);
    }

    free(eaf_handle);
}

static int gfx_anim_eaf_get_total_frames(void *handle)
{
    gfx_anim_eaf_handle_t *eaf_handle = (gfx_anim_eaf_handle_t *)handle;
    int total_frames;

    if (eaf_handle == NULL) {
        return -1;
    }

    total_frames = eaf_dec_get_total_frames(eaf_handle->eaf_handle);
    return total_frames > 0 ? (total_frames - 1) : 0;
}

static void gfx_anim_eaf_desc_from_header(gfx_anim_frame_desc_t *frame_desc, eaf_dec_header_t *header)
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
    frame_desc->num_colors = header->num_colors;

    header->block_len = NULL;
    header->palette = NULL;
}

static void gfx_anim_eaf_header_from_desc(const gfx_anim_frame_desc_t *frame_desc, eaf_dec_header_t *header)
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

static esp_err_t gfx_anim_eaf_get_frame_info(void *handle, int frame_index, gfx_anim_frame_desc_t *frame_desc)
{
    gfx_anim_eaf_handle_t *eaf_handle = (gfx_anim_eaf_handle_t *)handle;
    eaf_dec_header_t header;
    eaf_dec_type_t format;

    if (eaf_handle == NULL || frame_desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&header, 0, sizeof(header));
    format = eaf_dec_get_frame_info(eaf_handle->eaf_handle, frame_index, &header);
    if (format != EAF_DEC_TYPE_VALID) {
        eaf_dec_free_header(&header);
        return ESP_ERR_INVALID_RESPONSE;
    }

    gfx_anim_eaf_desc_from_header(frame_desc, &header);
    return ESP_OK;
}

static void gfx_anim_eaf_free_frame_info(gfx_anim_frame_desc_t *frame_desc)
{
    if (frame_desc == NULL) {
        return;
    }

    free(frame_desc->block_len);
    free(frame_desc->palette);
    memset(frame_desc, 0, sizeof(*frame_desc));
}

static const uint8_t *gfx_anim_eaf_get_frame_data(void *handle, int frame_index)
{
    gfx_anim_eaf_handle_t *eaf_handle = (gfx_anim_eaf_handle_t *)handle;
    return eaf_handle != NULL ? eaf_dec_get_frame_data(eaf_handle->eaf_handle, frame_index) : NULL;
}

static int gfx_anim_eaf_get_frame_size(void *handle, int frame_index)
{
    gfx_anim_eaf_handle_t *eaf_handle = (gfx_anim_eaf_handle_t *)handle;
    return eaf_handle != NULL ? eaf_dec_get_frame_size(eaf_handle->eaf_handle, frame_index) : -1;
}

static esp_err_t gfx_anim_eaf_decode_block(const gfx_anim_frame_desc_t *frame_desc, const uint8_t *block_data,
        int block_len, uint8_t *out_data, bool swap_color)
{
    eaf_dec_header_t header;

    if (frame_desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_anim_eaf_header_from_desc(frame_desc, &header);
    return eaf_dec_decode_block(&header, block_data, block_len, out_data, swap_color);
}

static bool gfx_anim_eaf_get_palette_color(const gfx_anim_frame_desc_t *frame_desc, uint8_t color_index,
        bool swap_bytes, gfx_color_t *result)
{
    eaf_dec_header_t header;

    if (frame_desc == NULL || result == NULL) {
        return true;
    }

    gfx_anim_eaf_header_from_desc(frame_desc, &header);
    return eaf_dec_get_palette_color(&header, color_index, swap_bytes, result);
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

const gfx_anim_decoder_ops_t *gfx_anim_decoder_get_eaf(void)
{
    static const gfx_anim_decoder_ops_t s_eaf_decoder = {
        .name = "eaf",
        .can_open = gfx_anim_eaf_can_open,
        .open = gfx_anim_eaf_open,
        .close = gfx_anim_eaf_close,
        .get_total_frames = gfx_anim_eaf_get_total_frames,
        .get_frame_info = gfx_anim_eaf_get_frame_info,
        .free_frame_info = gfx_anim_eaf_free_frame_info,
        .get_frame_data = gfx_anim_eaf_get_frame_data,
        .get_frame_size = gfx_anim_eaf_get_frame_size,
        .decode_block = gfx_anim_eaf_decode_block,
        .get_palette_color = gfx_anim_eaf_get_palette_color,
    };

    return &s_eaf_decoder;
}
