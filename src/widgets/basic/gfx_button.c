/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_BUTTON
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "render/gfx_render_priv.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/input.h"
#include "gfx/widgets/button.h"
#include "fonts/gfx_font_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_BUTTON(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_BUTTON, TAG)
#define GFX_BUTTON_DEFAULT_WIDTH      120
#define GFX_BUTTON_DEFAULT_HEIGHT      44
#define GFX_BUTTON_TEXT_PAD_X          10
#define GFX_BUTTON_TEXT_PAD_Y           6

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_label_t label;   /* Must stay first so label internals can be reused safely. */

    struct {
        gfx_color_t bg_color;
        gfx_color_t bg_color_pressed;
        gfx_color_t border_color;
        uint16_t border_width;
        uint16_t radius;
        uint16_t text_pad_x;
        uint16_t text_pad_y;
        bool fill_enable;
    } style;

    struct {
        bool pressed;
    } state;
} gfx_button_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "button";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_button_init_default_state(gfx_button_t *button);
static void gfx_button_get_label_area(const gfx_object_t *obj, const gfx_area_t *obj_area, gfx_area_t *area);
static bool gfx_button_contains_point(gfx_object_t *obj, uint16_t x, uint16_t y);
static gfx_err_t gfx_button_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static gfx_err_t gfx_button_update(gfx_object_t *obj);
static gfx_err_t gfx_button_delete_impl(gfx_object_t *obj);
static gfx_err_t gfx_button_load_impl(gfx_object_t *obj);
static void gfx_button_release_impl(gfx_object_t *obj);
static void gfx_button_touch_event(gfx_object_t *obj, const void *event_data);

static const gfx_widget_class_t s_gfx_button_widget_class = {
    .type = GFX_OBJ_TYPE_BUTTON,
    .name = "button",
    .draw = gfx_button_draw,
    .delete = gfx_button_delete_impl,
    .load = gfx_button_load_impl,
    .release = gfx_button_release_impl,
    .update = gfx_button_update,
    .touch_event = gfx_button_touch_event,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_button_init_default_state(gfx_button_t *button)
{
    memset(button, 0, sizeof(*button));

    button->label.style.opa = 0xFF;
    button->label.style.color = GFX_COLOR_HEX(0xFFFFFF);
    button->label.style.bg_color = GFX_COLOR_HEX(0x000000);
    button->label.style.bg_enable = false;
    button->label.style.text_align = GFX_TEXT_ALIGN_CENTER;
    button->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    button->label.text.line_spacing = 2;

    button->style.bg_color = GFX_COLOR_HEX(0x2A6DF4);
    button->style.bg_color_pressed = GFX_COLOR_HEX(0x1E53BB);
    button->style.border_color = GFX_COLOR_HEX(0xD9E6FF);
    button->style.border_width = 1;
    button->style.radius = 6;
    button->style.text_pad_x = GFX_BUTTON_TEXT_PAD_X;
    button->style.text_pad_y = GFX_BUTTON_TEXT_PAD_Y;
    button->style.fill_enable = true;
    button->state.pressed = false;
}

static void gfx_button_get_label_area(const gfx_object_t *obj, const gfx_area_t *obj_area, gfx_area_t *area)
{
    gfx_button_t *button = (gfx_button_t *)obj->src;
    gfx_coord_t inner_x;
    gfx_coord_t inner_y;
    uint16_t inner_w;
    uint16_t inner_h;
    uint16_t text_h;
    uint16_t top_pad;

    inner_x = obj_area->x1 + (gfx_coord_t)button->style.text_pad_x;
    inner_y = obj_area->y1 + (gfx_coord_t)button->style.text_pad_y;
    inner_w = (obj->geometry.width > (button->style.text_pad_x * 2U)) ?
              (uint16_t)(obj->geometry.width - (button->style.text_pad_x * 2U)) :
              obj->geometry.width;
    inner_h = (obj->geometry.height > (button->style.text_pad_y * 2U)) ?
              (uint16_t)(obj->geometry.height - (button->style.text_pad_y * 2U)) :
              obj->geometry.height;

    if (inner_w == 0) {
        inner_w = obj->geometry.width;
    }
    if (inner_h == 0) {
        inner_h = obj->geometry.height;
    }

    text_h = inner_h;
    if (button->label.font.handle != NULL && button->label.font.handle->get_line_height != NULL) {
        int line_height = button->label.font.handle->get_line_height(button->label.font.handle);
        if (line_height > 0) {
            text_h = (uint16_t)line_height;
        }
    }
    top_pad = (inner_h > text_h) ? (uint16_t)((inner_h - text_h) / 2U) : 0U;
    inner_y += (gfx_coord_t)top_pad;

    area->x1 = inner_x;
    area->y1 = inner_y;
    area->x2 = (gfx_coord_t)(inner_x + (gfx_coord_t)inner_w);
    area->y2 = (gfx_coord_t)(inner_y + (gfx_coord_t)inner_h);
    button->label.style.bg_enable = false;
}

static bool gfx_button_contains_point(gfx_object_t *obj, uint16_t x, uint16_t y)
{
    gfx_area_t obj_area;

    if (obj == NULL) {
        return false;
    }

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return false;
    }

    return ((gfx_coord_t)x >= obj_area.x1) &&
           ((gfx_coord_t)y >= obj_area.y1) &&
           ((gfx_coord_t)x < obj_area.x2) &&
           ((gfx_coord_t)y < obj_area.y2);
}

static gfx_err_t gfx_button_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_button_t *button;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t label_area;
    gfx_color_t fill_color;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(ctx, GFX_ERR_INVALID_ARG);

    button = (gfx_button_t *)obj->src;
    GFX_RETURN_IF_NULL(button, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    fill_color = button->state.pressed ? button->style.bg_color_pressed : button->style.bg_color;
    if (button->style.fill_enable) {
        gfx_round_rect_fill_dsc_t fill_dsc = {
            .color = fill_color,
            .opa = 0xFFU,
            .radius = button->style.radius,
        };
        gfx_render_surface_round_rect_fill(obj->disp, &dst_surface, &obj_area, &fill_dsc);
    }
    if (button->style.border_width > 0U) {
        gfx_round_rect_stroke_dsc_t stroke_dsc = {
            .color = button->style.border_color,
            .opa = 0xFFU,
            .radius = button->style.radius,
            .width = button->style.border_width,
        };
        gfx_render_surface_round_rect_stroke(obj->disp, &dst_surface, &obj_area, &stroke_dsc);
    }

    gfx_button_get_label_area(obj, &obj_area, &label_area);
    (void)gfx_label_text_box_draw(obj, &button->label, ctx, &label_area, &clip_area);

    return GFX_OK;
}

static gfx_err_t gfx_button_update(gfx_object_t *obj)
{
    gfx_button_t *button;
    gfx_area_t obj_area;
    gfx_area_t label_area;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }
    button = (gfx_button_t *)obj->src;

    gfx_button_get_label_area(obj, &obj_area, &label_area);
    return gfx_label_text_box_update(obj, &button->label, &label_area);
}

static gfx_err_t gfx_button_delete_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    gfx_label_delete_state(obj, &((gfx_button_t *)obj->src)->label);
    free(obj->src);
    return GFX_OK;
}

static gfx_err_t gfx_button_load_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    return gfx_label_load_state(obj, &((gfx_button_t *)obj->src)->label);
}

static void gfx_button_release_impl(gfx_object_t *obj)
{
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_BUTTON) {
        return;
    }

    gfx_label_release_state(&((gfx_button_t *)obj->src)->label);
}

static void gfx_button_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_button_t *button;
    bool should_press;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    button = (gfx_button_t *)obj->src;

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        if (!button->state.pressed) {
            button->state.pressed = true;
            gfx_object_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_MOVE:
        should_press = gfx_button_contains_point(obj, event->x, event->y);
        if (button->state.pressed != should_press) {
            button->state.pressed = should_press;
            gfx_object_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        if (button->state.pressed) {
            button->state.pressed = false;
            gfx_object_invalidate(obj);
        }
        break;
    default:
        break;
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_object_t *gfx_button_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_button_t *button;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create button: display is NULL");
        return NULL;
    }

    button = calloc(1, sizeof(gfx_button_t));
    if (button == NULL) {
        GFX_LOGE(TAG, "create button: no mem for state");
        return NULL;
    }

    gfx_button_init_default_state(button);

    if (gfx_object_create_class_instance(disp, &s_gfx_button_widget_class,
                                         button, GFX_BUTTON_DEFAULT_WIDTH, GFX_BUTTON_DEFAULT_HEIGHT,
                                         "gfx_button_create", &obj) != GFX_OK) {
        free(button);
        GFX_LOGE(TAG, "create button: no mem for object");
        return NULL;
    }

    GFX_LOGD(TAG, "create button: object created");
    return obj;
}

gfx_err_t gfx_button_set_text(gfx_object_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_BUTTON(obj);

    gfx_button_t *button = (gfx_button_t *)obj->src;
    const char *new_text = text ? text : "";
    char *dup_text = NULL;
    size_t len;

    GFX_RETURN_IF_NULL(button, GFX_ERR_INVALID_STATE);

    len = strlen(new_text) + 1;
    dup_text = malloc(len);
    if (dup_text == NULL) {
        return GFX_ERR_NO_MEM;
    }
    memcpy(dup_text, new_text, len);

    free(button->label.text.text);
    button->label.text.text = dup_text;
    button->label.text.text_width = 0;
    button->label.scroll.offset = 0;
    button->label.snap.offset = 0;
    gfx_object_invalidate(obj);

    return GFX_OK;
}

gfx_err_t gfx_button_set_text_fmt(gfx_object_t *obj, const char *fmt, ...)
{
    char *buf = NULL;
    va_list args;
    va_list args_copy;
    int len;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(fmt != NULL, GFX_ERR_INVALID_ARG, TAG, "fmt is NULL");

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (len < 0) {
        va_end(args);
        return GFX_FAIL;
    }

    buf = malloc((size_t)len + 1U);
    if (buf == NULL) {
        va_end(args);
        return GFX_ERR_NO_MEM;
    }

    vsnprintf(buf, (size_t)len + 1U, fmt, args);
    va_end(args);

    ret = gfx_button_set_text(obj, buf);
    free(buf);
    return ret;
}

gfx_err_t gfx_button_set_font(gfx_object_t *obj, gfx_font_t font)
{
    gfx_button_t *button;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    button = (gfx_button_t *)obj->src;
    return gfx_label_set_font_source(obj, &button->label, font);
}

gfx_err_t gfx_button_set_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->label.style.color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_bg_color_pressed(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.bg_color_pressed = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_border_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.border_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_radius(gfx_object_t *obj, uint16_t radius)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.radius = radius;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_fill_enable(gfx_object_t *obj, bool enable)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.fill_enable = enable;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_text_padding(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.text_pad_x = pad_x;
    ((gfx_button_t *)obj->src)->style.text_pad_y = pad_y;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_button_set_text_align(gfx_object_t *obj, gfx_text_align_t align)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->label.style.text_align = align;
    gfx_object_invalidate(obj);
    return GFX_OK;
}
