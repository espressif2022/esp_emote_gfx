/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_types.h"
#include "core/gfx_obj.h"
#include "core/gfx_timer.h"
#include "widget/gfx_label.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/* Label property structure */
typedef struct {
    void *face;             /**< Font face handle */
    char *text;             /**< Text string */
    uint8_t font_size;      /**< Font size */
    gfx_color_t color;      /**< Text color */
    gfx_opa_t opa;          /**< Text opacity */
    gfx_color_t bg_color;   /**< Background color */
    bool bg_enable;         /**< Enable background */
    bool bg_dirty;          /**< Background needs redraw (but not text reparse) */
    gfx_opa_t *mask;        /**< Text mask buffer */
    gfx_text_align_t text_align;  /**< Text alignment */
    gfx_label_long_mode_t long_mode; /**< Long text handling mode */
    uint16_t line_spacing;  /**< Spacing between lines in pixels */
    
    /* Cached line data for scroll optimization */
    char **cached_lines;    /**< Cached parsed lines */
    int cached_line_count;  /**< Number of cached lines */
    int *cached_line_widths; /**< Cached width of each line for alignment */
    
    /* Horizontal scroll properties (only used when long_mode is SCROLL) */
    int32_t scroll_offset;  /**< Current horizontal scroll offset */
    uint32_t scroll_speed_ms; /**< Scrolling speed in milliseconds per pixel */
    bool scroll_loop;       /**< Enable continuous looping */
    bool scroll_active;     /**< Is scrolling currently active */
    bool scroll_dirty;      /**< Scroll position changed (needs redraw but not reparse) */
    void *scroll_timer;     /**< Timer handle for scrolling animation */
    int32_t text_width;     /**< Actual text width for scroll calculation */
} gfx_label_property_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Internal drawing functions
 *====================*/

/**
 * @brief Draw a label object
 * @param obj Label object
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
 * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 * @param swap Whether to swap the color format
 */
esp_err_t gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);

/**
 * @brief Get glyph descriptor for label rendering
 * @param obj Label object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_get_glphy_dsc(gfx_obj_t * obj);

/**
 * @brief Get default font configuration
 * @param font Font handle pointer
 * @param size Font size pointer
 * @param color Font color pointer
 * @param opa Font opacity pointer
 */
void gfx_get_default_font_config(gfx_font_t *font, uint16_t *size, gfx_color_t *color, gfx_opa_t *opa);

#ifdef __cplusplus
}
#endif 