/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "core/gfx_obj.h"
#include "widget/gfx_img.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_coord_t x;
    gfx_coord_t y;
} gfx_mesh_img_point_t;

typedef struct {
    int32_t x_q8;
    int32_t y_q8;
} gfx_mesh_img_point_q8_t;

/**********************
 *   PUBLIC API
 **********************/

gfx_obj_t *gfx_mesh_img_create(gfx_disp_t *disp);
esp_err_t gfx_mesh_img_set_src_desc(gfx_obj_t *obj, const gfx_img_src_t *src);
esp_err_t gfx_mesh_img_set_src(gfx_obj_t *obj, void *src);
esp_err_t gfx_mesh_img_set_grid(gfx_obj_t *obj, uint8_t cols, uint8_t rows);
size_t gfx_mesh_img_get_point_count(gfx_obj_t *obj);
esp_err_t gfx_mesh_img_get_point(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_t *point);
esp_err_t gfx_mesh_img_get_point_screen(gfx_obj_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y);
esp_err_t gfx_mesh_img_get_point_q8(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_q8_t *point);
esp_err_t gfx_mesh_img_get_point_screen_q8(gfx_obj_t *obj, size_t point_idx, int32_t *x_q8, int32_t *y_q8);
esp_err_t gfx_mesh_img_set_point(gfx_obj_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y);
esp_err_t gfx_mesh_img_set_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);
esp_err_t gfx_mesh_img_set_point_q8(gfx_obj_t *obj, size_t point_idx, int32_t x_q8, int32_t y_q8);
esp_err_t gfx_mesh_img_set_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);
esp_err_t gfx_mesh_img_set_rest_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);
esp_err_t gfx_mesh_img_set_rest_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);
esp_err_t gfx_mesh_img_reset_points(gfx_obj_t *obj);
esp_err_t gfx_mesh_img_set_ctrl_points_visible(gfx_obj_t *obj, bool visible);

/**
 * @brief Enable inward-only edge anti-aliasing.
 *
 * When enabled, outer edges of this mesh fade from full opacity to transparent
 * towards the geometric boundary (inside the triangle) instead of drawing
 * semi-transparent pixels outside. Prevents visible "bleed" on thin strokes.
 *
 * @param obj   Mesh image object.
 * @param inward true = inward AA (no outward bleed); false = default outward AA.
 */
esp_err_t gfx_mesh_img_set_aa_inward(gfx_obj_t *obj, bool inward);

/**
 * @brief Treat first and last grid columns as adjacent (closed strip).
 *
 * When enabled, the left edge of the first column and the right edge of the
 * last column are marked as internal (shared), so edge AA does not fade them
 * to transparent. Use for closed stroke paths where the strip endpoints
 * coincide geometrically.
 */
esp_err_t gfx_mesh_img_set_wrap_cols(gfx_obj_t *obj, bool wrap);

/**
 * @brief Use scanline polygon fill instead of triangle rasterization.
 *
 * When enabled (grid_rows must be 1), the mesh outline is filled as a closed
 * polygon using a scanline rasterizer with edge AA.  No texture mapping —
 * fills with a solid color.  Avoids diagonal-seam artifacts inherent in
 * per-triangle inward AA.
 *
 * @param fill_color  Solid fill color (typically white for strokes).
 */
esp_err_t gfx_mesh_img_set_scanline_fill(gfx_obj_t *obj, bool enable, gfx_color_t fill_color);

#ifdef __cplusplus
}
#endif
