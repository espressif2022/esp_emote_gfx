/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_label.h"
#include "widget/gfx_font_priv.h"

/**********************
 *      TYPEDEFS
 **********************/
/* Label context structure */
typedef struct {
    /* Text properties */
    struct {
        char *text;                     /**< Text string */
        gfx_label_long_mode_t long_mode; /**< Long text handling mode */
        uint16_t line_spacing;          /**< Spacing between lines */
        int32_t text_width;     /**< Total text width */
    } text;

    /* Style properties */
    struct {
        gfx_color_t color;              /**< Text color */
        gfx_opa_t opa;                  /**< Text opacity */
        gfx_color_t bg_color;           /**< Background color */
        bool bg_enable;                 /**< Enable background */
        gfx_text_align_t text_align;    /**< Text alignment */
    } style;

    /* Font context */
    struct {
        gfx_font_ctx_t *font_ctx;       /**< Unified font context */
    } font;

    /* Render buffer */
    struct {
        gfx_opa_t *mask;                /**< Text mask buffer */
        int32_t offset;                 /**< Offset of the text */
    } render;

    /* Cached line data for scroll optimization */
    struct {
        char **lines;                   /**< Cached parsed lines */
        int line_count;                 /**< Number of cached lines */
        int *line_widths;               /**< Cached line widths for alignment */
    } cache;

    /* Scroll properties */
    struct {
        int32_t offset;          /**< Current scroll offset */
        int32_t step;            /**< Scroll step size per timer tick (default: 1) */
        uint32_t speed;          /**< Scroll speed in ms per pixel */
        bool loop;               /**< Enable continuous looping */
        bool scrolling;          /**< Is currently scrolling */
        bool changed;           /**< Scroll position changed */
        void *timer;            /**< Timer handle for scroll animation */
    } scroll;

    /* Snap scroll properties */
    struct {
        uint32_t interval;      /**< Snap interval time in ms (time to display each section) */
        int32_t offset;         /**< Snap offset in pixels (auto-calculated as obj->geometry.width) */
        bool loop;              /**< Enable continuous snap looping */
        void *timer;            /**< Timer handle for snap animation */
    } snap;
} gfx_label_t;

/* Function to clear cached lines - implemented in gfx_draw_label.c */
void gfx_label_clear_cached_lines(gfx_label_t *label);

/* Function to scroll timer callback - implemented in gfx_draw_label.c */
void gfx_label_scroll_timer_callback(void *arg);

/* Function to snap timer callback - implemented in gfx_draw_label.c */
void gfx_label_snap_timer_callback(void *arg);

/* Function to get glphy dsc - implemented in gfx_draw_label.c */
esp_err_t gfx_get_glphy_dsc(gfx_obj_t *obj);

/* Function to draw label - implemented in gfx_draw_label.c */
esp_err_t gfx_draw_label(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);
