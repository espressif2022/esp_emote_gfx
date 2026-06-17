/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/widgets/label.h"

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
typedef void (*gfx_list_focus_cb_t)(gfx_object_t *obj, int32_t focused_index, void *user_data);

/**
 * @brief Selection callback for a list widget.
 *
 * `confirmed` is true when the selection came from an explicit user confirm
 * action such as a click/tap release. It is false for programmatic selection.
 *
 * @param obj List object.
 * @param selected_index Selected item index, or -1 when selection is cleared.
 * @param confirmed Whether this selection is a user confirmation.
 * @param user_data User data passed to gfx_list_set_select_cb().
 */
typedef void (*gfx_list_select_cb_t)(gfx_object_t *obj, int32_t selected_index,
                                     bool confirmed, void *user_data);

/**
 * @brief Page-load callback for a list widget.
 *
 * The callback is invoked when the current page changes. Applications may use
 * it to update external data or refill the list around page boundaries.
 *
 * @param obj List object.
 * @param page_index Current page index.
 * @param items_per_page Configured items per page.
 * @param user_data User data passed to gfx_list_set_page_load_cb().
 */
typedef void (*gfx_list_page_load_cb_t)(gfx_object_t *obj, uint16_t page_index,
                                        uint16_t items_per_page, void *user_data);

/**
 * @brief Create a list object on a display.
 *
 * @param disp Display that owns the list.
 * @return Created list object, or NULL on failure.
 */
gfx_object_t *gfx_list_create(gfx_display_t *disp);

/**
 * @brief Remove all items from a list.
 *
 * @param obj List object.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_clear(gfx_object_t *obj);

/**
 * @brief Append one text item to a list.
 *
 * @param obj List object.
 * @param text Item text. The string is copied by the list.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_add_item(gfx_object_t *obj, const char *text);

/**
 * @brief Replace all list items with a text array.
 *
 * Existing items are cleared first. Each string is copied by the list.
 *
 * @param obj List object.
 * @param items Array of item text pointers.
 * @param item_count Number of entries in items.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count);

/**
 * @brief Set the focused item index.
 *
 * The list keeps the focused item visible by adjusting top index if needed.
 * Use -1 to clear focus.
 *
 * @param obj List object.
 * @param index Item index to focus, or -1 to clear focus.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_focus(gfx_object_t *obj, int32_t index);

/**
 * @brief Get the focused item index.
 *
 * @param obj List object.
 * @return Focused item index, or -1 when no item is focused or obj is invalid.
 */
int32_t gfx_list_get_focus(gfx_object_t *obj);

/**
 * @brief Set the first visible item index.
 *
 * @param obj List object.
 * @param index Item index to show at the top.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_top_index(gfx_object_t *obj, uint16_t index);

/**
 * @brief Get the first visible item index.
 *
 * @param obj List object.
 * @return Top item index, or 0 when obj is invalid.
 */
uint16_t gfx_list_get_top_index(gfx_object_t *obj);

/**
 * @brief Set vertical scroll offset in pixels.
 *
 * The value is clamped to the valid content range. top_index is derived from
 * this offset and kept for item-step compatibility.
 *
 * @param obj List object.
 * @param scroll_y Vertical scroll offset in pixels.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_scroll_offset(gfx_object_t *obj, int32_t scroll_y);

/**
 * @brief Get vertical scroll offset in pixels.
 *
 * @param obj List object.
 * @return Current vertical scroll offset in pixels.
 */
int32_t gfx_list_get_scroll_offset(gfx_object_t *obj);

/**
 * @brief Set selected item index independently from focus.
 *
 * Use -1 to clear selection.
 *
 * @param obj List object.
 * @param index Selected item index, or -1.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_selected(gfx_object_t *obj, int32_t index);

/**
 * @brief Get selected item index.
 *
 * @param obj List object.
 * @return Selected item index, or -1 when no item is selected.
 */
int32_t gfx_list_get_selected(gfx_object_t *obj);

/**
 * @brief Confirm the current selected/focused item.
 *
 * If no item is selected, the focused item is selected and confirmed.
 *
 * @param obj List object.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_confirm(gfx_object_t *obj);

/**
 * @brief Set items per logical page.
 *
 * Use 0 to disable explicit paging and derive page size from visible rows.
 *
 * @param obj List object.
 * @param items_per_page Number of items in one page, or 0 for visible rows.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_items_per_page(gfx_object_t *obj, uint16_t items_per_page);

/**
 * @brief Get effective items per page.
 *
 * @param obj List object.
 * @return Items per page, derived from visible rows when configured as 0.
 */
uint16_t gfx_list_get_items_per_page(gfx_object_t *obj);

/**
 * @brief Set current page index and scroll to the page start.
 *
 * @param obj List object.
 * @param page_index Page index.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_page(gfx_object_t *obj, uint16_t page_index);

/**
 * @brief Move to the previous page when possible.
 *
 * @param obj List object.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_prev_page(gfx_object_t *obj);

/**
 * @brief Move to the next page when possible.
 *
 * @param obj List object.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_next_page(gfx_object_t *obj);

/**
 * @brief Get current page index.
 *
 * @param obj List object.
 * @return Current page index, or 0 when obj is invalid.
 */
uint16_t gfx_list_get_page(gfx_object_t *obj);

/**
 * @brief Get page count.
 *
 * @param obj List object.
 * @return Page count, or 0 when the list has no items.
 */
uint16_t gfx_list_get_page_count(gfx_object_t *obj);

/**
 * @brief Get the number of items in a list.
 *
 * @param obj List object.
 * @return Item count, or 0 when obj is invalid.
 */
uint16_t gfx_list_get_item_count(gfx_object_t *obj);

/**
 * @brief Get item text by index.
 *
 * @param obj List object.
 * @param index Item index.
 * @return Item text pointer owned by the list, or NULL if index is invalid.
 */
const char *gfx_list_get_item_text(gfx_object_t *obj, uint16_t index);

/**
 * @brief Set the font used to draw list items.
 *
 * @param obj List object.
 * @param font Font handle created by gfx_label_font_create(), or an LVGL font pointer.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_font(gfx_object_t *obj, gfx_font_t font);

/**
 * @brief Set fixed item height.
 *
 * @param obj List object.
 * @param height Item height in pixels.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_item_height(gfx_object_t *obj, uint16_t height);

/**
 * @brief Set item text padding.
 *
 * @param obj List object.
 * @param pad_x Horizontal text padding in pixels.
 * @param pad_y Vertical text padding in pixels.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_text_pad(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y);

/**
 * @brief Enable or disable snap-to-item after dragging.
 *
 * @param obj List object.
 * @param enable True to snap scroll offset to item boundaries.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_snap_to_item(gfx_object_t *obj, bool enable);

/**
 * @brief Set drag threshold in pixels.
 *
 * Small movements below this threshold are treated as clicks.
 *
 * @param obj List object.
 * @param threshold Drag threshold in pixels.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_drag_threshold(gfx_object_t *obj, uint16_t threshold);

/**
 * @brief Set the focus-change callback.
 *
 * @param obj List object.
 * @param cb Callback to invoke on focus change, or NULL to clear it.
 * @param user_data User data passed to cb.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_focus_cb(gfx_object_t *obj, gfx_list_focus_cb_t cb, void *user_data);

/**
 * @brief Set the selection callback.
 *
 * @param obj List object.
 * @param cb Callback to invoke on selection/confirmation, or NULL to clear it.
 * @param user_data User data passed to cb.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_select_cb(gfx_object_t *obj, gfx_list_select_cb_t cb, void *user_data);

/**
 * @brief Set the page-load callback.
 *
 * @param obj List object.
 * @param cb Callback to invoke when the current page changes, or NULL to clear it.
 * @param user_data User data passed to cb.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_page_load_cb(gfx_object_t *obj, gfx_list_page_load_cb_t cb, void *user_data);

/**
 * @brief Set the normal item background color.
 *
 * @param obj List object.
 * @param color Background color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the focused item background color.
 *
 * @param obj List object.
 * @param color Focused background color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_focus_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the selected item background color.
 *
 * @param obj List object.
 * @param color Selected background color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_selected_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the pressed item background color.
 *
 * @param obj List object.
 * @param color Pressed background color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_pressed_bg_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the normal item text color.
 *
 * @param obj List object.
 * @param color Text color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_text_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the focused item text color.
 *
 * @param obj List object.
 * @param color Focused text color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_focus_text_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the selected item text color.
 *
 * @param obj List object.
 * @param color Selected text color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_selected_text_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the pressed item text color.
 *
 * @param obj List object.
 * @param color Pressed text color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_pressed_text_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the list border color.
 *
 * @param obj List object.
 * @param color Border color.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_border_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the list border width.
 *
 * @param obj List object.
 * @param width Border width in pixels; 0 disables the border.
 * @return GFX_OK on success, GFX_ERR_* otherwise.
 */
gfx_err_t gfx_list_set_border_width(gfx_object_t *obj, uint16_t width);

#ifdef __cplusplus
}
#endif
