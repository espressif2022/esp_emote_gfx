/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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

/* Magic numbers for image headers */
#define C_ARRAY_HEADER_MAGIC    0x19

/**********************
 *      TYPEDEFS
 **********************/

/* Color format enumeration - simplified for public use */
typedef enum {
    GFX_COLOR_FORMAT_RGB565   = 0x04,  /**< RGB565 format without alpha channel */
    GFX_COLOR_FORMAT_RGB565A8 = 0x0A,  /**< RGB565 format with separate alpha channel */
} gfx_color_format_t;

typedef struct {
    uint32_t magic: 8;          /**< Magic number. Must be GFX_IMAGE_HEADER_MAGIC */
    uint32_t cf : 8;            /**< Color format: See `gfx_color_format_t` */
    uint32_t flags: 16;         /**< Image flags */
    uint32_t w: 16;             /**< Width of the image */
    uint32_t h: 16;             /**< Height of the image */
    uint32_t stride: 16;        /**< Number of bytes in a row */
    uint32_t reserved: 16;      /**< Reserved for future use */
} gfx_image_header_t;

/* Image descriptor structure - compatible with LVGL */
typedef struct {
    gfx_image_header_t header;   /**< A header describing the basics of the image */
    uint32_t data_size;         /**< Size of the image in bytes */
    const uint8_t *data;        /**< Pointer to the data of the image */
    const void *reserved;       /**< Reserved field for future use */
    const void *reserved_2;     /**< Reserved field for future use */
} gfx_image_dsc_t;

/**
 * @brief Public image source type.
 *
 * Use this enum together with `gfx_img_src_t` to describe where an image
 * payload comes from. The current implementation supports in-memory
 * `gfx_image_dsc_t` payloads and keeps room for future source types.
 */
typedef enum {
    GFX_IMG_SRC_TYPE_IMAGE_DSC = 0, /**< In-memory gfx_image_dsc_t payload */
} gfx_img_src_type_t;

/**
 * @brief Typed image source descriptor.
 *
 * `gfx_img_set_src_desc()` is the preferred API because it makes the source
 * type explicit. `gfx_img_set_src()` remains as a compatibility wrapper for
 * direct `gfx_image_dsc_t *` usage.
 */
typedef struct {
    gfx_img_src_type_t type;    /**< Source payload type */
    const void *data;           /**< Type-specific payload pointer */
} gfx_img_src_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create an image object on a display
 * @param disp Display from gfx_emote_add_disp(handle, &disp_cfg)
 * @return Pointer to the created image object, NULL on error
 */
gfx_obj_t *gfx_img_create(gfx_disp_t *disp);

/* Image setters */

/**
 * @brief Set the typed source descriptor for an image object
 *
 * This is the preferred source setter for new code. It keeps the public API
 * extensible when additional image source types are introduced.
 *
 * @param obj Pointer to the image object
 * @param src Pointer to the typed source descriptor
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_img_set_src_desc(gfx_obj_t *obj, const gfx_img_src_t *src);

/**
 * @brief Set the source data for an image object
 *
 * Compatibility wrapper for in-memory `gfx_image_dsc_t` payloads.
 *
 * @param obj Pointer to the image object
 * @param src Pointer to the image source data
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src);

#ifdef __cplusplus
}
#endif
