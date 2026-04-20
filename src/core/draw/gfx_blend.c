/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#ifndef GFX_HAVE_LIBTESS2
#define GFX_HAVE_LIBTESS2 0
#endif

#if GFX_HAVE_LIBTESS2
#include "tesselator.h"
#endif

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
static gfx_blend_perf_stats_t *s_active_perf_stats = NULL;

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline uint64_t gfx_blend_perf_elapsed_us(int64_t start_us)
{
    return (uint64_t)(esp_timer_get_time() - start_us);
}

#if GFX_HAVE_LIBTESS2
static inline int32_t gfx_sw_blend_round_tess_real_to_q8(TESSreal value)
{
    if (value >= 0.0f) {
        return (int32_t)(value + 0.5f);
    }
    return (int32_t)(value - 0.5f);
}

static inline uint8_t gfx_sw_blend_tess_neighbors_to_internal_edges(const TESSindex *neighbors)
{
    uint8_t internal_edges = 0;

    if (neighbors == NULL) {
        return 0;
    }

    if (neighbors[0] != TESS_UNDEF) {
        internal_edges |= 0x04;
    }
    if (neighbors[1] != TESS_UNDEF) {
        internal_edges |= 0x01;
    }
    if (neighbors[2] != TESS_UNDEF) {
        internal_edges |= 0x02;
    }

    return internal_edges;
}
#endif

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_sw_blend_perf_reset(gfx_blend_perf_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
}

void gfx_sw_blend_perf_bind(gfx_blend_perf_stats_t *stats)
{
    s_active_perf_stats = stats;
}

void gfx_sw_blend_perf_unbind(void)
{
    s_active_perf_stats = NULL;
}

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
        uint32_t color32 = ((uint32_t)color << 16) | color;

        if (((uintptr_t)buf & 0x3) && pixels > 0) {
            *buf++ = color;
            pixels--;
        }

        uint32_t *buf32 = (uint32_t *)buf;
        size_t pairs = pixels / 2;

        for (size_t i = 0; i < pairs; i++) {
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
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || area == NULL) {
        return;
    }
    int32_t w = area->x2 - area->x1;
    int32_t h = area->y2 - area->y1;
    if (w <= 0 || h <= 0) {
        return;
    }
    if (s_active_perf_stats != NULL) {
        perf_start_us = esp_timer_get_time();
    }
    for (int32_t y = area->y1; y < area->y2; y++) {
        uint16_t *row = dest_buf + (size_t)y * dest_stride + area->x1;
        gfx_sw_blend_fill(row, color, (size_t)w);
    }
    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->fill.calls++;
        s_active_perf_stats->fill.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->fill.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_opa_t *mask, gfx_coord_t mask_stride,
                       gfx_area_t *clip_area, gfx_color_t color, gfx_opa_t opa, bool swap)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int64_t perf_start_us = 0;

    int32_t x, y;
    uint32_t c32 = color.full + ((uint32_t)color.full << 16);

    if (w <= 0 || h <= 0) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = esp_timer_get_time();
    }

    /*Only the mask matters*/
    if (opa >= OPA_MAX) {
        int32_t x_end4 = w - 4;

        for (y = 0; y < h; y++) {
            for (x = 0; x < w && ((uintptr_t)mask & 0x3); x++) {
                FILL_NORMAL_MASK_PX(color, swap)
            }

            for (; x <= x_end4; x += 4) {
                uint32_t mask32 = *((uint32_t *)mask);
                if (mask32 == 0xFFFFFFFF) {
                    if ((uintptr_t)dest_buf & 0x3) {
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
    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->color_draw.calls++;
        s_active_perf_stats->color_draw.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->color_draw.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_img_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                           const gfx_color_t *src_buf, gfx_coord_t src_stride,
                           const gfx_opa_t *mask, gfx_coord_t mask_stride,
                           gfx_area_t *clip_area, bool swap)
{
    int32_t w = clip_area->x2 - clip_area->x1;
    int32_t h = clip_area->y2 - clip_area->y1;
    int64_t perf_start_us = 0;

    int32_t x, y;

    if (w <= 0 || h <= 0) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = esp_timer_get_time();
    }

    if (mask == NULL) {
        /* src_buf is expected to already be in native framebuffer order */
        size_t row_bytes = (size_t)w * sizeof(gfx_color_t);
        for (y = 0; y < h; y++) {
            memcpy(dest_buf, src_buf, row_bytes);
            dest_buf += dest_stride;
            src_buf += src_stride;
        }
        if (s_active_perf_stats != NULL) {
            s_active_perf_stats->image_draw.calls++;
            s_active_perf_stats->image_draw.pixels += (uint64_t)w * (uint64_t)h;
            s_active_perf_stats->image_draw.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
        }
        return;
    }

    gfx_color_t last_dest_color;
    gfx_color_t last_res_color;
    gfx_color_t last_src_color;
    gfx_opa_t last_mask = OPA_TRANSP;
    last_dest_color.full = dest_buf[0].full;
    last_res_color.full = dest_buf[0].full;
    last_src_color.full = src_buf[0].full;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (*mask) {
                if (*mask != last_mask || last_dest_color.full != dest_buf[x].full || last_src_color.full != src_buf[x].full) {
                    if (*mask == OPA_COVER) {
                        last_res_color = src_buf[x];
                    } else {
                        last_res_color = gfx_blend_color_mix(src_buf[x], dest_buf[x], *mask, swap);
                    }
                    last_mask = *mask;
                    last_dest_color.full = dest_buf[x].full;
                    last_src_color.full = src_buf[x].full;
                }
                dest_buf[x] = last_res_color;
            }
            mask++;
        }
        dest_buf += dest_stride;
        src_buf += src_stride;
        mask += (mask_stride - w);
    }
    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->image_draw.calls++;
        s_active_perf_stats->image_draw.pixels += (uint64_t)w * (uint64_t)h;
        s_active_perf_stats->image_draw.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_img_triangle_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                    const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                    const gfx_color_t *src_buf, gfx_coord_t src_stride, gfx_coord_t src_height,
                                    const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                    const gfx_sw_blend_img_vertex_t *v0,
                                    const gfx_sw_blend_img_vertex_t *v1,
                                    const gfx_sw_blend_img_vertex_t *v2,
                                    uint8_t internal_edges,
                                    const gfx_sw_blend_aa_edge_t *extra_aa_edges,
                                    uint8_t extra_aa_count,
                                    bool swap)
{
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            src_buf == NULL || v0 == NULL || v1 == NULL || v2 == NULL ||
            src_stride <= 0 || src_height <= 0) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = esp_timer_get_time();
    }

    (void)dest_stride;
    (void)src_stride;
    (void)src_height;
    (void)mask;
    (void)mask_stride;
    (void)internal_edges;
    (void)extra_aa_edges;
    (void)extra_aa_count;
    (void)swap;

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->triangle_draw.calls++;
        s_active_perf_stats->triangle_draw.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}

void gfx_sw_blend_polygon_fill(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                               const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                               gfx_color_t color,
                               const int32_t *vx, const int32_t *vy,
                               int vertex_count,
                               bool swap)
{
    int64_t perf_start_us = 0;

    if (dest_buf == NULL || buf_area == NULL || clip_area == NULL ||
            vx == NULL || vy == NULL || vertex_count < 3) {
        return;
    }

    if (s_active_perf_stats != NULL) {
        perf_start_us = esp_timer_get_time();
    }

    (void)dest_stride;
    (void)color;
    (void)swap;

    if (s_active_perf_stats != NULL) {
        s_active_perf_stats->triangle_draw.calls++;
        s_active_perf_stats->triangle_draw.time_us += gfx_blend_perf_elapsed_us(perf_start_us);
    }
}
