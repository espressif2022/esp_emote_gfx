/*
* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "decoder/gfx_aaf_format.h"

static const char *TAG = "gfx_aaf_format";

/*
 * AAF File Format Structure:
 *
 * Offset  Size    Description
 * 0       1       Magic number (0x89)
 * 1       3       Format string ("AAF")
 * 4       4       Total number of frames
 * 8       4       Checksum of table + data
 * 12      4       Length of table + data
 * 16      N       Asset table (N = total_frames * 8)
 * 16+N    M       Frame data (M = sum of all frame sizes)
 */

#define GFX_AAF_MAGIC_HEAD          0x5A5A
#define GFX_AAF_MAGIC_LEN           2

/* File format magic number: 0x89 "AAF" */
#define GFX_AAF_FORMAT_MAGIC        0x89
#define GFX_AAF_FORMAT_STR          "AAF"

#define GFX_AAF_FORMAT_OFFSET       0
#define GFX_AAF_STR_OFFSET          1
#define GFX_AAF_NUM_OFFSET          4
#define GFX_AAF_CHECKSUM_OFFSET     8
#define GFX_AAF_TABLE_LEN           12
#define GFX_AAF_TABLE_OFFSET        16

/**
 * @brief Asset table structure, contains detailed information for each asset.
 */
#pragma pack(1)

typedef struct {
    uint32_t asset_size;          /*!< Size of the asset */
    uint32_t asset_offset;        /*!< Offset of the asset */
} asset_table_entry_t;
#pragma pack()

typedef struct {
    const char *asset_mem;
    const asset_table_entry_t *table;
} asset_entry_t;

typedef struct {
    asset_entry_t *entries;
    int total_frames;
} gfx_aaf_format_ctx_t;

static uint32_t gfx_aaf_format_calc_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

esp_err_t gfx_aaf_format_init(const uint8_t *data, size_t data_len, gfx_aaf_format_handle_t *ret_parser)
{
    esp_err_t ret = ESP_OK;
    asset_entry_t *entries = NULL;

    gfx_aaf_format_ctx_t *parser = (gfx_aaf_format_ctx_t *)calloc(1, sizeof(gfx_aaf_format_ctx_t));
    ESP_GOTO_ON_FALSE(parser, ESP_ERR_NO_MEM, err, TAG, "no mem for parser handle");

    // Check file format magic number: 0x89 "AAF"
    ESP_GOTO_ON_FALSE(data[GFX_AAF_FORMAT_OFFSET] == GFX_AAF_FORMAT_MAGIC, ESP_ERR_INVALID_CRC, err, TAG, "bad file format magic");
    ESP_GOTO_ON_FALSE(memcmp(data + GFX_AAF_STR_OFFSET, GFX_AAF_FORMAT_STR, 3) == 0, ESP_ERR_INVALID_CRC, err, TAG, "bad file format string");

    int total_frames = *(int *)(data + GFX_AAF_NUM_OFFSET);
    uint32_t stored_chk = *(uint32_t *)(data + GFX_AAF_CHECKSUM_OFFSET);
    uint32_t stored_len = *(uint32_t *)(data + GFX_AAF_TABLE_LEN);

    uint32_t calculated_chk = gfx_aaf_format_calc_checksum((uint8_t *)(data + GFX_AAF_TABLE_OFFSET), stored_len);
    ESP_GOTO_ON_FALSE(calculated_chk == stored_chk, ESP_ERR_INVALID_CRC, err, TAG, "bad full checksum");

    entries = (asset_entry_t *)malloc(sizeof(asset_entry_t) * total_frames);

    asset_table_entry_t *table = (asset_table_entry_t *)(data + GFX_AAF_TABLE_OFFSET);
    for (int i = 0; i < total_frames; i++) {
        (entries + i)->table = (table + i);
        (entries + i)->asset_mem = (void *)(data + GFX_AAF_TABLE_OFFSET + total_frames * sizeof(asset_table_entry_t) + table[i].asset_offset);

        uint16_t *magic_ptr = (uint16_t *)(entries + i)->asset_mem;
        ESP_GOTO_ON_FALSE(*magic_ptr == GFX_AAF_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG, "bad file magic header");
    }

    parser->entries = entries;
    parser->total_frames = total_frames;

    *ret_parser = (gfx_aaf_format_handle_t)parser;

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

esp_err_t gfx_aaf_format_deinit(gfx_aaf_format_handle_t handle)
{
    assert(handle && "handle is invalid");
    gfx_aaf_format_ctx_t *parser = (gfx_aaf_format_ctx_t *)(handle);
    if (parser) {
        if (parser->entries) {
            free(parser->entries);
        }
        free(parser);
    }
    return ESP_OK;
}

int gfx_aaf_format_get_total_frames(gfx_aaf_format_handle_t handle)
{
    assert(handle && "handle is invalid");
    gfx_aaf_format_ctx_t *parser = (gfx_aaf_format_ctx_t *)(handle);

    return parser->total_frames;
}

const uint8_t *gfx_aaf_format_get_frame_data(gfx_aaf_format_handle_t handle, int index)
{
    assert(handle && "handle is invalid");

    gfx_aaf_format_ctx_t *parser = (gfx_aaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return (const uint8_t *)((parser->entries + index)->asset_mem + GFX_AAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return NULL;
    }
}

int gfx_aaf_format_get_frame_size(gfx_aaf_format_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    gfx_aaf_format_ctx_t *parser = (gfx_aaf_format_ctx_t *)(handle);

    if (parser->total_frames > index) {
        return ((parser->entries + index)->table->asset_size - GFX_AAF_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, parser->total_frames);
        return -1;
    }
}