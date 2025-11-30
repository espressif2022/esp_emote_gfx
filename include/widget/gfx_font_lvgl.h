/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file gfx_font_lvgl.h
 * @brief LVGL Font Compatibility Layer
 *
 * This header provides LVGL font structure definitions and compatibility
 * functions for the ESP Graphics Framework.
 */

#ifndef GFX_FONT_LV_PARSER_H
#define GFX_FONT_LV_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lvgl.h"

/**********************
 *   BINARY FONT CREATION FUNCTIONS
 **********************/

/**
 * @brief Load an LVGL font from binary data
 * @param bin_addr Pointer to binary data containing lv_font_t structure
 * @return Pointer to loaded lv_font_t, or NULL on failure
 */
lv_font_t *gfx_font_lv_load_from_binary(uint8_t *bin_addr);

/**
 * @brief Delete an LVGL font created from binary data
 * @param font Pointer to lv_font_t to delete
 */
void gfx_font_lv_delete(lv_font_t *font);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*GFX_FONT_LV_PARSER_H*/
