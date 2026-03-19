/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "core/gfx_obj.h"

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

/**********************
 *   PUBLIC API
 **********************/

gfx_obj_t *gfx_mesh_img_create(gfx_disp_t *disp);
esp_err_t gfx_mesh_img_set_src(gfx_obj_t *obj, void *src);
esp_err_t gfx_mesh_img_set_grid(gfx_obj_t *obj, uint8_t cols, uint8_t rows);
size_t gfx_mesh_img_get_point_count(gfx_obj_t *obj);
esp_err_t gfx_mesh_img_get_point(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_t *point);
esp_err_t gfx_mesh_img_get_point_screen(gfx_obj_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y);
esp_err_t gfx_mesh_img_set_point(gfx_obj_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y);
esp_err_t gfx_mesh_img_reset_points(gfx_obj_t *obj);
esp_err_t gfx_mesh_img_set_ctrl_points_visible(gfx_obj_t *obj, bool visible);

#ifdef __cplusplus
}
#endif
