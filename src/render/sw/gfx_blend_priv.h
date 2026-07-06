/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "common/gfx_types_priv.h"
#include "gfx/display.h"

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
 * @brief Fill a rectangle in a destination surface.
 *
 * Supports RGB565/RGB565_SWAPPED/RGB888/XRGB8888 destinations. `area` uses
 * destination-local half-open coordinates.
 */
void gfx_sw_blend_surface_fill(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);

/**
 * @brief Mix two semantic RGB565 colors with a given mix ratio (internal)
 * @param c1 First semantic RGB565 color
 * @param c2 Second semantic RGB565 color
 * @param mix Mix ratio (0-255)
 * @param swap Legacy compatibility parameter; ignored. Inputs must already be semantic RGB565.
 * @return Mixed color
 */
gfx_color_t gfx_blend_color_mix(gfx_color_t c1, gfx_color_t c2, uint8_t mix);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*=====================
 * Software blending functions
 *====================*/

/**
 * @brief Draw a masked semantic color into a destination surface.
 *
 * Supports RGB565/RGB565_SWAPPED/RGB888/XRGB8888 destinations. `area` uses
 * destination-local half-open coordinates and `mask` points to area origin.
 */
void gfx_sw_blend_mask_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                gfx_color_format_t dest_format,
                                const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);

/**
 * @brief Draw masked text using per-pixel semantic colors into a destination surface.
 *
 * Supports RGB565/RGB565_SWAPPED/RGB888/XRGB8888 destinations. `area` uses
 * destination-local half-open coordinates and mask/color_mask point to area origin.
 */
void gfx_sw_blend_mask_color_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                      gfx_color_format_t dest_format,
                                      const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                      const gfx_color_t *color_mask, gfx_coord_t color_mask_stride,
                                      const gfx_area_t *area, gfx_opa_t opa);

/**
 * @brief Draw an image into a destination surface.
 *
 * Supports RGB565/RGB565_SWAPPED/RGB888/XRGB8888 destinations and
 * RGB565/RGB565_SWAPPED/RGB888/RGB888A8/XRGB8888/ARGB8888 sources.
 */
void gfx_sw_blend_img_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const void *src_buf, gfx_coord_t src_stride,
                               const gfx_opa_t *mask, gfx_coord_t mask_stride,
                               gfx_area_t *clip_area, gfx_color_format_t src_format,
                               gfx_opa_t opa);

/**
 * @brief Draw a scaled image area into a destination surface.
 */
void gfx_sw_blend_img_scale_draw_fmt(void *dest_buf, gfx_coord_t dest_stride,
                                     gfx_color_format_t dest_format,
                                     const void *src_buf, gfx_coord_t src_stride,
                                     const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                     const gfx_area_t *dst_area, const gfx_area_t *clip_area,
                                     const gfx_area_t *src_area,
                                     gfx_color_format_t src_format, gfx_opa_t opa);

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
 * @param src_format Source image color format.
 */
void gfx_sw_blend_img_triangle_draw(void *dest_buf, gfx_coord_t dest_stride,
                                    gfx_color_format_t dest_format,
                                    const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                                    const void *src_buf, gfx_coord_t src_stride, gfx_coord_t src_height,
                                    const gfx_opa_t *mask, gfx_coord_t mask_stride,
                                    gfx_opa_t opa,
                                    const gfx_sw_blend_img_vertex_t *v0,
                                    const gfx_sw_blend_img_vertex_t *v1,
                                    const gfx_sw_blend_img_vertex_t *v2,
                                    uint8_t internal_edges,
                                    const gfx_sw_blend_aa_edge_t *extra_aa_edges,
                                    uint8_t extra_aa_count,
                                    gfx_color_format_t src_format);

/**
 * @brief Scanline polygon fill with edge anti-aliasing.
 *
 * Fills a closed polygon defined by vertex arrays with a solid color.
 * Uses even-odd fill rule. Vertices are in mesh subpixel coordinates
 * (same as gfx_sw_blend_img_vertex_t x/y).
 *
 * Designed for stroke outlines where no texture mapping is needed.
 */
void gfx_sw_blend_polygon_fill(void *dest_buf, gfx_coord_t dest_stride,
                               gfx_color_format_t dest_format,
                               const gfx_area_t *buf_area, const gfx_area_t *clip_area,
                               gfx_color_t color,
                               gfx_opa_t opa,
                               const int32_t *vx, const int32_t *vy,
                               int vertex_count);

void gfx_sw_blend_perf_reset(gfx_draw_perf_stats_t *stats);
void gfx_sw_blend_perf_bind(gfx_draw_perf_stats_t *stats);
void gfx_sw_blend_perf_unbind(void);

#ifdef __cplusplus
}
#endif
