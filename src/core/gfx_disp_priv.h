/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/event_groups.h"
#include "core/gfx_disp.h"
#include "core/gfx_obj_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *   FORWARD DECL
 *********************/
struct gfx_core_context;

/*********************
 *   DEFINES
 *********************/
#define GFX_DISP_INV_BUF_SIZE  16   /**< Max dirty areas per display */

/*********************
 *   INTERNAL STRUCTS
 *********************/
/** Per-display state; one per screen, linked list for multi-display */
struct gfx_disp {
    struct gfx_disp *next;
    struct gfx_core_context *ctx;

    uint32_t h_res;
    uint32_t v_res;
    struct {
        unsigned char swap : 1;
        unsigned char buff_dma : 1;
        unsigned char buff_spiram : 1;
        unsigned char double_buffer : 1;
    } flags;

    gfx_disp_flush_cb_t flush_cb;
    gfx_disp_update_cb_t update_cb;
    void *user_data;
    EventGroupHandle_t event_group;

    gfx_obj_child_t *child_list;
    uint16_t *buf1;
    uint16_t *buf2;
    uint16_t *buf_act;
    size_t buf_pixels;
    gfx_color_t bg_color;
    bool ext_bufs;
    bool flushing_last;
    bool swap_act_buf;

    gfx_area_t dirty_areas[GFX_DISP_INV_BUF_SIZE];
    uint8_t area_merged[GFX_DISP_INV_BUF_SIZE];
    uint8_t dirty_count;
};

/* ============================================================================
 * Internal buffer helpers (used by gfx_disp.c and gfx_core.c deinit)
 * ============================================================================ */

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

/* ============================================================================
 * Internal API (obj/widget/render only, not in public gfx_disp.h)
 * ============================================================================ */

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
 * @brief Get display size in pixels
 * @param disp Display (NULL allowed; then width/height get defaults)
 * @param width Output width
 * @param height Output height
 * @return ESP_OK
 * @internal Used by gfx_obj (alignment) and gfx_anim (parent size).
 */
esp_err_t gfx_disp_get_size(gfx_disp_t *disp, uint32_t *width, uint32_t *height);

/**
 * @brief Check if display is currently flushing the last block
 * @param disp Display
 * @return true if flushing last block, false otherwise
 * @internal Used by render path; not exposed to application.
 */
bool gfx_disp_is_flushing_last(gfx_disp_t *disp);

#ifdef __cplusplus
}
#endif
