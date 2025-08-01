/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "core/gfx_core.h"
#include "core/gfx_core_internal.h"
#include "core/gfx_timer.h"
#include "core/gfx_obj.h"
#include "widget/gfx_img.h"
#include "widget/gfx_img_internal.h"
#include "widget/gfx_label.h"
#include "widget/gfx_label_internal.h"
#include "widget/gfx_anim.h"
#include "core/gfx_types.h"
#include "decoder/gfx_aaf_dec.h"
#include "widget/gfx_anim_internal.h"
#include "widget/gfx_font_internal.h"
#include "decoder/gfx_aaf_format.h"
#include "decoder/gfx_img_decoder.h"

static const char *TAG = "gfx_obj";

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*=====================
 * Object creation
 *====================*/

gfx_obj_t * gfx_img_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for image object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_IMAGE;
    obj->parent_handle = handle;
    obj->is_visible = true;  // Default to hidden
    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_IMAGE, obj);
    ESP_LOGD(TAG, "Created image object");
    return obj;
}

gfx_obj_t * gfx_label_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for label object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->parent_handle = handle;
    obj->is_visible = true;  // Default to hidden

    gfx_label_property_t *label = (gfx_label_property_t *)malloc(sizeof(gfx_label_property_t));
    if (label == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for label object");
        free(obj);
        return NULL;
    }
    memset(label, 0, sizeof(gfx_label_property_t));

    // Apply default font configuration
    gfx_font_t default_font;
    uint16_t default_size;
    gfx_color_t bg_color;
    gfx_opa_t default_opa;

    // Get default font configuration from internal function
    gfx_get_default_font_config(&default_font, &default_size, &bg_color, &default_opa);

    label->font_size = default_size;
    label->color = bg_color;
    label->opa = default_opa;
    label->mask = NULL;
    label->bg_color = (gfx_color_t) {
        .full = 0x0000
    }; // Default: black background
    label->bg_enable = false; // Default: background disabled
    label->bg_dirty = false;  // Default: background not dirty
    label->text_align = GFX_TEXT_ALIGN_LEFT;  // Initialize with default left alignment
    label->long_mode = GFX_LABEL_LONG_CLIP;   // Default to clipping
    label->line_spacing = 2;

    // Initialize horizontal scroll properties
    label->scroll_offset = 0;
    label->scroll_speed_ms = 50;  // Default: 50ms per pixel
    label->scroll_loop = true;
    label->scroll_active = false;
    label->scroll_dirty = false;  // Default: scroll position not dirty
    label->scroll_timer = NULL;
    label->text_width = 0;

    // Initialize cached line data
    label->cached_lines = NULL;
    label->cached_line_count = 0;
    label->cached_line_widths = NULL;

    // Set default font automatically
    if (default_font) {
        label->face = (void *)default_font;
    }

    obj->src = label;

    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_LABEL, obj);
    ESP_LOGD(TAG, "Created label object with default font config");
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

gfx_obj_t * gfx_img_set_src(gfx_obj_t *obj, void *src)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return NULL;
    }

    if (obj->type != GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGE(TAG, "Object is not an image type");
        return NULL;
    }

    obj->src = src;

    // Update object size based on image data using unified decoder
    if (src != NULL) {
        gfx_image_header_t header;
        gfx_image_decoder_dsc_t dsc = {
            .src = src,
        };
        esp_err_t ret = gfx_image_decoder_info(&dsc, &header);
        if (ret == ESP_OK) {
            obj->width = header.w;
            obj->height = header.h;
        } else {
            ESP_LOGE(TAG, "Failed to get image info from source");
        }
    }

    ESP_LOGD(TAG, "Set image source, size: %dx%d", obj->width, obj->height);
    return obj;
}

void gfx_obj_set_pos(gfx_obj_t *obj, gfx_coord_t x, gfx_coord_t y)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    obj->x = x;
    obj->y = y;
    obj->use_align = false;

    ESP_LOGD(TAG, "Set object position: (%d, %d)", x, y);
}

void gfx_obj_set_size(gfx_obj_t *obj, uint16_t w, uint16_t h)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->type == GFX_OBJ_TYPE_ANIMATION || obj->type == GFX_OBJ_TYPE_IMAGE) {
        ESP_LOGW(TAG, "Set size for animation or image is not allowed");
    } else {
        obj->width = w;
        obj->height = h;
    }

    ESP_LOGD(TAG, "Set object size: %dx%d", w, h);
}

void gfx_obj_align(gfx_obj_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    if (obj->parent_handle == NULL) {
        ESP_LOGE(TAG, "Object has no parent handle");
        return;
    }

    // Validate alignment type
    if (align > GFX_ALIGN_OUT_BOTTOM_RIGHT) {
        ESP_LOGW(TAG, "Unknown alignment type: %d", align);
        return;
    }

    // Set alignment properties instead of directly setting position
    obj->align_type = align;
    obj->align_x_ofs = x_ofs;
    obj->align_y_ofs = y_ofs;
    obj->use_align = true;

    ESP_LOGD(TAG, "Set object alignment: type=%d, offset=(%d, %d)", align, x_ofs, y_ofs);
}

void gfx_obj_set_visible(gfx_obj_t *obj, bool visible)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    obj->is_visible = visible;
    ESP_LOGD(TAG, "Set object visibility: %s", visible ? "visible" : "hidden");
}

bool gfx_obj_get_visible(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return false;
    }

    return obj->is_visible;
}

/*=====================
 * Static helper functions
 *====================*/

void gfx_obj_calculate_aligned_position(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y)
{
    if (obj == NULL || x == NULL || y == NULL) {
        return;
    }

    if (!obj->use_align) {
        // Use absolute position if alignment is not enabled
        *x = obj->x;
        *y = obj->y;
        return;
    }

    gfx_coord_t calculated_x = 0;
    gfx_coord_t calculated_y = 0;

    // Calculate position based on alignment
    switch (obj->align_type) {
    case GFX_ALIGN_TOP_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_LEFT_MID:
        calculated_x = obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_CENTER:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width - obj->width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height - obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_TOP_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = -obj->height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_TOP:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_MID:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_LEFT_BOTTOM:
        calculated_x = -obj->width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_TOP:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_MID:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = ((gfx_coord_t)parent_height - obj->height) / 2 + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_RIGHT_BOTTOM:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_LEFT:
        calculated_x = obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_MID:
        calculated_x = ((gfx_coord_t)parent_width - obj->width) / 2 + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    case GFX_ALIGN_OUT_BOTTOM_RIGHT:
        calculated_x = (gfx_coord_t)parent_width + obj->align_x_ofs;
        calculated_y = (gfx_coord_t)parent_height + obj->align_y_ofs;
        break;
    default:
        ESP_LOGW(TAG, "Unknown alignment type: %d", obj->align_type);
        // Fall back to absolute position
        calculated_x = obj->x;
        calculated_y = obj->y;
        break;
    }

    *x = calculated_x;
    *y = calculated_y;
}

/*=====================
 * Getter functions
 *====================*/

void gfx_obj_get_pos(gfx_obj_t *obj, gfx_coord_t *x, gfx_coord_t *y)
{
    if (obj == NULL || x == NULL || y == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return;
    }

    *x = obj->x;
    *y = obj->y;
}

void gfx_obj_get_size(gfx_obj_t *obj, uint16_t *w, uint16_t *h)
{
    if (obj == NULL || w == NULL || h == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return;
    }

    *w = obj->width;
    *h = obj->height;
}

/*=====================
 * Other functions
 *====================*/

void gfx_obj_delete(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return;
    }

    ESP_LOGD(TAG, "Deleting object type: %d", obj->type);

    // Remove object from parent's child list first
    if (obj->parent_handle != NULL) {
        gfx_emote_remove_child(obj->parent_handle, obj);
    }

    if (obj->type == GFX_OBJ_TYPE_LABEL) {
        gfx_label_property_t *label = (gfx_label_property_t *)obj->src;
        if (label) {
            // Clean up scroll timer
            if (label->scroll_timer) {
                gfx_timer_delete(obj->parent_handle, label->scroll_timer);
                label->scroll_timer = NULL;
            }

            // Clean up cached line data
            if (label->cached_lines) {
                for (int i = 0; i < label->cached_line_count; i++) {
                    if (label->cached_lines[i]) {
                        free(label->cached_lines[i]);
                    }
                }
                free(label->cached_lines);
                label->cached_lines = NULL;
                label->cached_line_count = 0;
            }

            // Clean up cached line widths
            if (label->cached_line_widths) {
                free(label->cached_line_widths);
                label->cached_line_widths = NULL;
            }

            if (label->text) {
                free(label->text);
            }
            if (label->mask) {
                free(label->mask);
            }
            free(label);
        }
    } else if (obj->type == GFX_OBJ_TYPE_ANIMATION) {
        gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
        if (anim) {
            // Stop animation if playing
            if (anim->is_playing) {
                gfx_anim_stop(obj);
            }

            // Delete timer
            if (anim->timer != NULL) {
                gfx_timer_delete((void *)obj->parent_handle, anim->timer);
                anim->timer = NULL;
            }

            // Free frame processing information
            gfx_anim_free_frame_info(&anim->frame);

            // Free file descriptor
            if (anim->file_desc) {
                gfx_aaf_format_deinit(anim->file_desc);
            }

            free(anim);
        }
    }
    free(obj);
}

/*=====================
 * Animation object functions
 *====================*/

static void gfx_anim_timer_callback(void *arg)
{
    gfx_obj_t *obj = (gfx_obj_t *)arg;
    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;

    if (!anim || !anim->is_playing) {
        ESP_LOGD(TAG, "anim is NULL or not playing");
        return;
    }

    anim->current_frame++;
    ESP_LOGD("anim cb", " %lu (%lu / %lu)", anim->current_frame, anim->start_frame, anim->end_frame);

    // Check if we've reached the end
    if (anim->current_frame > anim->end_frame) {
        if (anim->repeat) {
            ESP_LOGD(TAG, "REPEAT");
            anim->current_frame = anim->start_frame;
        } else {
            ESP_LOGD(TAG, "STOP");
            anim->is_playing = false;
            return;
        }
    }

    // Mark object as dirty to trigger redraw
    obj->is_dirty = true;
}

gfx_obj_t * gfx_anim_create(gfx_handle_t handle)
{
    gfx_obj_t *obj = (gfx_obj_t *)malloc(sizeof(gfx_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for animation object");
        return NULL;
    }

    memset(obj, 0, sizeof(gfx_obj_t));
    obj->parent_handle = handle;
    obj->is_visible = true;  // Default to hidden

    gfx_anim_property_t *anim = (gfx_anim_property_t *)malloc(sizeof(gfx_anim_property_t));
    if (anim == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for animation property");
        free(obj);
        return NULL;
    }
    memset(anim, 0, sizeof(gfx_anim_property_t));

    // Initialize animation properties
    anim->file_desc = NULL;
    anim->start_frame = 0;
    anim->end_frame = 0;
    anim->current_frame = 0;
    anim->fps = 30; // Default FPS
    anim->repeat = true;
    anim->is_playing = false;

    // Create timer during object creation
    uint32_t period_ms = 1000 / anim->fps; // Convert FPS to period in milliseconds
    anim->timer = gfx_timer_create((void *)obj->parent_handle, gfx_anim_timer_callback, period_ms, obj);
    if (anim->timer == NULL) {
        ESP_LOGE(TAG, "Failed to create animation timer");
        free(anim);
        free(obj);
        return NULL;
    }

    // Initialize pre-parsed header fields
    memset(&anim->frame.header, 0, sizeof(gfx_aaf_header_t));

    // Initialize pre-fetched frame data fields
    anim->frame.frame_data = NULL;
    anim->frame.frame_size = 0;

    // Initialize pre-allocated parsing resources fields
    anim->frame.block_offsets = NULL;
    anim->frame.pixel_buffer = NULL;
    anim->frame.color_palette = NULL;

    // Initialize decoding state
    anim->frame.last_block = -1;

    // Initialize widget-specific display properties
    anim->mirror_enabled = false;
    anim->mirror_offset = 0;

    obj->src = anim;
    obj->type = GFX_OBJ_TYPE_ANIMATION;

    gfx_emote_add_chlid(handle, GFX_OBJ_TYPE_ANIMATION, obj);
    return obj;
}

esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len)
{
    if (obj == NULL || src_data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGE(TAG, "Object is not an animation type");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    // Stop current animation if playing
    if (anim->is_playing) {
        ESP_LOGD(TAG, "stop current animation");
        gfx_anim_stop(obj);
    }

    // Free previously parsed header if exists
    if (anim->frame.header.width > 0) {  // Check if header data exists instead of header_valid flag
        gfx_aaf_free_header(&anim->frame.header);
        memset(&anim->frame.header, 0, sizeof(gfx_aaf_header_t));  // Clear header data
    }

    // Clear pre-fetched frame data
    anim->frame.frame_data = NULL;
    anim->frame.frame_size = 0;

    gfx_aaf_format_handle_t new_desc;
    gfx_aaf_format_init(src_data, src_len, &new_desc);
    if (new_desc == NULL) {
        ESP_LOGE(TAG, "Failed to initialize asset parser");
        return ESP_FAIL;
    }

    //delete old file_desc
    if (anim->file_desc) {
        gfx_aaf_format_deinit(anim->file_desc);
        anim->file_desc = NULL;
    }

    anim->file_desc = new_desc;
    anim->start_frame = 0;
    anim->current_frame = 0;
    anim->end_frame = gfx_aaf_format_get_total_frames(new_desc) - 1;

    ESP_LOGD(TAG, "set src, start: %lu, end: %lu, file_desc: %p", anim->start_frame, anim->end_frame, anim->file_desc);
    return ESP_OK;
}

esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGE(TAG, "Object is not an animation type");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    int total_frames = gfx_aaf_format_get_total_frames(anim->file_desc);

    anim->start_frame = start;
    anim->end_frame = (end > total_frames - 1) ? (total_frames - 1) : end;
    anim->current_frame = start;

    if (anim->fps != fps) {
        ESP_LOGI(TAG, "FPS changed from %lu to %lu, updating timer period", anim->fps, fps);
        anim->fps = fps;

        if (anim->timer != NULL) {
            uint32_t new_period_ms = 1000 / fps;
            gfx_timer_set_period(anim->timer, new_period_ms);
            ESP_LOGI(TAG, "Animation timer period updated to %lu ms for %lu FPS", new_period_ms, fps);
        }
    }

    anim->repeat = repeat;

    ESP_LOGD(TAG, "Set animation segment: %lu -> %lu, fps: %lu, repeat: %d", start, end, fps, repeat);
    return ESP_OK;
}

esp_err_t gfx_anim_start(gfx_obj_t *obj)
{
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGE(TAG, "Object is not an animation type");
        return ESP_ERR_INVALID_ARG;
    }

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
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGE(TAG, "Object is not an animation type");
        return ESP_ERR_INVALID_ARG;
    }

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
    if (obj == NULL) {
        ESP_LOGE(TAG, "Object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_ANIMATION) {
        ESP_LOGE(TAG, "Object is not an animation type");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_anim_property_t *anim = (gfx_anim_property_t *)obj->src;
    if (anim == NULL) {
        ESP_LOGE(TAG, "Animation property is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    // Set mirror properties
    anim->mirror_enabled = enabled;
    anim->mirror_offset = offset;

    ESP_LOGD(TAG, "Set animation mirror: enabled=%s, offset=%d", enabled ? "true" : "false", offset);
    return ESP_OK;
}