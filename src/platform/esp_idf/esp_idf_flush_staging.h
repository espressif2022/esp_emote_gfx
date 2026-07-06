/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/gfx_types_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *staging_fb;
    uint32_t hor_res;
    uint32_t ver_res;
    gfx_color_format_t format;
    gfx_coord_t x1;
    gfx_coord_t y1;
    gfx_coord_t x2;
    gfx_coord_t y2;
    const void *src;
    gfx_coord_t src_stride_px;
} gfx_platform_flush_staging_patch_t;

bool gfx_platform_flush_staging_copy_patch(const gfx_platform_flush_staging_patch_t *patch);
bool gfx_platform_flush_staging_rotate_patch(const gfx_platform_flush_staging_patch_t *patch,
        int16_t rotation_cw);

#ifdef __cplusplus
}
#endif
