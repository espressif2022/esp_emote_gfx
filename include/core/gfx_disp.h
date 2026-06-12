/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core/gfx_err.h"
#include "core/gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/
typedef struct gfx_display gfx_display_t;
typedef struct gfx_backend gfx_backend_t;

typedef enum {
    GFX_DISPLAY_EVENT_IDLE = 0,
    GFX_DISPLAY_EVENT_ONE_FRAME_DONE,
    GFX_DISPLAY_EVENT_PART_FRAME_DONE,
    GFX_DISPLAY_EVENT_ALL_FRAME_DONE,
} gfx_display_event_t;

typedef struct {
    uint64_t calls;           /**< Number of API calls */
    uint64_t pixels;          /**< Processed pixels */
    uint64_t time_us;         /**< Elapsed time in microseconds */
} gfx_perf_counter_t;

typedef struct {
    gfx_perf_counter_t fill;              /**< Solid area fills */
    gfx_perf_counter_t solid;             /**< Solid-color draw operations */
    gfx_perf_counter_t image;             /**< Image draw operations */
    gfx_perf_counter_t shape;             /**< Shape/mesh draw operations */
    uint64_t shape_covered_pixels;        /**< Shape pixels covered by rasterization */
    uint64_t shape_aa_pixels;             /**< Shape edge-AA pixels */
} gfx_draw_perf_stats_t;

typedef struct {
    uint32_t dirty_pixels;            /**< Dirty pixels in the latest rendered frame */
    uint64_t frame_time_us;           /**< Total frame time */
    uint64_t render_time_us;          /**< Time spent in render phase */
    uint64_t flush_time_us;           /**< Time spent in flush callbacks */
    uint32_t flush_count;             /**< Number of flush calls */
    gfx_draw_perf_stats_t draw;       /**< Draw-stage details */
} gfx_display_perf_stats_t;

typedef void (*gfx_display_flush_cb_t)(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1,
                                       gfx_coord_t x2, gfx_coord_t y2, const void *data);
typedef void (*gfx_display_update_cb_t)(gfx_display_t *disp, gfx_display_event_t event, const void *obj);

/*********************
 *   CONFIG STRUCTS
 *********************/
/** Passed to gfx_display_add() for multi-screen setup */
typedef struct {
    uint32_t h_res;                          /**< Screen width in pixels */
    uint32_t v_res;                          /**< Screen height in pixels */
    gfx_backend_t *backend;             /**< Optional display backend. gfx_display_add() takes ownership on success. If NULL, flush_cb is used. */
    gfx_display_flush_cb_t flush_cb;            /**< Legacy flush callback for this display */
    gfx_display_update_cb_t update_cb;       /**< Update callback (frame/playback events) */
    void *user_data;                         /**< User data for this display */
    struct {
        unsigned char swap : 1;              /**< Color swap flag */
        unsigned char buff_dma : 1;          /**< Prefer DMA-capable internal buffers when supported */
        unsigned char buff_spiram : 1;       /**< Alloc buffer in PSRAM (internal alloc only) */
        unsigned char double_buffer : 1;     /**< Alloc second buffer for double buffering (internal alloc only) */
        unsigned char full_frame : 1;    /**< 1 = buf1/buf2 are full-screen framebuffers (e.g. RGB); draw at chunk region. 0 = partition buffer; draw from start. */
    } flags;
    struct {
        void *buf1;                          /**< Frame buffer 1 (NULL = internal alloc) */
        void *buf2;                          /**< Frame buffer 2 (NULL = internal alloc) */
        size_t buf_pixels;                   /**< Size per buffer in pixels (0 = auto) */
    } buffers;
} gfx_display_config_t;

/**********************
 * PUBLIC API
 **********************/

/**
 * @brief Add a display (multi-screen support)
 *
 * @param handle Graphics handle from gfx_core_init
 * @param cfg Display configuration (resolution, flush callback, buffers)
 * @return New display pointer on success, or NULL on failure
 */
gfx_display_t *gfx_display_add(gfx_handle_t handle, const gfx_display_config_t *cfg);

/**
 * @brief Remove a display and release its child objects and resources.
 *
 * @param disp Display from gfx_display_add; safe to pass NULL
 */
void gfx_display_delete(gfx_display_t *disp);

/**
 * @brief Invalidate full screen of a display to trigger refresh
 *
 * @param disp Display from gfx_display_add
 */
void gfx_display_refresh_all(gfx_display_t *disp);

/**
 * @brief Notify that flush is done (e.g. from panel IO callback)
 *
 * @param disp Display from gfx_display_add
 * @param swap_act_buf Whether to swap the active buffer
 * @return true on success, false on failure
 */
bool gfx_display_flush_ready(gfx_display_t *disp, bool swap_act_buf);

/**
 * @brief Get user data for a display
 *
 * @param disp Display from gfx_display_add
 * @return User data pointer, or NULL
 */
void *gfx_display_get_user_data(gfx_display_t *disp);

/**
 * @brief Get display horizontal resolution in pixels
 *
 * @param disp Display from gfx_display_add (NULL allowed; returns default width)
 * @return Width in pixels
 */
uint32_t gfx_display_get_h_res(gfx_display_t *disp);

/**
 * @brief Get display vertical resolution in pixels
 *
 * @param disp Display from gfx_display_add (NULL allowed; returns default height)
 * @return Height in pixels
 */
uint32_t gfx_display_get_v_res(gfx_display_t *disp);

/**
 * @brief Check if display is currently flushing the last block
 *
 * @param disp Display from gfx_display_add
 * @return true if flushing last block, false otherwise
 */
bool gfx_display_is_flushing_last(gfx_display_t *disp);

/**
 * @brief Get latest per-display performance statistics
 *
 * Stats are updated once a frame is rendered. If no frame has rendered yet,
 * all fields remain zero.
 *
 * @param disp Display handle
 * @param out_stats Output stats structure
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_display_get_perf_stats(gfx_display_t *disp, gfx_display_perf_stats_t *out_stats);

/**
 * @brief Set default background color for a display
 *
 * @param disp Display from gfx_display_add
 * @param color Background color (e.g. RGB565)
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_display_set_bg_color(gfx_display_t *disp, gfx_color_t color);

/**
 * @brief Enable or disable drawing the background (fill with bg_color before widgets)
 *
 * @param disp Display from gfx_display_add
 * @param enable true to enable background (default), false to disable background
 * @return GFX_OK on success
 */
gfx_err_t gfx_display_set_bg_enable(gfx_display_t *disp, bool enable);

#ifdef __cplusplus
}
#endif
