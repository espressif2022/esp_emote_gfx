/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "core/gfx_disp.h"
#include "core/object/gfx_obj_priv.h"

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
struct gfx_disp {
    struct gfx_disp *next;
    struct gfx_core_context *ctx;

    /** Resolution */
    struct {
        uint32_t h_res;
        uint32_t v_res;
    } res;

    /** Option flags */
    struct {
        unsigned char swap : 1;
        unsigned char full_frame : 1;
    } flags;

    /** Callbacks and user data */
    struct {
        gfx_disp_flush_cb_t flush_cb;
        gfx_disp_update_cb_t update_cb;
        void *user_data;
    } cb;

    /** Sync (event group for flush done) */
    struct {
        EventGroupHandle_t event_group;
    } sync;

    /** Child object list */
    gfx_obj_child_t *child_list;

    /** Frame buffers */
    struct {
        uint16_t *buf1;
        uint16_t *buf2;
        uint16_t *buf_act;
        size_t buf_pixels;
        bool ext_bufs;
    } buf;

    /** Display style (e.g. background color) */
    struct {
        gfx_color_t bg_color;
        bool bg_enable;   /**< true = fill background before draw; default true */
    } style;

    /** Render state (flush / swap) */
    struct {
        bool flushing_last;
        bool swap_act_buf;
        uint32_t dirty_pixels;
        uint64_t frame_time_us;
        uint64_t render_time_us;
        uint64_t flush_time_us;
        uint32_t flush_count;
        gfx_blend_perf_stats_t blend;
    } render;

    /** Dirty / invalidation state */
    struct {
        gfx_area_t areas[GFX_DISP_INV_BUF_SIZE];
        uint8_t merged[GFX_DISP_INV_BUF_SIZE];
        uint8_t count;
    } dirty;

    /** Pending sync: dirty areas from previous frame to sync into buf_act at next render start (only non-merged areas, no merged flags) */
    struct {
        gfx_area_t areas[GFX_DISP_INV_BUF_SIZE];
        uint8_t count;
    } sync_pending;
};

/*********************
 *   INTERNAL API
 *********************/

/* Buffer helpers (used by gfx_disp.c and gfx_core.c deinit) */

/**
 * @brief Free display frame buffers
 * @param disp Display whose buffers to free (internal alloc only; ext_bufs are not freed)
 * @return ESP_OK
 * @internal Used by gfx_core deinit when tearing down displays.
 */
esp_err_t gfx_disp_buf_free(gfx_disp_t *disp);

/**
 * @brief Initialize display buffers from config
 * @param disp Display to init (h_res, v_res already set)
 * @param cfg Display config (buffers.buf1/buf2/buf_pixels)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if internal alloc fails
 * @internal Used by gfx_disp_add when cfg->buffers.buf1 is NULL.
 */
esp_err_t gfx_disp_buf_init(gfx_disp_t *disp, const gfx_disp_config_t *cfg);

/* Object/render helpers (obj/widget/render only, not in public gfx_disp.h) */

/**
 * @brief Add a child object to a display
 * @param disp Display to attach to
 * @param type Child type (GFX_OBJ_TYPE_IMAGE, GFX_OBJ_TYPE_LABEL, etc.)
 * @param src Child object pointer (e.g. gfx_obj_t *)
 * @return ESP_OK on success
 * @internal Used by gfx_anim_create, gfx_img_create, gfx_label_create, gfx_qrcode_create.
 */
esp_err_t gfx_disp_add_child(gfx_disp_t *disp, void *src);

/**
 * @brief Remove a child object from a display
 * @param disp Display that owns the child
 * @param src Child object pointer to remove (e.g. gfx_obj_t *)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not in list
 * @internal Used by gfx_obj_delete.
 */
esp_err_t gfx_disp_remove_child(gfx_disp_t *disp, void *src);

/**
 * @brief Delete and detach every child object owned by a display.
 * @param disp Display that owns the child list
 * @return ESP_OK on success
 * @internal Used during display/core teardown to ensure widget destructors run.
 */
esp_err_t gfx_disp_delete_children(gfx_disp_t *disp);

#ifdef __cplusplus
}
#endif
