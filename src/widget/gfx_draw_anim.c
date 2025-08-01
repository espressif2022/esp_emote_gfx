/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "core/gfx_core.h"
#include "core/gfx_obj.h"
#include "widget/gfx_anim.h"
#include "widget/gfx_comm.h"
#include "decoder/gfx_aaf_dec.h"
#include "decoder/gfx_jpeg_dec.h"
#include "widget/gfx_font_internal.h"
#include "widget/gfx_anim_internal.h"
#include "core/gfx_obj_internal.h"
#include "decoder/gfx_aaf_format.h"

static const char *TAG = "gfx_anim";

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_anim_decode_block(const uint8_t *data, const uint32_t *offsets, const gfx_aaf_header_t *header,
                                       int block, uint8_t *decode_buffer, int width, int block_height, bool swap_color);
static void gfx_anim_render_4bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const gfx_aaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        bool mirror_enabled, int16_t mirror_offset, int dest_x_offset);
static void gfx_anim_render_8bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const gfx_aaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        bool mirror_enabled, int16_t mirror_offset, int dest_x_offset);

static void gfx_anim_render_24bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                         const uint8_t *src_buf, gfx_coord_t src_stride,
                                         gfx_area_t *clip_area, bool swap_color,
                                         bool mirror_enabled, int16_t mirror_offset, int dest_x_offset);

/**
 * @brief Free frame processing information and allocated resources
 * @param frame Frame processing information structure
 */
void gfx_anim_free_frame_info(gfx_anim_frame_info_t *frame)
{
    // Free previously parsed header if exists
    if (frame->header.width > 0) {  // Check if header data is valid instead of header_valid flag
        gfx_aaf_free_header(&frame->header);
        memset(&frame->header, 0, sizeof(gfx_aaf_header_t));  // Clear header data
    }

    // Free previously allocated parsing resources if exists
    if (frame->block_offsets) {
        free(frame->block_offsets);
        frame->block_offsets = NULL;
    }
    if (frame->pixel_buffer) {
        free(frame->pixel_buffer);
        frame->pixel_buffer = NULL;
    }
    if (frame->color_palette) {
        free(frame->color_palette);
        frame->color_palette = NULL;
    }

    // Clear frame data
    frame->frame_data = NULL;
    frame->frame_size = 0;
    frame->last_block = -1;
}

/**
 * @brief Preprocess animation frame data and allocate parsing resources
 * @param anim Animation property structure
 * @return true if preprocessing was successful, false otherwise
 */
bool gfx_anim_preprocess_frame(gfx_anim_property_t *anim)
{
    // Free previous frame info and allocated resources
    gfx_anim_free_frame_info(&anim->frame);

    const void *frame_data = gfx_aaf_format_get_frame_data(anim->file_desc, anim->current_frame);
    size_t frame_size = gfx_aaf_format_get_frame_size(anim->file_desc, anim->current_frame);

    if (frame_data == NULL) {
        ESP_LOGW(TAG, "Failed to get frame data for frame %lu", anim->current_frame);
        return false;
    }

    anim->frame.frame_data = frame_data;
    anim->frame.frame_size = frame_size;

    gfx_aaf_format_t format = gfx_aaf_parse_header(frame_data, frame_size, &anim->frame.header);
    if (format != GFX_AAF_FORMAT_SBMP) {
        ESP_LOGW(TAG, "Failed to parse header for frame %lu, format: %d", anim->current_frame, format);
        return false;
    }

    // Pre-allocate parsing resources and calculate offsets
    const gfx_aaf_header_t *header = &anim->frame.header;
    int aaf_blocks = header->blocks;
    int block_height = header->block_height;
    int width = header->width;
    uint16_t color_depth = 0;

    // Allocate offsets array
    anim->frame.block_offsets = (uint32_t *)malloc(aaf_blocks * sizeof(uint32_t));
    if (anim->frame.block_offsets == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for block offsets");
        return false;
    }

    // Allocate decode buffer based on bit depth
    if (header->bit_depth == 4) {
        anim->frame.pixel_buffer = (uint8_t *)malloc(width * (block_height + (block_height % 2)) / 2);
    } else if (header->bit_depth == 8) {
        anim->frame.pixel_buffer = (uint8_t *)malloc(width * block_height);
    } else if (header->bit_depth == 24) {
        anim->frame.pixel_buffer = (uint8_t *)heap_caps_aligned_alloc(16, width * block_height * 2, MALLOC_CAP_DEFAULT);//esp_new_jpg needed aligned 16
    }
    if (anim->frame.pixel_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for pixel buffer, bit_depth: %d", header->bit_depth);
        free(anim->frame.block_offsets);
        anim->frame.block_offsets = NULL;
        return false;
    }

    // Determine color depth and allocate palette cache
    if (header->bit_depth == 4) {
        color_depth = 16;
    } else if (header->bit_depth == 8) {
        color_depth = 256;
    } else if (header->bit_depth == 24) {
        color_depth = 0;
    }

    if (color_depth) {
        anim->frame.color_palette = (uint32_t *)heap_caps_malloc(color_depth * sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (anim->frame.color_palette == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for color palette");
            free(anim->frame.pixel_buffer);
            free(anim->frame.block_offsets);
            anim->frame.pixel_buffer = NULL;
            anim->frame.block_offsets = NULL;
            return false;
        }

        // Initialize palette cache
        for (int i = 0; i < color_depth; i++) {
            anim->frame.color_palette[i] = 0xFFFFFFFF; // Use 0xFFFFFFFF as sentinel value
        }
    }

    // Calculate offsets
    gfx_aaf_calculate_offsets(header, anim->frame.block_offsets);

    ESP_LOGD(TAG, "Pre-allocated parsing resources for frame %lu", anim->current_frame);
    return true;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void gfx_draw_animation(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap_color)
{
    if (obj == NULL || obj->src == NULL) {
        ESP_LOGD(TAG, "Invalid object or source");
        return;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGW(TAG, "Object is not an animation type");
        return;
    }

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;

    if (anim->file_desc == NULL) {
        ESP_LOGE(TAG, "Animation file descriptor is NULL");
        return;
    }

    const void *frame_data;

    if (anim->frame.frame_data != NULL) {  // Check if frame data exists instead of data_ready flag
        frame_data = anim->frame.frame_data;
    } else {
        ESP_LOGD(TAG, "Frame data not ready for frame %lu", anim->current_frame);
        return;
    }

    if (frame_data == NULL) {
        ESP_LOGD(TAG, "Failed to get frame data for frame %lu", anim->current_frame);
        return;
    }

    // Get frame processing information
    if (anim->frame.header.width == 0) {  // Check if header is valid instead of header_valid flag
        ESP_LOGD(TAG, "Header not valid for frame %lu", anim->current_frame);
        return;
    }

    // Use pre-allocated parsing resources
    uint8_t *decode_buffer = anim->frame.pixel_buffer;
    uint32_t *offsets = anim->frame.block_offsets;
    uint32_t *palette_cache = anim->frame.color_palette;

    if (!offsets || !decode_buffer) {
        ESP_LOGE(TAG, "Parsing resources not allocated for frame %lu", anim->current_frame);
        return;
    }

    // Get header pointer for convenience
    const gfx_aaf_header_t *header = &anim->frame.header;

    // Get screen dimensions for alignment calculation
    uint32_t parent_width, parent_height;
    if (obj->parent_handle != NULL) {
        esp_err_t ret = gfx_emote_get_screen_size(obj->parent_handle, &parent_width, &parent_height);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get screen size, using defaults");
            parent_width = DEFAULT_SCREEN_WIDTH;
            parent_height = DEFAULT_SCREEN_HEIGHT;
        }
    } else {
        parent_width = DEFAULT_SCREEN_WIDTH;
        parent_height = DEFAULT_SCREEN_HEIGHT;
    }

    gfx_coord_t obj_x = obj->x;
    gfx_coord_t obj_y = obj->y;

    obj->width = header->width;
    obj->height = header->height;

    gfx_obj_calculate_aligned_position(obj, parent_width, parent_height, &obj_x, &obj_y);

    gfx_area_t clip_object;
    clip_object.x1 = MAX(x1, obj_x);
    clip_object.y1 = MAX(y1, obj_y);
    clip_object.x2 = MIN(x2, obj_x + obj->width);
    clip_object.y2 = MIN(y2, obj_y + obj->height);

    if (clip_object.x1 >= clip_object.x2 || clip_object.y1 >= clip_object.y2) {
        return;
    }

    // Decode the frame
    int width = header->width;
    int height = header->height;
    int block_height = header->block_height;
    int aaf_blocks = header->blocks;
    int *last_block = &anim->frame.last_block;

    // Process each AAF block that overlaps with the destination area
    for (int block = 0; block < aaf_blocks; block++) {
        int block_start_y = block * block_height;
        int block_end_y = (block == aaf_blocks - 1) ? height : (block + 1) * block_height;

        int block_start_x = 0;
        int block_end_x = width;

        block_start_y += obj_y;
        block_end_y += obj_y;
        block_start_x += obj_x;
        block_end_x += obj_x;

        gfx_area_t clip_block;
        clip_block.x1 = MAX(clip_object.x1, block_start_x);
        clip_block.y1 = MAX(clip_object.y1, block_start_y);
        clip_block.x2 = MIN(clip_object.x2, block_end_x);
        clip_block.y2 = MIN(clip_object.y2, block_end_y);

        if (clip_block.x1 >= clip_block.x2 || clip_block.y1 >= clip_block.y2) {
            continue;
        }

        // Calculate source buffer offset relative to the decoded block data
        int src_offset_x = clip_block.x1 - block_start_x;
        int src_offset_y = clip_block.y1 - block_start_y;

        // Check if source offsets are within valid range before decoding
        if (src_offset_x < 0 || src_offset_y < 0 ||
                src_offset_x >= width || src_offset_y >= block_height) {
            continue;
        }

        // Decode block if not already decoded
        if (block != *last_block) {
            esp_err_t decode_result = gfx_anim_decode_block(frame_data, offsets, header, block, decode_buffer, width, block_height, swap_color);
            if (decode_result != ESP_OK) {
                continue;
            }
            *last_block = block;
        }

        gfx_coord_t dest_buffer_stride = (x2 - x1);
        gfx_coord_t source_buffer_stride = width;

        uint8_t *source_pixels = NULL;

        if (header->bit_depth == 24) {// RGB565
            source_pixels = decode_buffer + src_offset_y * (source_buffer_stride * 2) + src_offset_x * 2;
        } else if (header->bit_depth == 4) {
            source_pixels = decode_buffer + src_offset_y * (source_buffer_stride / 2) + src_offset_x / 2;
        } else {
            source_pixels = decode_buffer + src_offset_y * source_buffer_stride + src_offset_x;
        }

        int dest_x_offset = clip_block.x1 - x1;

        gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + (clip_block.y1 - y1) * dest_buffer_stride + dest_x_offset;

        if (header->bit_depth == 4) {
            gfx_anim_render_4bit_pixels(
                dest_pixels,
                dest_buffer_stride,
                source_pixels,
                source_buffer_stride,
                header, palette_cache,
                &clip_block,
                swap_color, anim->mirror_enabled, anim->mirror_offset, dest_x_offset);
        } else if (header->bit_depth == 8) {
            gfx_anim_render_8bit_pixels(
                dest_pixels,
                dest_buffer_stride,
                source_pixels,
                source_buffer_stride,
                header, palette_cache,
                &clip_block,
                swap_color,
                anim->mirror_enabled, anim->mirror_offset, dest_x_offset);
        } else if (header->bit_depth == 24) {
            gfx_anim_render_24bit_pixels(
                dest_pixels,
                dest_buffer_stride,
                source_pixels,
                source_buffer_stride,
                &clip_block,
                swap_color,
                anim->mirror_enabled, anim->mirror_offset, dest_x_offset);
        } else {
            ESP_LOGE(TAG, "Unsupported bit depth: %d", header->bit_depth);
            continue;
        }
    }

    obj->is_dirty = false;
}

/*=====================
 * Static helper functions
 *====================*/
static esp_err_t gfx_anim_decode_block(const uint8_t *data, const uint32_t *offsets, const gfx_aaf_header_t *header,
                                       int block, uint8_t *decode_buffer, int width, int block_height, bool swap_color)
{
    const uint8_t *block_data = data + offsets[block];
    int block_len = header->block_len[block];
    uint8_t encoding_type = block_data[0];

    esp_err_t decode_result = ESP_FAIL;

    if (encoding_type == GFX_AAF_ENCODING_RLE) {
        size_t rle_out_len = width * block_height;

        decode_result = gfx_aaf_rle_decode(block_data + 1, block_len - 1,
                                           decode_buffer, rle_out_len);
    } else if (encoding_type == GFX_AAF_ENCODING_HUFFMAN) {
        size_t rle_out_len = width * block_height;

        uint8_t *huffman_buffer = malloc(rle_out_len);
        if (huffman_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for Huffman buffer");
            return ESP_FAIL;
        }

        size_t huffman_out_len = 0;
        gfx_aaf_huffman_decode(block_data, block_len, huffman_buffer, &huffman_out_len);

        if (huffman_out_len > rle_out_len) {
            ESP_LOGE(TAG, "Huffman output size mismatch: expected %d, got %d", rle_out_len, huffman_out_len);
            free(huffman_buffer);
            return ESP_FAIL;
        }

        decode_result = gfx_aaf_rle_decode(huffman_buffer, huffman_out_len,
                                           decode_buffer, rle_out_len);
        free(huffman_buffer);
    } else if (encoding_type == GFX_AAF_ENCODING_HUFFMAN_DIRECT) {
        size_t huffman_out_len = 0;
        esp_err_t huffman_ret = gfx_aaf_huffman_decode(block_data, block_len, decode_buffer, &huffman_out_len);
        if (huffman_ret != ESP_OK) {
            ESP_LOGE(TAG, "Direct Huffman decode failed for block %d", block);
            return ESP_FAIL;
        }

        if (huffman_out_len != width * block_height) {
            ESP_LOGE(TAG, "Direct Huffman output size mismatch: expected %d, got %d", width * block_height, huffman_out_len);
            return ESP_FAIL;
        }

        decode_result = ESP_OK;
    } else if (encoding_type == GFX_AAF_ENCODING_JPEG) {
        uint32_t w, h;
        size_t jpg_out_len = width * block_height * 2; // RGB565 = 2 bytes per pixel
        esp_err_t jpeg_ret = gfx_jpeg_decode(block_data + 1, block_len - 1, decode_buffer, jpg_out_len, &w, &h, swap_color);
        if (jpeg_ret != ESP_OK) {
            ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(jpeg_ret));
            return ESP_FAIL;
        }

        decode_result = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Unknown encoding type: %02X", encoding_type);
        return ESP_FAIL;
    }

    if (decode_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode block %d", block);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Render 4-bit pixels directly to destination buffer
 * @param dest_buf Destination buffer
 * @param dest_stride Destination buffer stride
 * @param src_buf Source buffer data
 * @param src_stride Source buffer stride
 * @param header Image header
 * @param palette_cache Palette cache
 * @param clip_area Clipping area
 * @param swap_color Whether to swap color bytes
 * @param mirror_enabled Whether mirror is enabled
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination buffer x offset
 */
static void gfx_anim_render_4bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const gfx_aaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        bool mirror_enabled, int16_t mirror_offset, int dest_x_offset)
{
    int width = header->width;
    int clip_w = clip_area->x2 - clip_area->x1;
    int clip_h = clip_area->y2 - clip_area->y1;

    for (int y = 0; y < clip_h; y++) {
        for (int x = 0; x < clip_w; x += 2) {
            uint8_t packed_gray = src_buf[y * src_stride / 2 + (x / 2)];
            uint8_t index1 = (packed_gray & 0xF0) >> 4;
            uint8_t index2 = (packed_gray & 0x0F);

            if (palette_cache[index1] == 0xFFFFFFFF) {
                gfx_color_t color = gfx_aaf_parse_palette(header, index1, swap_color);
                palette_cache[index1] = color.full;
            }

            gfx_color_t color_val1;
            color_val1.full = (uint16_t)palette_cache[index1];
            dest_buf[y * dest_stride + x] = color_val1;

            // Sync write to mirror position if mirror is enabled
            if (mirror_enabled) {
                int mirror_x = width + mirror_offset + width - 1 - x;

                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf[y * dest_stride + mirror_x] = color_val1;
                }
            }

            if (palette_cache[index2] == 0xFFFFFFFF) {
                gfx_color_t color = gfx_aaf_parse_palette(header, index2, swap_color);
                palette_cache[index2] = color.full;
            }

            gfx_color_t color_val2;
            color_val2.full = (uint16_t)palette_cache[index2];
            dest_buf[y * dest_stride + x + 1] = color_val2;

            if (mirror_enabled) {
                int mirror_x = width + mirror_offset + width - 1 - (x + 1);

                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf[y * dest_stride + mirror_x] = color_val1;
                }
            }
        }
    }
}

/**
 * @brief Render 8-bit pixels directly to destination buffer
 * @param dest_buf Destination buffer
 * @param dest_stride Destination buffer stride
 * @param src_buf Source buffer data
 * @param src_stride Source buffer stride
 * @param header Image header
 * @param palette_cache Palette cache
 * @param clip_area Clipping area
 * @param swap_color Whether to swap color bytes
 * @param mirror_enabled Whether mirror is enabled
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination buffer x offset
 */
static void gfx_anim_render_8bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const gfx_aaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        bool mirror_enabled, int16_t mirror_offset, int dest_x_offset)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int32_t width = header->width;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            uint8_t index = src_buf[y * src_stride + x];
            if (palette_cache[index] == 0xFFFFFFFF) {
                gfx_color_t color = gfx_aaf_parse_palette(header, index, swap_color);
                palette_cache[index] = color.full;
            }

            gfx_color_t color_val;
            color_val.full = (uint16_t)palette_cache[index];
            dest_buf[y * dest_stride + x] = color_val;

            // Sync write to mirror position if mirror is enabled
            if (mirror_enabled) {
                int mirror_x = width + mirror_offset + width - 1 - x;

                // Check boundary for mirror position
                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf[y * dest_stride + mirror_x] = color_val;
                }
            }
        }
    }
}

static void gfx_anim_render_24bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                         const uint8_t *src_buf, gfx_coord_t src_stride,
                                         gfx_area_t *clip_area, bool swap_color,
                                         bool mirror_enabled, int16_t mirror_offset, int dest_x_offset)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int32_t width = src_stride;

    uint16_t *src_buf_16 = (uint16_t *)src_buf;
    uint16_t *dest_buf_16 = (uint16_t *)dest_buf;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            dest_buf_16[y * dest_stride + x] = src_buf_16[y * src_stride + x];

            // Sync write to mirror position if mirror is enabled
            if (mirror_enabled) {
                int mirror_x = width + mirror_offset + width - 1 - x;

                // Check boundary for mirror position
                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf_16[y * dest_stride + mirror_x] = src_buf_16[y * src_stride + x];
                }
            }
        }
    }
}