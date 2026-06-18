/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
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

/* Magic number for gfx_image_header_t. */
#define GFX_IMAGE_HEADER_MAGIC  0x19

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint32_t magic: 8;          /**< Magic number. Must be GFX_IMAGE_HEADER_MAGIC */
    uint32_t cf : 8;            /**< Color format: See `gfx_color_format_t` */
    uint32_t flags: 16;         /**< Reserved image flags */
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
 * Use this enum together with `gfx_image_src_t` to describe where an image
 * payload comes from. The current implementation supports in-memory
 * `gfx_image_dsc_t` payloads and keeps room for future source types.
 */
typedef enum {
    GFX_IMAGE_SRC_TYPE_IMAGE_DSC = 0, /**< In-memory gfx_image_dsc_t payload */
    GFX_IMAGE_SRC_TYPE_MEMORY,        /**< In-memory encoded image bytes */
    GFX_IMAGE_SRC_TYPE_FILE,          /**< File path encoded image source */
} gfx_image_src_type_t;

/**
 * @brief Typed image source descriptor.
 *
 * `gfx_image_set_source_desc()` makes the source type explicit so additional image
 * source types can be introduced without changing the setter shape.
 */
typedef struct {
    gfx_image_src_type_t type;    /**< Source payload type */
    const void *data;           /**< Type-specific payload pointer */
    size_t data_len;            /**< Type-specific payload length, required for memory sources */
} gfx_image_src_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create an image object on a display
 * @param disp Display from gfx_display_add()
 * @return Pointer to the created image object, NULL on error
 */
gfx_object_t *gfx_image_create(gfx_display_t *disp);

/* Image setters */

/**
 * @brief Set the typed source descriptor for an image object
 *
 * This is the preferred source setter for new code. It keeps the public API
 * extensible when additional image source types are introduced.
 *
 * @param obj Pointer to the image object
 * @param src Pointer to the typed source descriptor
 * @return GFX_OK on success, GFX_ERR_* otherwise
 */
gfx_err_t gfx_image_set_source_desc(gfx_object_t *obj, const gfx_image_src_t *src);

#ifdef __cplusplus
}
#endif
