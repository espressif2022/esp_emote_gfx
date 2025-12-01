/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  QR Code handle used by the display function
  */
typedef const uint8_t *qrcode_wrapper_handle_t;

/**
  * @brief  QR Code configuration options
  */
typedef struct {
    void (*display_func)(qrcode_wrapper_handle_t qrcode, void *user_data);   /**< Function called for displaying the QR Code after encoding is complete */
    int max_qrcode_version;                             /**< Max QR Code Version to be used. Range: 2 - 40 */
    int qrcode_ecc_level;                               /**< Error Correction Level for QR Code */
    void *user_data;                                    /**< User data */
} qrcode_wrapper_config_t;

/**
  * @brief  Error Correction Level in a QR Code Symbol
  */
enum {
    QRCODE_WRAPPER_ECC_LOW,     /**< QR Code Error Tolerance of 7% */
    QRCODE_WRAPPER_ECC_MED,     /**< QR Code Error Tolerance of 15% */
    QRCODE_WRAPPER_ECC_QUART,   /**< QR Code Error Tolerance of 25% */
    QRCODE_WRAPPER_ECC_HIGH     /**< QR Code Error Tolerance of 30% */
};

/**
  * @brief  Encodes the given string into a QR Code and calls the display function
  *
  * @attention 1. Can successfully encode a UTF-8 string of up to 2953 bytes or an alphanumeric
  *               string of up to 4296 characters or any digit string of up to 7089 characters
  *
  * @param  cfg   Configuration used for QR Code encoding.
  * @param  text  String to encode into a QR Code.
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: Failed to encode string into a QR Code
  *    - ESP_ERR_NO_MEM: Failed to allocate buffer for given max_qrcode_version
  */
esp_err_t qrcode_wrapper_generate(qrcode_wrapper_config_t *cfg, const char *text);

/**
  * @brief  Displays QR Code on the console
  *
  * @param  qrcode  QR Code handle used by the display function.
  */
void qrcode_wrapper_print_console(qrcode_wrapper_handle_t qrcode, void *user_data);

/**
  * @brief  Returns the side length of the given QR Code
  *
  * @param  qrcode  QR Code handle used by the display function.
  *
  * @return
  *    - val[21, 177]: Side length of QR Code
  */
int qrcode_wrapper_get_size(qrcode_wrapper_handle_t qrcode);

/**
  * @brief  Returns the Pixel value for the given coordinates
  *         False indicates White and True indicates Black
  *
  * @attention 1. Coordinates for top left corner are (x=0, y=0)
  * @attention 2. For out of bound coordinates false (White) is returned
  *
  * @param  qrcode  QR Code handle used by the display function.
  * @param  x  X-Coordinate of QR Code module
  * @param  y  Y-Coordinate of QR Code module
  *
  * @return
  *    - true: (x, y) Pixel is Black
  *    - false: (x, y) Pixel is White
  */
bool qrcode_wrapper_get_module(qrcode_wrapper_handle_t qrcode, int x, int y);

#define QRCODE_WRAPPER_CONFIG_DEFAULT() (qrcode_wrapper_config_t) { \
    .display_func = qrcode_wrapper_print_console, \
    .max_qrcode_version = 10, \
    .qrcode_ecc_level = QRCODE_WRAPPER_ECC_LOW, \
    .user_data = NULL, \
}

#ifdef __cplusplus
}
#endif
