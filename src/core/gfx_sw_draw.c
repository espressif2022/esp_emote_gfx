/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stddef.h>

#include "core/gfx_blend_priv.h"
#include "core/gfx_sw_draw_priv.h"

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

    if (!gfx_sw_draw_get_pixel_ptr(&pixel, dest_buf, dest_stride, buf_area, clip_area, x, y)) {
        return;
    }

    if (opa >= 0xFF) {
        *pixel = color;
    } else if (opa > 0) {
        *pixel = gfx_blend_color_mix(color, *pixel, opa, swap);
    }
}

void gfx_sw_draw_hline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x1, gfx_coord_t x2, gfx_coord_t y,
                       gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_coord_t draw_x1;
    gfx_coord_t draw_x2;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || x2 <= x1) {
        return;
    }

    draw_x1 = (x1 > clip_area->x1) ? x1 : clip_area->x1;
    draw_x2 = (x2 < clip_area->x2) ? x2 : clip_area->x2;

    if (draw_x2 <= draw_x1 || y < clip_area->y1 || y >= clip_area->y2) {
        return;
    }

    for (gfx_coord_t x = draw_x1; x < draw_x2; ++x) {
        gfx_sw_draw_point(dest_buf, dest_stride, buf_area, clip_area, x, y, color, opa, swap);
    }
}

void gfx_sw_draw_vline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x, gfx_coord_t y1, gfx_coord_t y2,
                       gfx_color_t color, gfx_opa_t opa, bool swap)
{
    gfx_coord_t draw_y1;
    gfx_coord_t draw_y2;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || y2 <= y1) {
        return;
    }

    draw_y1 = (y1 > clip_area->y1) ? y1 : clip_area->y1;
    draw_y2 = (y2 < clip_area->y2) ? y2 : clip_area->y2;

    if (draw_y2 <= draw_y1 || x < clip_area->x1 || x >= clip_area->x2) {
        return;
    }

    for (gfx_coord_t y = draw_y1; y < draw_y2; ++y) {
        gfx_sw_draw_point(dest_buf, dest_stride, buf_area, clip_area, x, y, color, opa, swap);
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
