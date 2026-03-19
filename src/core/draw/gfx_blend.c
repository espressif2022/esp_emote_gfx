/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"

#include "common/gfx_comm.h"
#include "core/draw/gfx_blend_priv.h"

/*********************
 *      DEFINES
 *********************/

#define OPA_MAX      253  /*Opacities above this will fully cover*/
#define OPA_TRANSP   0
#define OPA_COVER    0xFF

#define FILL_NORMAL_MASK_PX(color, swap)                              \
    if(*mask == OPA_COVER) *dest_buf = color;                \
    else *dest_buf = gfx_blend_color_mix(color, *dest_buf, *mask, swap);     \
    mask++;                                                     \
    dest_buf++;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline float gfx_sw_blend_edge_func(float ax, float ay, float bx, float by, float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static inline int32_t gfx_sw_blend_clamp_coord(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_color_t gfx_blend_color_mix(gfx_color_t c1, gfx_color_t c2, uint8_t mix, bool swap)
{
    gfx_color_t ret;

    if (swap) {
        c1.full = c1.full << 8 | c1.full >> 8;
        c2.full = c2.full << 8 | c2.full >> 8;
    }
    /*Source: https://stackoverflow.com/a/50012418/1999969*/
    mix = (uint32_t)((uint32_t)mix + 4) >> 3;
    uint32_t bg = (uint32_t)((uint32_t)c2.full | ((uint32_t)c2.full << 16)) &
                  0x7E0F81F; /*0b00000111111000001111100000011111*/
    uint32_t fg = (uint32_t)((uint32_t)c1.full | ((uint32_t)c1.full << 16)) & 0x7E0F81F;
    uint32_t result = ((((fg - bg) * mix) >> 5) + bg) & 0x7E0F81F;
    ret.full = (uint16_t)((result >> 16) | result);
    if (swap) {
        ret.full = ret.full << 8 | ret.full >> 8;
    }

    return ret;
}

void gfx_sw_blend_fill(uint16_t *buf, uint16_t color, size_t pixels)
{
    if ((color & 0xFF) == (color >> 8)) {
        memset(buf, color & 0xFF, pixels * sizeof(uint16_t));
    } else {
        uint32_t color32 = (color << 16) | color;
        uint32_t *buf32 = (uint32_t *)buf;
        size_t pixels_half = pixels / 2;

        for (size_t i = 0; i < pixels_half; i++) {
            buf32[i] = color32;
        }

        if (pixels & 1) {
            buf[pixels - 1] = color;
        }
    }
}

void gfx_sw_blend_fill_area(uint16_t *dest_buf, gfx_coord_t dest_stride,
                            const gfx_area_t *area, uint16_t color)
{
    if (dest_buf == NULL || area == NULL) {
        return;
    }
    int32_t w = area->x2 - area->x1;
    int32_t h = area->y2 - area->y1;
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int32_t y = area->y1; y < area->y2; y++) {
        uint16_t *row = dest_buf + (size_t)y * dest_stride + area->x1;
        gfx_sw_blend_fill(row, color, (size_t)w);
    }
}

void gfx_sw_blend_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_opa_t *mask, gfx_coord_t mask_stride,
                       gfx_area_t *clip_area, gfx_color_t color, gfx_opa_t opa, bool swap)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;

    int32_t x, y;
    uint32_t c32 = color.full + ((uint32_t)color.full << 16);

    /*Only the mask matters*/
    if (opa >= OPA_MAX) {
        int32_t x_end4 = w - 4;

        for (y = 0; y < h; y++) {
            for (x = 0; x < w && ((unsigned int)(mask) & 0x3); x++) {
                FILL_NORMAL_MASK_PX(color, swap)
            }

            for (; x <= x_end4; x += 4) {
                uint32_t mask32 = *((uint32_t *)mask);
                if (mask32 == 0xFFFFFFFF) {
                    if ((unsigned int)dest_buf & 0x3) {/*dest_buf is not 4-byte aligned*/
                        dest_buf[0] = color;
                        ((uint32_t *)(dest_buf + 1))[0] = c32;
                        dest_buf[3] = color;
                    } else {
                        uint32_t *d32 = (uint32_t *)dest_buf;
                        d32[0] = c32;
                        d32[1] = c32;
                    }
                    dest_buf += 4;
                    mask += 4;
                } else if (mask32) {
                    FILL_NORMAL_MASK_PX(color, swap)
                    FILL_NORMAL_MASK_PX(color, swap)
                    FILL_NORMAL_MASK_PX(color, swap)
                    FILL_NORMAL_MASK_PX(color, swap)
                } else { //transparent
                    mask += 4;
                    dest_buf += 4;
                }
            }

            for (; x < w ; x++) {
                FILL_NORMAL_MASK_PX(color, swap)
            }
            dest_buf += (dest_stride - w);
            mask += (mask_stride - w);
        }
    } else { /*With opacity*/
        /*Buffer the result color to avoid recalculating the same color*/
        gfx_color_t last_dest_color;
        gfx_color_t last_res_color;
        gfx_opa_t last_mask = OPA_TRANSP;
        last_dest_color.full = dest_buf[0].full;
        last_res_color.full = dest_buf[0].full;
        gfx_opa_t opa_tmp = OPA_TRANSP;

        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                if (*mask) {
                    if (*mask != last_mask) {
                        opa_tmp = (*mask == OPA_COVER) ? opa : ((uint32_t)((uint32_t)(*mask) * opa) >> 8);
                    }
                    if (*mask != last_mask || last_dest_color.full != dest_buf[x].full) {
                        if (opa_tmp == OPA_COVER) {
                            last_res_color = color;
                        } else {
                            last_res_color = gfx_blend_color_mix(color, dest_buf[x], opa_tmp, swap);
                        }
                        last_mask = *mask;
                        last_dest_color.full = dest_buf[x].full;
                    }
                    dest_buf[x] = last_res_color;
                }
                mask++;
            }
            dest_buf += dest_stride;
            mask += (mask_stride - w);
        }
    }
}

void gfx_sw_blend_img_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                           const gfx_color_t *src_buf, gfx_coord_t src_stride,
                           const gfx_opa_t *mask, gfx_coord_t mask_stride,
                           gfx_area_t *clip_area, bool swap)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;

    int32_t x, y;

    if (mask == NULL) {
        size_t row_bytes = (size_t)w * sizeof(gfx_color_t);
        for (y = 0; y < h; y++) {
            memcpy(dest_buf, src_buf, row_bytes);
            dest_buf += dest_stride;
            src_buf += src_stride;
        }
        return;
    }

    // Slow path: with mask
    gfx_color_t last_dest_color;
    gfx_color_t last_res_color;
    gfx_color_t last_src_color;
    gfx_opa_t last_mask = OPA_TRANSP;
    last_dest_color.full = dest_buf[0].full;
    last_res_color.full = dest_buf[0].full;
    last_src_color.full = src_buf[0].full;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (mask == NULL || *mask) {
                if (mask == NULL || *mask != last_mask || last_dest_color.full != dest_buf[x].full || last_src_color.full != src_buf[x].full) {
                    if (mask == NULL || *mask == OPA_COVER) {
                        last_res_color = src_buf[x];
                    } else {
                        last_res_color = gfx_blend_color_mix(src_buf[x], dest_buf[x], *mask, swap);
                    }
                    if (mask) {
                        last_mask = *mask;
                    }
                    last_dest_color.full = dest_buf[x].full;
                    last_src_color.full = src_buf[x].full;
                }
                dest_buf[x] = last_res_color;
            }
            if (mask) {
                mask++;
            }
        }
        dest_buf += dest_stride;
        src_buf += src_stride;
        if (mask) {
            mask += (mask_stride - w);
        }
    }
}

void gfx_sw_blend_img_triangle_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                    const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                    const gfx_color_t *src_buf, gfx_coord_t src_stride, gfx_coord_t src_height,
                                    const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                    const gfx_sw_blend_img_vertex_t *v0,
                                    const gfx_sw_blend_img_vertex_t *v1,
                                    const gfx_sw_blend_img_vertex_t *v2,
                                    bool swap)
{
    float area;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            src_buf == NULL || v0 == NULL || v1 == NULL || v2 == NULL ||
            src_stride <= 0 || src_height <= 0) {
        return;
    }

    area = gfx_sw_blend_edge_func((float)v0->x, (float)v0->y,
                                  (float)v1->x, (float)v1->y,
                                  (float)v2->x, (float)v2->y);
    if (fabsf(area) < 0.0001f) {
        return;
    }

    min_x = MIN(v0->x, MIN(v1->x, v2->x));
    min_y = MIN(v0->y, MIN(v1->y, v2->y));
    max_x = MAX(v0->x, MAX(v1->x, v2->x));
    max_y = MAX(v0->y, MAX(v1->y, v2->y));

    min_x = MAX(min_x, MAX(buf_area->x1, clip_area->x1));
    min_y = MAX(min_y, MAX(buf_area->y1, clip_area->y1));
    max_x = MIN(max_x, MIN(buf_area->x2 - 1, clip_area->x2 - 1));
    max_y = MIN(max_y, MIN(buf_area->y2 - 1, clip_area->y2 - 1));

    if (min_x > max_x || min_y > max_y) {
        return;
    }

    for (int32_t y = min_y; y <= max_y; y++) {
        for (int32_t x = min_x; x <= max_x; x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = gfx_sw_blend_edge_func((float)v1->x, (float)v1->y,
                                              (float)v2->x, (float)v2->y,
                                              px, py) / area;
            float w1 = gfx_sw_blend_edge_func((float)v2->x, (float)v2->y,
                                              (float)v0->x, (float)v0->y,
                                              px, py) / area;
            float w2 = 1.0f - w0 - w1;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            int32_t src_x = (int32_t)lroundf(w0 * (float)v0->u + w1 * (float)v1->u + w2 * (float)v2->u);
            int32_t src_y = (int32_t)lroundf(w0 * (float)v0->v + w1 * (float)v1->v + w2 * (float)v2->v);
            gfx_color_t *dst_pixel;
            gfx_color_t src_color;

            src_x = gfx_sw_blend_clamp_coord(src_x, 0, src_stride - 1);
            src_y = gfx_sw_blend_clamp_coord(src_y, 0, src_height - 1);

            dst_pixel = dest_buf + (size_t)(y - buf_area->y1) * dest_stride + (size_t)(x - buf_area->x1);
            src_color = src_buf[(size_t)src_y * src_stride + (size_t)src_x];

            if (mask != NULL) {
                gfx_opa_t src_opa = mask[(size_t)src_y * mask_stride + (size_t)src_x];
                if (src_opa == 0U) {
                    continue;
                }
                if (src_opa >= OPA_COVER) {
                    *dst_pixel = src_color;
                } else {
                    *dst_pixel = gfx_blend_color_mix(src_color, *dst_pixel, src_opa, swap);
                }
            } else {
                *dst_pixel = src_color;
            }
        }
    }
}
