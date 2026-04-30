/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "common/gfx_config_internal.h"
#include "common/gfx_mesh_frac.h"
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
#define GFX_MESH_IMG_Q8_SHIFT          GFX_MESH_FRAC_SHIFT
#define GFX_MESH_IMG_Q8_ONE            GFX_MESH_FRAC_ONE

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_img_src_t image_src;
    gfx_image_header_t header;
    uint8_t grid_cols;
    uint8_t grid_rows;
    bool ctrl_points_visible;
    int32_t bounds_min_x_q8;
    int32_t bounds_min_y_q8;
    int32_t bounds_max_x_q8;
    int32_t bounds_max_y_q8;
    gfx_coord_t bounds_min_x;
    gfx_coord_t bounds_min_y;
    gfx_coord_t bounds_max_x;
    gfx_coord_t bounds_max_y;
    size_t point_count;
    gfx_mesh_img_point_q8_t *rest_points;
    gfx_mesh_img_point_q8_t *points;
    bool aa_inward;
    bool wrap_cols;
    bool scanline_fill;
    gfx_opa_t opacity;
    gfx_color_t scanline_color;
    int32_t *scanline_vx;
    int32_t *scanline_vy;
    size_t scanline_capacity;
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
static void gfx_mesh_img_free_scratch(gfx_mesh_img_t *mesh);
static esp_err_t gfx_mesh_img_alloc_points(gfx_mesh_img_t *mesh, uint8_t cols, uint8_t rows);
static esp_err_t gfx_mesh_img_alloc_scratch(gfx_mesh_img_t *mesh);
static void gfx_mesh_img_update_bounds(gfx_obj_t *obj, gfx_mesh_img_t *mesh);
static void gfx_mesh_img_reset_rest_points(gfx_mesh_img_t *mesh);
static void gfx_mesh_img_get_draw_origin_q8(gfx_obj_t *obj, const gfx_mesh_img_t *mesh, int32_t *x_q8, int32_t *y_q8);
static esp_err_t gfx_mesh_img_load_header(const gfx_img_src_t *src, gfx_image_header_t *header);
static esp_err_t gfx_mesh_img_prepare_decoder(const gfx_mesh_img_t *mesh, gfx_image_decoder_dsc_t *decoder_dsc);
static void gfx_mesh_img_draw_ctrl_points(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx, const gfx_mesh_img_t *mesh);
static esp_err_t gfx_mesh_img_validate_src(const gfx_img_src_t *src);

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

static inline int32_t gfx_mesh_img_floor_q8_to_int(int32_t value_q8)
{
    if (value_q8 >= 0) {
        return value_q8 >> GFX_MESH_IMG_Q8_SHIFT;
    }
    return -(((-value_q8) + GFX_MESH_IMG_Q8_ONE - 1) >> GFX_MESH_IMG_Q8_SHIFT);
}

static inline int32_t gfx_mesh_img_ceil_q8_to_int(int32_t value_q8)
{
    if (value_q8 >= 0) {
        return (value_q8 + GFX_MESH_IMG_Q8_ONE - 1) >> GFX_MESH_IMG_Q8_SHIFT;
    }
    return -((-value_q8) >> GFX_MESH_IMG_Q8_SHIFT);
}

static inline gfx_coord_t gfx_mesh_img_round_q8_to_coord(int32_t value_q8)
{
    if (value_q8 >= 0) {
        return (gfx_coord_t)((value_q8 + (GFX_MESH_IMG_Q8_ONE / 2)) >> GFX_MESH_IMG_Q8_SHIFT);
    }
    return (gfx_coord_t)(-(((-value_q8) + (GFX_MESH_IMG_Q8_ONE / 2)) >> GFX_MESH_IMG_Q8_SHIFT));
}

static inline int32_t gfx_mesh_img_clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int32_t gfx_mesh_img_isqrt64(uint64_t value)
{
    uint64_t op, res, one;
    if (value <= 1U) {
        return (int32_t)value;
    }
    op = value; res = 0U; one = 1ULL << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0U) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (int32_t)res;
}

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

static void gfx_mesh_img_free_scratch(gfx_mesh_img_t *mesh)
{
    if (mesh == NULL) {
        return;
    }

    if (mesh->scanline_vx != NULL) {
        free(mesh->scanline_vx);
        mesh->scanline_vx = NULL;
    }
    if (mesh->scanline_vy != NULL) {
        free(mesh->scanline_vy);
        mesh->scanline_vy = NULL;
    }
    mesh->scanline_capacity = 0U;
}

static esp_err_t gfx_mesh_img_alloc_scratch(gfx_mesh_img_t *mesh)
{
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_ARG, TAG, "mesh state is NULL");

    gfx_mesh_img_free_scratch(mesh);

    mesh->scanline_vx = calloc(GFX_MESH_IMG_SCANLINE_MAX_VERTS, sizeof(*mesh->scanline_vx));
    ESP_RETURN_ON_FALSE(mesh->scanline_vx != NULL, ESP_ERR_NO_MEM, TAG, "no mem for scanline vx");

    mesh->scanline_vy = calloc(GFX_MESH_IMG_SCANLINE_MAX_VERTS, sizeof(*mesh->scanline_vy));
    if (mesh->scanline_vy == NULL) {
        gfx_mesh_img_free_scratch(mesh);
        return ESP_ERR_NO_MEM;
    }

    mesh->scanline_capacity = GFX_MESH_IMG_SCANLINE_MAX_VERTS;
    return ESP_OK;
}

static esp_err_t gfx_mesh_img_alloc_points(gfx_mesh_img_t *mesh, uint8_t cols, uint8_t rows)
{
    size_t point_count;
    gfx_mesh_img_point_q8_t *new_rest_points;
    gfx_mesh_img_point_q8_t *new_points;

    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_ARG, TAG, "mesh state is NULL");
    ESP_RETURN_ON_FALSE(cols > 0U && rows > 0U, ESP_ERR_INVALID_ARG, TAG, "grid must be at least 1x1");

    point_count = (size_t)(cols + 1U) * (size_t)(rows + 1U);

    new_rest_points = calloc(point_count, sizeof(gfx_mesh_img_point_q8_t));
    ESP_RETURN_ON_FALSE(new_rest_points != NULL, ESP_ERR_NO_MEM, TAG, "no mem for rest points");

    new_points = calloc(point_count, sizeof(gfx_mesh_img_point_q8_t));
    if (new_points == NULL) {
        free(new_rest_points);
        return ESP_ERR_NO_MEM;
    }

    gfx_mesh_img_free_points(mesh);
    mesh->rest_points = new_rest_points;
    mesh->points = new_points;
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

            mesh->rest_points[index].x_q8 = x << GFX_MESH_IMG_Q8_SHIFT;
            mesh->rest_points[index].y_q8 = y << GFX_MESH_IMG_Q8_SHIFT;
            mesh->points[index] = mesh->rest_points[index];
            index++;
        }
    }
}

static void gfx_mesh_img_update_bounds(gfx_obj_t *obj, gfx_mesh_img_t *mesh)
{
    int32_t min_x_q8;
    int32_t min_y_q8;
    int32_t max_x_q8;
    int32_t max_y_q8;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;

    if (obj == NULL || mesh == NULL) {
        return;
    }

    if (mesh->points == NULL || mesh->point_count == 0U) {
        obj->geometry.width = mesh->header.w;
        obj->geometry.height = mesh->header.h;
        mesh->bounds_min_x_q8 = 0;
        mesh->bounds_min_y_q8 = 0;
        mesh->bounds_max_x_q8 = (mesh->header.w > 0U) ? ((int32_t)(mesh->header.w - 1U) << GFX_MESH_IMG_Q8_SHIFT) : 0;
        mesh->bounds_max_y_q8 = (mesh->header.h > 0U) ? ((int32_t)(mesh->header.h - 1U) << GFX_MESH_IMG_Q8_SHIFT) : 0;
        mesh->bounds_min_x = 0;
        mesh->bounds_min_y = 0;
        mesh->bounds_max_x = (mesh->header.w > 0U) ? (gfx_coord_t)(mesh->header.w - 1U) : 0;
        mesh->bounds_max_y = (mesh->header.h > 0U) ? (gfx_coord_t)(mesh->header.h - 1U) : 0;
        return;
    }

    min_x_q8 = mesh->points[0].x_q8;
    min_y_q8 = mesh->points[0].y_q8;
    max_x_q8 = mesh->points[0].x_q8;
    max_y_q8 = mesh->points[0].y_q8;

    for (size_t i = 1; i < mesh->point_count; i++) {
        min_x_q8 = MIN(min_x_q8, mesh->points[i].x_q8);
        min_y_q8 = MIN(min_y_q8, mesh->points[i].y_q8);
        max_x_q8 = MAX(max_x_q8, mesh->points[i].x_q8);
        max_y_q8 = MAX(max_y_q8, mesh->points[i].y_q8);
    }

    min_x = gfx_mesh_img_floor_q8_to_int(min_x_q8);
    min_y = gfx_mesh_img_floor_q8_to_int(min_y_q8);
    max_x = gfx_mesh_img_ceil_q8_to_int(max_x_q8);
    max_y = gfx_mesh_img_ceil_q8_to_int(max_y_q8);

    mesh->bounds_min_x_q8 = min_x_q8;
    mesh->bounds_min_y_q8 = min_y_q8;
    mesh->bounds_max_x_q8 = max_x_q8;
    mesh->bounds_max_y_q8 = max_y_q8;
    if (min_x < INT16_MIN || min_y < INT16_MIN || max_x > INT16_MAX || max_y > INT16_MAX ||
            (int64_t)max_x - min_x + 1 > UINT16_MAX ||
            (int64_t)max_y - min_y + 1 > UINT16_MAX) {
        GFX_LOGW(TAG, "mesh bounds exceed geometry range, clamping");
    }
    mesh->bounds_min_x = (gfx_coord_t)gfx_mesh_img_clamp_i32(min_x, INT16_MIN, INT16_MAX);
    mesh->bounds_min_y = (gfx_coord_t)gfx_mesh_img_clamp_i32(min_y, INT16_MIN, INT16_MAX);
    mesh->bounds_max_x = (gfx_coord_t)gfx_mesh_img_clamp_i32(max_x, INT16_MIN, INT16_MAX);
    mesh->bounds_max_y = (gfx_coord_t)gfx_mesh_img_clamp_i32(max_y, INT16_MIN, INT16_MAX);
    obj->geometry.width = (uint16_t)gfx_mesh_img_clamp_i32((int32_t)((int64_t)max_x - min_x + 1), 0, UINT16_MAX);
    obj->geometry.height = (uint16_t)gfx_mesh_img_clamp_i32((int32_t)((int64_t)max_y - min_y + 1), 0, UINT16_MAX);
}

static void gfx_mesh_img_get_draw_origin_q8(gfx_obj_t *obj, const gfx_mesh_img_t *mesh, int32_t *x_q8, int32_t *y_q8)
{
    if (obj == NULL || mesh == NULL || x_q8 == NULL || y_q8 == NULL) {
        return;
    }

    gfx_obj_calc_pos_in_parent(obj);
    *x_q8 = ((int32_t)obj->geometry.x << GFX_MESH_IMG_Q8_SHIFT) - mesh->bounds_min_x_q8;
    *y_q8 = ((int32_t)obj->geometry.y << GFX_MESH_IMG_Q8_SHIFT) - mesh->bounds_min_y_q8;
}

static esp_err_t gfx_mesh_img_load_header(const gfx_img_src_t *src, gfx_image_header_t *header)
{
    gfx_image_decoder_dsc_t dsc = {
        .src = *src,
    };

    ESP_RETURN_ON_ERROR(gfx_mesh_img_validate_src(src), TAG, "load mesh image header: source descriptor is invalid");
    ESP_RETURN_ON_FALSE(header != NULL, ESP_ERR_INVALID_ARG, TAG, "header is NULL");

    return gfx_image_decoder_info(&dsc, header);
}

static esp_err_t gfx_mesh_img_validate_src(const gfx_img_src_t *src)
{
    ESP_RETURN_ON_FALSE(src != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve mesh image src: descriptor is NULL");
    ESP_RETURN_ON_FALSE(src->data != NULL, ESP_ERR_INVALID_ARG, TAG, "resolve mesh image src: payload is NULL");

    switch (src->type) {
    case GFX_IMG_SRC_TYPE_IMAGE_DSC:
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t gfx_mesh_img_prepare_decoder(const gfx_mesh_img_t *mesh, gfx_image_decoder_dsc_t *decoder_dsc)
{
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_ARG, TAG, "mesh state is NULL");
    ESP_RETURN_ON_FALSE(decoder_dsc != NULL, ESP_ERR_INVALID_ARG, TAG, "decoder desc is NULL");
    ESP_RETURN_ON_ERROR(gfx_mesh_img_validate_src(&mesh->image_src), TAG, "prepare mesh image decoder: source descriptor is invalid");

    decoder_dsc->src = mesh->image_src;
    decoder_dsc->header = mesh->header;
    decoder_dsc->data = NULL;
    decoder_dsc->data_size = 0;
    decoder_dsc->user_data = NULL;

    return gfx_image_decoder_open(decoder_dsc);
}

static void gfx_mesh_img_draw_ctrl_points(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx, const gfx_mesh_img_t *mesh)
{
    int32_t origin_x_q8;
    int32_t origin_y_q8;
    gfx_color_t outer_color = GFX_COLOR_HEX(0x2dd4bf);
    gfx_color_t inner_color = GFX_COLOR_HEX(0xf8fafc);

    if (obj == NULL || ctx == NULL || mesh == NULL || !mesh->ctrl_points_visible) {
        return;
    }

    gfx_mesh_img_get_draw_origin_q8(obj, mesh, &origin_x_q8, &origin_y_q8);

    for (size_t i = 0; i < mesh->point_count; i++) {
        gfx_coord_t screen_x = gfx_mesh_img_round_q8_to_coord(origin_x_q8 + mesh->points[i].x_q8);
        gfx_coord_t screen_y = gfx_mesh_img_round_q8_to_coord(origin_y_q8 + mesh->points[i].y_q8);

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
    int32_t origin_x_q8;
    int32_t origin_y_q8;
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
    if (mesh->image_src.data == NULL) {
        GFX_LOGD(TAG, "draw mesh image: source descriptor has no payload");
        return ESP_ERR_INVALID_STATE;
    }
    if (mesh->opacity == 0U) {
        return ESP_OK;
    }

    color_format = (gfx_color_format_t)mesh->header.cf;
    if (color_format != GFX_COLOR_FORMAT_RGB565 && color_format != GFX_COLOR_FORMAT_RGB565A8) {
        GFX_LOGW(TAG, "draw mesh image: unsupported color format %u", color_format);
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_prepare_decoder(mesh, &decoder_dsc), TAG, "draw mesh image: open decoder failed");

    if (decoder_dsc.data == NULL) {
        GFX_LOGE(TAG, "draw mesh image: decoder returned no data");
        gfx_image_decoder_close(&decoder_dsc);
        return ESP_ERR_INVALID_STATE;
    }

    gfx_mesh_img_get_draw_origin_q8(obj, mesh, &origin_x_q8, &origin_y_q8);

    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
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

    if (mesh->scanline_fill && mesh->grid_rows == 1U && mesh->points != NULL) {
        int cols = mesh->grid_cols;
        int poly_n = mesh->wrap_cols ? (cols + 1) : ((cols + 1) * 2);
        int32_t *pvx = mesh->scanline_vx;
        int32_t *pvy = mesh->scanline_vy;
        bool scanline_drawn = false;

        if (pvx != NULL && pvy != NULL && poly_n <= (int)mesh->scanline_capacity) {
            if (mesh->wrap_cols) {
                int row1 = cols + 1;
                for (int c = 0; c <= cols; c++) {
                    pvx[c] = origin_x_q8 + mesh->points[row1 + c].x_q8;
                    pvy[c] = origin_y_q8 + mesh->points[row1 + c].y_q8;
                }
            } else {
                for (int c = 0; c <= cols; c++) {
                    pvx[c] = origin_x_q8 + mesh->points[c].x_q8;
                    pvy[c] = origin_y_q8 + mesh->points[c].y_q8;
                }
                for (int c = 0; c <= cols; c++) {
                    int src_idx = 2 * cols + 1 - c;
                    pvx[cols + 1 + c] = origin_x_q8 + mesh->points[src_idx].x_q8;
                    pvy[cols + 1 + c] = origin_y_q8 + mesh->points[src_idx].y_q8;
                }
            }
            gfx_sw_blend_polygon_fill((gfx_color_t *)ctx->buf, ctx->stride,
                                      &ctx->buf_area, &clip_area,
                                      mesh->scanline_color,
                                      mesh->opacity,
                                      pvx, pvy, poly_n, ctx->swap);
            scanline_drawn = true;
        } else {
            GFX_LOGW(TAG, "draw mesh image: scanline fill capacity too small (%d > %u)",
                     poly_n, (unsigned int)mesh->scanline_capacity);
        }

        if (scanline_drawn) {
            gfx_mesh_img_draw_ctrl_points(obj, ctx, mesh);
            gfx_image_decoder_close(&decoder_dsc);
            return ESP_OK;
        }
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
                int32_t p0x_q8 = mesh->points[idx00].x_q8, p1x_q8 = mesh->points[idx10].x_q8;
                int32_t p2x_q8 = mesh->points[idx01].x_q8, p3x_q8 = mesh->points[idx11].x_q8;
                int32_t p0y_q8 = mesh->points[idx00].y_q8, p1y_q8 = mesh->points[idx10].y_q8;
                int32_t p2y_q8 = mesh->points[idx01].y_q8, p3y_q8 = mesh->points[idx11].y_q8;
                int32_t cmin_x = gfx_mesh_img_floor_q8_to_int(MIN(MIN(p0x_q8, p1x_q8), MIN(p2x_q8, p3x_q8)) + origin_x_q8);
                int32_t cmax_x = gfx_mesh_img_ceil_q8_to_int(MAX(MAX(p0x_q8, p1x_q8), MAX(p2x_q8, p3x_q8)) + origin_x_q8);
                int32_t cmin_y = gfx_mesh_img_floor_q8_to_int(MIN(MIN(p0y_q8, p1y_q8), MIN(p2y_q8, p3y_q8)) + origin_y_q8);
                int32_t cmax_y = gfx_mesh_img_ceil_q8_to_int(MAX(MAX(p0y_q8, p1y_q8), MAX(p2y_q8, p3y_q8)) + origin_y_q8);
                if (cmax_x < clip_area.x1 || cmin_x >= clip_area.x2 ||
                        cmax_y < clip_area.y1 || cmin_y >= clip_area.y2) {
                    continue;
                }
            }

            /*
             * Build the 4 quad corners: TL, TR, BR, BL.
             * Then pick the shorter diagonal to avoid gaps when miter
             * joins make the quad non-convex at sharp stroke corners.
             */
            gfx_sw_blend_img_vertex_t qv[4];
            gfx_sw_blend_img_vertex_t tri1[3];
            gfx_sw_blend_img_vertex_t tri2[3];
            uint8_t ie1, ie2;
            bool alt_diag;

            qv[0].x = origin_x_q8 + mesh->points[idx00].x_q8;
            qv[0].y = origin_y_q8 + mesh->points[idx00].y_q8;
            qv[0].u = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx00].x_q8);
            qv[0].v = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx00].y_q8);

            qv[1].x = origin_x_q8 + mesh->points[idx10].x_q8;
            qv[1].y = origin_y_q8 + mesh->points[idx10].y_q8;
            qv[1].u = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx10].x_q8);
            qv[1].v = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx10].y_q8);

            qv[2].x = origin_x_q8 + mesh->points[idx11].x_q8;
            qv[2].y = origin_y_q8 + mesh->points[idx11].y_q8;
            qv[2].u = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx11].x_q8);
            qv[2].v = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx11].y_q8);

            qv[3].x = origin_x_q8 + mesh->points[idx01].x_q8;
            qv[3].y = origin_y_q8 + mesh->points[idx01].y_q8;
            qv[3].u = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx01].x_q8);
            qv[3].v = gfx_mesh_img_round_q8_to_coord(mesh->rest_points[idx01].y_q8);

            {
                int64_t dx1 = (int64_t)qv[0].x - qv[2].x;
                int64_t dy1 = (int64_t)qv[0].y - qv[2].y;
                int64_t dx2 = (int64_t)qv[1].x - qv[3].x;
                int64_t dy2 = (int64_t)qv[1].y - qv[3].y;
                alt_diag = (dx2 * dx2 + dy2 * dy2 < dx1 * dx1 + dy1 * dy1);
            }

            /*
             * Internal-edge masks — suppress AA on edges shared with a
             * neighbouring triangle to prevent dark-seam artefacts.
             *
             * Default diagonal TL→BR:
             *   Tri1 (TL,TR,BR): e0=right, e1=diag, e2=top
             *   Tri2 (TL,BR,BL): e0=bottom, e1=left, e2=diag
             *
             * Alt diagonal TR→BL:
             *   Tri1 (TR,BL,TL): e0=left, e1=top, e2=diag
             *   Tri2 (TR,BR,BL): e0=bottom, e1=diag, e2=right
             */
            if (!alt_diag) {
                tri1[0] = qv[0]; tri1[1] = qv[1]; tri1[2] = qv[2];
                tri2[0] = qv[0]; tri2[1] = qv[2]; tri2[2] = qv[3];
                ie1 = 0x02;
                ie2 = 0x04;
                if (col + 1U < mesh->grid_cols || mesh->wrap_cols) {
                    ie1 |= 0x01;
                }
                if (row > 0U)                                      {
                    ie1 |= 0x04;
                }
                if (row + 1U < mesh->grid_rows)                    {
                    ie2 |= 0x01;
                }
                if (col > 0U || mesh->wrap_cols)                   {
                    ie2 |= 0x02;
                }
            } else {
                tri1[0] = qv[1]; tri1[1] = qv[3]; tri1[2] = qv[0];
                tri2[0] = qv[1]; tri2[1] = qv[2]; tri2[2] = qv[3];
                ie1 = 0x04;
                ie2 = 0x02;
                if (col > 0U || mesh->wrap_cols)                   {
                    ie1 |= 0x01;
                }
                if (row > 0U)                                      {
                    ie1 |= 0x02;
                }
                if (row + 1U < mesh->grid_rows)                    {
                    ie2 |= 0x01;
                }
                if (col + 1U < mesh->grid_cols || mesh->wrap_cols) {
                    ie2 |= 0x04;
                }
            }

            if (mesh->wrap_cols && mesh->grid_rows == 1U &&
                    qv[0].x == qv[1].x && qv[0].y == qv[1].y) {
                /*
                 * Hub/rim fan fill: row 0 is a repeated hub, row 1 is the rim.
                 * The hub→hub edge is degenerate and internal; if inward AA
                 * treats it as an outer edge the whole sector can fade out.
                 */
                if (!alt_diag) {
                    ie1 |= 0x04;  /* Tri1 top hub edge */
                } else {
                    ie1 |= 0x02;  /* Tri1 top hub edge */
                }
            }
            if (mesh->aa_inward) {
                ie1 |= GFX_BLEND_TRI_AA_INWARD;
                ie2 |= GFX_BLEND_TRI_AA_INWARD;
            }

            /*
             * Extra AA edges: each triangle receives the sibling's
             * non-internal outer edges so inward AA fades correctly
             * across the whole quad boundary.
             */
            gfx_sw_blend_aa_edge_t xaa1[GFX_BLEND_MAX_EXTRA_AA_EDGES];
            gfx_sw_blend_aa_edge_t xaa2[GFX_BLEND_MAX_EXTRA_AA_EDGES];
            uint8_t xaa1_n = 0, xaa2_n = 0;

            if (mesh->aa_inward) {
                int32_t ax, ay, bx, by, ea, eb;

                if (!alt_diag) {
                    if (!(ie2 & 0x01)) {
                        ax = qv[2].x; ay = qv[2].y; bx = qv[3].x; by = qv[3].y;
                        ea = by - ay; eb = ax - bx;
                        xaa1[xaa1_n].a = ea; xaa1[xaa1_n].b = eb;
                        xaa1[xaa1_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa1[xaa1_n].len < 1) {
                            xaa1[xaa1_n].len = 1;
                        }
                        xaa1[xaa1_n].vx = ax; xaa1[xaa1_n].vy = ay;
                        xaa1_n++;
                    }
                    if (!(ie2 & 0x02) && xaa1_n < GFX_BLEND_MAX_EXTRA_AA_EDGES) {
                        ax = qv[3].x; ay = qv[3].y; bx = qv[0].x; by = qv[0].y;
                        ea = by - ay; eb = ax - bx;
                        xaa1[xaa1_n].a = ea; xaa1[xaa1_n].b = eb;
                        xaa1[xaa1_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa1[xaa1_n].len < 1) {
                            xaa1[xaa1_n].len = 1;
                        }
                        xaa1[xaa1_n].vx = ax; xaa1[xaa1_n].vy = ay;
                        xaa1_n++;
                    }
                    if (!(ie1 & 0x04)) {
                        ax = qv[0].x; ay = qv[0].y; bx = qv[1].x; by = qv[1].y;
                        ea = by - ay; eb = ax - bx;
                        xaa2[xaa2_n].a = ea; xaa2[xaa2_n].b = eb;
                        xaa2[xaa2_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa2[xaa2_n].len < 1) {
                            xaa2[xaa2_n].len = 1;
                        }
                        xaa2[xaa2_n].vx = ax; xaa2[xaa2_n].vy = ay;
                        xaa2_n++;
                    }
                    if (!(ie1 & 0x01) && xaa2_n < GFX_BLEND_MAX_EXTRA_AA_EDGES) {
                        ax = qv[1].x; ay = qv[1].y; bx = qv[2].x; by = qv[2].y;
                        ea = by - ay; eb = ax - bx;
                        xaa2[xaa2_n].a = ea; xaa2[xaa2_n].b = eb;
                        xaa2[xaa2_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa2[xaa2_n].len < 1) {
                            xaa2[xaa2_n].len = 1;
                        }
                        xaa2[xaa2_n].vx = ax; xaa2[xaa2_n].vy = ay;
                        xaa2_n++;
                    }
                } else {
                    if (!(ie2 & 0x01)) {
                        ax = qv[2].x; ay = qv[2].y; bx = qv[3].x; by = qv[3].y;
                        ea = by - ay; eb = ax - bx;
                        xaa1[xaa1_n].a = ea; xaa1[xaa1_n].b = eb;
                        xaa1[xaa1_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa1[xaa1_n].len < 1) {
                            xaa1[xaa1_n].len = 1;
                        }
                        xaa1[xaa1_n].vx = ax; xaa1[xaa1_n].vy = ay;
                        xaa1_n++;
                    }
                    if (!(ie2 & 0x04) && xaa1_n < GFX_BLEND_MAX_EXTRA_AA_EDGES) {
                        ax = qv[1].x; ay = qv[1].y; bx = qv[2].x; by = qv[2].y;
                        ea = by - ay; eb = ax - bx;
                        xaa1[xaa1_n].a = ea; xaa1[xaa1_n].b = eb;
                        xaa1[xaa1_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa1[xaa1_n].len < 1) {
                            xaa1[xaa1_n].len = 1;
                        }
                        xaa1[xaa1_n].vx = ax; xaa1[xaa1_n].vy = ay;
                        xaa1_n++;
                    }
                    if (!(ie1 & 0x01)) {
                        ax = qv[3].x; ay = qv[3].y; bx = qv[0].x; by = qv[0].y;
                        ea = by - ay; eb = ax - bx;
                        xaa2[xaa2_n].a = ea; xaa2[xaa2_n].b = eb;
                        xaa2[xaa2_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa2[xaa2_n].len < 1) {
                            xaa2[xaa2_n].len = 1;
                        }
                        xaa2[xaa2_n].vx = ax; xaa2[xaa2_n].vy = ay;
                        xaa2_n++;
                    }
                    if (!(ie1 & 0x02) && xaa2_n < GFX_BLEND_MAX_EXTRA_AA_EDGES) {
                        ax = qv[0].x; ay = qv[0].y; bx = qv[1].x; by = qv[1].y;
                        ea = by - ay; eb = ax - bx;
                        xaa2[xaa2_n].a = ea; xaa2[xaa2_n].b = eb;
                        xaa2[xaa2_n].len = gfx_mesh_img_isqrt64((uint64_t)((int64_t)ea * ea + (int64_t)eb * eb));
                        if (xaa2[xaa2_n].len < 1) {
                            xaa2[xaa2_n].len = 1;
                        }
                        xaa2[xaa2_n].vx = ax; xaa2[xaa2_n].vy = ay;
                        xaa2_n++;
                    }
                }
            }

            gfx_sw_blend_img_triangle_draw((gfx_color_t *)ctx->buf, ctx->stride,
                                           &ctx->buf_area, &clip_area,
                                           src_pixels, src_stride, src_height,
                                           alpha_mask, alpha_mask ? src_stride : 0,
                                           mesh->opacity,
                                           &tri1[0], &tri1[1], &tri1[2],
                                           ie1,
                                           xaa1_n ? xaa1 : NULL, xaa1_n,
                                           ctx->swap);
            gfx_sw_blend_img_triangle_draw((gfx_color_t *)ctx->buf, ctx->stride,
                                           &ctx->buf_area, &clip_area,
                                           src_pixels, src_stride, src_height,
                                           alpha_mask, alpha_mask ? src_stride : 0,
                                           mesh->opacity,
                                           &tri2[0], &tri2[1], &tri2[2],
                                           ie2,
                                           xaa2_n ? xaa2 : NULL, xaa2_n,
                                           ctx->swap);
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
        gfx_mesh_img_free_scratch(mesh);
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
    mesh->opacity = 0xFFU;

    if (gfx_mesh_img_alloc_points(mesh, GFX_MESH_IMG_DEFAULT_COLS, GFX_MESH_IMG_DEFAULT_ROWS) != ESP_OK) {
        free(mesh);
        return NULL;
    }

    if (gfx_obj_create_class_instance(disp, &s_gfx_mesh_img_widget_class,
                                      mesh, 0, 0, "gfx_mesh_img_create", &obj) != ESP_OK) {
        gfx_mesh_img_free_points(mesh);
        gfx_mesh_img_free_scratch(mesh);
        free(mesh);
        return NULL;
    }

    GFX_LOGD(TAG, "create mesh image: object created");
    return obj;
}

esp_err_t gfx_mesh_img_set_src_desc(gfx_obj_t *obj, const gfx_img_src_t *src)
{
    gfx_mesh_img_t *mesh;
    gfx_image_header_t header;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_ERROR(gfx_mesh_img_validate_src(src), TAG, "set mesh image src: validate descriptor failed");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh image src: state is NULL");

    ESP_RETURN_ON_ERROR(gfx_mesh_img_load_header(src, &header), TAG, "set mesh image src: query header failed");
    ESP_RETURN_ON_FALSE(header.cf == GFX_COLOR_FORMAT_RGB565 || header.cf == GFX_COLOR_FORMAT_RGB565A8,
                        ESP_ERR_NOT_SUPPORTED, TAG, "set mesh image src: unsupported color format");

    gfx_obj_invalidate(obj);

    mesh->image_src = *src;
    mesh->header = header;
    gfx_mesh_img_reset_rest_points(mesh);
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);

    GFX_LOGD(TAG, "set mesh image src: %ux%u grid=%ux%u",
             header.w, header.h, mesh->grid_cols, mesh->grid_rows);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_src(gfx_obj_t *obj, void *src)
{
    const gfx_img_src_t compat_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = src,
    };

    return gfx_mesh_img_set_src_desc(obj, &compat_src);
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

    point->x = gfx_mesh_img_round_q8_to_coord(mesh->points[point_idx].x_q8);
    point->y = gfx_mesh_img_round_q8_to_coord(mesh->points[point_idx].y_q8);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_get_point_screen(gfx_obj_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y)
{
    gfx_mesh_img_t *mesh;
    int32_t origin_x_q8;
    int32_t origin_y_q8;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(x != NULL && y != NULL, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen: output is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "get mesh point screen: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen: index out of range");

    gfx_mesh_img_get_draw_origin_q8(obj, mesh, &origin_x_q8, &origin_y_q8);
    *x = gfx_mesh_img_round_q8_to_coord(origin_x_q8 + mesh->points[point_idx].x_q8);
    *y = gfx_mesh_img_round_q8_to_coord(origin_y_q8 + mesh->points[point_idx].y_q8);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_get_point_q8(gfx_obj_t *obj, size_t point_idx, gfx_mesh_img_point_q8_t *point)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(point != NULL, ESP_ERR_INVALID_ARG, TAG, "get mesh point q8: output is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "get mesh point q8: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "get mesh point q8: index out of range");

    *point = mesh->points[point_idx];
    return ESP_OK;
}

esp_err_t gfx_mesh_img_get_point_screen_q8(gfx_obj_t *obj, size_t point_idx, int32_t *x_q8, int32_t *y_q8)
{
    gfx_mesh_img_t *mesh;
    int32_t origin_x_q8;
    int32_t origin_y_q8;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(x_q8 != NULL && y_q8 != NULL, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen q8: output is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "get mesh point screen q8: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "get mesh point screen q8: index out of range");

    gfx_mesh_img_get_draw_origin_q8(obj, mesh, &origin_x_q8, &origin_y_q8);
    *x_q8 = origin_x_q8 + mesh->points[point_idx].x_q8;
    *y_q8 = origin_y_q8 + mesh->points[point_idx].y_q8;
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_point_q8(gfx_obj_t *obj, size_t point_idx, int32_t x_q8, int32_t y_q8)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh point q8: state is NULL");
    ESP_RETURN_ON_FALSE(point_idx < mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh point q8: index out of range");

    gfx_obj_invalidate(obj);
    mesh->points[point_idx].x_q8 = x_q8;
    mesh->points[point_idx].y_q8 = y_q8;
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_point(gfx_obj_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y)
{
    return gfx_mesh_img_set_point_q8(obj, point_idx,
                                     (int32_t)x << GFX_MESH_IMG_Q8_SHIFT,
                                     (int32_t)y << GFX_MESH_IMG_Q8_SHIFT);
}

esp_err_t gfx_mesh_img_set_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "set mesh points q8: input is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh points q8: state is NULL");
    ESP_RETURN_ON_FALSE(point_count == mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh points q8: count mismatch");

    gfx_obj_invalidate(obj);
    memcpy(mesh->points, points, point_count * sizeof(*points));
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "set mesh points: input is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh points: state is NULL");
    ESP_RETURN_ON_FALSE(point_count == mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh points: count mismatch");

    gfx_obj_invalidate(obj);
    for (size_t i = 0; i < point_count; i++) {
        mesh->points[i].x_q8 = (int32_t)points[i].x << GFX_MESH_IMG_Q8_SHIFT;
        mesh->points[i].y_q8 = (int32_t)points[i].y << GFX_MESH_IMG_Q8_SHIFT;
    }
    gfx_mesh_img_update_bounds(obj, mesh);
    gfx_obj_update_layout(obj);
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_rest_points_q8(gfx_obj_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "set mesh rest points q8: input is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh rest points q8: state is NULL");
    ESP_RETURN_ON_FALSE(point_count == mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh rest points q8: count mismatch");

    gfx_obj_invalidate(obj);
    memcpy(mesh->rest_points, points, point_count * sizeof(*points));
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_rest_points(gfx_obj_t *obj, const gfx_mesh_img_point_t *points, size_t point_count)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    ESP_RETURN_ON_FALSE(points != NULL, ESP_ERR_INVALID_ARG, TAG, "set mesh rest points: input is NULL");

    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh rest points: state is NULL");
    ESP_RETURN_ON_FALSE(point_count == mesh->point_count, ESP_ERR_INVALID_ARG, TAG, "set mesh rest points: count mismatch");

    gfx_obj_invalidate(obj);
    for (size_t i = 0; i < point_count; i++) {
        mesh->rest_points[i].x_q8 = (int32_t)points[i].x << GFX_MESH_IMG_Q8_SHIFT;
        mesh->rest_points[i].y_q8 = (int32_t)points[i].y << GFX_MESH_IMG_Q8_SHIFT;
    }
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

esp_err_t gfx_mesh_img_set_opa(gfx_obj_t *obj, gfx_opa_t opa)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh opacity: state is NULL");

    mesh->opacity = opa;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_aa_inward(gfx_obj_t *obj, bool inward)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh aa inward: state is NULL");

    mesh->aa_inward = inward;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_wrap_cols(gfx_obj_t *obj, bool wrap)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set mesh wrap cols: state is NULL");

    mesh->wrap_cols = wrap;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_mesh_img_set_scanline_fill(gfx_obj_t *obj, bool enable, gfx_color_t fill_color)
{
    gfx_mesh_img_t *mesh;

    CHECK_OBJ_TYPE_MESH_IMAGE(obj);
    mesh = (gfx_mesh_img_t *)obj->src;
    ESP_RETURN_ON_FALSE(mesh != NULL, ESP_ERR_INVALID_STATE, TAG, "set scanline fill: state is NULL");

    if (enable && (mesh->scanline_vx == NULL || mesh->scanline_vy == NULL)) {
        ESP_RETURN_ON_ERROR(gfx_mesh_img_alloc_scratch(mesh), TAG, "set scanline fill: alloc scratch failed");
    } else if (!enable) {
        gfx_mesh_img_free_scratch(mesh);
    }

    mesh->scanline_fill = enable;
    mesh->scanline_color = fill_color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}
