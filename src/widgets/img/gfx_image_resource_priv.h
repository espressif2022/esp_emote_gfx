/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "codecs/image/gfx_image_decoder_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gfx_image_src_t src;
    gfx_image_header_t header;
    gfx_image_decoder_dsc_t decoder;
} gfx_image_resource_t;

gfx_err_t gfx_image_resource_set_source(gfx_image_resource_t *resource, const gfx_image_src_t *src);
gfx_err_t gfx_image_resource_open(gfx_image_resource_t *resource);
void gfx_image_resource_close(gfx_image_resource_t *resource);
bool gfx_image_resource_is_open(const gfx_image_resource_t *resource);
uint8_t gfx_image_resource_pixel_size(const gfx_image_resource_t *resource);
gfx_coord_t gfx_image_resource_stride_px(const gfx_image_resource_t *resource);
gfx_coord_t gfx_image_resource_height(const gfx_image_resource_t *resource);
gfx_color_format_t gfx_image_resource_format(const gfx_image_resource_t *resource);
const uint8_t *gfx_image_resource_pixels(const gfx_image_resource_t *resource);
const gfx_opa_t *gfx_image_resource_alpha(const gfx_image_resource_t *resource);
gfx_coord_t gfx_image_resource_alpha_stride(const gfx_image_resource_t *resource);

#ifdef __cplusplus
}
#endif
