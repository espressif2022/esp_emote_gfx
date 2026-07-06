/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/runtime/gfx_core_priv.h"
#include "core/display/gfx_backend_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *buf;
    gfx_area_t buf_area;
    gfx_area_t clip_area;
    gfx_coord_t stride;
    gfx_color_format_t format;
} gfx_render_surface_t;

typedef struct {
    const void *pixels;
    gfx_coord_t stride;
    gfx_color_format_t format;
    const gfx_opa_t *alpha;
    gfx_coord_t alpha_stride;
} gfx_render_image_t;

typedef struct {
    gfx_color_t color;
    gfx_opa_t opa;
    uint16_t radius;
} gfx_round_rect_fill_dsc_t;

typedef struct {
    gfx_color_t color;
    gfx_opa_t opa;
    uint16_t radius;
    uint16_t width;
} gfx_round_rect_stroke_dsc_t;

/**
 * @brief Handle rendering of all objects in the scene (iterates over all displays)
 * @param ctx Player context
 * @return true if rendering was performed, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx);

/**
 * @brief Render all dirty areas for one display
 */
void gfx_render_dirty_areas(gfx_display_t *disp);

/**
 * @brief Render a single dirty area with dynamic height-based blocking
 * @param is_last_area true if this is the last dirty area in the list (flushing_last = last chunk of this area AND is_last_area)
 */
void gfx_render_part_area(gfx_display_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area);

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags for one display
 */
void gfx_render_cleanup(gfx_display_t *disp);

/**
 * @brief Print summary of dirty areas for one display
 * @return Total dirty pixels
 */
uint32_t gfx_render_area_summary(gfx_display_t *disp);

/**
 * @brief Draw child objects for one display using draw context (buf_area + clip_area)
 * @param ctx Draw context: buf, buf_area, clip_area, stride, format
 *            buf_area and clip_area use half-open bounds [x1, x2) x [y1, y2)
 */
void gfx_render_draw_child_objects(gfx_display_t *disp, const gfx_draw_ctx_t *ctx);

/**
 * @brief Draw one object and its descendants.
 *
 * This is used by composite widgets that own child objects but need custom
 * ordering/transform decisions before drawing them.
 */
void gfx_render_draw_object_tree(gfx_display_t *disp, gfx_object_t *obj, const gfx_draw_ctx_t *ctx);

/**
 * @brief Update child objects for one display
 */
void gfx_render_update_child_objects(gfx_display_t *disp);

/**
 * @brief Round a half-open area outward to backend/render alignment limits.
 *
 * The input and output use half-open bounds [x1, x2) x [y1, y2). The returned
 * area is clipped to limit when limit is not NULL. Width/height alignment of 0
 * or 1 means no alignment requirement.
 */
gfx_area_t gfx_render_roundup_area(const gfx_area_t *area,
                                   const gfx_render_alignment_t *alignment,
                                   const gfx_area_t *limit);

/**
 * @brief Round a byte count upward to the configured stride alignment.
 * @param stride_bytes Original byte count.
 * @param alignment Backend/render alignment metadata; NULL or <=1 means unchanged.
 * @return Aligned byte count.
 */
uint32_t gfx_render_roundup_stride_bytes(uint32_t stride_bytes,
        const gfx_render_alignment_t *alignment);

/**
 * @brief Check whether an address satisfies backend/render address alignment.
 * @param ptr Address to check.
 * @param alignment Backend/render alignment metadata; NULL or <=1 means aligned.
 * @return true when the address can be used by an aligned backend operation.
 */
bool gfx_render_is_addr_aligned(const void *ptr,
                                const gfx_render_alignment_t *alignment);

/**
 * @brief Check whether a backend op can use the destination surface/area.
 *
 * The check covers capability, destination address alignment, stride-byte
 * alignment, and width/height alignment. Callers still need to check the
 * operation function pointer and format-specific constraints.
 */
bool gfx_render_backend_op_can_use_dst(const gfx_backend_t *backend,
                                       uint32_t cap,
                                       const gfx_backend_surface_t *dst,
                                       const gfx_area_t *area);

/**
 * @brief Try to draw an image through backend blit/blend acceleration.
 *
 * Returns true only when a backend operation was accepted and completed
 * successfully. Callers must fall back to software drawing when false.
 */
bool gfx_render_backend_image(gfx_display_t *disp,
                              const gfx_draw_ctx_t *ctx,
                              const gfx_area_t *area,
                              const gfx_backend_image_t *src,
                              gfx_coord_t src_x,
                              gfx_coord_t src_y,
                              gfx_opa_t opa);

void gfx_render_surface_fill(gfx_display_t *disp,
                             const gfx_render_surface_t *dst,
                             const gfx_area_t *area,
                             gfx_color_t color,
                             gfx_opa_t opa);

void gfx_render_surface_rect_stroke(gfx_display_t *disp,
                                    const gfx_render_surface_t *dst,
                                    const gfx_area_t *area,
                                    uint16_t width,
                                    gfx_color_t color,
                                    gfx_opa_t opa);

void gfx_render_surface_round_rect_fill(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *area,
                                        const gfx_round_rect_fill_dsc_t *dsc);

void gfx_render_surface_round_rect_stroke(gfx_display_t *disp,
        const gfx_render_surface_t *dst,
        const gfx_area_t *area,
        const gfx_round_rect_stroke_dsc_t *dsc);

bool gfx_render_surface_blit_image(gfx_display_t *disp,
                                   const gfx_render_surface_t *dst,
                                   const gfx_area_t *area,
                                   const gfx_render_image_t *src,
                                   gfx_coord_t src_x,
                                   gfx_coord_t src_y,
                                   gfx_opa_t opa);

void gfx_render_surface_draw_mask(gfx_display_t *disp,
                                  const gfx_render_surface_t *dst,
                                  const gfx_area_t *area,
                                  const gfx_opa_t *mask,
                                  gfx_coord_t mask_stride,
                                  gfx_color_t color,
                                  gfx_opa_t opa);

void gfx_render_surface_draw_color_mask(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *area,
                                        const gfx_opa_t *mask,
                                        gfx_coord_t mask_stride,
                                        const gfx_color_t *color_mask,
                                        gfx_coord_t color_mask_stride,
                                        gfx_opa_t opa);

/**
 * @brief Try to draw a scaled image through backend scale acceleration.
 *
 * Returns true only when a backend scale operation was accepted and completed
 * successfully. `dst_area`, `clip_area`, and `src_area` use half-open bounds.
 * Callers must fall back to software drawing when false.
 */
bool gfx_render_backend_scale(gfx_display_t *disp,
                              const gfx_draw_ctx_t *ctx,
                              const gfx_area_t *dst_area,
                              const gfx_area_t *clip_area,
                              const gfx_backend_image_t *src,
                              const gfx_area_t *src_area,
                              gfx_opa_t opa);

bool gfx_render_surface_scale_image(gfx_display_t *disp,
                                    const gfx_render_surface_t *dst,
                                    const gfx_area_t *dst_area,
                                    const gfx_render_image_t *src,
                                    const gfx_area_t *src_area,
                                    gfx_opa_t opa);

/**
 * @brief Try to draw a rotated image through backend transform acceleration.
 *
 * `angle` uses clockwise degrees; only 0/90/180/270 are accepted by PPA today.
 * Returns true only when a backend transform operation completed successfully.
 */
bool gfx_render_backend_transform(gfx_display_t *disp,
                                  const gfx_draw_ctx_t *ctx,
                                  const gfx_area_t *dst_area,
                                  const gfx_area_t *clip_area,
                                  const gfx_backend_image_t *src,
                                  const gfx_area_t *src_area,
                                  int16_t angle,
                                  gfx_opa_t opa);

bool gfx_render_surface_transform_image(gfx_display_t *disp,
                                        const gfx_render_surface_t *dst,
                                        const gfx_area_t *dst_area,
                                        const gfx_render_image_t *src,
                                        const gfx_area_t *src_area,
                                        int16_t angle,
                                        gfx_opa_t opa);

#ifdef __cplusplus
}
#endif
