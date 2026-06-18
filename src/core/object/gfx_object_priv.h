/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "core/gfx_types_priv.h"
#include "core/gfx_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
/* Object types */
#define GFX_OBJ_TYPE_SCREEN       0x00  /**< Screen type (reserved) */
#define GFX_OBJ_TYPE_IMAGE        0x01
#define GFX_OBJ_TYPE_LABEL        0x02
#define GFX_OBJ_TYPE_ANIMATION    0x03
#define GFX_OBJ_TYPE_QRCODE       0x04
#define GFX_OBJ_TYPE_BUTTON       0x05
#define GFX_OBJ_TYPE_MESH_IMAGE   0x06
#define GFX_OBJ_TYPE_LIST         0x07
#define GFX_OBJ_TYPE_FACE_EMOTE   0x08
#define GFX_OBJ_TYPE_WHEEL        0x09
#define GFX_OBJ_TYPE_LOBSTER_EMOTE 0x0A
#define GFX_OBJ_TYPE_PAGEFLOW     0x0B
#define GFX_OBJ_TYPE_STICKMAN_EMOTE 0x0C
#define GFX_OBJ_TYPE_COVERFLOW    0x0D
#define GFX_OBJ_TYPE_CONTAINER    0x0E

#define DEFAULT_SCREEN_WIDTH  320
#define DEFAULT_SCREEN_HEIGHT 240

/**********************
 *      TYPEDEFS
 **********************/

typedef struct gfx_draw_ctx {
    void *buf;                  /**< Buffer start (chunk start or offset into full-frame) */
    gfx_area_t buf_area;        /**< Half-open screen rect [x1, x2) x [y1, y2); buf[0] maps to (buf_area.x1, buf_area.y1) */
    gfx_area_t clip_area;       /**< Half-open screen rect [x1, x2) x [y1, y2) for this draw pass */
    gfx_coord_t stride;         /**< Row stride in pixels (chunk width or h_res) */
    gfx_color_format_t format;  /**< Internal render buffer color format */
    uint8_t pixel_size;         /**< Internal render buffer bytes per pixel */
} gfx_draw_ctx_t;

typedef gfx_err_t (*gfx_object_draw_fn_t)(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
typedef gfx_err_t (*gfx_object_delete_fn_t)(gfx_object_t *obj);
typedef gfx_err_t (*gfx_object_load_fn_t)(gfx_object_t *obj);
typedef void (*gfx_object_release_fn_t)(gfx_object_t *obj);
typedef gfx_err_t (*gfx_object_update_fn_t)(gfx_object_t *obj);
typedef void (*gfx_object_touch_fn_t)(gfx_object_t *obj, const void *event);

typedef struct gfx_widget_class {
    uint8_t type;                  /**< Stable object type id */
    const char *name;              /**< Debug-friendly class name */
    gfx_object_draw_fn_t draw;        /**< Draw callback */
    gfx_object_delete_fn_t delete;    /**< Delete callback */
    gfx_object_load_fn_t load;        /**< Resource load callback */
    gfx_object_release_fn_t release;  /**< Resource release callback */
    gfx_object_update_fn_t update;    /**< Update callback */
    gfx_object_touch_fn_t touch_event;/**< Touch callback */
} gfx_widget_class_t;

struct gfx_object {
    void *src;                  /**< Source data (image, label, etc.) */
    uint8_t type;               /**< Object type */
    const gfx_widget_class_t *klass; /**< Registered class metadata */
    gfx_display_t *disp;           /**< Display this object belongs to */
    struct gfx_object *parent;      /**< Parent object for composite widgets; NULL for display root children */
    struct gfx_object_child_t *child_list; /**< Child object list, drawn after this object */

    struct {
        gfx_coord_t x;          /**< X position */
        gfx_coord_t y;          /**< Y position */
        uint16_t width;         /**< Object width */
        uint16_t height;        /**< Object height */
    } geometry;

    struct {
        gfx_coord_t x;          /**< Local X requested by user/layout */
        gfx_coord_t y;          /**< Local Y requested by user/layout */
        uint16_t width;         /**< Local width requested by user/layout */
        uint16_t height;        /**< Local height requested by user/layout */
    } local_geometry;

    struct {
        gfx_area_t abs_area;    /**< Cached inclusive absolute bounds after layout resolve and ancestor clipping */
    } resolved;

    struct {
        uint8_t type;           /**< Alignment type (see GFX_ALIGN_* constants) */
        gfx_coord_t x_ofs;      /**< X offset for alignment */
        gfx_coord_t y_ofs;      /**< Y offset for alignment */
        gfx_object_t *target;      /**< Reference object for align_to; NULL means align to display */
        bool enabled;           /**< Whether to use alignment instead of absolute position */
    } align;

    struct {
        bool is_visible: 1;       /**< Object visibility */
        bool layout_dirty: 1;     /**< Whether layout needs to be recalculated before rendering */
        bool dirty: 1;            /**< Whether the object is dirty */
        bool resource_loaded: 1;   /**< Whether widget resources are currently loaded */
        bool resource_dirty: 1;    /**< Whether widget resources need reload */
        bool manual_child_draw: 1; /**< Parent draws child objects itself instead of renderer recursion */
        bool input_passthrough: 1; /**< Object is skipped as a touch target; children are still tested */
        bool clip_children: 1;     /**< Child draw/hit-test is clipped to this object bounds */
        bool abs_area_valid: 1;    /**< Whether resolved.abs_area cache is valid */
    } state;

    struct {
        gfx_object_draw_fn_t draw;       /**< Draw function pointer */
        gfx_object_delete_fn_t delete;   /**< Delete function pointer */
        gfx_object_load_fn_t load;       /**< Load function pointer */
        gfx_object_release_fn_t release; /**< Release function pointer */
        gfx_object_update_fn_t update;   /**< Update function pointer */
        gfx_object_touch_fn_t touch_event; /**< Touch event (optional, NULL = no handler) */
    } vfunc;

    /** Application touch callback (from gfx_object_set_touch_cb) */
    gfx_object_touch_cb_t user_touch_cb;
    void *user_touch_data;

    struct {
        uint32_t create_seq;            /**< Monotonic object creation sequence */
        const char *class_name;         /**< Widget class name */
        const char *create_tag;         /**< Creation annotation tag */
    } trace;
};

typedef struct gfx_object_child_t {
    void *src;
    struct gfx_object_child_t *next;
} gfx_object_child_t;

/**********************
 *   INTERNAL API
 **********************/

gfx_err_t gfx_widget_class_register(const gfx_widget_class_t *klass);
const gfx_widget_class_t *gfx_widget_class_get(uint8_t type);
gfx_err_t gfx_object_init_class_instance(gfx_object_t *obj, gfx_display_t *disp, const gfx_widget_class_t *klass, void *src);
gfx_err_t gfx_object_create_class_instance(gfx_display_t *disp, const gfx_widget_class_t *klass,
        void *src, uint16_t width, uint16_t height,
        const char *create_tag, gfx_object_t **out_obj);

gfx_err_t gfx_object_child_list_add(gfx_object_child_t **list, gfx_object_t *obj);
gfx_err_t gfx_object_child_list_remove(gfx_object_child_t **list, gfx_object_t *obj);
bool gfx_object_child_list_contains(gfx_object_child_t *list, gfx_object_t *obj, uint32_t create_seq);
void gfx_object_child_list_free_nodes(gfx_object_child_t **list);

void gfx_object_cal_aligned_pos(gfx_object_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y);
void gfx_object_calc_pos_in_parent(gfx_object_t *obj);
bool gfx_object_resolve_abs_area_unclipped(gfx_object_t *obj, gfx_area_t *area);
gfx_err_t gfx_object_load_resource(gfx_object_t *obj);
void gfx_object_release_resource(gfx_object_t *obj);
void gfx_object_mark_resource_dirty(gfx_object_t *obj);
void gfx_object_set_manual_child_draw(gfx_object_t *obj, bool enable);
void gfx_object_set_input_passthrough(gfx_object_t *obj, bool enable);
void gfx_object_set_input_passthrough_tree(gfx_object_t *obj, bool enable);
void gfx_object_set_clip_children(gfx_object_t *obj, bool enable);
void gfx_object_invalidate_abs_area_cache_tree(gfx_object_t *obj);

#ifdef __cplusplus
}
#endif
