/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "widget/gfx_img.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    GFX_IMAGE_FORMAT_UNKNOWN = 0,  /**< Unknown format */
    GFX_IMAGE_FORMAT_C_ARRAY = 1,  /**< C array format */
} gfx_image_format_t;

typedef struct {
    gfx_img_src_t src;          /**< Typed image source descriptor */
    gfx_image_header_t header;  /**< Image header information */
    const uint8_t *data;        /**< Decoded/native image pixel data */
    uint32_t data_size;         /**< Size of decoded data */
    void *user_data;            /**< User data for decoder */
} gfx_image_decoder_dsc_t;

typedef struct gfx_image_decoder_t gfx_image_decoder_t;

struct gfx_image_decoder_t {
    const char *name;           /**< Decoder name */
    esp_err_t (*info_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
    esp_err_t (*open_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
    void (*close_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
};

/**********************
 *   INTERNAL API
 **********************/

gfx_image_format_t gfx_image_detect_format(const void *src);
esp_err_t gfx_image_decoder_register(gfx_image_decoder_t *decoder);
esp_err_t gfx_image_decoder_info(gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
esp_err_t gfx_image_decoder_open(gfx_image_decoder_dsc_t *dsc);
void gfx_image_decoder_close(gfx_image_decoder_dsc_t *dsc);
esp_err_t gfx_image_decoder_init(void);
esp_err_t gfx_image_decoder_deinit(void);

#ifdef __cplusplus
}
#endif
