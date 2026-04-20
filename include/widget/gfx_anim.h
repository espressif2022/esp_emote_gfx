/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "core/gfx_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    GFX_ANIM_SEGMENT_ACTION_CONTINUE = 0,
    GFX_ANIM_SEGMENT_ACTION_PAUSE,
} gfx_anim_segment_action_t;

/**
 * @brief Playback description for one animation segment.
 *
 * A segment defines:
 * - frame range
 * - playback speed
 * - total repeat count
 * - what to do when the segment finishes
 *
 * Use `gfx_anim_set_segment()` for the simple single-segment case.
 * Use `gfx_anim_set_segments()` when you need a playback plan.
 */
typedef struct {
    uint32_t start;      /* inclusive start frame */
    uint32_t end;        /* inclusive end frame */
    uint32_t fps;        /* playback fps for this segment */
    uint32_t play_count; /* total plays for this segment, 0 means forever */
    gfx_anim_segment_action_t end_action; /* action after the last play finishes */
} gfx_anim_segment_t;

/**
 * @brief Public animation source type.
 *
 * The current implementation supports in-memory animation payloads.
 * The enum exists so future source types can be added without changing the
 * source-setting API shape again.
 */
typedef enum {
    GFX_ANIM_SRC_TYPE_MEMORY = 0, /**< In-memory animation payload */
} gfx_anim_src_type_t;

/**
 * @brief Typed animation source descriptor.
 *
 * `gfx_anim_set_src_desc()` is the preferred source setter for new code.
 * `gfx_anim_set_src()` remains as a compatibility wrapper for raw memory
 * buffers and length pairs.
 */
typedef struct {
    gfx_anim_src_type_t type; /**< Source payload type */
    const void *data;         /**< Type-specific payload pointer */
    size_t data_len;          /**< Payload length in bytes */
} gfx_anim_src_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create an animation object on a display
 * @param disp Display from gfx_emote_add_disp(handle, &disp_cfg)
 * @return Pointer to the created animation object
 */
gfx_obj_t *gfx_anim_create(gfx_disp_t *disp);

/* Animation setters */

/**
 * @brief Set the typed source descriptor for an animation object
 *
 * This is the preferred source setter for new code.
 *
 * @param obj Pointer to the animation object
 * @param src Pointer to the typed source descriptor
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_set_src_desc(gfx_obj_t *obj, const gfx_anim_src_t *src);

/**
 * @brief Set the source data for an animation object
 *
 * Compatibility wrapper for in-memory animation payloads.
 *
 * @param obj Pointer to the animation object
 * @param src_data Source data
 * @param src_len Source data length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len);

/**
 * @brief Set the segment for an animation object
 * @param obj Pointer to the animation object
 * @param start Start frame index
 * @param end End frame index
 * @param fps Frames per second
 * @param repeat Whether to repeat the animation
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat);
/**
 * @brief Set a segment playback plan for an animation object
 * @param obj Pointer to the animation object
 * @param segments Segment plan array
 * @param segment_count Number of segment entries in the array
 *
 * Each segment uses `play_count` to describe the total number of plays:
 * - `play_count = 1`: play once
 * - `play_count = N`: play N times
 * - `play_count = 0`: play forever
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_set_segments(gfx_obj_t *obj, const gfx_anim_segment_t *segments, size_t segment_count);

/**
 * @brief Drain the remaining segment plan and block until playback finishes
 *
 * This API is intended for segment-plan mode. If playback is currently inside
 * a loop phase of the active segment, or paused after a segment `end_action`,
 * calling this API will drain the remaining plan exactly once:
 * - the active segment continues from its current frame to its end
 * - the active segment's remaining repeat count is ignored
 * - all following segments are played once in order
 * - pause actions in the remaining plan are ignored
 *
 * The function blocks until the remaining plan has finished.
 *
 * Do not call this API while holding the graphics lock.
 *
 * @param obj Pointer to the animation object
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if there is no remaining work,
 *         or another ESP_ERR_* code on failure
 */
esp_err_t gfx_anim_play_left_to_tail(gfx_obj_t *obj);

/**
 * @brief Start the animation
 * @param obj Pointer to the animation object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_start(gfx_obj_t *obj);

/**
 * @brief Stop the animation
 * @param obj Pointer to the animation object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_stop(gfx_obj_t *obj);

/**
 * @brief Set mirror display for an animation object
 *
 * Manual mirror duplicates the rendered image horizontally and inserts the
 * provided offset between the original and mirrored copy.
 * For display-width-aware mirroring, use `gfx_anim_set_auto_mirror()`.
 *
 * @param obj Pointer to the animation object
 * @param enabled Whether to enable mirror display
 * @param offset Mirror offset in pixels
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset);

/**
 * @brief Set auto mirror alignment for animation object
 *
 * Auto mirror computes the mirror offset from the current display width.
 * Compared with `gfx_anim_set_mirror()`, this mode is easier to use when the
 * animation should mirror around the display center without a fixed offset.
 *
 * @param obj Animation object
 * @param enabled Whether to enable auto mirror alignment
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled);

#ifdef __cplusplus
}
#endif
