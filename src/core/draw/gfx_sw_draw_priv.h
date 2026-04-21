/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void gfx_sw_draw_point(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x, gfx_coord_t y,
                       gfx_color_t color, gfx_opa_t opa, bool swap);

void gfx_sw_draw_hline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x1, gfx_coord_t x2, gfx_coord_t y,
                       gfx_color_t color, gfx_opa_t opa, bool swap);

void gfx_sw_draw_vline(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                       gfx_coord_t x, gfx_coord_t y1, gfx_coord_t y2,
                       gfx_color_t color, gfx_opa_t opa, bool swap);

void gfx_sw_draw_rect_stroke(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                             const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                             const gfx_area_t *rect, uint16_t line_width,
                             gfx_color_t color, gfx_opa_t opa, bool swap);

#ifdef __cplusplus
}
#endif
