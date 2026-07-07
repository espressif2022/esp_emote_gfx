/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a lightweight container object.
 *
 * A container can own child objects through gfx_object_add_child(). Children are
 * rendered after the container itself and receive hit-testing before the parent.
 * Child coordinates are still screen-space in this first object-tree version.
 * Optional child clipping can restrict child draw and hit-test to container
 * bounds.
 *
 * @param disp Display from gfx_display_add().
 * @return Container object, or NULL on failure.
 */
gfx_object_t *gfx_container_create(gfx_display_t *disp);

/**
 * @brief Set whether the container background is drawn.
 * @param obj Container object.
 * @param enable True to draw the background.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_bg_enable(gfx_object_t *obj, bool enable);

/**
 * @brief Set the container background color.
 * @param obj Container object.
 * @param color Background color.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the container border color.
 * @param obj Container object.
 * @param color Border color.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_border_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the container border width.
 * @param obj Container object.
 * @param width Border width in pixels; 0 disables the border.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_border_width(gfx_object_t *obj, uint16_t width);

/**
 * @brief Set the container corner radius.
 * @param obj Container object.
 * @param radius Corner radius in pixels; 0 draws a rectangle.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_radius(gfx_object_t *obj, uint16_t radius);

/**
 * @brief Set whether the container border is drawn as dashed.
 *
 * Dashed mode is intended for small rounded/circle indicators. Rectangular
 * containers continue to use the normal solid stroke path when radius is 0.
 *
 * @param obj Container object.
 * @param enable True to draw a dashed border.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_border_dash_enable(gfx_object_t *obj, bool enable);

/**
 * @brief Set whether children are clipped to the container bounds.
 * @param obj Container object.
 * @param enable True to clip child draw and hit-test to the container area.
 * @return GFX_OK on success, error code otherwise.
 */
gfx_err_t gfx_container_set_clip_children(gfx_object_t *obj, bool enable);

#ifdef __cplusplus
}
#endif
