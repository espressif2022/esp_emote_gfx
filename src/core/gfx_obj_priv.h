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

/* Graphics object structure - internal definition */
struct gfx_obj {
    /* Basic properties */
    void *src;                  /**< Source data (image, label, etc.) */
    int type;                   /**< Object type */
    gfx_handle_t parent_handle; /**< Parent graphics handle */

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
        bool is_visible;        /**< Object visibility */
        bool layout_dirty;      /**< Whether layout needs to be recalculated before rendering */
    } state;

    /* Virtual function table */
    struct {
        gfx_obj_draw_fn_t draw;    /**< Draw function pointer */
        gfx_obj_delete_fn_t delete; /**< Delete function pointer */
    } vfunc;
};

typedef struct gfx_core_child_t {
    int type;
    void *src;
    struct gfx_core_child_t *next;  // Pointer to next child in the list
} gfx_core_child_t;

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
