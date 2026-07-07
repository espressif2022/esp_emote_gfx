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
#include "gfx/input.h"
#include "gfx/widgets/progress_bar.h"
#include "render/gfx_render_priv.h"

#define CHECK_OBJ_TYPE_PROGRESS_BAR(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_PROGRESS_BAR, TAG)
#define GFX_PROGRESS_BAR_DEFAULT_WIDTH   128
#define GFX_PROGRESS_BAR_DEFAULT_HEIGHT   12

typedef struct {
    gfx_color_t track_color;
    gfx_color_t fill_color;
    gfx_color_t thumb_color;
    gfx_color_t thumb_border_color;
    uint16_t radius;
    uint16_t fill_pad;
    uint16_t thumb_border_width;
    uint16_t value_permille;
    gfx_progress_bar_dir_t direction;
    bool interactive;
    bool pressed;
    gfx_progress_bar_value_changed_cb_t value_changed_cb;
    void *value_changed_user_data;
} gfx_progress_bar_t;

static const char *const TAG = "progress_bar";

static gfx_err_t gfx_progress_bar_set_value_internal(gfx_object_t *obj, uint16_t permille, bool notify);
static void gfx_progress_bar_touch_event(gfx_object_t *obj, const void *event_data);

static void gfx_progress_bar_init_default(gfx_progress_bar_t *bar)
{
    memset(bar, 0, sizeof(*bar));
    bar->track_color = GFX_COLOR_HEX(0x26313B);
    bar->fill_color = GFX_COLOR_HEX(0x2F8CFF);
    bar->thumb_color = GFX_COLOR_HEX(0xF7FBFF);
    bar->thumb_border_color = GFX_COLOR_HEX(0x78BEFF);
    bar->radius = 6;
    bar->fill_pad = 1;
    bar->thumb_border_width = 2;
    bar->direction = GFX_PROGRESS_BAR_DIR_HORIZONTAL;
}

static uint16_t gfx_progress_bar_value_from_point(gfx_object_t *obj, const gfx_progress_bar_t *bar,
        uint16_t x, uint16_t y)
{
    gfx_area_t obj_area;
    gfx_coord_t track_x1;
    gfx_coord_t track_x2;
    gfx_coord_t track_y1;
    gfx_coord_t track_y2;
    uint32_t length;

    if (obj == NULL || bar == NULL || !gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return 0;
    }

    if (bar->direction == GFX_PROGRESS_BAR_DIR_VERTICAL) {
        track_y1 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad);
        track_y2 = (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad);
        if (track_y2 <= track_y1) {
            return 0;
        }
        if ((gfx_coord_t)y <= track_y1) {
            return 1000;
        }
        if ((gfx_coord_t)y >= track_y2) {
            return 0;
        }
        length = (uint32_t)(track_y2 - track_y1);
        return (uint16_t)(((uint32_t)(track_y2 - (gfx_coord_t)y) * 1000U) / length);
    }

    track_x1 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad);
    track_x2 = (gfx_coord_t)(obj_area.x2 - (gfx_coord_t)bar->fill_pad);
    if (track_x2 <= track_x1) {
        return 0;
    }
    if ((gfx_coord_t)x <= track_x1) {
        return 0;
    }
    if ((gfx_coord_t)x >= track_x2) {
        return 1000;
    }
    length = (uint32_t)(track_x2 - track_x1);
    return (uint16_t)(((uint32_t)((gfx_coord_t)x - track_x1) * 1000U) / length);
}

static gfx_err_t gfx_progress_bar_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_progress_bar_t *bar;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    uint16_t inner_w;
    uint16_t inner_h;
    uint16_t fill_w;
    uint16_t fill_h;
    uint16_t fill_radius;
    gfx_area_t fill_area;
    gfx_area_t thumb_area;
    gfx_render_surface_t dst_surface;
    gfx_round_rect_fill_dsc_t round_dsc;
    gfx_round_rect_stroke_dsc_t stroke_dsc;
    uint16_t thumb_size;
    uint16_t thumb_radius;
    gfx_coord_t thumb_center_x = 0;
    gfx_coord_t thumb_center_y = 0;
    uint16_t stroke_width;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(ctx != NULL, GFX_ERR_INVALID_ARG, TAG, "draw context is NULL");
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area) ||
            !gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    bar = (gfx_progress_bar_t *)obj->src;
    dst_surface = (gfx_render_surface_t) {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };
    round_dsc = (gfx_round_rect_fill_dsc_t) {
        .color = bar->track_color,
        .opa = 0xFFU,
        .radius = bar->radius,
    };
    gfx_render_surface_round_rect_fill(obj->disp, &dst_surface, &obj_area, &round_dsc);

    inner_w = obj->geometry.width > bar->fill_pad * 2U
              ? (uint16_t)(obj->geometry.width - bar->fill_pad * 2U)
              : 0U;
    inner_h = obj->geometry.height > bar->fill_pad * 2U
              ? (uint16_t)(obj->geometry.height - bar->fill_pad * 2U)
              : 0U;
    if (inner_w == 0U || inner_h == 0U) {
        return GFX_OK;
    }

    fill_w = 0U;
    fill_h = 0U;
    if (bar->direction == GFX_PROGRESS_BAR_DIR_VERTICAL) {
        fill_h = (uint16_t)(((uint32_t)inner_h * bar->value_permille + 999U) / 1000U);
        if (bar->value_permille > 0U && fill_h == 0U) {
            fill_h = 1U;
        }
        if (fill_h > inner_h) {
            fill_h = inner_h;
        }
    } else {
        fill_w = (uint16_t)(((uint32_t)inner_w * bar->value_permille + 999U) / 1000U);
        if (bar->value_permille > 0U && fill_w == 0U) {
            fill_w = 1U;
        }
        if (fill_w > inner_w) {
            fill_w = inner_w;
        }
    }

    if (bar->interactive) {
        if (bar->direction == GFX_PROGRESS_BAR_DIR_VERTICAL) {
            thumb_size = inner_w;
            thumb_center_y = (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)fill_h);
            if (thumb_center_y < (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)(thumb_size / 2U))) {
                thumb_center_y = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)(thumb_size / 2U));
            }
            if (thumb_center_y > (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)((thumb_size + 1U) / 2U))) {
                thumb_center_y = (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)((thumb_size + 1U) / 2U));
            }
        } else {
            thumb_size = inner_h;
            thumb_center_x = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)fill_w);
            if (thumb_center_x < (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)(thumb_size / 2U))) {
                thumb_center_x = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)(thumb_size / 2U));
            }
            if (thumb_center_x > (gfx_coord_t)(obj_area.x2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)((thumb_size + 1U) / 2U))) {
                thumb_center_x = (gfx_coord_t)(obj_area.x2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)((thumb_size + 1U) / 2U));
            }
        }
    }

    if (fill_w > 0U || fill_h > 0U) {
        fill_radius = bar->radius > bar->fill_pad ? (uint16_t)(bar->radius - bar->fill_pad) : bar->radius;
        if (bar->direction == GFX_PROGRESS_BAR_DIR_VERTICAL) {
            fill_area = (gfx_area_t) {
                .x1 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad),
                .y1 = bar->interactive ? thumb_center_y :
                      (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad - (gfx_coord_t)fill_h),
                      .x2 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)inner_w),
                      .y2 = (gfx_coord_t)(obj_area.y2 - (gfx_coord_t)bar->fill_pad),
            };
        } else {
            fill_area = (gfx_area_t) {
                .x1 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad),
                .y1 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad),
                .x2 = bar->interactive ? thumb_center_x :
                      (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)fill_w),
                      .y2 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)inner_h),
            };
        }
        if (gfx_area_intersect_exclusive(&clip_area, &fill_area, &ctx->clip_area)) {
            round_dsc.color = bar->fill_color;
            round_dsc.radius = fill_radius;
            gfx_render_surface_round_rect_fill(obj->disp, &dst_surface, &fill_area, &round_dsc);
        }
    }

    if (!bar->interactive || inner_h == 0U) {
        return GFX_OK;
    }

    if (bar->direction == GFX_PROGRESS_BAR_DIR_VERTICAL) {
        thumb_size = inner_w;
        thumb_area = (gfx_area_t) {
            .x1 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad),
            .y1 = (gfx_coord_t)(thumb_center_y - (gfx_coord_t)(thumb_size / 2U)),
            .x2 = (gfx_coord_t)(obj_area.x1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)inner_w),
            .y2 = (gfx_coord_t)(thumb_center_y - (gfx_coord_t)(thumb_size / 2U) + (gfx_coord_t)thumb_size),
        };
    } else {
        thumb_size = inner_h;
        thumb_area = (gfx_area_t) {
            .x1 = (gfx_coord_t)(thumb_center_x - (gfx_coord_t)(thumb_size / 2U)),
            .y1 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad),
            .x2 = (gfx_coord_t)(thumb_center_x - (gfx_coord_t)(thumb_size / 2U) + (gfx_coord_t)thumb_size),
            .y2 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)bar->fill_pad + (gfx_coord_t)inner_h),
        };
    }
    if (!gfx_area_intersect_exclusive(&clip_area, &thumb_area, &ctx->clip_area)) {
        return GFX_OK;
    }
    thumb_radius = (uint16_t)(thumb_size / 2U);
    round_dsc.color = bar->thumb_color;
    round_dsc.radius = thumb_radius;
    gfx_render_surface_round_rect_fill(obj->disp, &dst_surface, &thumb_area, &round_dsc);
    stroke_width = bar->thumb_border_width;
    if (bar->pressed && stroke_width > 0U) {
        stroke_width++;
    }
    stroke_dsc = (gfx_round_rect_stroke_dsc_t) {
        .color = bar->pressed ? bar->fill_color : bar->thumb_border_color,
        .opa = 0xFFU,
        .radius = thumb_radius,
        .width = stroke_width,
    };
    gfx_render_surface_round_rect_stroke(obj->disp, &dst_surface, &thumb_area, &stroke_dsc);
    return GFX_OK;
}

static gfx_err_t gfx_progress_bar_delete_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    free(obj->src);
    obj->src = NULL;
    return GFX_OK;
}

static const gfx_widget_class_t s_gfx_progress_bar_widget_class = {
    .type = GFX_OBJ_TYPE_PROGRESS_BAR,
    .name = "progress_bar",
    .draw = gfx_progress_bar_draw,
    .delete = gfx_progress_bar_delete_impl,
    .touch_event = gfx_progress_bar_touch_event,
};

static void gfx_progress_bar_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_progress_bar_t *bar;
    uint16_t value;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    bar = (gfx_progress_bar_t *)obj->src;
    if (!bar->interactive) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        bar->pressed = true;
        value = gfx_progress_bar_value_from_point(obj, bar, event->x, event->y);
        (void)gfx_progress_bar_set_value_internal(obj, value, true);
        break;
    case GFX_TOUCH_EVENT_MOVE:
        value = gfx_progress_bar_value_from_point(obj, bar, event->x, event->y);
        (void)gfx_progress_bar_set_value_internal(obj, value, true);
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        value = gfx_progress_bar_value_from_point(obj, bar, event->x, event->y);
        (void)gfx_progress_bar_set_value_internal(obj, value, true);
        if (bar->pressed) {
            bar->pressed = false;
            gfx_object_invalidate(obj);
        }
        break;
    default:
        break;
    }
}

gfx_object_t *gfx_progress_bar_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_progress_bar_t *bar;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create progress bar: display is NULL");
        return NULL;
    }

    bar = calloc(1, sizeof(*bar));
    if (bar == NULL) {
        GFX_LOGE(TAG, "create progress bar: no mem for state");
        return NULL;
    }
    gfx_progress_bar_init_default(bar);

    if (gfx_object_create_class_instance(disp, &s_gfx_progress_bar_widget_class,
                                         bar, GFX_PROGRESS_BAR_DEFAULT_WIDTH,
                                         GFX_PROGRESS_BAR_DEFAULT_HEIGHT,
                                         "gfx_progress_bar_create", &obj) != GFX_OK) {
        free(bar);
        return NULL;
    }
    return obj;
}

gfx_err_t gfx_progress_bar_set_value(gfx_object_t *obj, uint16_t permille)
{
    return gfx_progress_bar_set_value_internal(obj, permille, false);
}

static gfx_err_t gfx_progress_bar_set_value_internal(gfx_object_t *obj, uint16_t permille, bool notify)
{
    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    if (permille > 1000U) {
        permille = 1000U;
    }
    gfx_progress_bar_t *bar = (gfx_progress_bar_t *)obj->src;
    if (bar->value_permille != permille) {
        bar->value_permille = permille;
        gfx_object_invalidate(obj);
        if (notify && bar->value_changed_cb != NULL) {
            bar->value_changed_cb(obj, permille, bar->value_changed_user_data);
        }
    } else if (notify && bar->pressed) {
        gfx_object_invalidate(obj);
    }
    return GFX_OK;
}

uint16_t gfx_progress_bar_get_value(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_PROGRESS_BAR || obj->src == NULL) {
        return 0;
    }

    return ((gfx_progress_bar_t *)obj->src)->value_permille;
}

gfx_err_t gfx_progress_bar_set_colors(gfx_object_t *obj, gfx_color_t track_color, gfx_color_t fill_color)
{
    gfx_progress_bar_t *bar;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    bar = (gfx_progress_bar_t *)obj->src;
    bar->track_color = track_color;
    bar->fill_color = fill_color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_thumb_style(gfx_object_t *obj, gfx_color_t color,
        gfx_color_t border_color, uint16_t border_width)
{
    gfx_progress_bar_t *bar;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    bar = (gfx_progress_bar_t *)obj->src;
    bar->thumb_color = color;
    bar->thumb_border_color = border_color;
    bar->thumb_border_width = border_width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_radius(gfx_object_t *obj, uint16_t radius)
{
    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    ((gfx_progress_bar_t *)obj->src)->radius = radius;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_fill_pad(gfx_object_t *obj, uint16_t fill_pad)
{
    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    ((gfx_progress_bar_t *)obj->src)->fill_pad = fill_pad;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_direction(gfx_object_t *obj, gfx_progress_bar_dir_t direction)
{
    gfx_progress_bar_t *bar;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");
    GFX_RETURN_ON_FALSE(direction == GFX_PROGRESS_BAR_DIR_HORIZONTAL ||
                        direction == GFX_PROGRESS_BAR_DIR_VERTICAL,
                        GFX_ERR_INVALID_ARG, TAG, "invalid direction");

    bar = (gfx_progress_bar_t *)obj->src;
    if (bar->direction != direction) {
        bar->direction = direction;
        gfx_object_invalidate(obj);
    }
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_interactive(gfx_object_t *obj, bool interactive)
{
    gfx_progress_bar_t *bar;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    bar = (gfx_progress_bar_t *)obj->src;
    bar->interactive = interactive;
    if (!interactive) {
        bar->pressed = false;
    }
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_progress_bar_set_value_changed_cb(gfx_object_t *obj,
        gfx_progress_bar_value_changed_cb_t cb, void *user_data)
{
    gfx_progress_bar_t *bar;

    CHECK_OBJ_TYPE_PROGRESS_BAR(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    bar = (gfx_progress_bar_t *)obj->src;
    bar->value_changed_cb = cb;
    bar->value_changed_user_data = user_data;
    return GFX_OK;
}
