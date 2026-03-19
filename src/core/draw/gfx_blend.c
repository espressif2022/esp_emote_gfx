/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

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



static int32_t gfx_sw_blend_isqrt(int32_t n)
{
    int32_t x, y;
    if (n <= 1) {
        return n;
    }
    x = n;
    y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
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
    /*
     * Fixed-point incremental edge-walking rasterizer.
     *
     * Instead of computing 3 edge functions + 2 float divides per pixel,
     * we pre-compute linear step constants and walk the edge values with
     * pure integer adds in the inner loop.
     *
     * Fractional precision: 16 bits (sufficient for sub-1024px coordinates).
     */
#define FRAC_BITS  16
#define FRAC_ONE   (1 << FRAC_BITS)
#define FRAC_HALF  (1 << (FRAC_BITS - 1))

    int32_t area_2x;
    int32_t min_x, min_y, max_x, max_y;

    /* Edge function step constants (integer, not shifted yet) */
    int32_t e0_a, e0_b, e1_a, e1_b;

    /* Edge anti-aliasing: inverse edge lengths (Q6 scaled by 256) */
    int32_t e0_inv, e1_inv, e2_inv;

    /* UV interpolation gradients (fixed-point) */
    int32_t du_dx, dv_dx, du_dy, dv_dy;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            src_buf == NULL || v0 == NULL || v1 == NULL || v2 == NULL ||
            src_stride <= 0 || src_height <= 0) {
        return;
    }

    /*
     * Compute 2× signed area of the triangle (integer).
     * Must match the edge function convention: E0 + E1 + E2 = area_2x.
     * area_2x = edge_func(v0, v1, v2) = (v2-v0) × (v1-v0) z-component
     */
    area_2x = (int32_t)(v2->x - v0->x) * (int32_t)(v1->y - v0->y)
            - (int32_t)(v2->y - v0->y) * (int32_t)(v1->x - v0->x);

    if (area_2x == 0) {
        return; /* Degenerate triangle */
    }

    /* Bounding box (expanded by 1 pixel for edge anti-aliasing) */
    min_x = MIN(v0->x, MIN(v1->x, v2->x)) - 1;
    min_y = MIN(v0->y, MIN(v1->y, v2->y)) - 1;
    max_x = MAX(v0->x, MAX(v1->x, v2->x)) + 1;
    max_y = MAX(v0->y, MAX(v1->y, v2->y)) + 1;

    /* Clip to buffer and clip area */
    min_x = MAX(min_x, MAX(buf_area->x1, clip_area->x1));
    min_y = MAX(min_y, MAX(buf_area->y1, clip_area->y1));
    max_x = MIN(max_x, MIN(buf_area->x2 - 1, clip_area->x2 - 1));
    max_y = MIN(max_y, MIN(buf_area->y2 - 1, clip_area->y2 - 1));

    if (min_x > max_x || min_y > max_y) {
        return;
    }

    /*
     * Edge function for w0 (weight of v0):
     *   E0(x,y) = (x - v1x)*(v2y - v1y) - (y - v1y)*(v2x - v1x)
     *   dE0/dx = (v2y - v1y) = e0_a
     *   dE0/dy = (v1x - v2x) = e0_b  (note: negated x-diff)
     *
     * Edge function for w1 (weight of v1):
     *   E1(x,y) = (x - v2x)*(v0y - v2y) - (y - v2y)*(v0x - v2x)
     *   dE1/dx = (v0y - v2y) = e1_a
     *   dE1/dy = (v2x - v0x) = e1_b
     */
    e0_a = (int32_t)(v2->y - v1->y);
    e0_b = (int32_t)(v1->x - v2->x);
    e1_a = (int32_t)(v0->y - v2->y);
    e1_b = (int32_t)(v2->x - v0->x);

    {
        int32_t e2_a = -(e0_a + e1_a);
        int32_t e2_b = -(e0_b + e1_b);
        int32_t e0_len = gfx_sw_blend_isqrt(e0_a * e0_a + e0_b * e0_b);
        int32_t e1_len = gfx_sw_blend_isqrt(e1_a * e1_a + e1_b * e1_b);
        int32_t e2_len = gfx_sw_blend_isqrt(e2_a * e2_a + e2_b * e2_b);
        if (e0_len < 1) { e0_len = 1; }
        if (e1_len < 1) { e1_len = 1; }
        if (e2_len < 1) { e2_len = 1; }
        e0_inv = (256 * 64 + e0_len / 2) / e0_len;
        e1_inv = (256 * 64 + e1_len / 2) / e1_len;
        e2_inv = (256 * 64 + e2_len / 2) / e2_len;
    }

    /*
     * UV gradient computation (fixed-point).
     *
     * u(x,y) = w0*u0 + w1*u1 + w2*u2  where w_i = E_i / area_2x
     *
     * du/dx = (e0_a * u0 + e1_a * u1 + (-e0_a - e1_a) * u2) / area_2x
     *       = (e0_a * (u0 - u2) + e1_a * (u1 - u2)) / area_2x
     *
     * We compute in fixed-point by shifting numerator up by FRAC_BITS.
     */
    {
        int64_t num;

        num = (int64_t)e0_a * (v0->u - v2->u) + (int64_t)e1_a * (v1->u - v2->u);
        du_dx = (int32_t)((num << FRAC_BITS) / area_2x);

        num = (int64_t)e0_a * (v0->v - v2->v) + (int64_t)e1_a * (v1->v - v2->v);
        dv_dx = (int32_t)((num << FRAC_BITS) / area_2x);

        num = (int64_t)e0_b * (v0->u - v2->u) + (int64_t)e1_b * (v1->u - v2->u);
        du_dy = (int32_t)((num << FRAC_BITS) / area_2x);

        num = (int64_t)e0_b * (v0->v - v2->v) + (int64_t)e1_b * (v1->v - v2->v);
        dv_dy = (int32_t)((num << FRAC_BITS) / area_2x);
    }

    /*
     * Compute edge function values at the starting point (min_x, min_y).
     */
    {
        int32_t w0_row = (int32_t)(min_x - v1->x) * e0_a + (int32_t)(min_y - v1->y) * e0_b;
        int32_t w1_row = (int32_t)(min_x - v2->x) * e1_a + (int32_t)(min_y - v2->y) * e1_b;

        /* UV at starting point (fixed-point) */
        int64_t u_start_num = (int64_t)w0_row * v0->u + (int64_t)w1_row * v1->u
                            + (int64_t)(area_2x - w0_row - w1_row) * v2->u;
        int64_t v_start_num = (int64_t)w0_row * v0->v + (int64_t)w1_row * v1->v
                            + (int64_t)(area_2x - w0_row - w1_row) * v2->v;
        int32_t u_row = (int32_t)((u_start_num << FRAC_BITS) / area_2x);
        int32_t v_row = (int32_t)((v_start_num << FRAC_BITS) / area_2x);

        for (int32_t y = min_y; y <= max_y; y++) {
            int32_t w0 = w0_row;
            int32_t w1 = w1_row;
            int32_t u_cur = u_row;
            int32_t v_cur = v_row;
            gfx_color_t *dst_row = dest_buf + (size_t)(y - buf_area->y1) * dest_stride
                                 + (size_t)(min_x - buf_area->x1);

            for (int32_t x = min_x; x <= max_x; x++) {
                int32_t w2 = area_2x - w0 - w1;

                if (area_2x > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                : (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                    /* Inside triangle — full coverage */
                    int32_t src_x = (u_cur + FRAC_HALF) >> FRAC_BITS;
                    int32_t src_y = (v_cur + FRAC_HALF) >> FRAC_BITS;

                    src_x = gfx_sw_blend_clamp_coord(src_x, 0, src_stride - 1);
                    src_y = gfx_sw_blend_clamp_coord(src_y, 0, src_height - 1);

                    gfx_color_t src_color = src_buf[(size_t)src_y * src_stride + (size_t)src_x];

                    if (mask != NULL) {
                        gfx_opa_t src_opa = mask[(size_t)src_y * mask_stride + (size_t)src_x];
                        if (src_opa == 0U) {
                            goto next_pixel;
                        }
                        if (src_opa >= OPA_COVER) {
                            *dst_row = src_color;
                        } else {
                            *dst_row = gfx_blend_color_mix(src_color, *dst_row, src_opa, swap);
                        }
                    } else {
                        *dst_row = src_color;
                    }
                } else {
                    /* Outside triangle — edge AA: blend if within 1 px */
                    int32_t max_od = 0;
                    if (area_2x > 0) {
                        if (w0 < 0) { int32_t d = ((-w0) * e0_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                        if (w1 < 0) { int32_t d = ((-w1) * e1_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                        if (w2 < 0) { int32_t d = ((-w2) * e2_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                    } else {
                        if (w0 > 0) { int32_t d = (w0 * e0_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                        if (w1 > 0) { int32_t d = (w1 * e1_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                        if (w2 > 0) { int32_t d = (w2 * e2_inv + 32) >> 6; if (d > max_od) { max_od = d; } }
                    }

                    if (max_od > 0 && max_od < 256) {
                        gfx_opa_t aa_opa = (gfx_opa_t)(256 - max_od);
                        int32_t src_x = (u_cur + FRAC_HALF) >> FRAC_BITS;
                        int32_t src_y = (v_cur + FRAC_HALF) >> FRAC_BITS;
                        src_x = gfx_sw_blend_clamp_coord(src_x, 0, src_stride - 1);
                        src_y = gfx_sw_blend_clamp_coord(src_y, 0, src_height - 1);
                        gfx_color_t src_color = src_buf[(size_t)src_y * src_stride + (size_t)src_x];
                        if (mask != NULL) {
                            gfx_opa_t src_opa = mask[(size_t)src_y * mask_stride + (size_t)src_x];
                            aa_opa = (gfx_opa_t)(((uint32_t)aa_opa * src_opa + 128) >> 8);
                        }
                        if (aa_opa > 0) {
                            *dst_row = gfx_blend_color_mix(src_color, *dst_row, aa_opa, swap);
                        }
                    }
                }

next_pixel:
                w0 += e0_a;
                w1 += e1_a;
                u_cur += du_dx;
                v_cur += dv_dx;
                dst_row++;
            }

            w0_row += e0_b;
            w1_row += e1_b;
            u_row += du_dy;
            v_row += dv_dy;
        }
    }

#undef FRAC_BITS
#undef FRAC_ONE
#undef FRAC_HALF
}
