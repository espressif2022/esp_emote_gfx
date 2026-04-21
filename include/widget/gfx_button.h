/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a button object on a display
 * @param disp Display from gfx_disp_add()
 * @return Pointer to the created button object
 */
gfx_obj_t *gfx_button_create(gfx_disp_t *disp);

/**
 * @brief Set the label text for a button
 * @param obj Button object
 * @param text Text string; NULL is treated as an empty string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_text(gfx_obj_t *obj, const char *text);

/**
 * @brief Set the label text for a button using printf-style formatting
 * @param obj Button object
 * @param fmt Format string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...);

/**
 * @brief Set the font used by the button label
 * @param obj Button object
 * @param font Font handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_font(gfx_obj_t *obj, gfx_font_t font);

/**
 * @brief Set the label text color for a button
 * @param obj Button object
 * @param color Text color
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_text_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the normal background color for a button
 * @param obj Button object
 * @param color Background color
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_bg_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the pressed background color for a button
 * @param obj Button object
 * @param color Pressed background color
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_bg_color_pressed(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the border color for a button
 * @param obj Button object
 * @param color Border color
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_border_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the border width for a button
 * @param obj Button object
 * @param width Border width in pixels; 0 disables the border
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_border_width(gfx_obj_t *obj, uint16_t width);

/**
 * @brief Set the text alignment for a button label
 * @param obj Button object
 * @param align Text alignment
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_button_set_text_align(gfx_obj_t *obj, gfx_text_align_t align);

#ifdef __cplusplus
}
#endif
