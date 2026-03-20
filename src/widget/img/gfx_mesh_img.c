/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/draw/gfx_sw_draw_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/gfx_mesh_img.h"
#include "widget/img/gfx_img_dec_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_MESH_IMAGE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_MESH_IMAGE, TAG)
#define GFX_MESH_IMG_DEFAULT_COLS      1U
#define GFX_MESH_IMG_DEFAULT_ROWS      1U
#define GFX_MESH_IMG_CTRL_POINT_RADIUS 2

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    void *image_src;
    gfx_image_header_t header;
    uint8_t grid_cols;
    uint8_t grid_rows;
    bool ctrl_points_visible;
    gfx_coord_t bounds_min_x;
    gfx_coord_t bounds_min_y;
    gfx_coord_t bounds_max_x;
    gfx_coord_t bounds_max_y;
    size_t point_count;
    gfx_mesh_img_point_t *rest_points;
    gfx_mesh_img_point_t *points;
} gfx_mesh_img_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "mesh_img";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static esp_err_t gfx_mesh_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_mesh_img_delete_impl(gfx_obj_t *obj);
static void gfx_mesh_img_free_points(gfx_mesh_img_t *mesh);
static esp_err_t gfx_mesh_img_alloc_points(gfx_mesh_img_t *mesh, uint8_t cols, uint8_t rows);
static void gfx_mesh_img_update_bounds(gfx_obj_t *obj, gfx_mesh_img_t *mesh);
static void gfx_mesh_img_reset_rest_points(gfx_mesh_img_t *mesh);
static void gfx_mesh_img_get_draw_origin(gfx_obj_t *obj, const gfx_mesh_img_t *mesh, gfx_coord_t *x, gfx_coord_t *y);
static esp_err_t gfx_mesh_img_load_header(void *src, gfx_image_header_t *header);
static esp_err_t gfx_mesh_img_prepare_decoder(const gfx_mesh_img_t *mesh, gfx_image_decoder_dsc_t *decoder_dsc);
static void gfx_mesh_img_draw_ctrl_points(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx, const gfx_mesh_img_t *mesh);

static const gfx_widget_class_t s_gfx_mesh_img_widget_class = {
    .type = GFX_OBJ_TYPE_MESH_IMAGE,
    .name = "mesh_img",
    .draw = gfx_mesh_img_draw,
    .delete = gfx_mesh_img_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_mesh_img_free_points(gfx_mesh_img_t *mesh)
{
    if (mesh == NULL) {
        return;
    }

    if (mesh->rest_points != NULL) {
        free(mesh->rest_points);
        mesh->rest_points = NULL;
    }

    if (mesh->points != NULL) {
        free(mesh->points);
        mesh->points = NULL;
    }

    mesh->point_count = 0;
}

static esp_err_t gfx_mesh_img_alloc_points(gfx_mesh_img_t *mesh, uint8_t cols, uint8_t rows)
{
    size_t point_count;

    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_ARG, TAG, "mesh state is NULL");
    ESP_RETURN_ON_FALSE(cols > 0U && rows > 0U, ESP_ERR_INVALID_ARG, TAG, "grid must be at least 1x1");

    point_count = (size_t)(cols + 1U) * (size_t)(rows + 1U);
    gfx_mesh_img_free_points(mesh);

    mesh->rest_points = calloc(point_count, sizeof(gfx_mesh_img_point_t));
    ESP_RETURN_ON_FALSE(mesh->rest_points != NULL, ESP_ERR_NO_MEM, TAG, "no mem for rest points");

    mesh->points = calloc(point_count, sizeof(gfx_mesh_img_point_t));
    if (mesh->points == NULL) {
        free(mesh->rest_points);
        mesh->rest_points = NULL;
        return ESP_ERR_NO_MEM;
    }

    mesh->grid_cols = cols;
    mesh->grid_rows = rows;
    mesh->point_count = point_count;
    return ESP_OK;
}

static void gfx_mesh_img_reset_rest_points(gfx_mesh_img_t *mesh)
{
    int32_t width;
    int32_t height;
    size_t index = 0;

    if (mesh == NULL || mesh->rest_points == NULL || mesh->points == NULL ||
            mesh->grid_cols == 0U || mesh->grid_rows == 0U) {
        return;
    }

    width = (mesh->header.w > 0U) ? ((int32_t)mesh->header.w - 1) : 0;
    height = (mesh->header.h > 0U) ? ((int32_t)mesh->header.h - 1) : 0;

    for (uint8_t row = 0; row <= mesh->grid_rows; row++) {
        int32_t y = (mesh->grid_rows > 0U) ? ((height * row) / mesh->grid_rows) : 0;

        for (uint8_t col = 0; col <= mesh->grid_cols; col++) {
            int32_t x = (mesh->grid_cols > 0U) ? ((width * col) / mesh->grid_cols) : 0;

            mesh->rest_points[index].x = (gfx_coord_t)x;
            mesh->rest_points[index].y = (gfx_coord_t)y;
            mesh->points[index] = mesh->rest_points[index];
            index++;
        }
    }
}

static void gfx_mesh_img_update_bounds(gfx_obj_t *obj, gfx_mesh_img_t *mesh)
{
    gfx_coord_t min_x;
    gfx_coord_t min_y;
    gfx_coord_t max_x;
    gfx_coord_t max_y;

    if (obj == NULL || mesh == NULL) {
        return;
    }

    if (mesh->points == NULL || mesh->point_count == 0U) {
        obj->geometry.width = mesh->header.w;
        obj->geometry.height = mesh->header.h;
        mesh->bounds_min_x = 0;
        mesh->bounds_min_y = 0;
        mesh->bounds_max_x = (mesh->header.w > 0U) ? (gfx_coord_t)(mesh->header.w - 1U) : 0;
        mesh->bounds_max_y = (mesh->header.h > 0U) ? (gfx_coord_t)(mesh->header.h - 1U) : 0;
        return;
    }

    min_x = mesh->points[0].x;
    min_y = mesh->points[0].y;
    max_x = mesh->points[0].x;
    max_y = mesh->points[0].y;

    for (size_t i = 1; i < mesh->point_count; i++) {
        min_x = MIN(min_x, mesh->points[i].x);
        min_y = MIN(min_y, mesh->points[i].y);
        max_x = MAX(max_x, mesh->points[i].x);
        max_y = MAX(max_y, mesh->points[i].y);
    }

    mesh->bounds_min_x = min_x;
    mesh->bounds_min_y = min_y;
    mesh->bounds_max_x = max_x;
    mesh->bounds_max_y = max_y;
    obj->geometry.width = (uint16_t)(max_x - min_x + 1);
    obj->geometry.height = (uint16_t)(max_y - min_y + 1);
}

static void gfx_mesh_img_get_draw_origin(gfx_obj_t *obj, const gfx_mesh_img_t *mesh, gfx_coord_t *x, gfx_coord_t *y)
{
    if (obj == NULL || mesh == NULL || x == NULL || y == NULL) {
        return;
    }

    gfx_obj_calc_pos_in_parent(obj);
    *x = obj->geometry.x - mesh->bounds_min_x;
    *y = obj->geometry.y - mesh->bounds_min_y;
}

static esp_err_t gfx_mesh_img_load_header(void *src, gfx_image_header_t *header)
{
    gfx_image_decoder_dsc_t dsc = {
        .src = src,
    };

    ESP_RETURN_ON_FALSE(src != NULL, ESP_ERR_INVALID_ARG, TAG, "source is NULL");
    ESP_RETURN_ON_FALSE(header != NULL, ESP_ERR_INVALID_ARG, TAG, "header is NULL");

    return gfx_image_decoder_info(&dsc, header);
}

static esp_err_t gfx_mesh_img_prepare_decoder(const gfx_mesh_img_t *mesh, gfx_image_decoder_dsc_t *decoder_dsc)
{
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_ARG, TAG, "mesh state is NULL");
    ESP_RETURN_ON_FALSE(decoder_dsc != NULL, ESP_ERR_INVALID_ARG, TAG, "decoder desc is NULL");

    decoder_dsc->src = mesh->image_src;
    decoder_dsc->header = mesh->header;
    decoder_dsc->data = NULL;
    decoder_dsc->data_size = 0;
    decoder_dsc->user_data = NULL;

    return gfx_image_decoder_open(decoder_dsc);
}

static void gfx_mesh_img_draw_ctrl_points(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx, const gfx_mesh_img_t *mesh)
{
    gfx_coord_t origin_x;
    gfx_coord_t origin_y;
    gfx_color_t outer_color = GFX_COLOR_HEX(0x2dd4bf);
    gfx_color_t inner_color = GFX_COLOR_HEX(0xf8fafc);

    if (obj == NULL || ctx == NULL || mesh == NULL || !mesh->ctrl_points_visible) {
        return;
    }

    gfx_mesh_img_get_draw_origin(obj, mesh, &origin_x, &origin_y);

    for (size_t i = 0; i < mesh->point_count; i++) {
        gfx_coord_t screen_x = origin_x + mesh->points[i].x;
        gfx_coord_t screen_y = origin_y + mesh->points[i].y;

        for (gfx_coord_t dy = -GFX_MESH_IMG_CTRL_POINT_RADIUS; dy <= GFX_MESH_IMG_CTRL_POINT_RADIUS; dy++) {
            for (gfx_coord_t dx = -GFX_MESH_IMG_CTRL_POINT_RADIUS; dx <= GFX_MESH_IMG_CTRL_POINT_RADIUS; dx++) {
                gfx_color_t color = (dx == 0 && dy == 0) ? inner_color : outer_color;
                gfx_sw_draw_point((gfx_color_t *)ctx->buf, ctx->stride,
                                  &ctx->buf_area, &ctx->clip_area,
                                  screen_x + dx, screen_y + dy,
                                  color, 0xFF, ctx->swap);
            }
        }
    }
}

static esp_err_t gfx_mesh_img_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_mesh_img_t *mesh;
    gfx_image_decoder_dsc_t decoder_dsc;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_coord_t origin_x;
    gfx_coord_t origin_y;
    gfx_coord_t src_stride;
    gfx_coord_t src_height;
    gfx_color_format_t color_format;
    const gfx_color_t *src_pixels;
    const gfx_opa_t *alpha_mask = NULL;

    if (obj == NULL || obj->src == NULL || ctx == NULL) {
        GFX_LOGE(TAG, "draw mesh image: invalid object or source");
        return ESP_ERR_INVALID_ARG;
    }

    if (obj->type != GFX_OBJ_TYPE_MESH_IMAGE) {
        GFX_LOGE(TAG, "draw mesh image: object type mismatch");
        return ESP_ERR_INVALID_ARG;
    }

    mesh = (gfx_mesh_img_t *)obj->src;
    if (mesh->image_src == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    color_format = (gfx_color_format_t)mesh->header.cf;
    if (color_format != GFX_COLOR_FORMAT_RGB565 && color_format != GFX_COLOR_FORMAT_RGB565A8) {
        GFX_LOGW(TAG, "draw mesh image: unsupported color format %u", color_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_prepare_decoder(mesh, &decoder_dsc), TAG, "draw mesh image: open decoder failed");

    if (decoder_dsc.data == NULL) {
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_ERR_INVALID_STATE;
    }

    gfx_mesh_img_get_draw_origin(obj, mesh, &origin_x, &origin_y);

    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;

    if (!gfx_area_intersect(&clip_area, &ctx->clip_area, &obj_area)) {
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_OK;
    }

    src_stride = (mesh->header.stride > 0U) ? (gfx_coord_t)(mesh->header.stride / GFX_PIXEL_SIZE_16BPP) : (gfx_coord_t)mesh->header.w;
    src_height = (gfx_coord_t)mesh->header.h;
    src_pixels = (const gfx_color_t *)decoder_dsc.data;

    if (color_format == GFX_COLOR_FORMAT_RGB565A8) {
        alpha_mask = (const gfx_opa_t *)((const uint8_t *)decoder_dsc.data +
                                         (size_t)src_stride * src_height * GFX_PIXEL_SIZE_16BPP);
    }

    for (uint8_t row = 0; row < mesh->grid_rows; row++) {
        size_t row_offset = (size_t)row * (mesh->grid_cols + 1U);

        for (uint8_t col = 0; col < mesh->grid_cols; col++) {
            size_t idx00 = row_offset + col;
            size_t idx10 = idx00 + 1U;
            size_t idx01 = idx00 + mesh->grid_cols + 1U;
            size_t idx11 = idx01 + 1U;

            /* Early clip-out: skip cells entirely outside clip area */
            {
                gfx_coord_t p0x = mesh->points[idx00].x, p1x = mesh->points[idx10].x;
                gfx_coord_t p2x = mesh->points[idx01].x, p3x = mesh->points[idx11].x;
                gfx_coord_t p0y = mesh->points[idx00].y, p1y = mesh->points[idx10].y;
                gfx_coord_t p2y = mesh->points[idx01].y, p3y = mesh->points[idx11].y;
                gfx_coord_t cmin_x = MIN(MIN(p0x, p1x), MIN(p2x, p3x)) + origin_x;
                gfx_coord_t cmax_x = MAX(MAX(p0x, p1x), MAX(p2x, p3x)) + origin_x;
                gfx_coord_t cmin_y = MIN(MIN(p0y, p1y), MIN(p2y, p3y)) + origin_y;
                gfx_coord_t cmax_y = MAX(MAX(p0y, p1y), MAX(p2y, p3y)) + origin_y;
                if (cmax_x < clip_area.x1 || cmin_x >= clip_area.x2 ||
                    cmax_y < clip_area.y1 || cmin_y >= clip_area.y2) {
                    continue;
                }
            }

            gfx_sw_blend_img_vertex_t tri1[3];
            gfx_sw_blend_img_vertex_t tri2[3];

            /*
             * Internal-edge masks: suppress AA on edges shared with a
             * neighbouring triangle to prevent dark-seam artefacts.
             *
             * Tri1 (v0=TL, v1=TR, v2=BR):
             *   edge 0 (TR→BR) = right  — internal when col+1 < cols
             *   edge 1 (BR→TL) = diag   — always internal
             *   edge 2 (TL→TR) = top    — internal when row > 0
             *
             * Tri2 (v0=TL, v1=BR, v2=BL):
             *   edge 0 (BR→BL) = bottom — internal when row+1 < rows
             *   edge 1 (BL→TL) = left   — internal when col > 0
             *   edge 2 (TL→BR) = diag   — always internal
             */
            uint8_t ie1 = 0x02;
            uint8_t ie2 = 0x04;
            if (col + 1U < mesh->grid_cols) { ie1 |= 0x01; }
            if (row > 0U)                   { ie1 |= 0x04; }
            if (row + 1U < mesh->grid_rows) { ie2 |= 0x01; }
            if (col > 0U)                   { ie2 |= 0x02; }

            /* --------- Triangle 1 --------- */
            tri1[0].x = origin_x + mesh->points[idx00].x;
            tri1[0].y = origin_y + mesh->points[idx00].y;
            tri1[0].u = mesh->rest_points[idx00].x;
            tri1[0].v = mesh->rest_points[idx00].y;

            tri1[1].x = origin_x + mesh->points[idx10].x;
            tri1[1].y = origin_y + mesh->points[idx10].y;
            tri1[1].u = mesh->rest_points[idx10].x;
            tri1[1].v = mesh->rest_points[idx10].y;

            tri1[2].x = origin_x + mesh->points[idx11].x;
            tri1[2].y = origin_y + mesh->points[idx11].y;
            tri1[2].u = mesh->rest_points[idx11].x;
            tri1[2].v = mesh->rest_points[idx11].y;

            /* --------- Triangle 2 --------- */
            tri2[0].x = origin_x + mesh->points[idx00].x;
            tri2[0].y = origin_y + mesh->points[idx00].y;
            tri2[0].u = mesh->rest_points[idx00].x;
            tri2[0].v = mesh->rest_points[idx00].y;

            tri2[1].x = origin_x + mesh->points[idx11].x;
            tri2[1].y = origin_y + mesh->points[idx11].y;
            tri2[1].u = mesh->rest_points[idx11].x;
            tri2[1].v = mesh->rest_points[idx11].y;

            tri2[2].x = origin_x + mesh->points[idx01].x;
            tri2[2].y = origin_y + mesh->points[idx01].y;
            tri2[2].u = mesh->rest_points[idx01].x;
            tri2[2].v = mesh->rest_points[idx01].y;

            gfx_sw_blend_img_triangle_draw((gfx_color_t *)ctx->buf, ctx->stride,
                                           &ctx->buf_area, &clip_area,
                                           src_pixels, src_stride, src_height,
                                           alpha_mask, alpha_mask ? src_stride : 0,
                                           &tri1[0], &tri1[1], &tri1[2],
                                           ie1, ctx->swap);
            gfx_sw_blend_img_triangle_draw((gfx_color_t *)ctx->buf, ctx->stride,
                                           &ctx->buf_area, &clip_area,
                                           src_pixels, src_stride, src_height,
                                           alpha_mask, alpha_mask ? src_stride : 0,
                                           &tri2[0], &tri2[1], &tri2[2],
                                           ie2, ctx->swap);
        }
    }

    gfx_mesh_img_draw_ctrl_points(obj, ctx, mesh);
    gfx_image_decoder_close(&decoder_dsc);
    return ESP_OK;
}

static esp_err_t gfx_mesh_img_delete_impl(gfx_obj_t *obj)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);

    mesh = (gfx_mesh_img_t *)obj->src;
    if (mesh != NULL) {
        gfx_mesh_img_free_points(mesh);
        free(mesh);
        obj->src = NULL;
    }

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_mesh_img_create(gfx_disp_t *disp)
{
    gfx_obj_t *obj;
    gfx_mesh_img_t *mesh;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create mesh image: display is NULL");
        return NULL;
    }

    mesh = calloc(1, sizeof(gfx_mesh_img_t));
    if (mesh == NULL) {
        GFX_LOGE(TAG, "create mesh image: no mem for state");
        return NULL;
    }

    if (gfx_mesh_img_alloc_points(mesh, GFX_MESH_IMG_DEFAULT_COLS, GFX_MESH_IMG_DEFAULT_ROWS) != ESP_OK) {
        free(mesh);
        return NULL;
    }

    if (gfx_obj_create_class_instance(disp, &s_gfx_mesh_img_widget_class,
                                      mesh, 0, 0, "gfx_mesh_img_create", &obj) != ESP_OK) {
        gfx_mesh_img_free_points(mesh);
        free(mesh);
        return NULL;
    }

    return obj;
}

esp_err_t gfx_mesh_img_set_src(gfx_obj_t *obj, void *src)
{
    gfx_mesh_img_t *mesh;
    gfx_image_header_t header;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(src != NULL, ESP_ERR_INVALID_ARG, TAG, "set mesh image src: source is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh image src: state is NULL");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_load_header(src, &header), TAG, "set mesh image src: query header failed");
    ESP_RETURN_ON_FALSE(header.cf == GFX_COLOR_FORMAT_RGB565 || header.cf == GFX_COLOR_FORMAT_RGB565A8,
                        ESP_ERR_NOT_SUPPORTED, TAG, "set mesh image src: unsupported color format");

    gfx_obj_invalidate(obj);

    mesh->image_src = src;
    mesh->header = header;
    gfx_mesh_img_reset_rest_points(mesh);
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set mesh image src: %ux%u grid=%ux%u",
             header.w, header.h, mesh->grid_cols, mesh->grid_rows);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_grid(gfx_obj_t *obj, uint8_t cols, uint8_t rows)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh grid: state is NULL");

    gfx_obj_invalidate(obj);
    ESP_RETURN_ON_ERROR(gfx_mesh_img_alloc_points(mesh, cols, rows), TAG, "set mesh grid: alloc points failed");
    gfx_mesh_img_reset_rest_points(mesh);
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set mesh grid: %ux%u", cols, rows);
    return ESP_OK;
}

size_t gfx_mesh_img_get_point_count(gfx_obj_t *obj)
{
    gfx_mesh_img_t *mesh;

    if (obj == NULL || obj->type != GFX_OBJ_TYPE_MESH_IMAGE || obj->src == NULL) {
        return 0U;
    }

    mesh = (gfx_mesh_img_t *)obj->src;
    return mesh->point_count;
}

esp_err_t gfx_mesh_img_get_point(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_t *point)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(point != NULL, ESP_ERR_INVALID_ARG, TAG, "get mesh point: output is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "get mesh point: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "get mesh point: index out of range");

    *point = mesh->points[point_idx];
    return ESP_OK;
}

esp_err_t gfx_mesh_img_get_point_screen(gfx_obj_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y)
{
    gfx_mesh_img_t *mesh;
    gfx_coord_t origin_x;
    gfx_coord_t origin_y;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(x != NULL && y != NULL, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen: output is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "get mesh point screen: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen: index out of range");

    gfx_mesh_img_get_draw_origin(obj, mesh, &origin_x, &origin_y);
    *x = origin_x + mesh->points[point_idx].x;
    *y = origin_y + mesh->points[point_idx].y;
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_point(gfx_obj_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh point: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh point: index out of range");

    gfx_obj_invalidate(obj);
    mesh->points[point_idx].x = x;
    mesh->points[point_idx].y = y;
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_reset_points(gfx_obj_t *obj)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "reset mesh points: state is NULL");

    gfx_obj_invalidate(obj);
    gfx_mesh_img_reset_rest_points(mesh);
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_ctrl_points_visible(gfx_obj_t *obj, bool visible)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh ctrl points visible: state is NULL");

    mesh->ctrl_points_visible = visible;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}
