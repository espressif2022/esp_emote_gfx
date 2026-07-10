/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/display.h"
#include "core/object/gfx_object_priv.h"
#include "core/display/gfx_backend_priv.h"
#include "platform/gfx_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/
struct gfx_core_context;

/*********************
 *   DEFINES
 *********************/
#ifdef CONFIG_GFX_DISP_INV_BUF_SIZE
#define GFX_DISP_INV_BUF_SIZE  CONFIG_GFX_DISP_INV_BUF_SIZE
#else
#define GFX_DISP_INV_BUF_SIZE  64
#endif

/*********************
 *   INTERNAL STRUCTS
 *********************/
/** Per-display state; one per screen, linked list for multi-display. Fields grouped by category. */
struct gfx_display {
    struct gfx_display *next;
    struct gfx_core_context *ctx;

    /** Resolution */
    struct {
        uint32_t h_res;
        uint32_t v_res;
    } res;

    /** Pixel formats */
    struct {
        gfx_color_format_t render_format;    /**< Internal software render buffer format */
        gfx_color_format_t output_format;    /**< Flush/backend output format */
        uint8_t render_pixel_size;
        uint8_t output_pixel_size;
    } format;

    /** Callbacks and user data */
    struct {
        gfx_display_update_cb_t update_cb;
        void *user_data;
    } cb;

    gfx_backend_t *backend;

    /** Sync (event group for flush done) */
    struct {
        gfx_platform_event_t event_group;
    } sync;

    /** Child object list */
    gfx_object_child_t *child_list;

    /** Frame buffers */
    struct {
        void *buf1;
        void *buf2;
        void *buf_act;
        void *flush_buf;
        size_t buf_pixels;
        size_t flush_buf_bytes;
        bool ext_bufs;
    } buf;

    /** Display style (e.g. background color) */
    struct {
        gfx_color_t bg_color;
        bool bg_enable;   /**< true = fill background before draw; default true */
    } style;

    /** Render state (flush) */
    struct {
        bool flushing_last;
        uint32_t dirty_pixels;
        uint64_t frame_time_us;
        uint64_t render_time_us;
        uint64_t flush_time_us;
        uint32_t flush_count;
        gfx_draw_perf_stats_t draw;
    } render;

    /** Dirty / invalidation state */
    struct {
        gfx_area_t areas[GFX_DISP_INV_BUF_SIZE];
        uint8_t merged[GFX_DISP_INV_BUF_SIZE];
        uint8_t count;
    } dirty;

    /** Host/test injected touch capture state */
    struct {
        gfx_object_t *pressed_obj;
        uint32_t pressed_obj_seq;
        uint8_t pressed_id;
    } injected_touch;

    /**
     * Optional arena scene backend (package UI path).
     * When non-NULL, render/touch prefer arena over child object list.
     * Typed as void* to avoid pulling arena headers into every display user.
     */
    void *gfx_arena_scene;

};

/*********************
 *   INTERNAL API
 *********************/

/* Buffer helpers (used by gfx_display.c and gfx_core.c deinit) */

/**
 * @brief Free display frame buffers
 * @param disp Display whose buffers to free (internal alloc only; ext_bufs are not freed)
 * @return GFX_OK
 * @internal Used by gfx_core deinit when tearing down displays.
 */
gfx_err_t gfx_display_buf_free(gfx_display_t *disp);

/**
 * @brief Initialize display buffers from config
 * @param disp Display to init (h_res, v_res already set)
 * @param cfg Display config (buffers.buf1/buf2/buf_pixels)
 * @return GFX_OK on success, GFX_ERR_NO_MEM if internal alloc fails
 * @internal Used by gfx_display_add when cfg->buffers.buf1 is NULL.
 */
gfx_err_t gfx_display_buf_init(gfx_display_t *disp, const gfx_display_config_t *cfg);

/* Object/render helpers (obj/widget/render only, not in public gfx_disp.h) */

/**
 * @brief Add a child object to a display
 * @param disp Display to attach to
 * @param type Child type (GFX_OBJ_TYPE_IMAGE, GFX_OBJ_TYPE_LABEL, etc.)
 * @param src Child object pointer (e.g. gfx_object_t *)
 * @return GFX_OK on success
 * @internal Used by widget create functions such as gfx_image_create and gfx_label_create.
 */
gfx_err_t gfx_display_add_child(gfx_display_t *disp, void *src);

/**
 * @brief Remove a child object from a display
 * @param disp Display that owns the child
 * @param src Child object pointer to remove (e.g. gfx_object_t *)
 * @return GFX_OK on success, GFX_ERR_NOT_FOUND if not in list
 * @internal Used by gfx_object_delete.
 */
gfx_err_t gfx_display_remove_child(gfx_display_t *disp, void *src);

/**
 * @brief Delete and detach every child object owned by a display.
 * @param disp Display that owns the child list
 * @return GFX_OK on success
 * @internal Used during display/core teardown to ensure widget destructors run.
 */
gfx_err_t gfx_display_delete_children(gfx_display_t *disp);

bool gfx_display_has_full_frame_buf(const gfx_display_t *disp);

/**
 * @brief Return the topmost visible object containing a point.
 * @param disp Display to hit-test.
 * @param x Screen-space x coordinate.
 * @param y Screen-space y coordinate.
 * @return Topmost object at the point, or NULL when no object is hit.
 * @internal Used by input dispatch and host injection.
 */
gfx_object_t *gfx_display_hit_test(gfx_display_t *disp, uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif
