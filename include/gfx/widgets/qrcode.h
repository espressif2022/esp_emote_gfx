/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "gfx/display.h"
#include "gfx/error.h"
#include "gfx/object.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**
 * QR Code error correction level
 */
typedef enum {
    GFX_QRCODE_ECC_LOW = 0,      /**< The QR Code can tolerate about 7% erroneous codewords */
    GFX_QRCODE_ECC_MEDIUM,       /**< The QR Code can tolerate about 15% erroneous codewords */
    GFX_QRCODE_ECC_QUARTILE,     /**< The QR Code can tolerate about 25% erroneous codewords */
    GFX_QRCODE_ECC_HIGH          /**< The QR Code can tolerate about 30% erroneous codewords */
} gfx_qrcode_ecc_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create a QR Code object on a display
 * @param disp Display from gfx_display_add()
 * @return Pointer to the created QR Code object, or NULL on failure
 */
gfx_object_t *gfx_qrcode_create(gfx_display_t *disp);

/* QR code setters */

/**
 * @brief Set the data/text for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param data Pointer to the null-terminated string to encode
 * @return GFX_OK on success, error code otherwise
 * @note The length is automatically calculated using strlen()
 */
gfx_err_t gfx_qrcode_set_data(gfx_object_t *obj, const char *data);

/**
 * @brief Set the size for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param size Size in pixels (both width and height)
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_qrcode_set_size(gfx_object_t *obj, uint16_t size);

/**
 * @brief Set the error correction level for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param ecc Error correction level
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_qrcode_set_ecc(gfx_object_t *obj, gfx_qrcode_ecc_t ecc);

/**
 * @brief Set the foreground color for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param color Foreground color (QR modules color)
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_qrcode_set_color(gfx_object_t *obj, gfx_color_t color);

/**
 * @brief Set the background color for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param bg_color Background color
 * @return GFX_OK on success, error code otherwise
 */
gfx_err_t gfx_qrcode_set_bg_color(gfx_object_t *obj, gfx_color_t bg_color);

#ifdef __cplusplus
}
#endif
