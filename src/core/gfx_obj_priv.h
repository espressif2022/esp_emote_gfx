/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "core/gfx_core.h"
#include "core/gfx_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

// Default screen dimensions for alignment calculation
#define DEFAULT_SCREEN_WIDTH  320
#define DEFAULT_SCREEN_HEIGHT 240

/**********************
 *      TYPEDEFS
 **********************/

/** Object draw vfunc (internal) */
typedef esp_err_t (*gfx_obj_draw_fn_t)(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);
/** Object delete vfunc (internal) */
typedef esp_err_t (*gfx_obj_delete_fn_t)(gfx_obj_t *obj);
/** Object update vfunc (internal) */
typedef esp_err_t (*gfx_obj_update_fn_t)(gfx_obj_t *obj);
/** Object touch event vfunc (internal; event is const gfx_touch_event_t *) */
typedef void (*gfx_obj_touch_fn_t)(gfx_obj_t *obj, const void *event);

/* Graphics object structure - internal definition */
struct gfx_obj {
    /* Basic properties */
    void *src;                  /**< Source data (image, label, etc.) */
    int type;                   /**< Object type */
    gfx_disp_t *disp;           /**< Display this object belongs to (from gfx_emote_add_disp) */

    /* Geometry */
    struct {
        gfx_coord_t x;          /**< X position */
        gfx_coord_t y;          /**< Y position */
        uint16_t width;         /**< Object width */
        uint16_t height;        /**< Object height */
    } geometry;

    /* Alignment */
    struct {
        uint8_t type;           /**< Alignment type (see GFX_ALIGN_* constants) */
        gfx_coord_t x_ofs;      /**< X offset for alignment */
        gfx_coord_t y_ofs;      /**< Y offset for alignment */
        bool enabled;           /**< Whether to use alignment instead of absolute position */
    } align;

    /* Rendering state */
    struct {
        bool is_visible: 1;       /**< Object visibility */
        bool layout_dirty: 1;     /**< Whether layout needs to be recalculated before rendering */
        bool dirty: 1;            /**< Whether the object is dirty */
    } state;

    /* Virtual function table */
    struct {
        gfx_obj_draw_fn_t draw;       /**< Draw function pointer */
        gfx_obj_delete_fn_t delete;   /**< Delete function pointer */
        gfx_obj_update_fn_t update;   /**< Update function pointer */
        gfx_obj_touch_fn_t touch_event; /**< Touch event (optional, NULL = no handler) */
    } vfunc;

    /** Application touch callback (from gfx_obj_set_touch_cb) */
    gfx_obj_touch_cb_t user_touch_cb;
    void *user_touch_data;
};

typedef struct gfx_obj_child_t {
    void *src;
    struct gfx_obj_child_t *next;
} gfx_obj_child_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Internal alignment functions
 *====================*/

/**
 * @brief Calculate aligned position for an object (internal use)
 * @param obj Pointer to the object
 * @param parent_width Parent container width in pixels
 * @param parent_height Parent container height in pixels
 * @param x Pointer to store calculated X coordinate
 * @param y Pointer to store calculated Y coordinate
 */
void gfx_obj_cal_aligned_pos(gfx_obj_t *obj, uint32_t parent_width, uint32_t parent_height, gfx_coord_t *x, gfx_coord_t *y);

/**
 * @brief Get parent dimensions and calculate aligned object position
 *
 * This is a convenience function that combines getting parent screen size
 * and calculating the aligned position of the object. It modifies obj->geometry.x
 * and obj->geometry.y in place based on the alignment settings.
 *
 * @param obj Pointer to the object
 */
void gfx_obj_calc_pos_in_parent(gfx_obj_t *obj);

#ifdef __cplusplus
}
#endif
