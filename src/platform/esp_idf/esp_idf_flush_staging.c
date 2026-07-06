/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include <string.h>

#include "platform/esp_idf/esp_idf_flush_staging.h"

static uint8_t gfx_platform_flush_staging_pixel_size(gfx_color_format_t format)
{
    uint8_t px = gfx_color_format_get_size(format);

    return px > 0U ? px : 2U;
}

bool gfx_platform_flush_staging_copy_patch(const gfx_platform_flush_staging_patch_t *patch)
{
    uint8_t px;
    gfx_coord_t rect_w;
    gfx_coord_t rect_h;
    const uint8_t *src;
    uint8_t *dst;
    size_t copy_bytes;
    size_t src_stride_bytes;
    size_t dst_stride_bytes;

    if (patch == NULL || patch->staging_fb == NULL || patch->src == NULL ||
            patch->hor_res == 0U || patch->ver_res == 0U ||
            patch->x2 <= patch->x1 || patch->y2 <= patch->y1 || patch->src_stride_px <= 0) {
        return false;
    }

    px = gfx_platform_flush_staging_pixel_size(patch->format);
    rect_w = (gfx_coord_t)(patch->x2 - patch->x1);
    rect_h = (gfx_coord_t)(patch->y2 - patch->y1);

    /*
     * The source is a partition render buffer whose row-0 maps to screen row y1.
     * gfx_platform_async_fbcpy_region uses a symmetric (x,y) offset on BOTH
     * src and dst, which would over-index the small partition buffer.
     * Use a direct row-by-row copy instead: offset dst to (x1, y1) in the
     * full-screen staging FB, then copy contiguous rows from src[0..].
     */
    src = (const uint8_t *)patch->src;
    dst = (uint8_t *)patch->staging_fb;

    dst += ((size_t)patch->y1 * (size_t)patch->hor_res + (size_t)patch->x1) * px;

    copy_bytes      = (size_t)rect_w * px;
    src_stride_bytes = (size_t)patch->src_stride_px * px;
    dst_stride_bytes = (size_t)patch->hor_res * px;

    for (gfx_coord_t row = 0; row < rect_h; row++) {
        memcpy(dst, src, copy_bytes);
        src += src_stride_bytes;
        dst += dst_stride_bytes;
    }

    return true;
}
