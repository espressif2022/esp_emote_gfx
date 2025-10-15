/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "core/gfx_core_internal.h"
#include "widget/gfx_comm.h"
#include "widget/gfx_anim_internal.h"

static const char *TAG = "gfx_anim";

/*********************
 *      DEFINES
 *********************/

/* Helper macro for type checking */
#define CHECK_OBJ_TYPE_ANIMATION(obj) \
    do { \
        ESP_RETURN_ON_ERROR((obj == NULL) ? ESP_ERR_INVALID_ARG : ESP_OK, TAG, "Object is NULL"); \
        ESP_RETURN_ON_ERROR((obj->type != GFX_OBJ_TYPE_ANIMATION) ? ESP_ERR_INVALID_ARG : ESP_OK, TAG, \
                           "Object is not an ANIMATION type (type=%d). Cannot use animation API on non-animation objects.", obj->type); \
    } while(0)

/**********************
 *      TYPEDEFS
 **********************/

/**
 * @brief Bit depth enumeration for renderer selection
 */
typedef enum {
    GFX_ANIM_DEPTH_4BIT = 4,   /*!< 4-bit color depth */
    GFX_ANIM_DEPTH_8BIT = 8,   /*!< 8-bit color depth */
    GFX_ANIM_DEPTH_24BIT = 24, /*!< 24-bit color depth */
    GFX_ANIM_DEPTH_MAX = 3     /*!< Maximum number of depth types */
} gfx_anim_depth_t;

/**
 * @brief Function pointer type for pixel renderers
 * @param dest_buf Destination buffer
 * @param dest_stride Destination buffer stride
 * @param src_buf Source pixel buffer
 * @param src_stride Source buffer stride
 * @param header EAF header information (may be NULL for 24-bit)
 * @param palette_cache Color palette cache (may be NULL for 24-bit)
 * @param clip_area Clipping area
 * @param swap_color Whether to swap color bytes
 * @param mirror_mode Mirror mode
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination X offset
 */
typedef void (*gfx_anim_pixel_renderer_cb_t)(
    gfx_color_t *dest_buf, gfx_coord_t dest_stride,
    const uint8_t *src_buf, gfx_coord_t src_stride,
    const eaf_header_t *header, uint32_t *palette_cache,
    gfx_area_t *clip_area, bool swap_color,
    gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset);

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_anim_render_4bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset);
static void gfx_anim_render_8bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset);

static void gfx_anim_render_24bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
        const uint8_t *src_buf, gfx_coord_t src_stride,
        const eaf_header_t *header, uint32_t *palette_cache,
        gfx_area_t *clip_area, bool swap_color,
        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset);

/* Renderer management functions */
static esp_err_t gfx_anim_render_pixels(uint8_t bit_depth,
                                        gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset);

/**********************
 *  STATIC VARIABLES
 **********************/
static const gfx_anim_pixel_renderer_cb_t g_anim_renderers[GFX_ANIM_DEPTH_MAX] = {
    gfx_anim_render_4bit_pixels,     // GFX_ANIM_DEPTH_4BIT = 4 (index 0)
    gfx_anim_render_8bit_pixels,     // GFX_ANIM_DEPTH_8BIT = 8 (index 1)
    gfx_anim_render_24bit_pixels,    // GFX_ANIM_DEPTH_24BIT = 24 (index 2)
};

/**********************
 *  HELPER FUNCTIONS
 **********************/

/**
 * @brief Free frame processing information and allocated resources
 * @param frame Frame processing information structure
 */
void gfx_anim_free_frame_info(gfx_anim_frame_info_t *frame)
{
    if (frame->header.width > 0) {
        eaf_free_header(&frame->header);
        memset(&frame->header, 0, sizeof(eaf_header_t));
    }

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

    frame->frame_data = NULL;
    frame->frame_size = 0;
    frame->last_block = -1;
}

/**
 * @brief Preprocess animation frame data and allocate parsing resources
 * @param anim Animation property structure
 * @return true if preprocessing was successful, false otherwise
 */
esp_err_t gfx_anim_preprocess_frame(gfx_anim_property_t *anim)
{
    esp_err_t ret = ESP_OK;

    gfx_anim_free_frame_info(&anim->frame);

    const void *frame_data = eaf_get_frame_data(anim->file_desc, anim->current_frame);
    size_t frame_size = eaf_get_frame_size(anim->file_desc, anim->current_frame);

    if (frame_data == NULL) {
        ESP_LOGD(TAG, "Failed to get frame data for frame %"PRIu32"", anim->current_frame);
        return ESP_FAIL;
    }

    anim->frame.frame_data = frame_data;
    anim->frame.frame_size = frame_size;

    eaf_format_type_t format = eaf_get_frame_info(anim->file_desc, anim->current_frame, &anim->frame.header);
    if (format == EAF_FORMAT_FLAG) {
        return ESP_FAIL;
    } else if (format == EAF_FORMAT_INVALID) {
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_STATE, err, TAG, "Invalid EAF format for frame %lu", anim->current_frame);
    }

    const eaf_header_t *header = &anim->frame.header;
    int num_blocks = header->blocks;
    int block_height = header->block_height;
    int width = header->width;
    uint16_t color_depth = 0;

    anim->frame.block_offsets = (uint32_t *)malloc(num_blocks * sizeof(uint32_t));
    ESP_GOTO_ON_FALSE(anim->frame.block_offsets != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate memory for block offsets");

    if (header->bit_depth == 4) {
        anim->frame.pixel_buffer = (uint8_t *)malloc(width * (block_height + (block_height % 2)) / 2);
    } else if (header->bit_depth == 8) {
        anim->frame.pixel_buffer = (uint8_t *)malloc(width * block_height);
    } else if (header->bit_depth == 24) {
        anim->frame.pixel_buffer = (uint8_t *)heap_caps_aligned_alloc(16, width * block_height * 2, MALLOC_CAP_DEFAULT); // JPEG decoder requires 16-byte alignment
    }
    ESP_GOTO_ON_FALSE(anim->frame.pixel_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate memory for pixel buffer, bit_depth: %d", header->bit_depth);

    if (header->bit_depth == 4) {
        color_depth = 16;
    } else if (header->bit_depth == 8) {
        color_depth = 256;
    } else if (header->bit_depth == 24) {
        color_depth = 0;
    }

    if (color_depth) {
        anim->frame.color_palette = (uint32_t *)heap_caps_malloc(color_depth * sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_GOTO_ON_FALSE(anim->frame.color_palette != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate memory for color palette");

        for (int i = 0; i < color_depth; i++) {
            anim->frame.color_palette[i] = 0xFFFFFFFF;
        }
    }

    eaf_calculate_offsets(header, anim->frame.block_offsets);

    ESP_LOGD(TAG, "Pre-allocated parsing resources for frame %"PRIu32"", anim->current_frame);
    return ret;

err:
    if (anim->frame.block_offsets) {
        free(anim->frame.block_offsets);
        anim->frame.block_offsets = NULL;
    }
    if (anim->frame.pixel_buffer) {
        free(anim->frame.pixel_buffer);
        anim->frame.pixel_buffer = NULL;
    }
    if (anim->frame.color_palette) {
        free(anim->frame.color_palette);
        anim->frame.color_palette = NULL;
    }
    return ESP_FAIL;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

esp_err_t gfx_draw_animation(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap_color)
{
    ESP_RETURN_ON_FALSE(obj != NULL && obj->src != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid object or source");
    ESP_RETURN_ON_FALSE(obj->type == GFX_OBJ_TYPE_ANIMATION, ESP_ERR_INVALID_ARG, TAG, "Object is not an animation type");

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim->file_desc == NULL) {
        ESP_LOGD(TAG, "Animation file descriptor is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const void *frame_data = anim->frame.frame_data;
    if (frame_data == NULL) {
        ESP_LOGD(TAG, "Frame data not ready for frame %"PRIu32"", anim->current_frame);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_FALSE(anim->frame.header.width > 0, ESP_ERR_INVALID_STATE, TAG, "Header not valid for frame %lu", anim->current_frame);

    uint8_t *decode_buffer = anim->frame.pixel_buffer;
    uint32_t *offsets = anim->frame.block_offsets;
    uint32_t *palette_cache = anim->frame.color_palette;

    ESP_RETURN_ON_FALSE(offsets != NULL && decode_buffer != NULL, ESP_ERR_INVALID_STATE, TAG, "Parsing resources not allocated for frame %lu", anim->current_frame);

    const eaf_header_t *header = &anim->frame.header;


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
        return ESP_ERR_INVALID_STATE;
    }

    int width = header->width;
    int height = header->height;
    int block_height = header->block_height;
    int num_blocks = header->blocks;
    int *last_block = &anim->frame.last_block;

    for (int block = 0; block < num_blocks; block++) {
        int block_start_y = block * block_height;
        int block_end_y = (block == num_blocks - 1) ? height : (block + 1) * block_height;

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

        int src_offset_x = clip_block.x1 - block_start_x;
        int src_offset_y = clip_block.y1 - block_start_y;

        if (src_offset_x < 0 || src_offset_y < 0 ||
                src_offset_x >= width || src_offset_y >= block_height) {
            continue;
        }

        if (block != *last_block) {
            const uint8_t *block_data = (const uint8_t *)frame_data + anim->frame.block_offsets[block];
            int block_len = header->block_len[block];

            esp_err_t decode_result = eaf_decode_block(header, block_data, block_len, decode_buffer, swap_color);
            if (decode_result != ESP_OK) {
                continue;
            }
            *last_block = block;
        }

        gfx_coord_t dest_buffer_stride = (x2 - x1);
        gfx_coord_t source_buffer_stride = width;

        uint8_t *source_pixels = NULL;

        if (header->bit_depth == 24) {
            source_pixels = decode_buffer + src_offset_y * (source_buffer_stride * 2) + src_offset_x * 2;
        } else if (header->bit_depth == 4) {
            source_pixels = decode_buffer + src_offset_y * (source_buffer_stride / 2) + src_offset_x / 2;
        } else {
            source_pixels = decode_buffer + src_offset_y * source_buffer_stride + src_offset_x;
        }

        int dest_x_offset = clip_block.x1 - x1;

        gfx_color_t *dest_pixels = (gfx_color_t *)dest_buf + (clip_block.y1 - y1) * dest_buffer_stride + dest_x_offset;

        esp_err_t render_result = gfx_anim_render_pixels(
                                      header->bit_depth,
                                      dest_pixels,
                                      dest_buffer_stride,
                                      source_pixels,
                                      source_buffer_stride,
                                      header, palette_cache,
                                      &clip_block,
                                      swap_color,
                                      anim->mirror_mode, anim->mirror_offset, dest_x_offset);

        if (render_result != ESP_OK) {
            continue;
        }
    }

    obj->is_dirty = false;
    gfx_obj_invalidate(obj);

    return ESP_OK;
}

/*=====================
 * Renderer Management Functions
 *====================*/

/**
 * @brief Render pixels using registered renderer
 * @param bit_depth Bit depth (4, 8, or 24)
 * @param dest_buf Destination buffer
 * @param dest_stride Destination buffer stride
 * @param src_buf Source buffer data
 * @param src_stride Source buffer stride
 * @param header Image header
 * @param palette_cache Palette cache
 * @param clip_area Clipping area
 * @param swap_color Whether to swap color bytes
 * @param mirror_mode Mirror mode
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination buffer x offset
 * @return ESP_OK on success, ESP_FAIL on failure
 */
static esp_err_t gfx_anim_render_pixels(uint8_t bit_depth,
                                        gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset)
{
    int index;
    switch (bit_depth) {
    case 4:  index = 0; break;
    case 8:  index = 1; break;
    case 24: index = 2; break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "Unsupported bit depth: %d", bit_depth);
    }

    g_anim_renderers[index](dest_buf, dest_stride, src_buf, src_stride,
                            header, palette_cache, clip_area, swap_color,
                            mirror_mode, mirror_offset, dest_x_offset);

    return ESP_OK;
}

/*=====================
 * Static helper functions
 *====================*/

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
 * @param mirror_mode Whether mirror is enabled
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination buffer x offset
 */
static void gfx_anim_render_4bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset)
{
    int width = header->width;
    int clip_w = clip_area->x2 - clip_area->x1;
    int clip_h = clip_area->y2 - clip_area->y1;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (dest_stride - (src_stride + dest_x_offset) * 2);
    }

    for (int y = 0; y < clip_h; y++) {
        for (int x = 0; x < clip_w; x += 2) {
            uint8_t packed_gray = src_buf[y * src_stride / 2 + (x / 2)];
            uint8_t index1 = (packed_gray & 0xF0) >> 4;
            uint8_t index2 = (packed_gray & 0x0F);

            if (palette_cache[index1] == 0xFFFFFFFF) {
                gfx_color_t color = eaf_palette_get_color(header, index1, swap_color);
                palette_cache[index1] = color.full;
            }

            gfx_color_t color_val1;
            color_val1.full = (uint16_t)palette_cache[index1];
            dest_buf[y * dest_stride + x] = color_val1;

            if (mirror_mode != GFX_MIRROR_DISABLED) {

                int mirror_x = width + mirror_offset + width - 1 - x;

                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf[y * dest_stride + mirror_x] = color_val1;
                }
            }

            if (palette_cache[index2] == 0xFFFFFFFF) {
                gfx_color_t color = eaf_palette_get_color(header, index2, swap_color);
                palette_cache[index2] = color.full;
            }

            gfx_color_t color_val2;
            color_val2.full = (uint16_t)palette_cache[index2];
            dest_buf[y * dest_stride + x + 1] = color_val2;

            if (mirror_mode != GFX_MIRROR_DISABLED) {
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
 * @param mirror_mode Whether mirror is enabled
 * @param mirror_offset Mirror offset
 * @param dest_x_offset Destination buffer x offset
 */
static void gfx_anim_render_8bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                        const uint8_t *src_buf, gfx_coord_t src_stride,
                                        const eaf_header_t *header, uint32_t *palette_cache,
                                        gfx_area_t *clip_area, bool swap_color,
                                        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int32_t width = header->width;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (dest_stride - (src_stride + dest_x_offset) * 2);
    }

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            uint8_t index = src_buf[y * src_stride + x];
            if (palette_cache[index] == 0xFFFFFFFF) {
                gfx_color_t color = eaf_palette_get_color(header, index, swap_color);
                palette_cache[index] = color.full;
            }

            gfx_color_t color_val;
            color_val.full = (uint16_t)palette_cache[index];
            dest_buf[y * dest_stride + x] = color_val;

            if (mirror_mode != GFX_MIRROR_DISABLED) {
                int mirror_x = width + mirror_offset + width - 1 - x;

                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf[y * dest_stride + mirror_x] = color_val;
                }
            }
        }
    }
}

static void gfx_anim_render_24bit_pixels(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
        const uint8_t *src_buf, gfx_coord_t src_stride,
        const eaf_header_t *header, uint32_t *palette_cache,
        gfx_area_t *clip_area, bool swap_color,
        gfx_mirror_mode_t mirror_mode, int16_t mirror_offset, int dest_x_offset)
{
    // Ignore unused parameters for 24-bit rendering
    (void)header;
    (void)palette_cache;
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int32_t width = src_stride;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (dest_stride - (src_stride + dest_x_offset) * 2);
    }

    uint16_t *src_buf_16 = (uint16_t *)src_buf;
    uint16_t *dest_buf_16 = (uint16_t *)dest_buf;

    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            dest_buf_16[y * dest_stride + x] = src_buf_16[y * src_stride + x];

            if (mirror_mode != GFX_MIRROR_DISABLED) {
                int mirror_x = width + mirror_offset + width - 1 - x;

                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < dest_stride) {
                    dest_buf_16[y * dest_stride + mirror_x] = src_buf_16[y * src_stride + x];
                }
            }
        }
    }
}

/*=====================
 * Animation object creation and management
 *====================*/

static void gfx_anim_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;

    if (!anim || !anim->is_playing) {
        ESP_LOGD(TAG, "anim is NULL or not playing, %p, %d", anim, anim->is_playing);
        return;
    }

    gfx_core_context_t *ctx = obj->parent_handle;
    if (anim->current_frame >= anim->end_frame) {
        if (anim->repeat) {
            ESP_LOGD(TAG, "REPEAT");
            if (ctx->callbacks.update_cb) {
                ctx->callbacks.update_cb(ctx, GFX_PLAYER_EVENT_ALL_FRAME_DONE, obj);
            }
            anim->current_frame = anim->start_frame;
        } else {
            ESP_LOGD(TAG, "STOP");
            anim->is_playing = false;
            if (ctx->callbacks.update_cb) {
                ctx->callbacks.update_cb(ctx, GFX_PLAYER_EVENT_ALL_FRAME_DONE, obj);
            }
            return;
        }
    } else {
        anim->current_frame++;
        if (ctx->callbacks.update_cb) {
            ctx->callbacks.update_cb(ctx, GFX_PLAYER_EVENT_ONE_FRAME_DONE, obj);
        }
        ESP_LOGD("anim cb", " %"PRIu32" (%"PRIu32" / %"PRIu32")", anim->current_frame, anim->start_frame, anim->end_frame);
    }

    obj->is_dirty = true;
    gfx_obj_invalidate(obj);
}

gfx_obj_t *gfx_anim_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for animation object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->parent_handle = handle;
    obj->is_visible = true;

    gfx_anim_property_t *anim = (gfx_anim_property_t *)malloc(sizeof(gfx_anim_property_t));
    if (anim == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for animation property");
        free(obj);
        return NULL;
    }
    memset(anim, 0, sizeof(gfx_anim_property_t));

    anim->file_desc = NULL;
    anim->start_frame = 0;
    anim->end_frame = 0;
    anim->current_frame = 0;
    anim->fps = 30;
    anim->repeat = true;
    anim->is_playing = false;

    anim->mirror_mode = GFX_MIRROR_DISABLED;
    anim->mirror_offset = 0;

    uint32_t period_ms = 1000 / anim->fps;
    anim->timer = gfx_timer_create((void *)obj->parent_handle, gfx_anim_timer_callback, period_ms, obj);
    if (anim->timer == NULL) {
        ESP_LOGE(TAG, "Failed to create animation timer");
        free(anim);
        free(obj);
        return NULL;
    }

    memset(&anim->frame.header, 0, sizeof(eaf_header_t));

    anim->frame.frame_data = NULL;
    anim->frame.frame_size = 0;

    anim->frame.block_offsets = NULL;
    anim->frame.pixel_buffer = NULL;
    anim->frame.color_palette = NULL;

    anim->frame.last_block = -1;

    anim->mirror_mode = GFX_MIRROR_DISABLED;
    anim->mirror_offset = 0;

    obj->src = anim;
    obj->type = GFX_OBJ_TYPE_ANIMATION;

    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_ANIMATION, obj);
    return obj;
}

esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    if (src_data == NULL) {
        ESP_LOGE(TAG, "Source data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    obj->is_dirty = true;
    gfx_obj_invalidate(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    if (anim->is_playing) {
        ESP_LOGD(TAG, "stop current animation");
        gfx_anim_stop(obj);
    }

    if (anim->frame.header.width > 0) {
        eaf_free_header(&anim->frame.header);
        memset(&anim->frame.header, 0, sizeof(eaf_header_t));
    }
    anim->frame.frame_data = NULL;
    anim->frame.frame_size = 0;

    eaf_format_handle_t new_desc;
    eaf_init(src_data, src_len, &new_desc);
    if (new_desc == NULL) {
        ESP_LOGE(TAG, "Failed to initialize asset parser");
        return ESP_FAIL;
    }

    if (anim->file_desc) {
        eaf_deinit(anim->file_desc);
        anim->file_desc = NULL;
    }

    anim->file_desc = new_desc;
    anim->start_frame = 0;
    anim->current_frame = 0;
    //last block is empty
    anim->end_frame = eaf_get_total_frames(new_desc) - 2;

    ESP_LOGD(TAG, "set src, start: %"PRIu32", end: %"PRIu32", file_desc: %p", anim->start_frame, anim->end_frame, anim->file_desc);
    return ESP_OK;
}

esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    int total_frames = eaf_get_total_frames(anim->file_desc);

    anim->start_frame = start;
    anim->end_frame = (end > total_frames - 2) ? (total_frames - 2) : end;
    anim->current_frame = start;

    if (anim->fps != fps) {
        ESP_LOGI(TAG, "FPS changed from %"PRIu32" to %"PRIu32", updating timer period", anim->fps, fps);
        anim->fps = fps;

        if (anim->timer != NULL) {
            uint32_t new_period_ms = 1000 / fps;
            gfx_timer_set_period(anim->timer, new_period_ms);
            ESP_LOGI(TAG, "Animation timer period updated to %"PRIu32" ms for %"PRIu32" FPS", new_period_ms, fps);
        }
    }

    anim->repeat = repeat;

    ESP_LOGD(TAG, "Set animation segment: %"PRIu32" -> %"PRIu32"(%d, %"PRIu32"), fps: %"PRIu32", repeat: %d", anim->start_frame, anim->end_frame, total_frames, end, fps, repeat);
    return ESP_OK;
}

esp_err_t gfx_anim_start(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    if (anim->file_desc == NULL) {
        ESP_LOGE(TAG, "Animation source not set");
        return ESP_ERR_INVALID_STATE;
    }

    if (anim->is_playing) {
        ESP_LOGD(TAG, "Animation is already playing");
        return ESP_OK;
    }

    anim->is_playing = true;
    anim->current_frame = anim->start_frame;

    ESP_LOGD(TAG, "Started animation");
    return ESP_OK;
}

esp_err_t gfx_anim_stop(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    if (!anim->is_playing) {
        ESP_LOGD(TAG, "Animation is not playing");
        return ESP_OK;
    }

    anim->is_playing = false;

    ESP_LOGD(TAG, "Stopped animation");
    return ESP_OK;
}

esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    anim->mirror_mode = enabled ? GFX_MIRROR_MANUAL : GFX_MIRROR_DISABLED;
    anim->mirror_offset = offset;

    ESP_LOGD(TAG, "Set animation mirror: enabled=%s, offset=%d", enabled ? "true" : "false", offset);
    return ESP_OK;
}

esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    anim->mirror_mode = enabled ? GFX_MIRROR_AUTO : GFX_MIRROR_DISABLED;

    ESP_LOGD(TAG, "Set auto mirror alignment: enabled=%s", enabled ? "true" : "false");
    return ESP_OK;
}

/*=====================
 * Animation object deletion
 *====================*/

esp_err_t gfx_anim_delete(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim) {
        if (anim->is_playing) {
            gfx_anim_stop(obj);
        }

        if (anim->timer != NULL) {
            gfx_timer_delete((void *)obj->parent_handle, anim->timer);
            anim->timer = NULL;
        }

        gfx_anim_free_frame_info(&anim->frame);

        if (anim->file_desc) {
            eaf_deinit(anim->file_desc);
        }

        free(anim);
    }
    return ESP_OK;
}
