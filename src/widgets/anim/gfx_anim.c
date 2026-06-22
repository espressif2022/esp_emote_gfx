/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_ANIM
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"
#include "platform/gfx_platform.h"
#include "gfx/widgets/anim.h"
#include "codecs/anim/gfx_anim_decoder_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_ANIMATION(obj)           CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_ANIMATION, TAG)

#define GFX_PALETTE_CACHE_TRANSPARENT           0x00010000U
#define GFX_PALETTE_CACHE_COLOR_MASK            0x0000FFFFU

#define GFX_PALETTE_IS_TRANSPARENT(cache_val)   ((cache_val) & GFX_PALETTE_CACHE_TRANSPARENT)
#define GFX_PALETTE_GET_COLOR(cache_val)        ((cache_val) & GFX_PALETTE_CACHE_COLOR_MASK)
#define GFX_PALETTE_SET_TRANSPARENT()           (GFX_PALETTE_CACHE_TRANSPARENT)
#define GFX_PALETTE_SET_COLOR(color_val)        ((color_val) & GFX_PALETTE_CACHE_COLOR_MASK)

#define GFX_ANIM_EVENT_PLAN_DONE                (1U << 0)
#define GFX_ANIM_EVENT_SEGMENT_PAUSED           (1U << 1)
#define GFX_ANIM_DRAIN_FRAME_STEP               2U

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    GFX_MIRROR_DISABLED = 0,
    GFX_MIRROR_MANUAL = 1,
    GFX_MIRROR_AUTO = 2
} gfx_mirror_mode_t;

typedef struct {
    gfx_anim_frame_desc_t desc;
    const void *frame_payload;
    size_t frame_payload_size;
    uint32_t *block_offsets;
    uint8_t *decode_buffer;
    uint32_t *palette_cache;
    /* Allocated capacities so the per-frame buffers are reused across frames
     * (anim geometry is constant) and only reallocated when they must grow. */
    size_t block_offsets_cap;   /*!< capacity in entries */
    size_t decode_buffer_cap;   /*!< capacity in bytes */
    size_t palette_cache_cap;   /*!< capacity in entries */
    uint16_t cached_block_index;
    bool has_cached_block;
} gfx_anim_frame_cache_t;

typedef struct {
    uint32_t start_frame;
    uint32_t end_frame;
    uint32_t current_frame;
    uint32_t fps;
    bool is_playing;
    bool repeat;
    gfx_anim_segment_t *segments;
    size_t segment_count;
    size_t segment_index;
    uint32_t segment_play_remaining;
    size_t pending_segment_index;
    bool segment_paused;
    bool drain_remaining_segments;
    gfx_platform_event_t event_group;
    gfx_timer_handle_t timer;
    gfx_anim_src_t src;
    const gfx_anim_decoder_t *decoder;
    void *decoder_ctx;
    gfx_anim_frame_cache_t frame;
    gfx_mirror_mode_t mirror_mode;
    int16_t mirror_offset;
} gfx_anim_t;

typedef enum {
    GFX_ANIM_DEPTH_4BIT = 4,
    GFX_ANIM_DEPTH_8BIT = 8,
    GFX_ANIM_DEPTH_24BIT = 24,
} gfx_anim_depth_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static gfx_err_t gfx_anim_delete(gfx_object_t *obj);
static gfx_err_t gfx_anim_update(gfx_object_t *obj);
static gfx_err_t gfx_draw_animation(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static void gfx_anim_free_frame_buffers(gfx_anim_frame_cache_t *frame);
static void gfx_anim_reset_runtime_state(gfx_anim_t *anim);
static void gfx_anim_reset_frame(gfx_anim_t *anim);
static void gfx_anim_clear_segments(gfx_anim_t *anim);
static void gfx_anim_release_source(gfx_anim_t *anim);
static bool gfx_anim_has_source(const gfx_anim_t *anim);
static uint32_t gfx_anim_get_frame_count(const gfx_anim_t *anim);
static gfx_err_t gfx_anim_prepare_frame(gfx_object_t *obj);
static gfx_err_t gfx_anim_apply_segment(gfx_object_t *obj, gfx_anim_t *anim, const gfx_anim_segment_t *segment, size_t segment_index);
static gfx_err_t gfx_anim_advance_segment(gfx_object_t *obj, gfx_anim_t *anim);
static void gfx_anim_signal_event(gfx_anim_t *anim, gfx_platform_event_bits_t bits);
static void gfx_anim_finish_plan(gfx_object_t *obj, gfx_anim_t *anim);
static gfx_err_t gfx_anim_validate_src(const gfx_anim_src_t *src);
static gfx_err_t gfx_anim_set_src_internal(gfx_object_t *obj, const gfx_anim_src_t *src);
static gfx_err_t gfx_anim_set_src_with_decoder_internal(gfx_object_t *obj, const gfx_anim_decoder_t *decoder,
        const gfx_anim_src_t *src);
static void gfx_anim_apply_natural_size(gfx_object_t *obj, const gfx_anim_t *anim,
                                        uint16_t width, uint16_t height);
static void gfx_anim_calculate_offsets(const gfx_anim_frame_desc_t *frame_desc, uint32_t *offsets);
static size_t gfx_anim_get_decode_buffer_size(const gfx_anim_frame_desc_t *frame_desc);
static gfx_err_t gfx_anim_init_palette_cache(gfx_object_t *obj, gfx_anim_t *anim);
static int32_t gfx_anim_get_effective_mirror_offset(const gfx_object_t *obj, const gfx_anim_t *anim);
static void gfx_anim_emit_update(gfx_object_t *obj, gfx_display_event_t event);
static uint8_t gfx_anim_get_4bit_pixel(const uint8_t *row, int32_t pixel_idx);
static uint8_t *gfx_anim_get_4bit_row(uint8_t *buffer, int32_t y, int32_t stride_pixels);
static int32_t gfx_anim_get_mirror_dst_x(int32_t width, int32_t mirror_offset, int32_t src_x_offset, int32_t x);
static uint8_t gfx_anim_surface_pixel_size(gfx_color_format_t format);
static void gfx_anim_write_rgb565_to_surface(uint8_t *dst, gfx_color_format_t format, uint16_t color);
static gfx_err_t gfx_anim_render_pixels(uint8_t bit_depth,
                                        const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset);
static void gfx_anim_render_4bit_pixels(const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset);
static void gfx_anim_render_8bit_pixels(const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset);
static void gfx_anim_render_24bit_pixels(const gfx_draw_ctx_t *ctx,
        const uint8_t *src_pixels, gfx_coord_t src_stride,
        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
        const gfx_area_t *draw_area,
        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
        int32_t src_x_offset, int32_t dest_x_offset);
static void gfx_anim_timer_callback(void *arg);

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *const TAG = "anim";
static const gfx_widget_class_t s_gfx_anim_widget_class = {
    .type = GFX_OBJ_TYPE_ANIMATION,
    .name = "anim",
    .draw = gfx_draw_animation,
    .delete = gfx_anim_delete,
    .update = gfx_anim_update,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_anim_free_frame_buffers(gfx_anim_frame_cache_t *frame)
{
    if (frame->block_offsets != NULL) {
        free(frame->block_offsets);
        frame->block_offsets = NULL;
    }
    if (frame->decode_buffer != NULL) {
        free(frame->decode_buffer);
        frame->decode_buffer = NULL;
    }
    if (frame->palette_cache != NULL) {
        free(frame->palette_cache);
        frame->palette_cache = NULL;
    }
    frame->block_offsets_cap = 0;
    frame->decode_buffer_cap = 0;
    frame->palette_cache_cap = 0;
}

static void gfx_anim_reset_runtime_state(gfx_anim_t *anim)
{
    if (anim == NULL) {
        return;
    }

    anim->segment_index = 0;
    anim->segment_play_remaining = 0;
    anim->pending_segment_index = 0;
    anim->segment_paused = false;
    anim->drain_remaining_segments = false;

    if (anim->event_group != NULL) {
        gfx_platform_event_clear(anim->event_group, GFX_ANIM_EVENT_PLAN_DONE | GFX_ANIM_EVENT_SEGMENT_PAUSED);
    }
}

static void gfx_anim_reset_frame(gfx_anim_t *anim)
{
    if (anim->decoder != NULL && anim->decoder->free_frame_desc != NULL) {
        anim->decoder->free_frame_desc(&anim->frame.desc);
    } else {
        memset(&anim->frame.desc, 0, sizeof(anim->frame.desc));
    }

    /* Keep block_offsets/decode_buffer/palette_cache allocated for reuse by the
     * next frame; they are released in gfx_anim_release_source(). The decoded
     * contents are invalidated by clearing the block cache below. */
    anim->frame.frame_payload = NULL;
    anim->frame.frame_payload_size = 0;
    anim->frame.cached_block_index = 0;
    anim->frame.has_cached_block = false;
}

static void gfx_anim_clear_segments(gfx_anim_t *anim)
{
    if (anim->segments != NULL) {
        free(anim->segments);
        anim->segments = NULL;
    }

    anim->segment_count = 0;
    gfx_anim_reset_runtime_state(anim);
}

static void gfx_anim_signal_event(gfx_anim_t *anim, gfx_platform_event_bits_t bits)
{
    if (anim != NULL && anim->event_group != NULL) {
        gfx_platform_event_set(anim->event_group, bits);
    }
}

static void gfx_anim_release_source(gfx_anim_t *anim)
{
    gfx_anim_reset_frame(anim);
    gfx_anim_free_frame_buffers(&anim->frame);
    gfx_anim_clear_segments(anim);

    if (anim->decoder != NULL && anim->decoder->close != NULL && anim->decoder_ctx != NULL) {
        anim->decoder->close(anim->decoder_ctx);
    }

    anim->decoder = NULL;
    anim->decoder_ctx = NULL;
    memset(&anim->src, 0, sizeof(anim->src));
    anim->start_frame = 0;
    anim->end_frame = 0;
    anim->current_frame = 0;
}

static bool gfx_anim_has_source(const gfx_anim_t *anim)
{
    return anim != NULL && anim->decoder != NULL && anim->decoder_ctx != NULL;
}

static uint32_t gfx_anim_get_frame_count(const gfx_anim_t *anim)
{
    if (!gfx_anim_has_source(anim)) {
        return 0;
    }

    return anim->decoder->get_frame_count(anim->decoder_ctx);
}

static gfx_err_t gfx_anim_validate_src(const gfx_anim_src_t *src)
{
    GFX_RETURN_ON_FALSE(src != NULL, GFX_ERR_INVALID_ARG, TAG, "set animation source: source is NULL");
    GFX_RETURN_ON_FALSE(src->data != NULL, GFX_ERR_INVALID_ARG, TAG, "set animation source: source payload is NULL");

    switch (src->type) {
    case GFX_ANIM_SRC_TYPE_MEMORY:
        GFX_RETURN_ON_FALSE(src->data_len > 0U, GFX_ERR_INVALID_ARG, TAG, "set animation source: source length must be greater than 0");
        return GFX_OK;
    case GFX_ANIM_SRC_TYPE_FILE:
        return GFX_OK;
    default:
        return GFX_ERR_NOT_SUPPORTED;
    }
}

static gfx_err_t gfx_anim_apply_segment(gfx_object_t *obj, gfx_anim_t *anim, const gfx_anim_segment_t *segment, size_t segment_index)
{
    uint32_t total_frames;

    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "apply segment: animation context is NULL");
    GFX_RETURN_ON_FALSE(segment != NULL, GFX_ERR_INVALID_ARG, TAG, "apply segment: segment is NULL");
    GFX_RETURN_ON_FALSE(gfx_anim_has_source(anim), GFX_ERR_INVALID_STATE, TAG, "apply segment: source is not set");
    GFX_RETURN_ON_FALSE(segment->fps > 0U, GFX_ERR_INVALID_ARG, TAG, "apply segment: fps must be greater than 0");
    GFX_RETURN_ON_FALSE(segment->end_action <= GFX_ANIM_SEGMENT_ACTION_PAUSE, GFX_ERR_INVALID_ARG, TAG, "apply segment: end action is invalid");

    total_frames = gfx_anim_get_frame_count(anim);
    GFX_RETURN_ON_FALSE(total_frames > 0U, GFX_ERR_INVALID_STATE, TAG, "apply segment: source contains no frames");
    GFX_RETURN_ON_FALSE(segment->start < total_frames, GFX_ERR_INVALID_ARG, TAG, "apply segment: start frame is out of range");
    GFX_RETURN_ON_FALSE(segment->end >= segment->start, GFX_ERR_INVALID_ARG, TAG, "apply segment: end frame must be >= start frame");

    anim->start_frame = segment->start;
    anim->end_frame = (segment->end >= total_frames) ? (total_frames - 1U) : segment->end;
    anim->current_frame = segment->start;
    anim->fps = segment->fps;
    anim->repeat = (segment->play_count == 0U) || (segment->play_count > 1U);
    anim->segment_index = segment_index;
    anim->segment_play_remaining = segment->play_count;
    anim->pending_segment_index = 0;
    anim->segment_paused = false;

    if (anim->timer != NULL) {
        gfx_timer_set_period(anim->timer, 1000 / segment->fps);
    }

    if (anim->event_group != NULL) {
        gfx_platform_event_clear(anim->event_group, GFX_ANIM_EVENT_PLAN_DONE | GFX_ANIM_EVENT_SEGMENT_PAUSED);
    }

    GFX_RETURN_ON_ERROR(gfx_anim_prepare_frame(obj), TAG, "apply segment: failed to prepare the start frame");
    gfx_object_invalidate(obj);
    GFX_LOGD(TAG, "applied segment[%u]: range:[%" PRIu32 "-%" PRIu32 "], fps:%" PRIu32 ", repeat:%" PRIu32,
             (unsigned int)segment_index, anim->start_frame, anim->end_frame, segment->fps, segment->play_count);
    return GFX_OK;
}

static gfx_err_t gfx_anim_advance_segment(gfx_object_t *obj, gfx_anim_t *anim)
{
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "advance segment: animation context is NULL");
    GFX_RETURN_ON_FALSE(anim->segments != NULL, GFX_ERR_NOT_FOUND, TAG, "advance segment: segment plan is not set");
    if ((anim->segment_index + 1U) >= anim->segment_count) {
        return GFX_ERR_NOT_FOUND;
    }

    return gfx_anim_apply_segment(obj, anim, &anim->segments[anim->segment_index + 1U], anim->segment_index + 1U);
}

static void gfx_anim_finish_plan(gfx_object_t *obj, gfx_anim_t *anim)
{
    if (anim == NULL) {
        return;
    }

    anim->is_playing = false;
    anim->segment_paused = false;
    anim->pending_segment_index = 0;
    anim->drain_remaining_segments = false;
    gfx_anim_signal_event(anim, GFX_ANIM_EVENT_PLAN_DONE);

    gfx_anim_emit_update(obj, GFX_DISPLAY_EVENT_ALL_FRAME_DONE);
}

static void gfx_anim_calculate_offsets(const gfx_anim_frame_desc_t *frame_desc, uint32_t *offsets)
{
    offsets[0] = frame_desc->data_offset;
    for (int i = 1; i < frame_desc->blocks; i++) {
        offsets[i] = offsets[i - 1] + frame_desc->block_len[i - 1];
    }
}

static size_t gfx_anim_get_decode_buffer_size(const gfx_anim_frame_desc_t *frame_desc)
{
    if (frame_desc->bit_depth == GFX_ANIM_DEPTH_4BIT) {
        return ((frame_desc->width + 1U) / 2U) * frame_desc->block_height;
    }
    if (frame_desc->bit_depth == GFX_ANIM_DEPTH_8BIT) {
        return frame_desc->width * frame_desc->block_height;
    }
    if (frame_desc->bit_depth == GFX_ANIM_DEPTH_24BIT) {
        return frame_desc->width * frame_desc->block_height * sizeof(gfx_color_t);
    }
    return 0;
}

static gfx_err_t gfx_anim_init_palette_cache(gfx_object_t *obj, gfx_anim_t *anim)
{
    (void)obj;
    const gfx_anim_frame_desc_t *frame_desc = &anim->frame.desc;
    const gfx_anim_decoder_t *decoder = anim->decoder;
    uint16_t palette_size = frame_desc->num_colors;

    if (frame_desc->bit_depth == GFX_ANIM_DEPTH_24BIT || palette_size == 0U) {
        return GFX_OK;
    }

    GFX_RETURN_ON_FALSE(decoder != NULL && decoder->read_palette_color != NULL,
                        GFX_ERR_INVALID_STATE, TAG, "init palette cache: decoder palette callback is missing");

    /* Reuse the palette cache across frames; only (re)allocate when it grows.
     * The entries themselves are rebuilt below for every frame. */
    if (anim->frame.palette_cache == NULL || anim->frame.palette_cache_cap < palette_size) {
        free(anim->frame.palette_cache);
        anim->frame.palette_cache = gfx_platform_malloc(palette_size * sizeof(uint32_t),
                                    GFX_PLATFORM_HEAP_INTERNAL | GFX_PLATFORM_HEAP_8BIT);
        GFX_RETURN_ON_FALSE(anim->frame.palette_cache != NULL, GFX_ERR_NO_MEM, TAG, "init palette cache: failed to allocate palette cache");
        anim->frame.palette_cache_cap = palette_size;
    }

    for (uint16_t i = 0; i < palette_size; i++) {
        gfx_color_t color;
        if (decoder->read_palette_color(frame_desc, i, &color)) {
            anim->frame.palette_cache[i] = GFX_PALETTE_SET_TRANSPARENT();
        } else {
            anim->frame.palette_cache[i] = GFX_PALETTE_SET_COLOR(color.full);
        }
    }

    return GFX_OK;
}

static int32_t gfx_anim_get_effective_mirror_offset(const gfx_object_t *obj, const gfx_anim_t *anim)
{
    if (anim->mirror_mode == GFX_MIRROR_AUTO) {
        gfx_area_t obj_area;
        gfx_coord_t obj_x;
        int32_t source_width = anim->frame.desc.width > 0 ? anim->frame.desc.width : obj->local_geometry.width;

        if (!gfx_object_get_abs_area_exclusive((gfx_object_t *)obj, &obj_area)) {
            obj_x = obj->local_geometry.x;
        } else {
            obj_x = obj_area.x1;
        }
        int32_t parent_w = (int32_t)gfx_display_get_h_res(obj->disp);
        return parent_w - ((source_width + obj_x) * 2);
    }

    if (anim->mirror_mode == GFX_MIRROR_MANUAL) {
        return anim->mirror_offset;
    }

    return 0;
}

static void gfx_anim_apply_natural_size(gfx_object_t *obj, const gfx_anim_t *anim,
                                        uint16_t width, uint16_t height)
{
    uint16_t resolved_width = width;
    uint16_t resolved_height = height;
    int32_t resolved_width_i32 = width;
    int32_t mirror_offset;

    if (obj == NULL || anim == NULL || width == 0U || height == 0U) {
        return;
    }

    if (anim->mirror_mode != GFX_MIRROR_DISABLED) {
        if (anim->mirror_mode == GFX_MIRROR_AUTO) {
            gfx_area_t obj_area;
            gfx_coord_t obj_x;

            if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
                obj_x = obj->local_geometry.x;
            } else {
                obj_x = obj_area.x1;
            }
            mirror_offset = (int32_t)gfx_display_get_h_res(obj->disp) - (((int32_t)width + obj_x) * 2);
        } else {
            mirror_offset = gfx_anim_get_effective_mirror_offset(obj, anim);
        }
        resolved_width_i32 = MAX(0, (int32_t)width * 2 + mirror_offset);
        resolved_width_i32 = MIN(resolved_width_i32, (int32_t)UINT16_MAX);
        resolved_width = (uint16_t)resolved_width_i32;
    }

    if (obj->geometry.width == resolved_width && obj->geometry.height == resolved_height &&
            obj->local_geometry.width == resolved_width && obj->local_geometry.height == resolved_height) {
        return;
    }

    gfx_object_invalidate_tree(obj);
    obj->geometry.width = resolved_width;
    obj->geometry.height = resolved_height;
    obj->local_geometry.width = resolved_width;
    obj->local_geometry.height = resolved_height;
    gfx_object_invalidate_abs_area_cache_tree(obj);
    gfx_object_update_layout(obj);
    gfx_object_invalidate_tree(obj);
}

static void gfx_anim_emit_update(gfx_object_t *obj, gfx_display_event_t event)
{
    if (obj != NULL && obj->disp != NULL && obj->disp->cb.update_cb != NULL) {
        obj->disp->cb.update_cb(obj->disp, event, obj);
    }
}

static uint8_t gfx_anim_get_4bit_pixel(const uint8_t *row, int32_t pixel_idx)
{
    uint8_t packed = row[pixel_idx / 2];

    return (pixel_idx & 1) ? (packed & 0x0FU) : (packed >> 4);
}

static uint8_t *gfx_anim_get_4bit_row(uint8_t *buffer, int32_t y, int32_t stride_pixels)
{
    int32_t stride_bytes = (stride_pixels + 1) / 2;

    return buffer + (y * stride_bytes);
}

static int32_t gfx_anim_get_mirror_dst_x(int32_t width, int32_t mirror_offset, int32_t src_x_offset, int32_t x)
{
    int32_t mirror_frame_x = width + mirror_offset + width - 1 - (src_x_offset + x);

    return mirror_frame_x - src_x_offset;
}

static uint8_t gfx_anim_surface_pixel_size(gfx_color_format_t format)
{
    if (format == GFX_COLOR_FORMAT_XRGB8888 || format == GFX_COLOR_FORMAT_ARGB8888) {
        return GFX_PIXEL_SIZE_32BPP;
    }
    return gfx_color_format_get_size(format);
}

static void gfx_anim_write_rgb565_to_surface(uint8_t *dst, gfx_color_format_t format, uint16_t color)
{
    if (dst == NULL) {
        return;
    }

    if (gfx_color_format_is_rgb565(format)) {
        gfx_color_write_rgb565_bytes(dst, format, color);
        return;
    }

    uint8_t r5 = (uint8_t)((color >> 11) & 0x1fU);
    uint8_t g6 = (uint8_t)((color >> 5) & 0x3fU);
    uint8_t b5 = (uint8_t)(color & 0x1fU);
    uint8_t r = (uint8_t)((r5 << 3) | (r5 >> 2));
    uint8_t g = (uint8_t)((g6 << 2) | (g6 >> 4));
    uint8_t b = (uint8_t)((b5 << 3) | (b5 >> 2));

    if (format == GFX_COLOR_FORMAT_RGB888 || format == GFX_COLOR_FORMAT_BGR888) {
        gfx_color_write_rgb888_bytes(dst, format, r, g, b);
    } else if (format == GFX_COLOR_FORMAT_XRGB8888 || format == GFX_COLOR_FORMAT_ARGB8888) {
        uint32_t px = 0xff000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        memcpy(dst, &px, sizeof(px));
    }
}

static gfx_err_t gfx_anim_prepare_frame(gfx_object_t *obj)
{
    gfx_err_t ret = GFX_OK;
    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    const gfx_anim_decoder_t *decoder = anim->decoder;
    uint32_t current_frame = anim->current_frame;

    GFX_RETURN_ON_FALSE(gfx_anim_has_source(anim), GFX_ERR_INVALID_STATE, TAG, "prepare frame: decoder is not ready");

    gfx_anim_reset_frame(anim);

    anim->frame.frame_payload = decoder->get_frame_payload(anim->decoder_ctx, current_frame);
    anim->frame.frame_payload_size = decoder->get_frame_payload_size(anim->decoder_ctx, current_frame);
    GFX_RETURN_ON_FALSE(anim->frame.frame_payload != NULL, GFX_FAIL, TAG,
                        "prepare frame[%" PRIu32 "]: frame payload is unavailable", current_frame);
    GFX_RETURN_ON_FALSE(anim->frame.frame_payload_size > 0, GFX_FAIL, TAG,
                        "prepare frame[%" PRIu32 "]: frame payload size is invalid", current_frame);

    GFX_GOTO_ON_ERROR(decoder->read_frame_desc(anim->decoder_ctx, current_frame, &anim->frame.desc), err, TAG,
                      "prepare frame[%" PRIu32 "]: failed to read frame descriptor", current_frame);

    size_t decode_buffer_size = gfx_anim_get_decode_buffer_size(&anim->frame.desc);
    GFX_GOTO_ON_FALSE(decode_buffer_size > 0, GFX_ERR_INVALID_ARG, err, TAG,
                      "prepare frame: unsupported bit depth %u", anim->frame.desc.bit_depth);

    /* Reuse the block-offset table when it is already large enough; only grow. */
    if (anim->frame.block_offsets == NULL || anim->frame.block_offsets_cap < anim->frame.desc.blocks) {
        free(anim->frame.block_offsets);
        anim->frame.block_offsets = malloc(anim->frame.desc.blocks * sizeof(uint32_t));
        GFX_GOTO_ON_FALSE(anim->frame.block_offsets != NULL, GFX_ERR_NO_MEM, err, TAG, "prepare frame: failed to allocate block offsets");
        anim->frame.block_offsets_cap = anim->frame.desc.blocks;
    }

    /* Reuse the decode buffer across frames (constant geometry); reallocate only
     * when a larger block payload is needed. 24-bit output keeps 16-byte
     * alignment for the blit path. */
    if (anim->frame.decode_buffer == NULL || anim->frame.decode_buffer_cap < decode_buffer_size) {
        free(anim->frame.decode_buffer);
        anim->frame.decode_buffer = NULL;
        if (anim->frame.desc.bit_depth == GFX_ANIM_DEPTH_24BIT) {
            anim->frame.decode_buffer = gfx_platform_aligned_alloc(16, decode_buffer_size, GFX_PLATFORM_HEAP_DEFAULT);
        } else {
            anim->frame.decode_buffer = malloc(decode_buffer_size);
        }
        GFX_GOTO_ON_FALSE(anim->frame.decode_buffer != NULL, GFX_ERR_NO_MEM, err, TAG, "prepare frame: failed to allocate decode buffer");
        anim->frame.decode_buffer_cap = decode_buffer_size;
    }

    GFX_GOTO_ON_ERROR(gfx_anim_init_palette_cache(obj, anim), err, TAG, "prepare frame: failed to initialize palette cache");

    gfx_anim_calculate_offsets(&anim->frame.desc, anim->frame.block_offsets);

    gfx_anim_apply_natural_size(obj, anim, anim->frame.desc.width, anim->frame.desc.height);

    // GFX_LOGD(TAG, "prepared frame[%" PRIu32 "] with decoder %s", current_frame,
    //          decoder->name ? decoder->name : "unknown");
    return ret;

err:
    gfx_anim_reset_frame(anim);
    return ret;
}

static gfx_err_t gfx_anim_render_pixels(uint8_t bit_depth,
                                        const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset)
{
    switch (bit_depth) {
    case GFX_ANIM_DEPTH_4BIT:
        gfx_anim_render_4bit_pixels(ctx, src_pixels, src_stride,
                                    frame_desc, palette_cache, draw_area,
                                    mirror_mode, mirror_offset, src_x_offset, dest_x_offset);
        return GFX_OK;
    case GFX_ANIM_DEPTH_8BIT:
        gfx_anim_render_8bit_pixels(ctx, src_pixels, src_stride,
                                    frame_desc, palette_cache, draw_area,
                                    mirror_mode, mirror_offset, src_x_offset, dest_x_offset);
        return GFX_OK;
    case GFX_ANIM_DEPTH_24BIT:
        gfx_anim_render_24bit_pixels(ctx, src_pixels, src_stride,
                                     frame_desc, palette_cache, draw_area,
                                     mirror_mode, mirror_offset, src_x_offset, dest_x_offset);
        return GFX_OK;
    default:
        return GFX_ERR_INVALID_ARG;
    }
}

static void gfx_anim_render_4bit_pixels(const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset)
{
    int32_t width = frame_desc->width;
    int32_t clip_width = draw_area->x2 - draw_area->x1;
    int32_t clip_height = draw_area->y2 - draw_area->y1;
    int32_t src_stride_bytes = (src_stride + 1) / 2;
    gfx_color_format_t dst_format = ctx->format;
    uint8_t dst_px_size = gfx_anim_surface_pixel_size(dst_format);
    uint8_t *dst_base = (uint8_t *)ctx->buf;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (ctx->stride - (src_stride + dest_x_offset) * 2);
    }

    for (int32_t y = 0; y < clip_height; y++) {
        const uint8_t *src_row = src_pixels + (y * src_stride_bytes);
        uint8_t *dst_row = dst_base + ((size_t)(draw_area->y1 + y) * ctx->stride +
                                       (size_t)draw_area->x1) * dst_px_size;

        for (int32_t x = 0; x < clip_width; x++) {
            uint8_t index = gfx_anim_get_4bit_pixel(src_row, src_x_offset + x);
            uint32_t palette_value = palette_cache[index];

            if (GFX_PALETTE_IS_TRANSPARENT(palette_value)) {
                continue;
            }

            gfx_color_t color = {
                .full = (uint16_t)GFX_PALETTE_GET_COLOR(palette_value),
            };
            gfx_anim_write_rgb565_to_surface(dst_row + (size_t)x * dst_px_size, dst_format, color.full);

            if (mirror_mode != GFX_MIRROR_DISABLED) {
                int32_t mirror_x = gfx_anim_get_mirror_dst_x(width, mirror_offset, src_x_offset, x);
                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < ctx->stride) {
                    gfx_anim_write_rgb565_to_surface(dst_row + (size_t)mirror_x * dst_px_size,
                                                     dst_format, color.full);
                }
            }
        }
    }
}

static void gfx_anim_render_8bit_pixels(const gfx_draw_ctx_t *ctx,
                                        const uint8_t *src_pixels, gfx_coord_t src_stride,
                                        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
                                        const gfx_area_t *draw_area,
                                        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
                                        int32_t src_x_offset, int32_t dest_x_offset)
{
    int32_t clip_width = draw_area->x2 - draw_area->x1;
    int32_t clip_height = draw_area->y2 - draw_area->y1;
    int32_t width = frame_desc->width;
    gfx_color_format_t dst_format = ctx->format;
    uint8_t dst_px_size = gfx_anim_surface_pixel_size(dst_format);
    uint8_t *dst_base = (uint8_t *)ctx->buf;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (ctx->stride - (src_stride + dest_x_offset) * 2);
    }

    for (int32_t y = 0; y < clip_height; y++) {
        const uint8_t *src = src_pixels + y * src_stride;
        uint8_t *dst = dst_base + ((size_t)(draw_area->y1 + y) * ctx->stride +
                                   (size_t)draw_area->x1) * dst_px_size;
        int x = 0;
        int x_end4 = clip_width - 4;

        for (; x <= x_end4; x += 4) {
            uint32_t p0 = palette_cache[src[x]];
            uint32_t p1 = palette_cache[src[x + 1]];
            uint32_t p2 = palette_cache[src[x + 2]];
            uint32_t p3 = palette_cache[src[x + 3]];
            uint32_t transparent_mask = (p0 | p1 | p2 | p3) & GFX_PALETTE_CACHE_TRANSPARENT;

            if (transparent_mask) {
                if (!(p0 & GFX_PALETTE_CACHE_TRANSPARENT)) {
                    gfx_anim_write_rgb565_to_surface(dst + (size_t)x * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p0));
                }
                if (!(p1 & GFX_PALETTE_CACHE_TRANSPARENT)) {
                    gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 1) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p1));
                }
                if (!(p2 & GFX_PALETTE_CACHE_TRANSPARENT)) {
                    gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 2) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p2));
                }
                if (!(p3 & GFX_PALETTE_CACHE_TRANSPARENT)) {
                    gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 3) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p3));
                }
            } else {
                gfx_anim_write_rgb565_to_surface(dst + (size_t)x * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p0));
                gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 1) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p1));
                gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 2) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p2));
                gfx_anim_write_rgb565_to_surface(dst + (size_t)(x + 3) * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p3));
            }
        }

        for (; x < clip_width; x++) {
            uint32_t p = palette_cache[src[x]];
            if (!(p & GFX_PALETTE_CACHE_TRANSPARENT)) {
                gfx_anim_write_rgb565_to_surface(dst + (size_t)x * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p));
            }
        }

        if (mirror_mode != GFX_MIRROR_DISABLED) {
            for (int32_t x_mirror = 0; x_mirror < clip_width; x_mirror++) {
                uint32_t p = palette_cache[src[x_mirror]];
                if (!GFX_PALETTE_IS_TRANSPARENT(p)) {
                    int32_t mirror_x = gfx_anim_get_mirror_dst_x(width, mirror_offset, src_x_offset, x_mirror);
                    if (mirror_x >= 0 && (dest_x_offset + mirror_x) < ctx->stride) {
                        gfx_anim_write_rgb565_to_surface(dst + (size_t)mirror_x * dst_px_size, dst_format, (uint16_t)GFX_PALETTE_GET_COLOR(p));
                    }
                }
            }
        }
    }
}

static void gfx_anim_render_24bit_pixels(const gfx_draw_ctx_t *ctx,
        const uint8_t *src_pixels, gfx_coord_t src_stride,
        const gfx_anim_frame_desc_t *frame_desc, uint32_t *palette_cache,
        const gfx_area_t *draw_area,
        gfx_mirror_mode_t mirror_mode, int32_t mirror_offset,
        int32_t src_x_offset, int32_t dest_x_offset)
{
    (void)frame_desc;
    (void)palette_cache;

    int32_t clip_width = draw_area->x2 - draw_area->x1;
    int32_t clip_height = draw_area->y2 - draw_area->y1;
    int32_t width = src_stride;
    uint8_t dst_px_size = gfx_anim_surface_pixel_size(ctx->format);
    uint8_t *dest_base = (uint8_t *)ctx->buf;

    if (mirror_mode == GFX_MIRROR_AUTO) {
        mirror_offset = (ctx->stride - (src_stride + dest_x_offset) * 2);
    }

    for (int32_t y = 0; y < clip_height; y++) {
        uint8_t *dst_row = dest_base + ((size_t)(draw_area->y1 + y) * ctx->stride +
                                        (size_t)draw_area->x1) * dst_px_size;
        const uint8_t *src_row = src_pixels + (size_t)y * src_stride * sizeof(gfx_color_t);
        int32_t x = 0;
        int32_t x_end4 = clip_width - 4;

        for (; x <= x_end4; x += 4) {
            const gfx_color_t *src_px = (const gfx_color_t *)(src_row + (size_t)x * sizeof(gfx_color_t));
            uint8_t *dst_px = dst_row + (size_t)x * dst_px_size;
            for (int i = 0; i < 4; i++) {
                gfx_anim_write_rgb565_to_surface(dst_px + (size_t)i * dst_px_size, ctx->format, src_px[i].full);
            }
        }

        for (; x < clip_width; x++) {
            gfx_anim_write_rgb565_to_surface(dst_row + (size_t)x * dst_px_size, ctx->format,
                                             ((const gfx_color_t *)(src_row))[x].full);
        }
    }

    if (mirror_mode != GFX_MIRROR_DISABLED) {
        for (int32_t y = 0; y < clip_height; y++) {
            uint8_t *dst_row = dest_base + ((size_t)(draw_area->y1 + y) * ctx->stride +
                                            (size_t)draw_area->x1) * dst_px_size;
            const gfx_color_t *src_row = (const gfx_color_t *)(src_pixels + (size_t)y * src_stride * sizeof(gfx_color_t));

            for (int32_t x = 0; x < clip_width; x++) {
                int32_t mirror_x = gfx_anim_get_mirror_dst_x(width, mirror_offset, src_x_offset, x);
                if (mirror_x >= 0 && (dest_x_offset + mirror_x) < ctx->stride) {
                    gfx_anim_write_rgb565_to_surface(dst_row + (size_t)mirror_x * dst_px_size,
                                                     ctx->format, src_row[x].full);
                }
            }
        }
    }
}

static gfx_err_t gfx_draw_animation(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGE(TAG, "draw animation: object, source, or draw context is NULL");
        return GFX_ERR_INVALID_ARG;
    }
    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        GFX_LOGE(TAG, "draw animation: object type is not animation");
        return GFX_ERR_INVALID_ARG;
    }

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    if (!gfx_anim_has_source(anim)) {
        return GFX_ERR_INVALID_STATE;
    }

    if (anim->frame.frame_payload == NULL) {
        return GFX_ERR_INVALID_STATE;
    }
    if (anim->frame.desc.width <= 0) {
        GFX_LOGE(TAG, "draw animation: frame[%" PRIu32 "] header is invalid", anim->current_frame);
        return GFX_ERR_INVALID_STATE;
    }

    const gfx_anim_frame_desc_t *frame_desc = &anim->frame.desc;
    uint8_t *decode_buffer = anim->frame.decode_buffer;
    uint32_t *block_offsets = anim->frame.block_offsets;
    uint32_t *palette_cache = anim->frame.palette_cache;
    uint16_t *cached_block_index = &anim->frame.cached_block_index;

    if (block_offsets == NULL || decode_buffer == NULL) {
        GFX_LOGE(TAG, "draw animation: frame[%" PRIu32 "] decode resources are not ready", anim->current_frame);
        return GFX_ERR_INVALID_STATE;
    }

    gfx_coord_t frame_width = (gfx_coord_t)frame_desc->width;
    gfx_coord_t frame_height = (gfx_coord_t)frame_desc->height;
    gfx_coord_t block_height = (gfx_coord_t)frame_desc->block_height;
    uint16_t num_blocks = frame_desc->blocks;

    gfx_area_t render_area = ctx->clip_area;
    gfx_area_t obj_area;
    gfx_area_t clip_area;

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    if (!gfx_area_intersect_exclusive(&clip_area, &render_area, &obj_area)) {
        return GFX_OK;
    }

    for (uint16_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        gfx_coord_t block_start_y = (gfx_coord_t)block_idx * block_height;
        gfx_coord_t block_end_y = (block_idx == (uint16_t)(num_blocks - 1U)) ? frame_height :
                                  (gfx_coord_t)(block_idx + 1U) * block_height;
        gfx_coord_t block_start_x = 0;
        gfx_coord_t block_end_x = frame_width;

        block_start_y += obj_area.y1;
        block_end_y += obj_area.y1;
        block_start_x += obj_area.x1;
        block_end_x += obj_area.x1;

        gfx_area_t block_area = {block_start_x, block_start_y, block_end_x, block_end_y};
        gfx_area_t clip_block;

        if (!gfx_area_intersect_exclusive(&clip_block, &clip_area, &block_area)) {
            continue;
        }

        gfx_coord_t src_offset_x = clip_block.x1 - block_start_x;
        gfx_coord_t src_offset_y = clip_block.y1 - block_start_y;

        if (src_offset_x < 0 || src_offset_y < 0 ||
                src_offset_x >= frame_width || src_offset_y >= block_height) {
            continue;
        }

        if (!anim->frame.has_cached_block || block_idx != *cached_block_index) {
            const uint8_t *block_payload = (const uint8_t *)anim->frame.frame_payload + block_offsets[block_idx];
            size_t block_payload_size = frame_desc->block_len[block_idx];
            gfx_err_t decode_result = anim->decoder->decode_frame_block(anim->decoder_ctx, frame_desc,
                                      block_payload, block_payload_size, decode_buffer);
            if (decode_result != GFX_OK) {
                continue;
            }
            *cached_block_index = block_idx;
            anim->frame.has_cached_block = true;
        }

        gfx_coord_t src_stride = frame_width;
        uint8_t *src_pixels = NULL;

        if (frame_desc->bit_depth == GFX_ANIM_DEPTH_24BIT) {
            src_pixels = GFX_BUFFER_OFFSET_16BPP(decode_buffer, src_offset_y, src_stride, src_offset_x);
        } else if (frame_desc->bit_depth == GFX_ANIM_DEPTH_4BIT) {
            src_pixels = gfx_anim_get_4bit_row(decode_buffer, src_offset_y, src_stride);
        } else if (frame_desc->bit_depth == GFX_ANIM_DEPTH_8BIT) {
            src_pixels = GFX_BUFFER_OFFSET_8BPP(decode_buffer, src_offset_y, src_stride, src_offset_x);
        } else {
            GFX_LOGE(TAG, "draw animation: unsupported bit depth %d", frame_desc->bit_depth);
            return GFX_ERR_INVALID_ARG;
        }

        gfx_area_t draw_area = {
            .x1 = clip_block.x1 - ctx->buf_area.x1,
            .y1 = clip_block.y1 - ctx->buf_area.y1,
            .x2 = clip_block.x2 - ctx->buf_area.x1,
            .y2 = clip_block.y2 - ctx->buf_area.y1,
        };
        int32_t dest_x_offset = clip_block.x1 - ctx->buf_area.x1;
        int32_t mirror_offset = gfx_anim_get_effective_mirror_offset(obj, anim);

        gfx_err_t render_result = gfx_anim_render_pixels(frame_desc->bit_depth,
                                  ctx,
                                  src_pixels, src_stride,
                                  frame_desc, palette_cache,
                                  &draw_area,
                                  anim->mirror_mode, mirror_offset, src_offset_x, dest_x_offset);
        if (render_result != GFX_OK) {
            continue;
        }
    }

    return GFX_OK;
}

static gfx_err_t gfx_anim_delete(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    if (anim != NULL) {
        if (anim->is_playing) {
            gfx_anim_stop(obj);
        }

        if (anim->timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, anim->timer);
            anim->timer = NULL;
        }

        if (anim->event_group != NULL) {
            gfx_platform_event_delete(anim->event_group);
            anim->event_group = NULL;
        }

        gfx_anim_release_source(anim);
        free(anim);
    }

    return GFX_OK;
}

static gfx_err_t gfx_anim_update(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);
    return obj->src != NULL ? GFX_OK : GFX_ERR_INVALID_STATE;
}

static void gfx_anim_timer_callback(void *arg)
{
    gfx_object_t *obj = (gfx_object_t *)arg;
    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    bool infinite_loop;
    bool repeat_current_segment;
    bool has_next_segment;
    gfx_anim_segment_action_t end_action;

    if (!anim || !anim->is_playing || obj->state.is_visible == false) {
        return;
    }

    if (anim->current_frame >= anim->end_frame) {
        has_next_segment = (anim->segments != NULL && (anim->segment_index + 1U) < anim->segment_count);

        if (anim->drain_remaining_segments) {
            infinite_loop = false;
            repeat_current_segment = false;
            end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;
        } else {
            infinite_loop = (anim->segments != NULL && anim->segment_count > 0U &&
                             anim->segments[anim->segment_index].play_count == 0U);
            repeat_current_segment = (anim->segments != NULL && anim->segment_count > 0U &&
                                      anim->segment_play_remaining > 1U);
            end_action = (anim->segments != NULL && anim->segment_count > 0U)
                         ? anim->segments[anim->segment_index].end_action
                         : GFX_ANIM_SEGMENT_ACTION_CONTINUE;
        }

        if (infinite_loop || repeat_current_segment) {
            if (repeat_current_segment) {
                anim->segment_play_remaining--;
            }

            GFX_LOGD(TAG, "timer: repeating segment[%u]", (unsigned int)anim->segment_index);
            anim->current_frame = anim->start_frame;
            if (gfx_anim_prepare_frame(obj) != GFX_OK) {
                return;
            }
            gfx_anim_emit_update(obj, GFX_DISPLAY_EVENT_PART_FRAME_DONE);
        } else if (has_next_segment && end_action == GFX_ANIM_SEGMENT_ACTION_PAUSE && !anim->drain_remaining_segments) {
            anim->is_playing = false;
            anim->segment_paused = true;
            anim->pending_segment_index = anim->segment_index + 1U;
            GFX_LOGD(TAG, "timer: pausing after segment[%u]", (unsigned int)anim->segment_index);
            gfx_anim_signal_event(anim, GFX_ANIM_EVENT_SEGMENT_PAUSED);
            gfx_anim_emit_update(obj, GFX_DISPLAY_EVENT_PART_FRAME_DONE);
            return;
        } else if (gfx_anim_advance_segment(obj, anim) == GFX_OK) {
            gfx_anim_emit_update(obj, GFX_DISPLAY_EVENT_PART_FRAME_DONE);
        } else {
            GFX_LOGD(TAG, "timer: segment plan completed");
            gfx_anim_finish_plan(obj, anim);
            return;
        }
    } else {
        uint32_t frame_step = anim->drain_remaining_segments ? GFX_ANIM_DRAIN_FRAME_STEP : 1U;
        uint32_t frames_left = anim->end_frame - anim->current_frame;

        anim->current_frame += MIN(frame_step, frames_left);
        if (gfx_anim_prepare_frame(obj) != GFX_OK) {
            return;
        }
        gfx_anim_emit_update(obj, GFX_DISPLAY_EVENT_ONE_FRAME_DONE);
        // GFX_LOGD(TAG, "timer: frame %" PRIu32 "/%" PRIu32, anim->current_frame, anim->end_frame);
    }

    gfx_object_invalidate(obj);
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_object_t *gfx_anim_create(gfx_display_t *disp)
{
    gfx_object_t *obj = NULL;
    gfx_anim_t *anim = NULL;
    uint32_t period_ms;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create animation: display must come from gfx_display_add");
        return NULL;
    }

    anim = (gfx_anim_t *)malloc(sizeof(gfx_anim_t));
    if (anim == NULL) {
        GFX_LOGE(TAG, "create animation: failed to allocate animation state");
        return NULL;
    }

    memset(anim, 0, sizeof(gfx_anim_t));
    anim->fps = 30;
    anim->repeat = true;
    anim->frame.cached_block_index = 0;
    anim->frame.has_cached_block = false;
    anim->event_group = gfx_platform_event_create();
    if (anim->event_group == NULL) {
        GFX_LOGE(TAG, "create animation: failed to create event group");
        free(anim);
        return NULL;
    }

    period_ms = 1000 / anim->fps;
    if (gfx_object_create_class_instance(disp, &s_gfx_anim_widget_class,
                                         anim, 0, 0, "gfx_anim_create", &obj) != GFX_OK) {
        GFX_LOGE(TAG, "create animation: failed to create object");
        gfx_platform_event_delete(anim->event_group);
        free(anim);
        return NULL;
    }

    anim->timer = gfx_timer_create(disp->ctx, gfx_anim_timer_callback, period_ms, obj);
    if (anim->timer == NULL) {
        GFX_LOGE(TAG, "create animation: failed to create timer");
        gfx_object_delete(obj);
        return NULL;
    }
    return obj;
}

gfx_err_t gfx_anim_set_src_desc(gfx_object_t *obj, const gfx_anim_src_t *src)
{
    return gfx_anim_set_src_internal(obj, src);
}

static gfx_err_t gfx_anim_set_src_internal(gfx_object_t *obj, const gfx_anim_src_t *src)
{
    const gfx_anim_decoder_t *decoder;

    CHECK_OBJ_TYPE_ANIMATION(obj);
    GFX_RETURN_ON_ERROR(gfx_anim_validate_src(src), TAG, "set animation source: validate source failed");

    decoder = gfx_anim_decoder_select(src);
    GFX_RETURN_ON_FALSE(decoder != NULL, GFX_ERR_NOT_FOUND, TAG, "set animation source: no decoder can open this source");
    return gfx_anim_set_src_with_decoder_internal(obj, decoder, src);
}

static gfx_err_t gfx_anim_set_src_with_decoder_internal(gfx_object_t *obj, const gfx_anim_decoder_t *decoder,
        const gfx_anim_src_t *src)
{
    gfx_err_t ret = GFX_OK;
    void *new_decoder_ctx = NULL;
    gfx_anim_info_t info;
    gfx_anim_t *anim;

    CHECK_OBJ_TYPE_ANIMATION(obj);
    GFX_RETURN_ON_FALSE(decoder != NULL, GFX_ERR_INVALID_ARG, TAG, "set animation source: decoder is NULL");
    GFX_RETURN_ON_ERROR(gfx_anim_validate_src(src), TAG, "set animation source: validate source failed");

    anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "set animation source: animation context is NULL");

    if (anim->is_playing) {
        gfx_anim_stop(obj);
    }

    GFX_RETURN_ON_ERROR(decoder->open(src, &new_decoder_ctx), TAG, "set animation source: open animation source failed");
    GFX_RETURN_ON_FALSE(new_decoder_ctx != NULL, GFX_ERR_INVALID_STATE, TAG, "set animation source: decoder returned a NULL context");

    memset(&info, 0, sizeof(info));
    ret = decoder->get_info(new_decoder_ctx, &info);
    if (ret != GFX_OK || info.frame_count == 0U || info.width == 0U || info.height == 0U) {
        decoder->close(new_decoder_ctx);
        return ret != GFX_OK ? ret : GFX_ERR_INVALID_SIZE;
    }

    gfx_object_invalidate(obj);
    gfx_anim_release_source(anim);
    gfx_anim_reset_runtime_state(anim);

    anim->src = *src;
    anim->decoder = decoder;
    anim->decoder_ctx = new_decoder_ctx;
    anim->start_frame = 0;
    anim->current_frame = 0;
    anim->end_frame = info.frame_count - 1U;

    gfx_anim_apply_natural_size(obj, anim, info.width, info.height);

    GFX_GOTO_ON_ERROR(gfx_anim_prepare_frame(obj), err, TAG, "set animation source: prepare the first frame failed");

    gfx_object_invalidate(obj);
    GFX_LOGD(TAG, "set animation source: decoder=%s frames=%" PRIu32 "-%" PRIu32,
             decoder->name ? decoder->name : "unknown", anim->start_frame, anim->end_frame);
    return GFX_OK;

err:
    gfx_anim_release_source(anim);
    return ret;
}

gfx_err_t gfx_anim_set_segment(gfx_object_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat)
{
    const gfx_anim_segment_t segment = {
        .start = start,
        .end = end,
        .fps = fps,
        .play_count = repeat ? 0U : 1U,
        .end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE,
    };

    return gfx_anim_set_segments(obj, &segment, 1U);
}

gfx_err_t gfx_anim_set_segments(gfx_object_t *obj, const gfx_anim_segment_t *segments, size_t segment_count)
{
    gfx_anim_t *anim;
    gfx_anim_segment_t *segment_copy;

    CHECK_OBJ_TYPE_ANIMATION(obj);
    GFX_RETURN_ON_FALSE(segments != NULL, GFX_ERR_INVALID_ARG, TAG, "set segments: segments is NULL");
    GFX_RETURN_ON_FALSE(segment_count > 0U, GFX_ERR_INVALID_ARG, TAG, "set segments: segment_count must be greater than 0");

    anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "set segments: animation context is NULL");
    GFX_RETURN_ON_FALSE(gfx_anim_has_source(anim), GFX_ERR_INVALID_STATE, TAG, "set segments: source is not set");

    segment_copy = calloc(segment_count, sizeof(gfx_anim_segment_t));
    GFX_RETURN_ON_FALSE(segment_copy != NULL, GFX_ERR_NO_MEM, TAG, "set segments: failed to allocate segment plan");

    memcpy(segment_copy, segments, segment_count * sizeof(gfx_anim_segment_t));

    gfx_anim_clear_segments(anim);
    anim->segments = segment_copy;
    anim->segment_count = segment_count;
    gfx_anim_reset_runtime_state(anim);

    return gfx_anim_apply_segment(obj, anim, &anim->segments[0], 0U);
}

gfx_err_t gfx_anim_play_left_to_tail(gfx_object_t *obj)
{
    gfx_platform_event_bits_t bits;
    gfx_anim_t *anim;

    CHECK_OBJ_TYPE_ANIMATION(obj);

    anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "drain plan: animation context is NULL");
    GFX_RETURN_ON_FALSE(gfx_anim_has_source(anim), GFX_ERR_INVALID_STATE, TAG, "drain plan: source is not set");
    GFX_RETURN_ON_FALSE(anim->segments != NULL && anim->segment_count > 0U, GFX_ERR_INVALID_STATE, TAG, "drain plan: segment plan is not set");
    GFX_RETURN_ON_FALSE(anim->event_group != NULL, GFX_ERR_INVALID_STATE, TAG, "drain plan: event group is not ready");

    if (!anim->is_playing && !anim->segment_paused &&
            anim->segment_index == (anim->segment_count - 1U) &&
            anim->current_frame >= anim->end_frame) {
        return GFX_OK; // already at the tail
    }

    gfx_platform_event_clear(anim->event_group, GFX_ANIM_EVENT_PLAN_DONE | GFX_ANIM_EVENT_SEGMENT_PAUSED);
    anim->drain_remaining_segments = true;

    if (anim->segment_paused) {
        GFX_RETURN_ON_FALSE(anim->pending_segment_index < anim->segment_count, GFX_ERR_INVALID_STATE, TAG, "drain plan: pending segment index is invalid");
        GFX_RETURN_ON_ERROR(gfx_anim_apply_segment(obj, anim, &anim->segments[anim->pending_segment_index], anim->pending_segment_index),
                            TAG, "drain plan: failed to resume the pending segment");
        anim->is_playing = true;
        gfx_object_invalidate(obj);
    } else if (!anim->is_playing) {
        GFX_RETURN_ON_ERROR(gfx_anim_start(obj), TAG, "drain plan: failed to start animation");
        anim->drain_remaining_segments = true;
    }

    bits = gfx_platform_event_wait(anim->event_group, GFX_ANIM_EVENT_PLAN_DONE, true, false, GFX_PLATFORM_WAIT_FOREVER);
    return (bits & GFX_ANIM_EVENT_PLAN_DONE) ? GFX_OK : GFX_FAIL;
}

gfx_err_t gfx_anim_start(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "start animation: animation context is NULL");
    GFX_RETURN_ON_FALSE(gfx_anim_has_source(anim), GFX_ERR_INVALID_STATE, TAG, "start animation: source is not set");

    if (anim->is_playing) {
        return GFX_OK;
    }

    if (anim->segment_paused) {
        GFX_RETURN_ON_FALSE(anim->segments != NULL, GFX_ERR_INVALID_STATE, TAG, "start animation: segment plan is not set");
        GFX_RETURN_ON_FALSE(anim->pending_segment_index < anim->segment_count, GFX_ERR_INVALID_STATE, TAG, "start animation: pending segment index is invalid");
        GFX_RETURN_ON_ERROR(gfx_anim_apply_segment(obj, anim, &anim->segments[anim->pending_segment_index], anim->pending_segment_index),
                            TAG, "start animation: failed to resume the next segment");
    } else {
        anim->current_frame = anim->start_frame;
        GFX_RETURN_ON_ERROR(gfx_anim_prepare_frame(obj), TAG, "start animation: failed to prepare the start frame");
    }

    if (anim->event_group != NULL) {
        gfx_platform_event_clear(anim->event_group, GFX_ANIM_EVENT_PLAN_DONE | GFX_ANIM_EVENT_SEGMENT_PAUSED);
    }

    anim->is_playing = true;
    anim->drain_remaining_segments = false;
    gfx_object_invalidate(obj);

    GFX_LOGD(TAG, "start animation: playback started");
    return GFX_OK;
}

gfx_err_t gfx_anim_stop(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "stop animation: animation context is NULL");

    if (!anim->is_playing && !anim->segment_paused) {
        return GFX_OK;
    }

    anim->is_playing = false;
    anim->segment_paused = false;
    anim->pending_segment_index = 0;
    anim->drain_remaining_segments = false;
    gfx_anim_signal_event(anim, GFX_ANIM_EVENT_PLAN_DONE);
    GFX_LOGD(TAG, "stop animation: playback stopped");
    return GFX_OK;
}

gfx_err_t gfx_anim_set_mirror(gfx_object_t *obj, bool enabled, int16_t offset)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "set mirror: animation context is NULL");

    anim->mirror_mode = enabled ? GFX_MIRROR_MANUAL : GFX_MIRROR_DISABLED;
    anim->mirror_offset = offset;

    GFX_LOGD(TAG, "set mirror: %s, offset=%d", enabled ? "manual mirroring enabled" : "mirroring disabled", offset);
    return GFX_OK;
}

gfx_err_t gfx_anim_set_auto_mirror(gfx_object_t *obj, bool enabled)
{
    CHECK_OBJ_TYPE_ANIMATION(obj);

    gfx_anim_t *anim = (gfx_anim_t *)obj->src;
    GFX_RETURN_ON_FALSE(anim != NULL, GFX_ERR_INVALID_STATE, TAG, "set auto mirror: animation context is NULL");

    anim->mirror_mode = enabled ? GFX_MIRROR_AUTO : GFX_MIRROR_DISABLED;

    GFX_LOGD(TAG, "set auto mirror: %s", enabled ? "enabled" : "disabled");
    return GFX_OK;
}
