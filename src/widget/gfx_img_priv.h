/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_obj.h"
#include "widget/gfx_img.h"
#include "decoder/gfx_img_dec_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Internal drawing functions
 *====================*/

/**
 * @brief Draw an image object (internal)
 * @param obj Image object
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
 * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 * @param swap Whether to swap byte order
 */
void gfx_draw_img(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);

/*=====================
 * Internal object management
 *====================*/

/**
 * @brief Delete image-specific resources (internal)
 * @param obj Image object
 * @note This function only handles image-specific cleanup.
 *       The base object structure is freed by gfx_obj_delete().
 */
esp_err_t gfx_img_delete(gfx_obj_t *obj);

#ifdef __cplusplus
}
#endif
