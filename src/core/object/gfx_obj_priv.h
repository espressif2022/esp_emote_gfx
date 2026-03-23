/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "core/gfx_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
#define DEFAULT_SCREEN_WIDTH  320
#define DEFAULT_SCREEN_HEIGHT 240

#define GFX_DRAW_CTX_DEST_PTR(ctx, x, y) \
    ((gfx_color_t *)((uint8_t *)(ctx)->buf + \
        ((y) - (ctx)->buf_area.y1) * (ctx)->stride * GFX_PIXEL_SIZE_16BPP + \
        ((x) - (ctx)->buf_area.x1) * GFX_PIXEL_SIZE_16BPP))

/**********************
 *      TYPEDEFS
 **********************/

typedef struct gfx_draw_ctx {
    void *buf;                  /**< Buffer start (chunk start or offset into full-frame) */
    gfx_area_t buf_area;        /**< Half-open screen rect [x1, x2) x [y1, y2); buf[0] maps to (buf_area.x1, buf_area.y1) */
    gfx_area_t clip_area;       /**< Half-open screen rect [x1, x2) x [y1, y2) for this draw pass */
    int stride;                 /**< Row stride in pixels (chunk width or h_res) */
    bool swap;                  /**< Color byte swap */
} gfx_draw_ctx_t;

typedef esp_err_t (*gfx_obj_draw_fn_t)(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
typedef esp_err_t (*gfx_obj_delete_fn_t)(gfx_obj_t *obj);
typedef esp_err_t (*gfx_obj_update_fn_t)(gfx_obj_t *obj);
typedef void (*gfx_obj_touch_fn_t)(gfx_obj_t *obj, const void *event);

typedef struct gfx_widget_class {
    uint8_t type;                  /**< Stable object type id */
    const char *name;              /**< Debug-friendly class name */
    gfx_obj_draw_fn_t draw;        /**< Draw callback */
    gfx_obj_delete_fn_t delete;    /**< Delete callback */
    gfx_obj_update_fn_t update;    /**< Update callback */
    gfx_obj_touch_fn_t touch_event;/**< Touch callback */
} gfx_widget_class_t;

struct gfx_obj {
    void *src;                  /**< Source data (image, label, etc.) */
    int type;                   /**< Object type */
    const gfx_widget_class_t *klass; /**< Registered class metadata */
    gfx_disp_t *disp;           /**< Display this object belongs to (from gfx_emote_add_disp) */

    struct {
        gfx_coord_t x;          /**< X position */
        gfx_coord_t y;          /**< Y position */
        uint16_t width;         /**< Object width */
        uint16_t height;        /**< Object height */
    } geometry;

    struct {
        uint8_t type;           /**< Alignment type (see GFX_ALIGN_* constants) */
        gfx_coord_t x_ofs;      /**< X offset for alignment */
        gfx_coord_t y_ofs;      /**< Y offset for alignment */
        gfx_obj_t *target;      /**< Reference object for align_to; NULL means align to display */
        bool enabled;           /**< Whether to use alignment instead of absolute position */
    } align;

    struct {
        bool is_visible: 1;       /**< Object visibility */
        bool layout_dirty: 1;     /**< Whether layout needs to be recalculated before rendering */
        bool dirty: 1;            /**< Whether the object is dirty */
    } state;

    struct {
        gfx_obj_draw_fn_t draw;       /**< Draw function pointer */
        gfx_obj_delete_fn_t delete;   /**< Delete function pointer */
        gfx_obj_update_fn_t update;   /**< Update function pointer */
        gfx_obj_touch_fn_t touch_event; /**< Touch event (optional, NULL = no handler) */
    } vfunc;

    /** Application touch callback (from gfx_obj_set_touch_cb) */
    gfx_obj_touch_cb_t user_touch_cb;
    void *user_touch_data;

    struct {
        uint32_t create_seq;            /**< Monotonic object creation sequence */
        const char *class_name;         /**< Widget class name */
        const char *create_tag;         /**< Creation annotation tag */
    } trace;
};

typedef struct gfx_obj_child_t {
    void *src;
    struct gfx_obj_child_t *next;
} gfx_obj_child_t;

/**********************
 *   INTERNAL API
 **********************/

esp_err_t gfx_widget_class_register(const gfx_widget_class_t *klass);
const gfx_widget_class_t *gfx_widget_class_get(uint8_t type);
esp_err_t gfx_obj_init_class_instance(gfx_obj_t *obj, gfx_disp_t *disp, const gfx_widget_class_t *klass, void *src);
esp_err_t gfx_obj_create_class_instance(gfx_disp_t *disp, const gfx_widget_class_t *klass,
                                        void *src, uint16_t width, uint16_t height,
                                        const char *create_tag, gfx_obj_t **out_obj);

void gfx_obj_cal_aligned_pos(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y);
void gfx_obj_calc_pos_in_parent(gfx_obj_t *obj);

#ifdef __cplusplus
}
#endif
