/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "gfx/fs.h"
#include "gfx/widgets/image.h"

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
    GFX_IMAGE_FORMAT_JPEG = 2,     /**< JPEG encoded byte stream */
} gfx_image_format_t;

typedef struct {
    gfx_image_src_t src;          /**< Typed image source descriptor */
    gfx_image_header_t header;  /**< Image header information */
    const uint8_t *data;        /**< Decoded/native image pixel data */
    uint32_t data_size;         /**< Size of decoded data */
    void *user_data;            /**< User data for decoder */
    gfx_fs_blob_t src_blob;     /**< Optional retained encoded file payload */
    bool retain_src_blob;       /**< Keep src_blob after info() for a later open() */
    void *decoder;              /**< Internal decoder owner for close routing */
} gfx_image_decoder_dsc_t;

typedef struct gfx_image_decoder_t gfx_image_decoder_t;

struct gfx_image_decoder_t {
    const char *name;           /**< Decoder name */
    gfx_err_t (*info_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
    gfx_err_t (*open_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
    void (*close_cb)(gfx_image_decoder_t *decoder, gfx_image_decoder_dsc_t *dsc);
};

/**********************
 *   INTERNAL API
 **********************/

gfx_image_format_t gfx_image_detect_format(const void *src);
gfx_err_t gfx_image_decoder_register(gfx_image_decoder_t *decoder);
gfx_err_t gfx_image_validate_dsc(const gfx_image_dsc_t *image_desc);
gfx_err_t gfx_image_decoder_info(gfx_image_decoder_dsc_t *dsc, gfx_image_header_t *header);
gfx_err_t gfx_image_decoder_open(gfx_image_decoder_dsc_t *dsc);
void gfx_image_decoder_close(gfx_image_decoder_dsc_t *dsc);
gfx_err_t gfx_image_decoder_init(void);
gfx_err_t gfx_image_decoder_deinit(void);

#ifdef __cplusplus
}
#endif
