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

typedef struct {
    gfx_color_t bg_color;
    gfx_color_t border_color;
    uint16_t border_width;
    bool bg_enable;
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
    container->bg_enable = true;
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
        gfx_render_surface_fill(obj->disp, &dst_surface, &clip_area,
                                container->bg_color, 0xFFU);
    }

    gfx_render_surface_rect_stroke(obj->disp,
                                   &dst_surface,
                                   &obj_area,
                                   container->border_width,
                                   container->border_color,
                                   0xFF);

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

gfx_err_t gfx_container_set_clip_children(gfx_object_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_CONTAINER(obj);

    gfx_object_invalidate_tree(obj);
    gfx_object_set_clip_children(obj, enable);
    gfx_object_invalidate_tree(obj);
    return GFX_OK;
}
