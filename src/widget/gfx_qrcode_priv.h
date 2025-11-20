/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_obj.h"
#include "widget/gfx_qrcode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/* QR Code context structure */
typedef struct {
    char *text;                 /**< QR Code text/text */
    size_t text_len;            /**< Length of text */
    uint8_t *qr_modules;        /**< Scaled QR Code image buffer (RGB565 format) */
    int qr_size;                /**< QR Code modules size (from esp_qrcode) */
    int scaled_size;            /**< Scaled image size in pixels (qr_size * scale) */
    uint16_t display_size;      /**< Display size in pixels */
    gfx_qrcode_ecc_t ecc;       /**< Error correction level */
    gfx_color_t color;          /**< Foreground color (modules) */
    gfx_color_t bg_color;       /**< Background color */
    bool needs_update;          /**< Flag to indicate QR code needs regeneration */
} gfx_qrcode_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Internal drawing functions
 *====================*/

/**
 * @brief Draw a QR Code object (internal)
 * @param obj QR Code object
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
    * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 * @param swap Whether to swap byte order
 */
void gfx_draw_qrcode(gfx_obj_t *obj, int x1, int y1, int x2, int y2, const void *dest_buf, bool swap);

/*=====================
 * Internal object management
 *====================*/

/**
 * @brief Delete QR Code-specific resources (internal)
 * @param obj QR Code object
 * @note This function only handles QR Code-specific cleanup.
 *       The base object structure is freed by gfx_obj_delete().
 */
esp_err_t gfx_qrcode_delete(gfx_obj_t *obj);

#ifdef __cplusplus
}
#endif
