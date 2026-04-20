/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stddef.h>

#include "core/draw/gfx_blend_priv.h"
#include "core/draw/gfx_sw_draw_priv.h"

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool gfx_sw_draw_get_pixel_ptr(gfx_color_t **pixel,
                                      gfx_color_t *dest_buf,
                                      gfx_coord_t dest_stride,
                                      const gfx_area_t *buf_area,
                                      const gfx_area_t *clip_area,
                                      gfx_coord_t x,
                                      gfx_coord_t y)
{
    if (pixel == NULL || dest_buf == NULL || buf_area == NULL || clip_area == NULL) {
        return false;
    }

    if (x < clip_area->x1 || x >= clip_area->x2 || y < clip_area->y1 || y >= clip_area->y2) {
        return false;
    }

    if (x < buf_area->x1 || x >= buf_area->x2 || y < buf_area->y1 || y >= buf_area->y2) {
        return false;
    }

    *pixel = dest_buf + (size_t)(y - buf_area->y1) * dest_stride + (size_t)(x - buf_area->x1);
    return true;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_sw_draw_point(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x, gfx_coord_t y,
                       gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_color_t *pixel = NULL;
    gfx_color_t draw_color = color;

    if (!gfx_sw_draw_get_pixel_ptr(&pixel, dest_buf, dest_stride, buf_area, clip_area, x, y)) {
        return;
    }

    if (opa >= 0xFF) {
        draw_color.full = gfx_color_to_native_u16(draw_color, swap);
        *pixel = draw_color;
    } else if (opa > 0) {
        *pixel = gfx_blend_color_mix(color, *pixel, opa, swap);
    }
}

void gfx_sw_draw_hline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x1, gfx_coord_t x2, gfx_coord_t y,
                       gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_color_t draw_color = color;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || x2 <= x1 || opa == 0) {
        return;
    }

    /* Clip against both clip_area and buf_area in one pass */
    gfx_coord_t draw_x1 = (x1 > clip_area->x1) ? x1 : clip_area->x1;
    gfx_coord_t draw_x2 = (x2 < clip_area->x2) ? x2 : clip_area->x2;
    if (draw_x1 < buf_area->x1) {
        draw_x1 = buf_area->x1;
    }
    if (draw_x2 > buf_area->x2) {
        draw_x2 = buf_area->x2;
    }

    if (draw_x2 <= draw_x1) {
        return;
    }
    if (y < clip_area->y1 || y >= clip_area->y2 || y < buf_area->y1 || y >= buf_area->y2) {
        return;
    }

    gfx_color_t *pixel = dest_buf + (size_t)(y - buf_area->y1) * dest_stride
                         + (size_t)(draw_x1 - buf_area->x1);
    size_t count = (size_t)(draw_x2 - draw_x1);

    if (opa >= 0xFF) {
        draw_color.full = gfx_color_to_native_u16(draw_color, swap);
        gfx_sw_blend_fill((uint16_t *)pixel, draw_color.full, count);
    } else {
        for (size_t i = 0; i < count; ++i) {
            pixel[i] = gfx_blend_color_mix(color, pixel[i], opa, swap);
        }
    }
}

void gfx_sw_draw_vline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x, gfx_coord_t y1, gfx_coord_t y2,
                       gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_color_t draw_color = color;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || y2 <= y1 || opa == 0) {
        return;
    }

    if (x < clip_area->x1 || x >= clip_area->x2 || x < buf_area->x1 || x >= buf_area->x2) {
        return;
    }

    /* Clip against both clip_area and buf_area in one pass */
    gfx_coord_t draw_y1 = (y1 > clip_area->y1) ? y1 : clip_area->y1;
    gfx_coord_t draw_y2 = (y2 < clip_area->y2) ? y2 : clip_area->y2;
    if (draw_y1 < buf_area->y1) {
        draw_y1 = buf_area->y1;
    }
    if (draw_y2 > buf_area->y2) {
        draw_y2 = buf_area->y2;
    }

    if (draw_y2 <= draw_y1) {
        return;
    }

    gfx_color_t *pixel = dest_buf + (size_t)(draw_y1 - buf_area->y1) * dest_stride
                         + (size_t)(x - buf_area->x1);

    if (opa >= 0xFF) {
        draw_color.full = gfx_color_to_native_u16(draw_color, swap);
        for (gfx_coord_t row = draw_y1; row < draw_y2; ++row) {
            *pixel = draw_color;
            pixel += dest_stride;
        }
    } else {
        for (gfx_coord_t row = draw_y1; row < draw_y2; ++row) {
            *pixel = gfx_blend_color_mix(color, *pixel, opa, swap);
            pixel += dest_stride;
        }
    }
}

void gfx_sw_draw_rect_stroke(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                             const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                             const gfx_area_t *rect, uint16_t line_width,
                             gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_coord_t max_line_w;
    gfx_coord_t max_line_h;
    gfx_coord_t stroke_w;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || rect == NULL || line_width == 0) {
        return;
    }

    if (rect->x2 <= rect->x1 || rect->y2 <= rect->y1) {
        return;
    }

    max_line_w = (gfx_coord_t)((line_width * 2U <= (uint16_t)(rect->x2 - rect->x1)) ? line_width : ((rect->x2 - rect->x1) / 2));
    max_line_h = (gfx_coord_t)((line_width * 2U <= (uint16_t)(rect->y2 - rect->y1)) ? line_width : ((rect->y2 - rect->y1) / 2));
    stroke_w = (max_line_w < max_line_h) ? max_line_w : max_line_h;

    if (stroke_w <= 0) {
        return;
    }

    for (gfx_coord_t i = 0; i < stroke_w; ++i) {
        gfx_sw_draw_hline(dest_buf, dest_stride, buf_area, clip_area,
                          rect->x1 + i, rect->x2 - i, rect->y1 + i,
                          color, opa, swap);
        gfx_sw_draw_hline(dest_buf, dest_stride, buf_area, clip_area,
                          rect->x1 + i, rect->x2 - i, rect->y2 - 1 - i,
                          color, opa, swap);
        gfx_sw_draw_vline(dest_buf, dest_stride, buf_area, clip_area,
                          rect->x1 + i, rect->y1 + i, rect->y2 - i,
                          color, opa, swap);
        gfx_sw_draw_vline(dest_buf, dest_stride, buf_area, clip_area,
                          rect->x2 - 1 - i, rect->y1 + i, rect->y2 - i,
                          color, opa, swap);
    }
}
