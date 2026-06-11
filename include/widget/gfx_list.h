/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Focus-change callback for a list widget.
 *
 * @param obj List object.
 * @param focused_index New focused item index, or -1 when no item is focused.
 * @param user_data User data passed to gfx_list_set_focus_cb().
 */
typedef void (*gfx_list_focus_cb_t)(gfx_obj_t *obj, int32_t focused_index, void *user_data);

/**
 * @brief Create a list object on a display.
 *
 * @param disp Display that owns the list.
 * @return Created list object, or NULL on failure.
 */
gfx_obj_t *gfx_list_create(gfx_disp_t *disp);

/**
 * @brief Remove all items from a list.
 *
 * @param obj List object.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_clear(gfx_obj_t *obj);

/**
 * @brief Append one text item to a list.
 *
 * @param obj List object.
 * @param text Item text. The string is copied by the list.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_add_item(gfx_obj_t *obj, const char *text);

/**
 * @brief Replace all list items with a text array.
 *
 * Existing items are cleared first. Each string is copied by the list.
 *
 * @param obj List object.
 * @param items Array of item text pointers.
 * @param item_count Number of entries in items.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_items(gfx_obj_t *obj, const char *const *items, uint16_t item_count);

/**
 * @brief Set the focused item index.
 *
 * The list keeps the focused item visible by adjusting top index if needed.
 * Use -1 to clear focus.
 *
 * @param obj List object.
 * @param index Item index to focus, or -1 to clear focus.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_focus(gfx_obj_t *obj, int32_t index);

/**
 * @brief Get the focused item index.
 *
 * @param obj List object.
 * @return Focused item index, or -1 when no item is focused or obj is invalid.
 */
int32_t gfx_list_get_focus(gfx_obj_t *obj);

/**
 * @brief Set the first visible item index.
 *
 * @param obj List object.
 * @param index Item index to show at the top.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_top_index(gfx_obj_t *obj, uint16_t index);

/**
 * @brief Get the first visible item index.
 *
 * @param obj List object.
 * @return Top item index, or 0 when obj is invalid.
 */
uint16_t gfx_list_get_top_index(gfx_obj_t *obj);

/**
 * @brief Get the number of items in a list.
 *
 * @param obj List object.
 * @return Item count, or 0 when obj is invalid.
 */
uint16_t gfx_list_get_item_count(gfx_obj_t *obj);

/**
 * @brief Get item text by index.
 *
 * @param obj List object.
 * @param index Item index.
 * @return Item text pointer owned by the list, or NULL if index is invalid.
 */
const char *gfx_list_get_item_text(gfx_obj_t *obj, uint16_t index);

/**
 * @brief Set the font used to draw list items.
 *
 * @param obj List object.
 * @param font Font handle created by gfx_label_font_create(), or an LVGL font pointer.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_font(gfx_obj_t *obj, gfx_font_t font);

/**
 * @brief Set fixed item height.
 *
 * @param obj List object.
 * @param height Item height in pixels.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_item_height(gfx_obj_t *obj, uint16_t height);

/**
 * @brief Set item text padding.
 *
 * @param obj List object.
 * @param pad_x Horizontal text padding in pixels.
 * @param pad_y Vertical text padding in pixels.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_text_pad(gfx_obj_t *obj, uint16_t pad_x, uint16_t pad_y);

/**
 * @brief Set the focus-change callback.
 *
 * @param obj List object.
 * @param cb Callback to invoke on focus change, or NULL to clear it.
 * @param user_data User data passed to cb.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_focus_cb(gfx_obj_t *obj, gfx_list_focus_cb_t cb, void *user_data);

/**
 * @brief Set the normal item background color.
 *
 * @param obj List object.
 * @param color Background color.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_bg_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the focused item background color.
 *
 * @param obj List object.
 * @param color Focused background color.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_focus_bg_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the normal item text color.
 *
 * @param obj List object.
 * @param color Text color.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_text_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the focused item text color.
 *
 * @param obj List object.
 * @param color Focused text color.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_focus_text_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the list border color.
 *
 * @param obj List object.
 * @param color Border color.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_border_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the list border width.
 *
 * @param obj List object.
 * @param width Border width in pixels; 0 disables the border.
 * @return ESP_OK on success, ESP_ERR_* otherwise.
 */
esp_err_t gfx_list_set_border_width(gfx_obj_t *obj, uint16_t width);

#ifdef __cplusplus
}
#endif
