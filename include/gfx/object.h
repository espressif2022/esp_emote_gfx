/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/error.h"
#include "gfx/types.h"
#include "gfx/input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alignment constants (similar to LVGL) */
#define GFX_ALIGN_DEFAULT         0x00
#define GFX_ALIGN_TOP_LEFT        0x00
#define GFX_ALIGN_TOP_MID         0x01
#define GFX_ALIGN_TOP_RIGHT       0x02
#define GFX_ALIGN_LEFT_MID        0x03
#define GFX_ALIGN_CENTER          0x04
#define GFX_ALIGN_RIGHT_MID       0x05
#define GFX_ALIGN_BOTTOM_LEFT     0x06
#define GFX_ALIGN_BOTTOM_MID      0x07
#define GFX_ALIGN_BOTTOM_RIGHT    0x08
#define GFX_ALIGN_OUT_TOP_LEFT    0x09
#define GFX_ALIGN_OUT_TOP_MID     0x0A
#define GFX_ALIGN_OUT_TOP_RIGHT   0x0B
#define GFX_ALIGN_OUT_LEFT_TOP    0x0C
#define GFX_ALIGN_OUT_LEFT_MID    0x0D
#define GFX_ALIGN_OUT_LEFT_BOTTOM 0x0E
#define GFX_ALIGN_OUT_RIGHT_TOP   0x0F
#define GFX_ALIGN_OUT_RIGHT_MID   0x10
#define GFX_ALIGN_OUT_RIGHT_BOTTOM 0x11
#define GFX_ALIGN_OUT_BOTTOM_LEFT 0x12
#define GFX_ALIGN_OUT_BOTTOM_MID  0x13
#define GFX_ALIGN_OUT_BOTTOM_RIGHT 0x14

/**********************
 *      TYPEDEFS
 **********************/
typedef struct gfx_object gfx_object_t;

/**
 * @brief Application-level touch callback (register with gfx_object_set_touch_cb)
 * @param obj Object that received the touch
 * @param event Touch event (PRESS / RELEASE / MOVE)
 * @param user_data User data passed to gfx_object_set_touch_cb
 */
typedef void (*gfx_object_touch_cb_t)(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data);

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Set the position of an object
 * @param obj Pointer to the object
 * @param x X coordinate
 * @param y Y coordinate
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_set_pos(gfx_object_t *obj, gfx_coord_t x, gfx_coord_t y);

/**
 * @brief Set the size of an object
 * @param obj Pointer to the object
 * @param w Width
 * @param h Height
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_set_size(gfx_object_t *obj, uint16_t w, uint16_t h);

/**
 * @brief Align an object relative to the screen or another object
 * @param obj Pointer to the object to align
 * @param align Alignment type (see GFX_ALIGN_* constants)
 * @param x_ofs X offset from the alignment position
 * @param y_ofs Y offset from the alignment position
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_align(gfx_object_t *obj, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);

/**
 * @brief Align an object relative to another object
 * @param obj Pointer to the object to align
 * @param base Reference object; NULL means align to the display
 * @param align Alignment type (see GFX_ALIGN_* constants)
 * @param x_ofs X offset from the alignment position
 * @param y_ofs Y offset from the alignment position
 * @return GFX_OK on success
 */
gfx_err_t gfx_object_align_to(gfx_object_t *obj, gfx_object_t *base, uint8_t align, gfx_coord_t x_ofs, gfx_coord_t y_ofs);

/**
 * @brief Set object visibility
 * @param obj Object to set visibility for
 * @param visible True to make object visible, false to hide
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_set_visible(gfx_object_t *obj, bool visible);

/**
 * @brief Get object visibility
 * @param obj Object to check visibility for
 * @return True if object is visible, false if hidden
 */
bool gfx_object_get_visible(gfx_object_t *obj);

/**
 * @brief Update object's layout (mark for recalculation before rendering)
 * @param obj Object to update layout
 * @note This is used when object properties that affect layout have changed,
 *       but the actual position calculation needs to be deferred until rendering
 */
void gfx_object_update_layout(gfx_object_t *obj);

/* Object getters */

/**
 * @brief Get the position of an object
 * @param obj Pointer to the object
 * @param x Pointer to store X coordinate
 * @param y Pointer to store Y coordinate
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_get_pos(gfx_object_t *obj, gfx_coord_t *x, gfx_coord_t *y);

/**
 * @brief Get the size of an object
 * @param obj Pointer to the object
 * @param w Pointer to store width
 * @param h Pointer to store height
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_get_size(gfx_object_t *obj, uint16_t *w, uint16_t *h);

/* Object management */

/**
 * @brief Delete an object
 * @param obj Pointer to the object to delete
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_delete(gfx_object_t *obj);

/**
 * @brief Register application touch callback for an object
 *
 * When this object is the hit target of a touch (PRESS/MOVE/RELEASE), the callback
 * is invoked. Pass NULL to unregister.
 *
 * @param obj Object to listen on
 * @param cb Callback (NULL to clear)
 * @param user_data Passed to cb
 * @return GFX_OK on success
 */
gfx_err_t gfx_object_set_touch_cb(gfx_object_t *obj, gfx_object_touch_cb_t cb, void *user_data);

/**
 * @brief Get object creation sequence id (monotonic per process lifetime)
 * @param obj Object pointer
 * @return Sequence id, or 0 if obj is NULL
 */
uint32_t gfx_object_get_trace_id(gfx_object_t *obj);

/**
 * @brief Get object class name (from registered widget class metadata)
 * @param obj Object pointer
 * @return Class name string, or NULL
 */
const char *gfx_object_get_class_name(gfx_object_t *obj);

/**
 * @brief Get object creation tag (creation-site annotation)
 * @param obj Object pointer
 * @return Creation tag string, or NULL
 */
const char *gfx_object_get_trace_tag(gfx_object_t *obj);

/**
 * @brief Add a child object to a parent container
 * @param parent Parent object
 * @param child Child object
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_add_child(gfx_object_t *parent, gfx_object_t *child);

/**
 * @brief Remove a child object from its parent
 * @param parent Parent object
 * @param child Child object
 * @return GFX_OK on success, or a GFX_ERR_* code on failure.
 */
gfx_err_t gfx_object_remove_child(gfx_object_t *parent, gfx_object_t *child);

/**
 * @brief Get the parent of an object
 * @param obj Object pointer
 * @return Parent object, or NULL if none
 */
gfx_object_t *gfx_object_get_parent(gfx_object_t *obj);

#ifdef __cplusplus
}
#endif
