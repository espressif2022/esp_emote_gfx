/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "core/gfx_types.h"
#include "core/gfx_disp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/**
 * Bit flag OR-ed into the `internal_edges` parameter of
 * gfx_sw_blend_img_triangle_draw to request inward-only edge AA.
 * When set, the rasterizer fades pixels near non-internal outer edges
 * from inside the triangle rather than drawing semi-transparent pixels
 * outside it. This prevents visible "bleed" on thin strokes (eyebrows).
 */
#define GFX_BLEND_TRI_AA_INWARD 0x80

#define GFX_BLEND_MAX_EXTRA_AA_EDGES 2

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int32_t x;
    int32_t y;
    gfx_coord_t u;
    gfx_coord_t v;
} gfx_sw_blend_img_vertex_t;

/**
 * Extra directed edge for cross-triangle inward AA.
 * Represents edge A→B in mesh subpixel coordinates.
 */
typedef struct {
    int32_t a;     /**< dy: B.y - A.y */
    int32_t b;     /**< -dx: A.x - B.x */
    int32_t len;   /**< sqrt(a² + b²) */
    int32_t vx;    /**< reference vertex x (A.x, mesh subpixel) */
    int32_t vy;    /**< reference vertex y (A.y, mesh subpixel) */
} gfx_sw_blend_aa_edge_t;

/**********************
 *      PRIVATE FUNCTIONS
 **********************/

/**
 * @brief Fast fill buffer with background color
 * @param buf Pointer to uint16_t buffer
 * @param color 16-bit color value
 * @param pixels Number of pixels to fill
 */
void gfx_sw_blend_fill(uint16_t *buf, uint16_t color, size_t pixels);

/**
 * @brief Fill a rectangle in dest buffer (standard blend form: dest_buf + stride + area)
 * @param dest_buf Destination buffer (uint16_t)
 * @param dest_stride Row stride in pixels
 * @param area Area to fill (x1,y1,x2,y2 exclusive end)
 * @param color 16-bit color value in native framebuffer byte order
 */
void gfx_sw_blend_fill_area(uint16_t *dest_buf, gfx_coord_t dest_stride,
                            const gfx_area_t *area, uint16_t color);

/**
 * @brief Mix two colors with a given mix ratio (internal)
 * @param c1 First color
 * @param c2 Second color
 * @param mix Mix ratio (0-255)
 * @param swap Whether to swap color format
 * @return Mixed color
 */
gfx_color_t gfx_blend_color_mix(gfx_color_t c1, gfx_color_t c2, uint8_t mix, bool swap);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Software blending functions
 *====================*/

/**
 * @brief Draw a blended color onto a destination buffer
 * @param dest_buf Pointer to the destination buffer where the color will be drawn
 * @param dest_stride Stride (width) of the destination buffer
 * @param mask Pointer to the mask buffer, if any
 * @param mask_stride Stride (width) of the mask buffer
 * @param clip_area Pointer to the clipping area, which limits the area to draw
 * @param color The color to draw in gfx_color_t type
 * @param opa The opacity of the color to draw (0-255)
 * @param swap Whether to swap the color format
 */
void gfx_sw_blend_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                       const gfx_opa_t *mask, gfx_coord_t mask_stride,
                       gfx_area_t *clip_area, gfx_color_t color, gfx_opa_t opa, bool swap);

/**
 * @brief Draw a blended image onto a destination buffer
 * @param dest_buf Pointer to the destination buffer where the image will be drawn
 * @param dest_stride Stride (width) of the destination buffer
 * @param src_buf Pointer to the source image buffer
 * @param src_stride Stride (width) of the source image buffer
 * @param mask Pointer to the mask buffer, if any
 * @param mask_stride Stride (width) of the mask buffer
 * @param clip_area Pointer to the clipping area, which limits the area to draw
 * @param swap Whether to swap the color format
 */
void gfx_sw_blend_img_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                           const gfx_color_t *src_buf, gfx_coord_t src_stride,
                           const gfx_opa_t *mask, gfx_coord_t mask_stride,
                           gfx_area_t *clip_area, bool swap);

/**
 * @brief Draw a textured triangle with edge anti-aliasing
 *
 * @param internal_edges Bitmask of edges shared with adjacent triangles.
 *        Bit 0 = edge 0 (v1→v2), bit 1 = edge 1 (v2→v0), bit 2 = edge 2 (v0→v1).
 *        AA is suppressed on flagged edges to prevent dark-seam artifacts.
 *        Pass 0 for standalone triangles (full AA on all edges).
 * @param extra_aa_edges  Optional array of extra directed edges for cross-triangle
 *        inward AA distance (NULL when not needed).
 * @param extra_aa_count  Number of entries in extra_aa_edges (0..MAX_EXTRA_AA_EDGES).
 */
void gfx_sw_blend_img_triangle_draw(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                                    const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                    const gfx_color_t *src_buf, gfx_coord_t src_stride, gfx_coord_t src_height,
                                    const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                    gfx_opa_t opa,
                                    const gfx_sw_blend_img_vertex_t *v0,
                                    const gfx_sw_blend_img_vertex_t *v1,
                                    const gfx_sw_blend_img_vertex_t *v2,
                                    uint8_t internal_edges,
                                    const gfx_sw_blend_aa_edge_t *extra_aa_edges,
                                    uint8_t extra_aa_count,
                                    bool swap);

/**
 * @brief Scanline polygon fill with edge anti-aliasing.
 *
 * Fills a closed polygon defined by vertex arrays with a solid color.
 * Uses even-odd fill rule. Vertices are in mesh subpixel coordinates
 * (same as gfx_sw_blend_img_vertex_t x/y).
 *
 * Designed for stroke outlines where no texture mapping is needed.
 */
void gfx_sw_blend_polygon_fill(gfx_color_t *dest_buf, gfx_coord_t dest_stride,
                               const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                               gfx_color_t color,
                               gfx_opa_t opa,
                               const int32_t *vx, const int32_t *vy,
                               int vertex_count,
                               bool swap);

void gfx_sw_blend_perf_reset(gfx_blend_perf_stats_t *stats);
void gfx_sw_blend_perf_bind(gfx_blend_perf_stats_t *stats);
void gfx_sw_blend_perf_unbind(void);

#ifdef __cplusplus
}
#endif
