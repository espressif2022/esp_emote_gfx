/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "gfx/widgets/label.h"
#include "fonts/gfx_font_priv.h"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    struct {
        char *text;                      /**< Text string */
        gfx_label_long_mode_t long_mode; /**< Long text handling mode */
        uint16_t line_spacing;           /**< Spacing between lines */
        int32_t text_width;              /**< Total text width */
    } text;

    struct {
        gfx_color_t color;               /**< Text color */
        gfx_opa_t opa;                   /**< Text opacity */
        gfx_color_t bg_color;            /**< Background color */
        bool bg_enable;                  /**< Enable background */
        gfx_text_align_t text_align;     /**< Text alignment */
    } style;

    struct {
        gfx_font_t source;               /**< Font source configured by public setter */
        gfx_font_handle_t handle;        /**< Internal font adapter handle */
    } font;

    struct {
        gfx_opa_t *mask;                 /**< Text mask buffer */
        size_t mask_capacity;            /**< Allocated mask buffer size in bytes */
        gfx_color_t *color_mask;         /**< Per-pixel text colors when inline markers are used */
        size_t color_mask_capacity;      /**< Allocated color mask size in bytes */
        bool inline_color;               /**< True when text contains ##0xRRGGBB markers */
        int32_t offset;                  /**< Offset of the text */
    } render;

    struct {
        int32_t offset;                  /**< Current scroll offset */
        int32_t step;                    /**< Scroll step size per timer tick */
        uint32_t speed;                  /**< Scroll speed in ms per pixel */
        bool loop;                       /**< Enable continuous looping */
        bool scrolling;                  /**< Is currently scrolling */
        void *timer;                     /**< Timer handle for scroll animation */
    } scroll;

    struct {
        uint32_t interval;               /**< Snap interval time in ms */
        int32_t offset;                  /**< Snap offset in pixels */
        bool loop;                       /**< Enable continuous snap looping */
        void *timer;                     /**< Timer handle for snap animation */
    } snap;
} gfx_label_t;
