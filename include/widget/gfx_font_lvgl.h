/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *   PUBLIC API
 **********************/

/*
* The following code (gfx_font_lv_load_from_binary and gfx_font_lv_delete)
* is derived from 78/xiaozhi-fonts project.
* Original source: https://github.com/78/xiaozhi-fonts
*/

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
}
#endif
