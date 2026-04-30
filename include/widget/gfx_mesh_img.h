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

/**
 * @brief Mesh control point in integer pixel coordinates.
 *
 * Use this type when callers only need whole-pixel positioning.
 * For subpixel precision, use `gfx_mesh_img_point_q8_t`.
 */
typedef struct {
    gfx_coord_t x;
    gfx_coord_t y;
} gfx_mesh_img_point_t;

/**
 * @brief Mesh control point in Q8 fixed-point coordinates.
 *
 * This variant is intended for subpixel-accurate deformation and animation.
 * Compared with `gfx_mesh_img_point_t`, it preserves 1/256 pixel precision.
 */
typedef struct {
    int32_t x_q8;
    int32_t y_q8;
} gfx_mesh_img_point_q8_t;

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create a mesh-image object on a display.
 *
 * A mesh-image widget deforms an image through a regular point grid.
 * Compared with `gfx_img_create()`, this widget supports per-point warp.
 *
 * @param disp Display that owns the object
 * @return Created object, or NULL on failure
 */
gfx_obj_t *gfx_mesh_img_create(gfx_disp_t *disp);

/**
 * @brief Set a typed image source descriptor for the mesh.
 *
 * This is the preferred source setter for new code. It keeps the source type
 * explicit and aligned with the image widget API.
 *
 * @param obj Mesh-image object
 * @param src Typed image source descriptor
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_src_desc(gfx_obj_t *obj, const gfx_img_src_t *src);

/**
 * @brief Set the image source for the mesh.
 *
 * Compatibility wrapper for direct `gfx_image_dsc_t *` payloads.
 *
 * @param obj Mesh-image object
 * @param src In-memory image source payload
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_src(gfx_obj_t *obj, void *src);

/**
 * @brief Configure mesh grid density.
 *
 * `cols` and `rows` describe the number of cells, not points.
 * The actual point count is `(cols + 1) * (rows + 1)`.
 *
 * @param obj Mesh-image object
 * @param cols Horizontal cell count
 * @param rows Vertical cell count
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_grid(gfx_obj_t *obj, uint8_t cols, uint8_t rows);

/**
 * @brief Get the current mesh point count.
 *
 * This reflects the current grid configuration.
 *
 * @param obj Mesh-image object
 * @return Number of points in the current mesh
 */
size_t gfx_mesh_img_get_point_count(gfx_obj_t *obj);

/**
 * @brief Get one mesh point in object-local pixel coordinates.
 *
 * Compared with `gfx_mesh_img_get_point_screen()`, this returns coordinates
 * relative to the mesh object itself.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param point Output point
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_get_point(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_t *point);

/**
 * @brief Get one mesh point in screen coordinates.
 *
 * Compared with `gfx_mesh_img_get_point()`, this includes the current object
 * position and alignment result.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param x Output screen x
 * @param y Output screen y
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_get_point_screen(gfx_obj_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y);

/**
 * @brief Get one mesh point in object-local Q8 coordinates.
 *
 * Compared with `gfx_mesh_img_get_point()`, this preserves subpixel precision.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param point Output Q8 point
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_get_point_q8(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_q8_t *point);

/**
 * @brief Get one mesh point in screen-space Q8 coordinates.
 *
 * This combines the current object position with the point's subpixel value.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param x_q8 Output screen x in Q8
 * @param y_q8 Output screen y in Q8
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_get_point_screen_q8(gfx_obj_t *obj, size_t point_idx, int32_t *x_q8, int32_t *y_q8);

/**
 * @brief Set one mesh point in object-local pixel coordinates.
 *
 * For subpixel updates, use `gfx_mesh_img_set_point_q8()`.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param x Local x
 * @param y Local y
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_point(gfx_obj_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y);

/**
 * @brief Set all mesh points in object-local pixel coordinates.
 *
 * The caller must provide exactly the current point count.
 * For subpixel updates, use `gfx_mesh_img_set_points_q8()`.
 *
 * @param obj Mesh-image object
 * @param points Point array
 * @param point_count Number of entries in `points`
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);

/**
 * @brief Set one mesh point in object-local Q8 coordinates.
 *
 * Compared with `gfx_mesh_img_set_point()`, this keeps subpixel precision.
 *
 * @param obj Mesh-image object
 * @param point_idx Point index in the current grid
 * @param x_q8 Local x in Q8
 * @param y_q8 Local y in Q8
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_point_q8(gfx_obj_t *obj, size_t point_idx, int32_t x_q8, int32_t y_q8);

/**
 * @brief Set all mesh points in object-local Q8 coordinates.
 *
 * This is the preferred batch API for smooth deformation animation.
 *
 * @param obj Mesh-image object
 * @param points Q8 point array
 * @param point_count Number of entries in `points`
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);

/**
 * @brief Set the rest pose points in object-local pixel coordinates.
 *
 * Rest points define the undeformed reference mesh used for texture sampling
 * and later reset operations. Compared with `gfx_mesh_img_set_points()`, this
 * updates the reference pose instead of only the current deformation.
 *
 * @param obj Mesh-image object
 * @param points Point array
 * @param point_count Number of entries in `points`
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_rest_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);

/**
 * @brief Set the rest pose points in object-local Q8 coordinates.
 *
 * Compared with `gfx_mesh_img_set_rest_points()`, this keeps subpixel
 * precision in the reference pose.
 *
 * @param obj Mesh-image object
 * @param points Q8 point array
 * @param point_count Number of entries in `points`
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_rest_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);

/**
 * @brief Reset current points back to the stored rest pose.
 *
 * @param obj Mesh-image object
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_reset_points(gfx_obj_t *obj);

/**
 * @brief Show or hide mesh control points for debugging.
 *
 * This affects only debug visualization and does not change the deformation.
 *
 * @param obj Mesh-image object
 * @param visible Whether control points should be drawn
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_ctrl_points_visible(gfx_obj_t *obj, bool visible);

/**
 * @brief Set uniform mesh opacity.
 *
 * This value is multiplied with any source alpha and anti-aliasing coverage.
 *
 * @param obj Mesh image object
 * @param opa Uniform opacity (0-255)
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_opa(gfx_obj_t *obj, gfx_opa_t opa);

/**
 * @brief Enable inward-only edge anti-aliasing.
 *
 * When enabled, outer edges of this mesh fade from full opacity to transparent
 * towards the geometric boundary (inside the triangle) instead of drawing
 * semi-transparent pixels outside. Prevents visible "bleed" on thin strokes.
 *
 * @param obj   Mesh image object.
 * @param inward true = inward AA (no outward bleed); false = default outward AA.
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_aa_inward(gfx_obj_t *obj, bool inward);

/**
 * @brief Treat first and last grid columns as adjacent (closed strip).
 *
 * When enabled, the left edge of the first column and the right edge of the
 * last column are marked as internal (shared), so edge AA does not fade them
 * to transparent. Use for closed stroke paths where the strip endpoints
 * coincide geometrically.
 *
 * Compared with `gfx_mesh_img_set_aa_inward()`, this changes edge topology
 * interpretation rather than AA direction.
 *
 * @return ESP_OK on success, ESP_ERR_* otherwise
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
 * Compared with the default textured-triangle mode, this is intended for
 * stroke-like meshes where a solid-color fill is preferable.
 *
 * @param obj Mesh image object
 * @param enable Whether scanline fill mode should be enabled
 * @param fill_color  Solid fill color (typically white for strokes).
 * @return ESP_OK on success, ESP_ERR_* otherwise
 */
esp_err_t gfx_mesh_img_set_scanline_fill(gfx_obj_t *obj, bool enable, gfx_color_t fill_color);

#ifdef __cplusplus
}
#endif
