/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_obj.h"

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
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * QR Code object creation
 *====================*/

/**
 * @brief Create a QR Code object on a display
 * @param disp Display from gfx_emote_add_disp(handle, &disp_cfg)
 * @return Pointer to the created QR Code object
 */
gfx_obj_t *gfx_qrcode_create(gfx_disp_t *disp);

/*=====================
 * QR Code setter functions
 *====================*/

/**
 * @brief Set the data/text for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param data Pointer to the null-terminated string to encode
 * @return ESP_OK on success, error code otherwise
 * @note The length is automatically calculated using strlen()
 */
esp_err_t gfx_qrcode_set_data(gfx_obj_t *obj, const char *data);

/**
 * @brief Set the size for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param size Size in pixels (both width and height)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_qrcode_set_size(gfx_obj_t *obj, uint16_t size);

/**
 * @brief Set the error correction level for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param ecc Error correction level
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_qrcode_set_ecc(gfx_obj_t *obj, gfx_qrcode_ecc_t ecc);

/**
 * @brief Set the foreground color for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param color Foreground color (QR modules color)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_qrcode_set_color(gfx_obj_t *obj, gfx_color_t color);

/**
 * @brief Set the background color for a QR Code object
 * @param obj Pointer to the QR Code object
 * @param bg_color Background color
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t gfx_qrcode_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color);

#ifdef __cplusplus
}
#endif
