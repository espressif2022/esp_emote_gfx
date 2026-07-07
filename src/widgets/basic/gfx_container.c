/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_OBJ
#include "common/gfx_log_priv.h"
#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"
#include "render/gfx_render_priv.h"
#include "gfx/widgets/container.h"

#define CHECK_OBJ_TYPE_CONTAINER(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_CONTAINER, TAG)
#define GFX_CONTAINER_DEFAULT_WIDTH    160
#define GFX_CONTAINER_DEFAULT_HEIGHT   120
#define GFX_CONTAINER_CIRCLE_DASHES      8

typedef struct {
    gfx_color_t bg_color;
    gfx_color_t border_color;
    uint16_t border_width;
    uint16_t radius;
    bool bg_enable;
    bool border_dash_enable;
} gfx_container_t;

static const char *const TAG = "container";

static gfx_err_t gfx_container_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static gfx_err_t gfx_container_delete_impl(gfx_object_t *obj);

static const gfx_widget_class_t s_gfx_container_widget_class = {
    .type = GFX_OBJ_TYPE_CONTAINER,
    .name = "container",
    .draw = gfx_container_draw,
    .delete = gfx_container_delete_impl,
};

static void gfx_container_init_default(gfx_container_t *container)
{
    memset(container, 0, sizeof(*container));
    container->bg_color = GFX_COLOR_HEX(0x101820);
    container->border_color = GFX_COLOR_HEX(0x3A506B);
    container->border_width = 1;
    container->radius = 0;
    container->bg_enable = true;
}

static uint32_t gfx_container_sq_i32(int32_t v)
{
    return (uint32_t)(v * v);
}

static uint32_t gfx_container_abs_i32(int32_t v)
{
    return (uint32_t)(v < 0 ? -v : v);
}

static bool gfx_container_point_in_circle(gfx_coord_t x, gfx_coord_t y, uint16_t w, uint16_t h)
{
    int32_t diameter = (w < h) ? (int32_t)w : (int32_t)h;
    int32_t dx;
    int32_t dy;
    int32_t radius;

    if (diameter <= 0) {
        return false;
    }

    dx = (int32_t)(2 * x) - ((int32_t)w - 1);
    dy = (int32_t)(2 * y) - ((int32_t)h - 1);
    radius = diameter - 1;
    return gfx_container_sq_i32(dx) + gfx_container_sq_i32(dy) <= gfx_container_sq_i32(radius);
}

static bool gfx_container_circle_dash_visible(gfx_coord_t x, gfx_coord_t y, uint16_t w, uint16_t h)
{
    int32_t dx = (int32_t)(2 * x) - ((int32_t)w - 1);
    int32_t dy = (int32_t)(2 * y) - ((int32_t)h - 1);
    uint32_t ax = gfx_container_abs_i32(dx);
    uint32_t ay = gfx_container_abs_i32(dy);
    uint32_t perimeter_pos;
    uint32_t perimeter_len = ((uint32_t)w + (uint32_t)h) * 2U;
    uint32_t dash_len;

    if (w == 0U || h == 0U || perimeter_len == 0U) {
        return false;
    }

    if (dy < 0 && ax >= ay) {
        perimeter_pos = (uint32_t)x;
    } else if (dx > 0 && ay > ax) {
        perimeter_pos = (uint32_t)w + (uint32_t)y;
    } else if (dy > 0 && ax >= ay) {
        perimeter_pos = (uint32_t)w + (uint32_t)h + (uint32_t)(w - 1U - (uint16_t)x);
    } else {
        perimeter_pos = (uint32_t)w + (uint32_t)h + (uint32_t)w + (uint32_t)(h - 1U - (uint16_t)y);
    }

    dash_len = perimeter_len / GFX_CONTAINER_CIRCLE_DASHES;
    if (dash_len == 0U) {
        dash_len = 1U;
    }
    return ((perimeter_pos / dash_len) & 1U) == 0U;
}

static void gfx_container_draw_dashed_circle_border(gfx_object_t *obj,
        const gfx_render_surface_t *dst_surface,
        const gfx_area_t *obj_area,
        const gfx_area_t *clip_area,
        const gfx_container_t *container)
{
    uint16_t w;
    uint16_t h;
    uint16_t border_width;

    if (obj == NULL || dst_surface == NULL || obj_area == NULL || clip_area == NULL ||
            container == NULL || container->border_width == 0U) {
        return;
    }

    w = obj->geometry.width;
    h = obj->geometry.height;
    border_width = container->border_width;

    for (gfx_coord_t y = clip_area->y1; y < clip_area->y2; y++) {
        for (gfx_coord_t x = clip_area->x1; x < clip_area->x2; x++) {
            gfx_coord_t local_x = (gfx_coord_t)(x - obj_area->x1);
            gfx_coord_t local_y = (gfx_coord_t)(y - obj_area->y1);
            bool outer = gfx_container_point_in_circle(local_x, local_y, w, h);
            bool inner = false;

            if (!outer || !gfx_container_circle_dash_visible(local_x, local_y, w, h)) {
                continue;
            }

            if (w > border_width * 2U && h > border_width * 2U) {
                gfx_coord_t inner_x = (gfx_coord_t)(local_x - (gfx_coord_t)border_width);
                gfx_coord_t inner_y = (gfx_coord_t)(local_y - (gfx_coord_t)border_width);
                inner = inner_x >= 0 && inner_y >= 0 &&
                        gfx_container_point_in_circle(inner_x, inner_y,
                                                      (uint16_t)(w - border_width * 2U),
                                                      (uint16_t)(h - border_width * 2U));
            }
            if (!inner) {
                gfx_area_t px_area = {
                    .x1 = x,
                    .y1 = y,
                    .x2 = (gfx_coord_t)(x + 1),
                    .y2 = (gfx_coord_t)(y + 1),
                };
                gfx_render_surface_fill(obj->disp, dst_surface, &px_area, container->border_color, 0xFFU);
            }
        }
    }
}

static gfx_err_t gfx_container_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_container_t *container;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    CHECK_OBJ_TYPE_CONTAINER(obj);
    GFX_RETURN_IF_NULL(ctx, GFX_ERR_INVALID_ARG);

    container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    if (container->bg_enable) {
        if (container->radius > 0U) {
            gfx_round_rect_fill_dsc_t fill_dsc = {
                .color = container->bg_color,
                .opa = 0xFFU,
                .radius = container->radius,
            };
            gfx_render_surface_round_rect_fill(obj->disp, &dst_surface, &obj_area, &fill_dsc);
        } else {
            gfx_render_surface_fill(obj->disp, &dst_surface, &clip_area,
                                    container->bg_color, 0xFFU);
        }
    }

    if (container->border_width > 0U) {
        if (container->border_dash_enable && container->radius > 0U) {
            gfx_container_draw_dashed_circle_border(obj, &dst_surface, &obj_area, &clip_area, container);
        } else if (container->radius > 0U) {
            gfx_round_rect_stroke_dsc_t stroke_dsc = {
                .color = container->border_color,
                .opa = 0xFFU,
                .radius = container->radius,
                .width = container->border_width,
            };
            gfx_render_surface_round_rect_stroke(obj->disp, &dst_surface, &obj_area, &stroke_dsc);
        } else {
            gfx_render_surface_rect_stroke(obj->disp,
                                           &dst_surface,
                                           &obj_area,
                                           container->border_width,
                                           container->border_color,
                                           0xFF);
        }
    }

    return GFX_OK;
}

static gfx_err_t gfx_container_delete_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);
    free(obj->src);
    obj->src = NULL;
    return GFX_OK;
}

gfx_object_t *gfx_container_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_container_t *container;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create container: display is NULL");
        return NULL;
    }

    container = calloc(1, sizeof(gfx_container_t));
    if (container == NULL) {
        GFX_LOGE(TAG, "create container: no mem for state");
        return NULL;
    }
    gfx_container_init_default(container);

    if (gfx_object_create_class_instance(disp, &s_gfx_container_widget_class,
                                         container, GFX_CONTAINER_DEFAULT_WIDTH, GFX_CONTAINER_DEFAULT_HEIGHT,
                                         "gfx_container_create", &obj) != GFX_OK) {
        free(container);
        return NULL;
    }

    return obj;
}

gfx_err_t gfx_container_set_bg_enable(gfx_object_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->bg_enable = enable;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_border_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->border_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->border_width = width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_radius(gfx_object_t *obj, uint16_t radius)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->radius = radius;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_border_dash_enable(gfx_object_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_container_t *container = (gfx_container_t *)obj->src;
    GFX_RETURN_IF_NULL(container, GFX_ERR_INVALID_STATE);

    container->border_dash_enable = enable;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_container_set_clip_children(gfx_object_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_object_invalidate_tree(obj);
    gfx_object_set_clip_children(obj, enable);
    gfx_object_invalidate_tree(obj);
    return GFX_OK;
}
