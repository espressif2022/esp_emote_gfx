/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stddef.h>

#include "render/sw/gfx_blend_priv.h"
#include "render/sw/gfx_sw_draw_priv.h"

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   PUBLIC FUNCTIONS
 **********************/
static bool gfx_sw_draw_clip_to_local(gfx_area_t *local_area,
                                      const gfx_area_t *buf_area,
                                      const gfx_area_t *clip_area,
                                      gfx_coord_t x1,
                                      gfx_coord_t y1,
                                      gfx_coord_t x2,
                                      gfx_coord_t y2)
{
    gfx_area_t draw_area;

    if (local_area == NULL || buf_area == NULL || clip_area == NULL || x2 <= x1 || y2 <= y1) {
        return false;
    }

    draw_area.x1 = x1 > clip_area->x1 ? x1 : clip_area->x1;
    draw_area.y1 = y1 > clip_area->y1 ? y1 : clip_area->y1;
    draw_area.x2 = x2 < clip_area->x2 ? x2 : clip_area->x2;
    draw_area.y2 = y2 < clip_area->y2 ? y2 : clip_area->y2;

    if (draw_area.x1 < buf_area->x1) {
        draw_area.x1 = buf_area->x1;
    }
    if (draw_area.y1 < buf_area->y1) {
        draw_area.y1 = buf_area->y1;
    }
    if (draw_area.x2 > buf_area->x2) {
        draw_area.x2 = buf_area->x2;
    }
    if (draw_area.y2 > buf_area->y2) {
        draw_area.y2 = buf_area->y2;
    }

    if (draw_area.x2 <= draw_area.x1 || draw_area.y2 <= draw_area.y1) {
        return false;
    }

    local_area->x1 = (gfx_coord_t)(draw_area.x1 - buf_area->x1);
    local_area->y1 = (gfx_coord_t)(draw_area.y1 - buf_area->y1);
    local_area->x2 = (gfx_coord_t)(draw_area.x2 - buf_area->x1);
    local_area->y2 = (gfx_coord_t)(draw_area.y2 - buf_area->y1);
    return true;
}

void gfx_sw_draw_point_fmt(void *dest_buf, gfx_coord_t dest_stride,
                           gfx_color_format_t dest_format,
                           const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                           gfx_coord_t x, gfx_coord_t y,
                           gfx_color_t color, gfx_opa_t opa)
{
    gfx_area_t local_area;

    if (dest_buf == NULL || opa == 0U) {
        return;
    }

    if (gfx_sw_draw_clip_to_local(&local_area, buf_area, clip_area, x, y,
                                  (gfx_coord_t)(x + 1), (gfx_coord_t)(y + 1))) {
        gfx_sw_blend_surface_fill(dest_buf, dest_stride, dest_format, &local_area, color, opa);
    }
}

void gfx_sw_draw_hline_fmt(void *dest_buf, gfx_coord_t dest_stride,
                           gfx_color_format_t dest_format,
                           const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                           gfx_coord_t x1, gfx_coord_t x2, gfx_coord_t y,
                           gfx_color_t color, gfx_opa_t opa)
{
    gfx_area_t local_area;

    if (dest_buf == NULL || opa == 0U || x2 <= x1) {
        return;
    }

    if (gfx_sw_draw_clip_to_local(&local_area, buf_area, clip_area, x1, y, x2, (gfx_coord_t)(y + 1))) {
        gfx_sw_blend_surface_fill(dest_buf, dest_stride, dest_format, &local_area, color, opa);
    }
}

void gfx_sw_draw_vline_fmt(void *dest_buf, gfx_coord_t dest_stride,
                           gfx_color_format_t dest_format,
                           const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                           gfx_coord_t x, gfx_coord_t y1, gfx_coord_t y2,
                           gfx_color_t color, gfx_opa_t opa)
{
    gfx_area_t local_area;

    if (dest_buf == NULL || opa == 0U || y2 <= y1) {
        return;
    }

    if (gfx_sw_draw_clip_to_local(&local_area, buf_area, clip_area, x, y1, (gfx_coord_t)(x + 1), y2)) {
        gfx_sw_blend_surface_fill(dest_buf, dest_stride, dest_format, &local_area, color, opa);
    }
}

void gfx_sw_draw_rect_stroke_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                 gfx_color_format_t dest_format,
                                 const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                 const gfx_area_t *rect, uint16_t line_width,
                                 gfx_color_t color, gfx_opa_t opa)
{
    gfx_coord_t max_line_w;
    gfx_coord_t max_line_h;
    gfx_coord_t stroke_w;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL || rect == NULL ||
            line_width == 0U || opa == 0U) {
        return;
    }

    if (rect->x2 <= rect->x1 || rect->y2 <= rect->y1) {
        return;
    }

    max_line_w = (gfx_coord_t)((line_width * 2U <= (uint16_t)(rect->x2 - rect->x1)) ?
                               line_width : ((rect->x2 - rect->x1) / 2));
    max_line_h = (gfx_coord_t)((line_width * 2U <= (uint16_t)(rect->y2 - rect->y1)) ?
                               line_width : ((rect->y2 - rect->y1) / 2));
    stroke_w = (max_line_w < max_line_h) ? max_line_w : max_line_h;

    for (gfx_coord_t i = 0; i < stroke_w; ++i) {
        gfx_sw_draw_hline_fmt(dest_buf, dest_stride, dest_format, buf_area, clip_area,
                              (gfx_coord_t)(rect->x1 + i), (gfx_coord_t)(rect->x2 - i),
                              (gfx_coord_t)(rect->y1 + i), color, opa);
        gfx_sw_draw_hline_fmt(dest_buf, dest_stride, dest_format, buf_area, clip_area,
                              (gfx_coord_t)(rect->x1 + i), (gfx_coord_t)(rect->x2 - i),
                              (gfx_coord_t)(rect->y2 - 1 - i), color, opa);
        gfx_sw_draw_vline_fmt(dest_buf, dest_stride, dest_format, buf_area, clip_area,
                              (gfx_coord_t)(rect->x1 + i), (gfx_coord_t)(rect->y1 + i),
                              (gfx_coord_t)(rect->y2 - i), color, opa);
        gfx_sw_draw_vline_fmt(dest_buf, dest_stride, dest_format, buf_area, clip_area,
                              (gfx_coord_t)(rect->x2 - 1 - i), (gfx_coord_t)(rect->y1 + i),
                              (gfx_coord_t)(rect->y2 - i), color, opa);
    }
}
