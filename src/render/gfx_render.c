/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_RENDER
#include "common/gfx_log_priv.h"

#include "core/display/gfx_refresh_priv.h"
#include "gfx/scene/arena_draw.h"
#include "gfx/scene/arena_scene.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "core/runtime/gfx_timer_priv.h"

/*********************
 *      DEFINES
 *********************/

#define GFX_RENDER_OPA_COVER 0xFFU
#define GFX_RENDER_AA_SUBPIXELS 4
#define GFX_RENDER_AA_SAMPLES (GFX_RENDER_AA_SUBPIXELS * GFX_RENDER_AA_SUBPIXELS)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "render";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static const void *gfx_render_prepare_flush_pixels(gfx_display_t *disp,
        const void *render_buf,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        gfx_coord_t stride);
static bool gfx_render_backend_fill(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                    const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);
static bool gfx_render_backend_draw_glyph(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
        const gfx_area_t *area, const gfx_opa_t *mask, gfx_coord_t mask_stride,
        gfx_color_t color, gfx_opa_t opa);
static void gfx_render_fill_area(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                 const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);
static void gfx_render_surface_fill_clipped(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_color_t color,
        gfx_opa_t opa);
static uint16_t gfx_render_clamp_radius(const gfx_area_t *area, uint16_t radius);
static gfx_opa_t gfx_render_scale_opa(gfx_opa_t opa, uint8_t cover);
static void gfx_render_surface_draw_aa_pixel(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        gfx_coord_t x,
        gfx_coord_t y,
        gfx_color_t color,
        gfx_opa_t opa,
        uint8_t cover);
static uint8_t gfx_render_round_rect_corner_cover(gfx_coord_t px,
        gfx_coord_t py,
        gfx_coord_t radius,
        bool outer);
static uint8_t gfx_render_round_rect_coverage(const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_coord_t x,
        gfx_coord_t y);
static void gfx_render_surface_round_rect_corners_aa(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_color_t color,
        gfx_opa_t opa);
static void gfx_render_surface_round_rect_stroke_corners_aa(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_coord_t stroke_w,
        gfx_color_t color,
        gfx_opa_t opa);
static gfx_coord_t gfx_render_align_floor(gfx_coord_t value, uint16_t alignment);
static gfx_coord_t gfx_render_align_ceil(gfx_coord_t value, uint16_t alignment);
static void gfx_render_update_object_tree(gfx_object_t *obj);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint32_t gfx_render_rgb565_to_xrgb888(uint16_t rgb565)
{
    uint32_t r = (rgb565 >> 11) & 0x1fU;
    uint32_t g = (rgb565 >> 5) & 0x3fU;
    uint32_t b = rgb565 & 0x1fU;

    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    return (r << 16) | (g << 8) | b;
}

static void gfx_render_write_rgb888(uint8_t *dst, gfx_color_format_t format, uint32_t rgb888)
{
    gfx_color_write_rgb888_bytes(dst, format,
                                 (uint8_t)((rgb888 >> 16) & 0xffU),
                                 (uint8_t)((rgb888 >> 8) & 0xffU),
                                 (uint8_t)(rgb888 & 0xffU));
}

static const void *gfx_render_prepare_flush_pixels(gfx_display_t *disp,
        const void *render_buf,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        gfx_coord_t stride)
{
    if (disp == NULL || render_buf == NULL ||
            disp->format.output_format == disp->format.render_format) {
        return render_buf;
    }

    if (disp->buf.flush_buf == NULL) {
        return NULL;
    }

    uint32_t w = (uint32_t)(x2 - x1);
    uint32_t h = (uint32_t)(y2 - y1);
    uint32_t src_stride = (uint32_t)(stride > 0 ? stride : (gfx_coord_t)w);
    uint32_t dst_stride = src_stride;
    const uint8_t *src_base = (const uint8_t *)render_buf;
    uint8_t *dst_base = (uint8_t *)disp->buf.flush_buf;

    if (gfx_display_has_full_frame_buf(disp)) {
        src_stride = disp->res.h_res;
        dst_stride = disp->res.h_res;
    }

    for (uint32_t y = 0; y < h; y++) {
        size_t src_px_offset = gfx_display_has_full_frame_buf(disp)
                               ? (size_t)(y1 + (gfx_coord_t)y) * src_stride + (size_t)x1
                               : (size_t)y * src_stride;
        size_t dst_px_offset = gfx_display_has_full_frame_buf(disp)
                               ? (size_t)(y1 + (gfx_coord_t)y) * dst_stride + (size_t)x1
                               : (size_t)y * dst_stride;
        const uint8_t *src_row = src_base + src_px_offset * disp->format.render_pixel_size;
        uint8_t *dst_row = dst_base + dst_px_offset * disp->format.output_pixel_size;

        if (disp->format.output_format == GFX_COLOR_FORMAT_RGB888 ||
                disp->format.output_format == GFX_COLOR_FORMAT_BGR888) {
            for (uint32_t x = 0; x < w; x++) {
                uint16_t semantic = gfx_color_read_rgb565_bytes(src_row + (size_t)x * disp->format.render_pixel_size,
                                    disp->format.render_format);
                gfx_render_write_rgb888(dst_row + (size_t)x * GFX_PIXEL_SIZE_24BPP,
                                        disp->format.output_format,
                                        gfx_render_rgb565_to_xrgb888(semantic));
            }
        } else if (gfx_color_format_is_rgb565(disp->format.output_format)) {
            for (uint32_t x = 0; x < w; x++) {
                uint16_t semantic = gfx_color_read_rgb565_bytes(src_row + (size_t)x * disp->format.render_pixel_size,
                                    disp->format.render_format);
                gfx_color_write_rgb565_bytes(dst_row + (size_t)x * disp->format.output_pixel_size,
                                             disp->format.output_format,
                                             semantic);
            }
        }
    }

    return disp->buf.flush_buf;
}

static bool gfx_render_backend_fill(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                    const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    if (disp == NULL || ctx == NULL || area == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (!gfx_render_backend_op_can_use_dst(disp->backend, GFX_BACKEND_CAP_FILL, &dst, area)) {
        return false;
    }

    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(disp->backend);
    if (ops == NULL || ops->fill == NULL) {
        return false;
    }

    return ops->fill(disp->backend, disp, &dst, area, color, opa) == GFX_OK;
}

static bool gfx_render_backend_draw_glyph(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
        const gfx_area_t *area, const gfx_opa_t *mask, gfx_coord_t mask_stride,
        gfx_color_t color, gfx_opa_t opa)
{
    if (disp == NULL || ctx == NULL || area == NULL || mask == NULL || mask_stride <= 0 || opa == 0U) {
        return false;
    }

    gfx_backend_t *backend = disp->backend;
    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(backend);
    if (ops == NULL || ops->draw_glyph == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (!gfx_render_backend_op_can_use_dst(backend, GFX_BACKEND_CAP_DRAW_GLYPH, &dst, area)) {
        return false;
    }

    return ops->draw_glyph(backend, disp, &dst, area, mask, mask_stride, color, opa) == GFX_OK;
}

static void gfx_render_fill_area(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                 const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    if (ctx == NULL || area == NULL || opa == 0U) {
        return;
    }

    if (opa == GFX_RENDER_OPA_COVER && gfx_render_backend_fill(disp, ctx, area, color, opa)) {
        return;
    }

    gfx_area_t local_area = {
        .x1 = (gfx_coord_t)(area->x1 - ctx->buf_area.x1),
        .y1 = (gfx_coord_t)(area->y1 - ctx->buf_area.y1),
        .x2 = (gfx_coord_t)(area->x2 - ctx->buf_area.x1),
        .y2 = (gfx_coord_t)(area->y2 - ctx->buf_area.y1),
    };

    gfx_sw_blend_surface_fill(ctx->buf, ctx->stride, ctx->format, &local_area, color, opa);
}

static void gfx_render_surface_fill_clipped(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_color_t color,
        gfx_opa_t opa)
{
    gfx_area_t clipped;

    if (dst == NULL || area == NULL || opa == 0U) {
        return;
    }
    if (!gfx_area_intersect_exclusive(&clipped, area, &dst->clip_area)) {
        return;
    }
    if (!gfx_area_intersect_exclusive(&clipped, &clipped, &dst->buf_area)) {
        return;
    }
    gfx_render_surface_fill(disp, dst, &clipped, color, opa);
}

static uint16_t gfx_render_clamp_radius(const gfx_area_t *area, uint16_t radius)
{
    gfx_coord_t w;
    gfx_coord_t h;
    gfx_coord_t max_radius;

    if (area == NULL || area->x2 <= area->x1 || area->y2 <= area->y1) {
        return 0U;
    }

    w = (gfx_coord_t)(area->x2 - area->x1);
    h = (gfx_coord_t)(area->y2 - area->y1);
    max_radius = ((w < h) ? w : h) / 2;
    if ((gfx_coord_t)radius > max_radius) {
        return (uint16_t)max_radius;
    }
    return radius;
}

static gfx_opa_t gfx_render_scale_opa(gfx_opa_t opa, uint8_t cover)
{
    if (cover == 0U || opa == 0U) {
        return 0U;
    }
    if (cover >= 255U) {
        return opa;
    }
    return (gfx_opa_t)(((uint32_t)opa * cover + 127U) / 255U);
}

static void gfx_render_surface_draw_aa_pixel(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        gfx_coord_t x,
        gfx_coord_t y,
        gfx_color_t color,
        gfx_opa_t opa,
        uint8_t cover)
{
    gfx_area_t px_area;
    gfx_opa_t px_opa;

    if (cover == 0U) {
        return;
    }

    px_opa = gfx_render_scale_opa(opa, cover);
    if (px_opa == 0U) {
        return;
    }

    px_area = (gfx_area_t) {
        .x1 = x,
        .y1 = y,
        .x2 = (gfx_coord_t)(x + 1),
        .y2 = (gfx_coord_t)(y + 1),
    };
    gfx_render_surface_fill_clipped(disp, dst, &px_area, color, px_opa);
}

static uint8_t gfx_render_round_rect_corner_cover(gfx_coord_t px,
        gfx_coord_t py,
        gfx_coord_t radius,
        bool outer)
{
    int32_t r_q = (int32_t)radius * GFX_RENDER_AA_SUBPIXELS * 2;
    int32_t limit = outer ? r_q : (r_q - 2);
    int32_t limit_sq;
    uint8_t count = 0;

    if (radius <= 0 || limit <= 0) {
        return outer ? 255U : 0U;
    }

    limit_sq = limit * limit;
    for (int sy = 0; sy < GFX_RENDER_AA_SUBPIXELS; sy++) {
        for (int sx = 0; sx < GFX_RENDER_AA_SUBPIXELS; sx++) {
            int32_t sample_x = (int32_t)px * GFX_RENDER_AA_SUBPIXELS * 2 + sx * 2 + 1;
            int32_t sample_y = (int32_t)py * GFX_RENDER_AA_SUBPIXELS * 2 + sy * 2 + 1;
            int32_t dx = sample_x - r_q;
            int32_t dy = sample_y - r_q;
            int32_t dist_sq = dx * dx + dy * dy;

            if (dist_sq <= limit_sq) {
                count++;
            }
        }
    }

    return (uint8_t)(((uint32_t)count * 255U + (GFX_RENDER_AA_SAMPLES / 2U)) / GFX_RENDER_AA_SAMPLES);
}

static uint8_t gfx_render_round_rect_coverage(const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_coord_t x,
        gfx_coord_t y)
{
    gfx_coord_t local_x;
    gfx_coord_t local_y;
    gfx_coord_t w;
    gfx_coord_t h;
    gfx_coord_t cx;
    gfx_coord_t cy;

    if (area == NULL || radius <= 0 ||
            x < area->x1 || x >= area->x2 || y < area->y1 || y >= area->y2) {
        return 0U;
    }

    w = (gfx_coord_t)(area->x2 - area->x1);
    h = (gfx_coord_t)(area->y2 - area->y1);
    local_x = (gfx_coord_t)(x - area->x1);
    local_y = (gfx_coord_t)(y - area->y1);

    if (local_x >= radius && local_x < (gfx_coord_t)(w - radius)) {
        return 255U;
    }
    if (local_y >= radius && local_y < (gfx_coord_t)(h - radius)) {
        return 255U;
    }

    cx = local_x < radius ? local_x : (gfx_coord_t)(w - 1 - local_x);
    cy = local_y < radius ? local_y : (gfx_coord_t)(h - 1 - local_y);
    return gfx_render_round_rect_corner_cover(cx, cy, radius, true);
}

static void gfx_render_surface_round_rect_corners_aa(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_color_t color,
        gfx_opa_t opa)
{
    if (dst == NULL || area == NULL || radius <= 0 || opa == 0U) {
        return;
    }

    for (gfx_coord_t y = 0; y < radius; y++) {
        for (gfx_coord_t x = 0; x < radius; x++) {
            uint8_t cover = gfx_render_round_rect_corner_cover(x, y, radius, true);
            gfx_coord_t left_x = (gfx_coord_t)(area->x1 + x);
            gfx_coord_t right_x = (gfx_coord_t)(area->x2 - 1 - x);
            gfx_coord_t top_y = (gfx_coord_t)(area->y1 + y);
            gfx_coord_t bottom_y = (gfx_coord_t)(area->y2 - 1 - y);

            if (cover == 0U) {
                continue;
            }

            gfx_render_surface_draw_aa_pixel(disp, dst, left_x, top_y, color, opa, cover);
            if (right_x != left_x) {
                gfx_render_surface_draw_aa_pixel(disp, dst, right_x, top_y, color, opa, cover);
            }
            if (bottom_y != top_y) {
                gfx_render_surface_draw_aa_pixel(disp, dst, left_x, bottom_y, color, opa, cover);
                if (right_x != left_x) {
                    gfx_render_surface_draw_aa_pixel(disp, dst, right_x, bottom_y, color, opa, cover);
                }
            }
        }
    }
}

static void gfx_render_surface_round_rect_stroke_corners_aa(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        gfx_coord_t radius,
        gfx_coord_t stroke_w,
        gfx_color_t color,
        gfx_opa_t opa)
{
    gfx_area_t inner_area;
    gfx_coord_t inner_radius;

    if (dst == NULL || area == NULL || radius <= 0 || stroke_w <= 0 || opa == 0U) {
        return;
    }

    inner_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)(area->x1 + stroke_w),
        .y1 = (gfx_coord_t)(area->y1 + stroke_w),
        .x2 = (gfx_coord_t)(area->x2 - stroke_w),
        .y2 = (gfx_coord_t)(area->y2 - stroke_w),
    };
    inner_radius = (gfx_coord_t)(radius - stroke_w);

    for (gfx_coord_t y = 0; y < radius; y++) {
        for (gfx_coord_t x = 0; x < radius; x++) {
            gfx_coord_t points[4][2] = {
                { (gfx_coord_t)(area->x1 + x), (gfx_coord_t)(area->y1 + y) },
                { (gfx_coord_t)(area->x2 - 1 - x), (gfx_coord_t)(area->y1 + y) },
                { (gfx_coord_t)(area->x1 + x), (gfx_coord_t)(area->y2 - 1 - y) },
                { (gfx_coord_t)(area->x2 - 1 - x), (gfx_coord_t)(area->y2 - 1 - y) },
            };

            for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
                gfx_coord_t px = points[i][0];
                gfx_coord_t py = points[i][1];
                uint8_t outer_cover = gfx_render_round_rect_coverage(area, radius, px, py);
                uint8_t inner_cover = gfx_render_round_rect_coverage(&inner_area, inner_radius, px, py);
                uint8_t cover;

                if (outer_cover <= inner_cover) {
                    continue;
                }
                cover = (uint8_t)(outer_cover - inner_cover);
                gfx_render_surface_draw_aa_pixel(disp, dst, px, py, color, opa, cover);
            }
        }
    }
}

void gfx_render_surface_fill(gfx_display_t *disp,
                             const gfx_render_surface_t *dst,
                             const gfx_area_t *area,
                             gfx_color_t color,
                             gfx_opa_t opa)
{
    gfx_draw_ctx_t ctx;

    if (dst == NULL) {
        return;
    }

    ctx = (gfx_draw_ctx_t) {
        .buf = dst->buf,
        .buf_area = dst->buf_area,
        .clip_area = dst->clip_area,
        .stride = dst->stride,
        .format = dst->format,
        .pixel_size = gfx_color_format_get_size(dst->format),
    };
    gfx_render_fill_area(disp, &ctx, area, color, opa);
}

void gfx_render_surface_rect_stroke(gfx_display_t *disp,
                                    const gfx_render_surface_t *dst,
                                    const gfx_area_t *area,
                                    uint16_t width,
                                    gfx_color_t color,
                                    gfx_opa_t opa)
{
    gfx_coord_t w;
    gfx_coord_t h;
    gfx_coord_t stroke_w;
    gfx_area_t band;

    if (dst == NULL || area == NULL || width == 0U || opa == 0U ||
            area->x2 <= area->x1 || area->y2 <= area->y1) {
        return;
    }

    w = (gfx_coord_t)(area->x2 - area->x1);
    h = (gfx_coord_t)(area->y2 - area->y1);
    stroke_w = (gfx_coord_t)width;
    if (stroke_w * 2 > w) {
        stroke_w = (gfx_coord_t)((w + 1) / 2);
    }
    if (stroke_w * 2 > h) {
        stroke_w = (gfx_coord_t)((h + 1) / 2);
    }
    if (stroke_w <= 0) {
        return;
    }

    if (stroke_w * 2 >= w || stroke_w * 2 >= h) {
        gfx_render_surface_fill_clipped(disp, dst, area, color, opa);
        return;
    }

    band = (gfx_area_t) {
        .x1 = area->x1,
        .y1 = area->y1,
        .x2 = area->x2,
        .y2 = (gfx_coord_t)(area->y1 + stroke_w),
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, color, opa);

    band.y1 = (gfx_coord_t)(area->y2 - stroke_w);
    band.y2 = area->y2;
    gfx_render_surface_fill_clipped(disp, dst, &band, color, opa);

    band = (gfx_area_t) {
        .x1 = area->x1,
        .y1 = (gfx_coord_t)(area->y1 + stroke_w),
        .x2 = (gfx_coord_t)(area->x1 + stroke_w),
        .y2 = (gfx_coord_t)(area->y2 - stroke_w),
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, color, opa);

    band.x1 = (gfx_coord_t)(area->x2 - stroke_w);
    band.x2 = area->x2;
    gfx_render_surface_fill_clipped(disp, dst, &band, color, opa);
}

void gfx_render_surface_round_rect_fill(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *area,
                                        const gfx_round_rect_fill_dsc_t *dsc)
{
    uint16_t radius;
    gfx_coord_t r;
    gfx_area_t band;

    if (dst == NULL || area == NULL || dsc == NULL || dsc->opa == 0U ||
            area->x2 <= area->x1 || area->y2 <= area->y1) {
        return;
    }

    radius = gfx_render_clamp_radius(area, dsc->radius);
    if (radius == 0U) {
        gfx_render_surface_fill_clipped(disp, dst, area, dsc->color, dsc->opa);
        return;
    }

    r = (gfx_coord_t)radius;
    band = (gfx_area_t) {
        .x1 = (gfx_coord_t)(area->x1 + r),
        .y1 = area->y1,
        .x2 = (gfx_coord_t)(area->x2 - r),
        .y2 = area->y2,
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    band = (gfx_area_t) {
        .x1 = area->x1,
        .y1 = (gfx_coord_t)(area->y1 + r),
        .x2 = (gfx_coord_t)(area->x1 + r),
        .y2 = (gfx_coord_t)(area->y2 - r),
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    band.x1 = (gfx_coord_t)(area->x2 - r);
    band.x2 = area->x2;
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    gfx_render_surface_round_rect_corners_aa(disp, dst, area, r, dsc->color, dsc->opa);
}

void gfx_render_surface_round_rect_stroke(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        const gfx_round_rect_stroke_dsc_t *dsc)
{
    uint16_t radius;
    gfx_coord_t r;
    gfx_coord_t stroke_w;
    gfx_area_t band;

    if (dst == NULL || area == NULL || dsc == NULL || dsc->opa == 0U ||
            dsc->width == 0U || area->x2 <= area->x1 || area->y2 <= area->y1) {
        return;
    }

    radius = gfx_render_clamp_radius(area, dsc->radius);
    if (radius == 0U) {
        gfx_render_surface_rect_stroke(disp, dst, area, dsc->width, dsc->color, dsc->opa);
        return;
    }

    r = (gfx_coord_t)radius;
    stroke_w = (gfx_coord_t)dsc->width;
    if (stroke_w >= r) {
        gfx_round_rect_fill_dsc_t fill_dsc = {
            .color = dsc->color,
            .opa = dsc->opa,
            .radius = radius,
        };
        gfx_render_surface_round_rect_fill(disp, dst, area, &fill_dsc);
        return;
    }

    band = (gfx_area_t) {
        .x1 = (gfx_coord_t)(area->x1 + r),
        .y1 = area->y1,
        .x2 = (gfx_coord_t)(area->x2 - r),
        .y2 = (gfx_coord_t)(area->y1 + stroke_w),
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    band.y1 = (gfx_coord_t)(area->y2 - stroke_w);
    band.y2 = area->y2;
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    band = (gfx_area_t) {
        .x1 = area->x1,
        .y1 = (gfx_coord_t)(area->y1 + r),
        .x2 = (gfx_coord_t)(area->x1 + stroke_w),
        .y2 = (gfx_coord_t)(area->y2 - r),
    };
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    band.x1 = (gfx_coord_t)(area->x2 - stroke_w);
    band.x2 = area->x2;
    gfx_render_surface_fill_clipped(disp, dst, &band, dsc->color, dsc->opa);

    gfx_render_surface_round_rect_stroke_corners_aa(disp, dst, area, r, stroke_w, dsc->color, dsc->opa);
}

static gfx_coord_t gfx_render_align_floor(gfx_coord_t value, uint16_t alignment)
{
    if (alignment <= 1U) {
        return value;
    }

    gfx_coord_t align = (gfx_coord_t)alignment;
    if (value >= 0) {
        return (gfx_coord_t)((value / align) * align);
    }

    return (gfx_coord_t)(-(((-value + align - 1) / align) * align));
}

static gfx_coord_t gfx_render_align_ceil(gfx_coord_t value, uint16_t alignment)
{
    if (alignment <= 1U) {
        return value;
    }

    gfx_coord_t align = (gfx_coord_t)alignment;
    if (value >= 0) {
        return (gfx_coord_t)(((value + align - 1) / align) * align);
    }

    return (gfx_coord_t)(-((-value / align) * align));
}

void gfx_render_draw_object_tree(gfx_display_t *disp, gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_area_t obj_area;
    gfx_draw_ctx_t child_ctx;

    if (obj == NULL || ctx == NULL || !obj->state.is_visible) {
        return;
    }

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return;
    }

    if (gfx_object_load_resource(obj) != GFX_OK) {
        return;
    }

    if (obj->vfunc.draw) {
        obj->vfunc.draw(obj, ctx);
    }

    if (!obj->state.manual_child_draw) {
        child_ctx = *ctx;
        if (obj->state.clip_children &&
                !gfx_area_intersect_exclusive(&child_ctx.clip_area, &ctx->clip_area, &obj_area)) {
            return;
        }

        for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
            gfx_object_t *child = (gfx_object_t *)child_node->src;
            gfx_render_draw_object_tree(disp, child, &child_ctx);
        }
    }
}

static void gfx_render_update_object_tree(gfx_object_t *obj)
{
    if (obj == NULL || !obj->state.is_visible) {
        return;
    }

    if (gfx_object_load_resource(obj) != GFX_OK) {
        return;
    }

    if (obj->vfunc.update) {
        obj->vfunc.update(obj);
    }

    for (gfx_object_child_t *child_node = obj->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_render_update_object_tree((gfx_object_t *)child_node->src);
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_render_draw_child_objects(gfx_display_t *disp, const gfx_draw_ctx_t *ctx)
{
    if (disp == NULL || disp->child_list == NULL || ctx == NULL) {
        return;
    }

    for (gfx_object_child_t *child_node = disp->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_render_draw_object_tree(disp, (gfx_object_t *)child_node->src, ctx);
    }
}


void gfx_render_update_child_objects(gfx_display_t *disp)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    for (gfx_object_child_t *child_node = disp->child_list; child_node != NULL; child_node = child_node->next) {
        gfx_render_update_object_tree((gfx_object_t *)child_node->src);
    }
}

uint32_t gfx_render_area_summary(gfx_display_t *disp)
{
    uint32_t total_dirty_pixels = 0;

    if (disp == NULL) {
        return 0;
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }
        gfx_area_t *area = &disp->dirty.areas[i];
        uint32_t area_size = gfx_area_get_size(area);
        total_dirty_pixels += area_size;
        // GFX_LOGD(TAG, "Draw area [%d]: (%d,%d)->(%d,%d) %dx%d",
        //          i, area->x1, area->y1, area->x2, area->y2,
        //          area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    }

    return total_dirty_pixels;
}

gfx_area_t gfx_render_roundup_area(const gfx_area_t *area,
                                   const gfx_render_alignment_t *alignment,
                                   const gfx_area_t *limit)
{
    gfx_area_t result = {0};

    if (area == NULL) {
        return result;
    }

    uint16_t width_align = alignment != NULL && alignment->width_px > 0U ? alignment->width_px : 1U;
    uint16_t height_align = alignment != NULL && alignment->height_px > 0U ? alignment->height_px : 1U;

    result.x1 = gfx_render_align_floor(area->x1, width_align);
    result.y1 = gfx_render_align_floor(area->y1, height_align);
    result.x2 = gfx_render_align_ceil(area->x2, width_align);
    result.y2 = gfx_render_align_ceil(area->y2, height_align);

    if (limit != NULL) {
        if (result.x1 < limit->x1) {
            result.x1 = limit->x1;
        }
        if (result.y1 < limit->y1) {
            result.y1 = limit->y1;
        }
        if (result.x2 > limit->x2) {
            result.x2 = limit->x2;
        }
        if (result.y2 > limit->y2) {
            result.y2 = limit->y2;
        }
    }

    return result;
}

uint32_t gfx_render_roundup_stride_bytes(uint32_t stride_bytes,
        const gfx_render_alignment_t *alignment)
{
    uint16_t stride_align = alignment != NULL && alignment->stride_bytes > 0U ? alignment->stride_bytes : 1U;

    if (stride_align <= 1U || stride_bytes == 0U) {
        return stride_bytes;
    }

    return ((stride_bytes + stride_align - 1U) / stride_align) * stride_align;
}

bool gfx_render_is_addr_aligned(const void *ptr,
                                const gfx_render_alignment_t *alignment)
{
    uint16_t addr_align = alignment != NULL && alignment->addr_bytes > 0U ? alignment->addr_bytes : 1U;

    if (addr_align <= 1U || ptr == NULL) {
        return true;
    }

    return (((uintptr_t)ptr % addr_align) == 0U);
}

bool gfx_render_backend_op_can_use_dst(const gfx_backend_t *backend,
                                       uint32_t cap,
                                       const gfx_backend_surface_t *dst,
                                       const gfx_area_t *area)
{
    if (backend == NULL || dst == NULL || area == NULL || dst->buf == NULL) {
        return false;
    }
    if (!gfx_backend_has_caps(backend, cap)) {
        return false;
    }
    if (area->x2 <= area->x1 || area->y2 <= area->y1 || dst->stride <= 0) {
        return false;
    }
    if (area->x1 < dst->area.x1 || area->y1 < dst->area.y1 ||
            area->x2 > dst->area.x2 || area->y2 > dst->area.y2) {
        return false;
    }

    gfx_render_alignment_t alignment = gfx_backend_get_alignment(backend);
    uint32_t stride_bytes = (uint32_t)dst->stride * gfx_color_format_get_size(dst->format);
    if (stride_bytes != gfx_render_roundup_stride_bytes(stride_bytes, &alignment)) {
        return false;
    }
    if (!gfx_render_is_addr_aligned(dst->buf, &alignment)) {
        return false;
    }

    gfx_area_t rounded = gfx_render_roundup_area(area, &alignment, &dst->area);
    return rounded.x1 == area->x1 &&
           rounded.y1 == area->y1 &&
           rounded.x2 == area->x2 &&
           rounded.y2 == area->y2;
}

bool gfx_render_backend_image(gfx_display_t *disp,
                              const gfx_draw_ctx_t *ctx,
                              const gfx_area_t *area,
                              const gfx_backend_image_t *src,
                              gfx_coord_t src_x,
                              gfx_coord_t src_y,
                              gfx_opa_t opa)
{
    if (disp == NULL || ctx == NULL || area == NULL || src == NULL || src->pixels == NULL || opa == 0U) {
        return false;
    }

    gfx_backend_t *backend = disp->backend;
    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(backend);
    if (ops == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    bool use_blend = src->alpha != NULL || gfx_color_format_has_pixel_alpha(src->format) ||
                     opa != GFX_RENDER_OPA_COVER;
    uint32_t cap = use_blend ? GFX_BACKEND_CAP_BLEND : GFX_BACKEND_CAP_BLIT;
    if (!gfx_render_backend_op_can_use_dst(backend, cap, &dst, area)) {
        return false;
    }

    if (use_blend) {
        if (ops->blend == NULL) {
            return false;
        }
        return ops->blend(backend, disp, &dst, area, src, src_x, src_y, opa) == GFX_OK;
    }

    if (ops->blit == NULL) {
        return false;
    }
    return ops->blit(backend, disp, &dst, area, src, src_x, src_y) == GFX_OK;
}

bool gfx_render_surface_blit_image(gfx_display_t *disp,
                                   const gfx_render_surface_t *dst,
                                   const gfx_area_t *area,
                                   const gfx_render_image_t *src,
                                   gfx_coord_t src_x,
                                   gfx_coord_t src_y,
                                   gfx_opa_t opa)
{
    gfx_draw_ctx_t ctx;
    gfx_backend_image_t backend_src;

    if (dst == NULL || src == NULL) {
        return false;
    }

    ctx = (gfx_draw_ctx_t) {
        .buf = dst->buf,
        .buf_area = dst->buf_area,
        .clip_area = dst->clip_area,
        .stride = dst->stride,
        .format = dst->format,
        .pixel_size = gfx_color_format_get_size(dst->format),
    };
    backend_src = (gfx_backend_image_t) {
        .pixels = src->pixels,
        .stride = src->stride,
        .format = src->format,
        .alpha = src->alpha,
        .alpha_stride = src->alpha_stride,
    };

    return gfx_render_backend_image(disp, &ctx, area, &backend_src, src_x, src_y, opa);
}

void gfx_render_surface_draw_mask(gfx_display_t *disp,
                                  const gfx_render_surface_t *dst,
                                  const gfx_area_t *area,
                                  const gfx_opa_t *mask,
                                  gfx_coord_t mask_stride,
                                  gfx_color_t color,
                                  gfx_opa_t opa)
{
    gfx_draw_ctx_t ctx;
    gfx_area_t local_area;

    if (dst == NULL || area == NULL || mask == NULL || mask_stride <= 0 || opa == 0U) {
        return;
    }

    ctx = (gfx_draw_ctx_t) {
        .buf = dst->buf,
        .buf_area = dst->buf_area,
        .clip_area = dst->clip_area,
        .stride = dst->stride,
        .format = dst->format,
        .pixel_size = gfx_color_format_get_size(dst->format),
    };

    if (gfx_render_backend_draw_glyph(disp, &ctx, area, mask, mask_stride, color, opa)) {
        return;
    }

    local_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)(area->x1 - dst->buf_area.x1),
        .y1 = (gfx_coord_t)(area->y1 - dst->buf_area.y1),
        .x2 = (gfx_coord_t)(area->x2 - dst->buf_area.x1),
        .y2 = (gfx_coord_t)(area->y2 - dst->buf_area.y1),
    };

    gfx_sw_blend_mask_draw_fmt(dst->buf, dst->stride, dst->format,
                               mask, mask_stride, &local_area, color, opa);
}

void gfx_render_surface_draw_color_mask(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *area,
                                        const gfx_opa_t *mask,
                                        gfx_coord_t mask_stride,
                                        const gfx_color_t *color_mask,
                                        gfx_coord_t color_mask_stride,
                                        gfx_opa_t opa)
{
    gfx_area_t local_area;

    if (dst == NULL || area == NULL || mask == NULL || color_mask == NULL ||
            mask_stride <= 0 || color_mask_stride <= 0 || opa == 0U) {
        return;
    }

    local_area = (gfx_area_t) {
        .x1 = (gfx_coord_t)(area->x1 - dst->buf_area.x1),
        .y1 = (gfx_coord_t)(area->y1 - dst->buf_area.y1),
        .x2 = (gfx_coord_t)(area->x2 - dst->buf_area.x1),
        .y2 = (gfx_coord_t)(area->y2 - dst->buf_area.y1),
    };

    gfx_sw_blend_mask_color_draw_fmt(dst->buf, dst->stride, dst->format,
                                     mask, mask_stride,
                                     color_mask, color_mask_stride,
                                     &local_area, opa);
}

bool gfx_render_backend_scale(gfx_display_t *disp,
                              const gfx_draw_ctx_t *ctx,
                              const gfx_area_t *dst_area,
                              const gfx_area_t *clip_area,
                              const gfx_backend_image_t *src,
                              const gfx_area_t *src_area,
                              gfx_opa_t opa)
{
    gfx_area_t draw_area;

    if (disp == NULL || ctx == NULL || dst_area == NULL || clip_area == NULL ||
            src == NULL || src->pixels == NULL || src_area == NULL || opa == 0U) {
        return false;
    }

    if (!gfx_area_intersect_exclusive(&draw_area, dst_area, clip_area) ||
            draw_area.x1 != dst_area->x1 || draw_area.y1 != dst_area->y1 ||
            draw_area.x2 != dst_area->x2 || draw_area.y2 != dst_area->y2) {
        return false;
    }

    gfx_backend_t *backend = disp->backend;
    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(backend);
    if (ops == NULL || ops->scale == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (!gfx_render_backend_op_can_use_dst(backend, GFX_BACKEND_CAP_SCALE, &dst, dst_area)) {
        return false;
    }

    return ops->scale(backend, disp, &dst, dst_area, src, src_area, opa) == GFX_OK;
}

bool gfx_render_surface_scale_image(gfx_display_t *disp,
                                    const gfx_render_surface_t *dst,
                                    const gfx_area_t *dst_area,
                                    const gfx_render_image_t *src,
                                    const gfx_area_t *src_area,
                                    gfx_opa_t opa)
{
    gfx_draw_ctx_t ctx;
    gfx_backend_image_t backend_src;

    if (dst == NULL || src == NULL) {
        return false;
    }

    ctx = (gfx_draw_ctx_t) {
        .buf = dst->buf,
        .buf_area = dst->buf_area,
        .clip_area = dst->clip_area,
        .stride = dst->stride,
        .format = dst->format,
        .pixel_size = gfx_color_format_get_size(dst->format),
    };
    backend_src = (gfx_backend_image_t) {
        .pixels = src->pixels,
        .stride = src->stride,
        .format = src->format,
        .alpha = src->alpha,
        .alpha_stride = src->alpha_stride,
    };

    return gfx_render_backend_scale(disp, &ctx, dst_area, &dst->clip_area, &backend_src, src_area, opa);
}

bool gfx_render_backend_transform(gfx_display_t *disp,
                                  const gfx_draw_ctx_t *ctx,
                                  const gfx_area_t *dst_area,
                                  const gfx_area_t *clip_area,
                                  const gfx_backend_image_t *src,
                                  const gfx_area_t *src_area,
                                  int16_t angle,
                                  gfx_opa_t opa)
{
    gfx_area_t draw_area;

    if (disp == NULL || ctx == NULL || dst_area == NULL || clip_area == NULL ||
            src == NULL || src->pixels == NULL || src_area == NULL || opa == 0U) {
        return false;
    }

    if (!gfx_area_intersect_exclusive(&draw_area, dst_area, clip_area) ||
            draw_area.x1 != dst_area->x1 || draw_area.y1 != dst_area->y1 ||
            draw_area.x2 != dst_area->x2 || draw_area.y2 != dst_area->y2) {
        return false;
    }

    gfx_backend_t *backend = disp->backend;
    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(backend);
    if (ops == NULL || ops->transform == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (!gfx_render_backend_op_can_use_dst(backend, GFX_BACKEND_CAP_TRANSFORM, &dst, dst_area)) {
        return false;
    }

    return ops->transform(backend, disp, &dst, dst_area, src, src_area, angle, opa) == GFX_OK;
}

bool gfx_render_surface_transform_image(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *dst_area,
                                        const gfx_render_image_t *src,
                                        const gfx_area_t *src_area,
                                        int16_t angle,
                                        gfx_opa_t opa)
{
    gfx_draw_ctx_t ctx;
    gfx_backend_image_t backend_src;

    if (dst == NULL || src == NULL) {
        return false;
    }

    ctx = (gfx_draw_ctx_t) {
        .buf = dst->buf,
        .buf_area = dst->buf_area,
        .clip_area = dst->clip_area,
        .stride = dst->stride,
        .format = dst->format,
        .pixel_size = gfx_color_format_get_size(dst->format),
    };
    backend_src = (gfx_backend_image_t) {
        .pixels = src->pixels,
        .stride = src->stride,
        .format = src->format,
        .alpha = src->alpha,
        .alpha_stride = src->alpha_stride,
    };

    return gfx_render_backend_transform(disp, &ctx, dst_area, &dst->clip_area,
                                        &backend_src, src_area, angle, opa);
}

static uint32_t gfx_render_stride_pixels_for_width(uint32_t width_px,
        const gfx_render_alignment_t *alignment,
        uint8_t pixel_size)
{
    uint32_t stride_bytes = gfx_render_roundup_stride_bytes(width_px * pixel_size, alignment);

    return stride_bytes / pixel_size;
}

void gfx_render_part_area(gfx_display_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area)
{
    if (disp == NULL || area == NULL) {
        return;
    }

    if (area->x2 < area->x1 || area->y2 < area->y1) {
        GFX_LOGE(TAG, "render area[%d]: invalid bounds (%d,%d)-(%d,%d)", area_idx,
                 area->x1, area->y1, area->x2, area->y2);
        return;
    }

    gfx_render_alignment_t alignment = gfx_backend_get_alignment(disp->backend);
    gfx_area_t display_limit = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (gfx_coord_t)disp->res.h_res,
        .y2 = (gfx_coord_t)disp->res.v_res,
    };
    gfx_area_t requested_area = {
        .x1 = area->x1,
        .y1 = area->y1,
        .x2 = (gfx_coord_t)(area->x2 + 1),
        .y2 = (gfx_coord_t)(area->y2 + 1),
    };
    gfx_area_t render_area = gfx_render_roundup_area(&requested_area, &alignment, &display_limit);
    if (render_area.x2 <= render_area.x1 || render_area.y2 <= render_area.y1) {
        GFX_LOGE(TAG, "render area[%d]: aligned area is empty", area_idx);
        return;
    }

    uint32_t render_w = (uint32_t)(render_area.x2 - render_area.x1);
    uint32_t render_h = (uint32_t)(render_area.y2 - render_area.y1);
    uint32_t stride_pixels = gfx_display_has_full_frame_buf(disp) ? disp->res.h_res :
                             gfx_render_stride_pixels_for_width(render_w, &alignment,
                                     disp->format.render_pixel_size);
    uint32_t row_h = disp->buf.buf_pixels / stride_pixels;
    if (row_h == 0) {
        GFX_LOGE(TAG, "render area[%d]: stride %" PRIu32 " px exceeds buffer, skipping", area_idx, stride_pixels);
        return;
    }
    if (row_h > render_h) {
        row_h = render_h;
    }

    disp->render.flushing_last = false;
    gfx_coord_t cur_y = render_area.y1;

    while (cur_y < render_area.y2) {
        int64_t render_start_us;
        int64_t flush_start_us;

        gfx_coord_t chunk_x1 = render_area.x1;
        gfx_coord_t chunk_y1 = cur_y;
        gfx_coord_t chunk_x2 = render_area.x2;
        uint32_t remaining_h = (uint32_t)(render_area.y2 - cur_y);
        uint32_t chunk_h = row_h < remaining_h ? row_h : remaining_h;
        gfx_coord_t chunk_y2 = (gfx_coord_t)(cur_y + (gfx_coord_t)chunk_h);
        if (chunk_y2 > render_area.y2) {
            chunk_y2 = render_area.y2;
        }

        void *buf = disp->buf.buf_act;

        gfx_coord_t dest_stride = (gfx_coord_t)stride_pixels;

        gfx_area_t buf_area;
        if (gfx_display_has_full_frame_buf(disp)) {
            buf_area.x1 = 0;
            buf_area.y1 = 0;
            buf_area.x2 = (gfx_coord_t)disp->res.h_res;
            buf_area.y2 = (gfx_coord_t)disp->res.v_res;
        } else {
            buf_area.x1 = chunk_x1;
            buf_area.y1 = chunk_y1;
            buf_area.x2 = (gfx_coord_t)(chunk_x1 + dest_stride);
            buf_area.y2 = chunk_y2;
        }

        gfx_draw_ctx_t draw_ctx = {
            .buf = buf,
            .buf_area = buf_area,
            .clip_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 },
            .stride = dest_stride,
            .format = disp->format.render_format,
            .pixel_size = disp->format.render_pixel_size,
        };

        render_start_us = gfx_platform_time_us();
        if (disp->style.bg_enable) {
            gfx_area_t fill_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 };
            gfx_render_fill_area(disp, &draw_ctx, &fill_area, disp->style.bg_color, GFX_RENDER_OPA_COVER);
        }
        /*
         * Package UI draws first. Object tree may overlay (Anim/Motion hosted
         * beside arena panels). Pure object path when no arena is attached.
         */
        if (disp->gfx_arena_scene != NULL) {
            gfx_arena_scene_t *ascene = (gfx_arena_scene_t *)disp->gfx_arena_scene;
            gfx_render_surface_t surf = {
                .buf = draw_ctx.buf,
                .buf_area = draw_ctx.buf_area,
                .clip_area = draw_ctx.clip_area,
                .stride = draw_ctx.stride,
                .format = draw_ctx.format,
            };
            (void)gfx_arena_draw_clipped(disp, &ascene->arena, &surf);
        }
        gfx_render_draw_child_objects(disp, &draw_ctx);
        disp->render.render_time_us += (uint64_t)(gfx_platform_time_us() - render_start_us);

        if (disp->backend != NULL) {
            // uint32_t chunk_px = render_w * (uint32_t)(chunk_y2 - chunk_y1);

            bool is_last_chunk = (chunk_y2 >= render_area.y2);
            disp->render.flushing_last = is_last_chunk && is_last_area;

            // GFX_LOGD(TAG, "Flush: (%d,%d)-(%d,%d) %" PRIu32 " px%s",
            //          chunk_x1, chunk_y1, chunk_x2 - 1, chunk_y2 - 1, chunk_px,
            //          disp->render.flushing_last ? " (last)" : "");

            flush_start_us = gfx_platform_time_us();
            const void *flush_pixels = gfx_render_prepare_flush_pixels(disp, buf, chunk_x1, chunk_y1,
                                       chunk_x2, chunk_y2, dest_stride);
            if (flush_pixels == NULL ||
                    gfx_backend_flush(disp, chunk_x1, chunk_y1, chunk_x2, chunk_y2,
                                      flush_pixels, dest_stride) != GFX_OK) {
                GFX_LOGE(TAG, "render area[%d]: backend flush failed", area_idx);
                return;
            }
            if (gfx_backend_wait_flush(disp) != GFX_OK) {
                GFX_LOGE(TAG, "render area[%d]: backend wait failed", area_idx);
                return;
            }
            disp->render.flush_time_us += (uint64_t)(gfx_platform_time_us() - flush_start_us);
            disp->render.flush_count++;
        }

        cur_y = chunk_y2;
    }
}

/**
 * @brief Render all dirty areas
 * @param disp Display
 */
void gfx_render_dirty_areas(gfx_display_t *disp)
{
    if (disp == NULL) {
        return;
    }

    disp->render.render_time_us = 0;
    disp->render.flush_time_us = 0;
    disp->render.flush_count = 0;
    gfx_sw_blend_perf_reset(&disp->render.draw);
    gfx_sw_blend_perf_bind(&disp->render.draw);

    uint8_t last_area_idx = 0;
    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (!disp->dirty.merged[i]) {
            last_area_idx = i;
        }
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }
        gfx_area_t *area = &disp->dirty.areas[i];
        bool is_last_area = (i == last_area_idx);
        gfx_render_part_area(disp, area, i, is_last_area);
    }
    gfx_sw_blend_perf_unbind();
}

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags
 * @param disp Display
 */
void gfx_render_cleanup(gfx_display_t *disp)
{
    if (disp == NULL) {
        return;
    }

    if (disp->dirty.count > 0) {
        gfx_invalidate_area_disp(disp, NULL);
    }
}

/**
 * @brief Handle rendering of all objects in the scene
 * @param ctx Player context
 * @return true if any display was rendered, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx)
{
    uint32_t now_ms = gfx_timer_tick_get();
    gfx_timer_mgr_t *mgr = &ctx->timer_mgr;
    const uint32_t fps_sample_window = 100;

    if (mgr->render_fps_last_tick == 0) {
        mgr->render_fps_last_tick = now_ms;
    } else {
        uint32_t elapsed_ms = gfx_timer_tick_elaps(mgr->render_fps_last_tick);
        mgr->render_fps_samples++;
        mgr->render_fps_elapsed_ms += elapsed_ms;
        mgr->render_fps_last_tick = now_ms;

        if (mgr->render_fps_samples >= fps_sample_window) {
            mgr->actual_fps = (mgr->render_fps_samples * 1000) / mgr->render_fps_elapsed_ms;
            mgr->render_fps_samples = 0;
            mgr->render_fps_elapsed_ms = 0;
        }
    }

    bool did_render = false;

    for (gfx_display_t *disp = ctx->disp; disp != NULL; disp = disp->next) {
        int64_t frame_start_us = gfx_platform_time_us();
        gfx_refresh_update_layout_dirty(disp);

        /* Arena list inertia must tick even when dirty was cleared last frame. */
        if (disp->gfx_arena_scene != NULL) {
            (void)gfx_arena_scene_tick((gfx_arena_scene_t *)disp->gfx_arena_scene);
        }

        if (disp->dirty.count > 1) {
            gfx_refresh_merge_areas(disp);
        } else if (disp->dirty.count == 0) {
            continue;
        }

        /* Anim/Motion timers invalidate objects even when arena is attached. */
        gfx_render_update_child_objects(disp);

        uint32_t dirty_px = gfx_render_area_summary(disp);
        gfx_render_dirty_areas(disp);
        uint64_t frame_time_us = (uint64_t)(gfx_platform_time_us() - frame_start_us);
        disp->render.dirty_pixels = dirty_px;
        disp->render.frame_time_us = frame_time_us;

        if (dirty_px > 0) {
            did_render = true;
            uint32_t screen_px = disp->res.h_res * disp->res.v_res;
            float dirty_pct = (dirty_px * 100.0f) / (float)screen_px;
            GFX_LOGD(TAG,
                     "%.1f%% (%" PRIu64 "ms) (%" PRIu64 "|%" PRIu64 ")",
                     dirty_pct,
                     frame_time_us / 1000,
                     disp->render.render_time_us / 1000,
                     disp->render.flush_time_us / 1000);
        }

        gfx_render_cleanup(disp);
    }

    return did_render;
}
