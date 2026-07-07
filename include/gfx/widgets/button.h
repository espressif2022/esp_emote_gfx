/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a button object on a display
 * @param disp Display from gfx_display_add()
 * @return Pointer to the created button object
 */
gfx_object_t *gfx_button_create(gfx_display_t *disp);

/**
 * @brief Set the label text for a button
 * @param obj Button object
 * @param text Text string; NULL is treated as an empty string
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_text(gfx_object_t *obj, const char *text);

/**
 * @brief Set the label text for a button using printf-style formatting
 * @param obj Button object
 * @param fmt Format string
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_text_fmt(gfx_object_t *obj, const char *fmt, ...);

/**
 * @brief Set the font used by the button label
 * @param obj Button object
 * @param font Font handle
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_font(gfx_object_t *obj, gfx_font_t font);

/**
 * @brief Set the label text color for a button
 * @param obj Button object
 * @param color Text color
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_text_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the normal background color for a button
 * @param obj Button object
 * @param color Background color
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the pressed background color for a button
 * @param obj Button object
 * @param color Pressed background color
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_bg_color_pressed(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the border color for a button
 * @param obj Button object
 * @param color Border color
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_border_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the border width for a button
 * @param obj Button object
 * @param width Border width in pixels; 0 disables the border
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_border_width(gfx_object_t *obj, uint16_t width);

/**
 * @brief Set the rounded rectangle corner radius for a button
 * @param obj Button object
 * @param radius Corner radius in pixels
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_radius(gfx_object_t *obj, uint16_t radius);

/**
 * @brief Enable or disable button background fill
 * @param obj Button object
 * @param enable True to draw the fill color, false for border/text only
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_fill_enable(gfx_object_t *obj, bool enable);

/**
 * @brief Set text padding inside the button
 * @param obj Button object
 * @param pad_x Horizontal padding in pixels
 * @param pad_y Vertical padding in pixels
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_text_padding(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y);

/**
 * @brief Set the text alignment for a button label
 * @param obj Button object
 * @param align Text alignment
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_button_set_text_align(gfx_object_t *obj, gfx_text_align_t align);

#ifdef __cplusplus
}
#endif
