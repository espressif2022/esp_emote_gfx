/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_BUTTON
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/input.h"
#include "gfx/widgets/image_button.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "widgets/img/gfx_image_resource_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

#define CHECK_OBJ_TYPE_IMAGE_BUTTON(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_IMAGE_BUTTON, TAG)
#define GFX_IMAGE_BUTTON_DEFAULT_WIDTH   120
#define GFX_IMAGE_BUTTON_DEFAULT_HEIGHT   44
#define GFX_IMAGE_BUTTON_TEXT_PAD_X       10
#define GFX_IMAGE_BUTTON_TEXT_PAD_Y        6

typedef struct {
    gfx_label_t label;
    gfx_image_resource_t released_resource;
    gfx_image_resource_t pressed_resource;
    uint16_t text_pad_x;
    uint16_t text_pad_y;
    bool pressed;
    bool touch_feedback_enabled;
} gfx_image_button_t;

static const char *const TAG = "image_button";

static void gfx_image_button_init_default_state(gfx_image_button_t *button)
{
    memset(button, 0, sizeof(*button));

    button->label.style.opa = 0xFF;
    button->label.style.color = GFX_COLOR_HEX(0xFFFFFF);
    button->label.style.bg_color = GFX_COLOR_HEX(0x000000);
    button->label.style.bg_enable = false;
    button->label.style.text_align = GFX_TEXT_ALIGN_CENTER;
    button->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    button->label.text.line_spacing = 2;
    button->text_pad_x = GFX_IMAGE_BUTTON_TEXT_PAD_X;
    button->text_pad_y = GFX_IMAGE_BUTTON_TEXT_PAD_Y;
    button->touch_feedback_enabled = true;
}

static gfx_image_resource_t *gfx_image_button_resolve_resource(gfx_image_button_t *button)
{
    if (button == NULL) {
        return NULL;
    }
    if (button->pressed && button->pressed_resource.src.data != NULL) {
        return &button->pressed_resource;
    }
    if (button->released_resource.src.data != NULL) {
        return &button->released_resource;
    }
    return NULL;
}

static bool gfx_image_button_contains_point(gfx_object_t *obj, uint16_t x, uint16_t y)
{
    gfx_area_t obj_area;

    if (obj == NULL || !gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return false;
    }

    return ((gfx_coord_t)x >= obj_area.x1) &&
           ((gfx_coord_t)y >= obj_area.y1) &&
           ((gfx_coord_t)x < obj_area.x2) &&
           ((gfx_coord_t)y < obj_area.y2);
}

static void gfx_image_button_get_label_area(const gfx_object_t *obj, const gfx_area_t *obj_area,
        gfx_area_t *area)
{
    gfx_image_button_t *button = (gfx_image_button_t *)obj->src;
    uint16_t inner_w;
    uint16_t inner_h;

    inner_w = (obj->geometry.width > (button->text_pad_x * 2U))
              ? (uint16_t)(obj->geometry.width - (button->text_pad_x * 2U))
              : obj->geometry.width;
    inner_h = (obj->geometry.height > (button->text_pad_y * 2U))
              ? (uint16_t)(obj->geometry.height - (button->text_pad_y * 2U))
              : obj->geometry.height;
    if (inner_w == 0U) {
        inner_w = obj->geometry.width;
    }
    if (inner_h == 0U) {
        inner_h = obj->geometry.height;
    }

    area->x1 = (gfx_coord_t)(obj_area->x1 + (gfx_coord_t)button->text_pad_x);
    area->y1 = (gfx_coord_t)(obj_area->y1 + (gfx_coord_t)button->text_pad_y);
    area->x2 = (gfx_coord_t)(area->x1 + (gfx_coord_t)inner_w);
    area->y2 = (gfx_coord_t)(area->y1 + (gfx_coord_t)inner_h);
    button->label.style.bg_enable = false;
}

static gfx_err_t gfx_image_button_draw_resource(gfx_object_t *obj, const gfx_draw_ctx_t *ctx,
        const gfx_area_t *obj_area, const gfx_area_t *clip_area, const gfx_image_resource_t *resource)
{
    const uint8_t *pixels;
    gfx_color_format_t format;
    uint8_t pixel_size;
    gfx_coord_t stride;
    gfx_coord_t src_x;
    gfx_coord_t src_y;
    gfx_area_t draw_local;
    gfx_render_surface_t dst_surface;
    gfx_render_image_t backend_src;
    gfx_render_image_t render_src;
    gfx_opa_t *alpha_mask = NULL;
    gfx_coord_t alpha_stride = 0;

    if (resource == NULL) {
        return GFX_OK;
    }

    pixels = gfx_image_resource_pixels(resource);
    if (pixels == NULL) {
        return GFX_ERR_INVALID_STATE;
    }

    format = gfx_image_resource_format(resource);
    pixel_size = gfx_image_resource_pixel_size(resource);
    stride = gfx_image_resource_stride_px(resource);
    if (pixel_size == 0U || stride <= 0) {
        return GFX_ERR_INVALID_SIZE;
    }

    src_x = (gfx_coord_t)(clip_area->x1 - obj_area->x1);
    src_y = (gfx_coord_t)(clip_area->y1 - obj_area->y1);
    pixels += ((size_t)src_y * stride + (size_t)src_x) * pixel_size;

    if (gfx_color_format_has_plane_alpha(format)) {
        const uint8_t *alpha_base = (const uint8_t *)gfx_image_resource_alpha(resource);
        alpha_mask = (gfx_opa_t *)GFX_BUFFER_OFFSET_8BPP(alpha_base, src_y,
                     gfx_image_resource_alpha_stride(resource), src_x);
        alpha_stride = gfx_image_resource_alpha_stride(resource);
    }

    draw_local = (gfx_area_t) {
        .x1 = (gfx_coord_t)(clip_area->x1 - ctx->buf_area.x1),
        .y1 = (gfx_coord_t)(clip_area->y1 - ctx->buf_area.y1),
        .x2 = (gfx_coord_t)(clip_area->x2 - ctx->buf_area.x1),
        .y2 = (gfx_coord_t)(clip_area->y2 - ctx->buf_area.y1),
    };
    dst_surface = (gfx_render_surface_t) {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };
    render_src = (gfx_render_image_t) {
        .pixels = pixels,
        .stride = stride,
        .format = format,
        .alpha = alpha_mask,
        .alpha_stride = alpha_stride,
    };
    backend_src = render_src;
    backend_src.pixels = gfx_image_resource_pixels(resource);
    if (gfx_color_format_has_plane_alpha(format)) {
        backend_src.alpha = (gfx_opa_t *)gfx_image_resource_alpha(resource);
    }

    if (gfx_render_surface_blit_image(obj->disp, &dst_surface, clip_area, &backend_src,
                                      src_x, src_y, 0xFFU)) {
        return GFX_OK;
    }

    gfx_sw_blend_img_draw_fmt(ctx->buf, ctx->stride, ctx->format,
                              render_src.pixels, render_src.stride,
                              render_src.alpha, render_src.alpha_stride,
                              &draw_local, render_src.format, 0xFFU);
    return GFX_OK;
}

static gfx_err_t gfx_image_button_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_image_button_t *button;
    gfx_image_resource_t *resource;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t label_area;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(ctx != NULL, GFX_ERR_INVALID_ARG, TAG, "draw context is NULL");

    button = (gfx_image_button_t *)obj->src;
    GFX_RETURN_ON_FALSE(button != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");
    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }
    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    resource = gfx_image_button_resolve_resource(button);
    ret = gfx_image_button_draw_resource(obj, ctx, &obj_area, &clip_area, resource);
    if (ret != GFX_OK) {
        return ret;
    }

    gfx_image_button_get_label_area(obj, &obj_area, &label_area);
    return gfx_label_text_box_draw(obj, &button->label, ctx, &label_area, &clip_area);
}

static gfx_err_t gfx_image_button_update(gfx_object_t *obj)
{
    gfx_image_button_t *button;
    gfx_area_t obj_area;
    gfx_area_t label_area;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    button = (gfx_image_button_t *)obj->src;
    gfx_image_button_get_label_area(obj, &obj_area, &label_area);
    return gfx_label_text_box_update(obj, &button->label, &label_area);
}

static gfx_err_t gfx_image_button_load_impl(gfx_object_t *obj)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    button = (gfx_image_button_t *)obj->src;
    GFX_RETURN_ON_ERROR(gfx_label_load_state(obj, &button->label), TAG, "load label failed");
    if (button->released_resource.src.data != NULL) {
        GFX_RETURN_ON_ERROR(gfx_image_resource_open(&button->released_resource),
                            TAG, "load released image failed");
    }
    if (button->pressed_resource.src.data != NULL) {
        GFX_RETURN_ON_ERROR(gfx_image_resource_open(&button->pressed_resource),
                            TAG, "load pressed image failed");
    }
    return GFX_OK;
}

static void gfx_image_button_release_impl(gfx_object_t *obj)
{
    gfx_image_button_t *button;

    if (obj == NULL || obj->type != GFX_OBJ_TYPE_IMAGE_BUTTON || obj->src == NULL) {
        return;
    }

    button = (gfx_image_button_t *)obj->src;
    gfx_image_resource_close(&button->released_resource);
    gfx_image_resource_close(&button->pressed_resource);
    gfx_label_release_state(&button->label);
}

static gfx_err_t gfx_image_button_delete_impl(gfx_object_t *obj)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    button = (gfx_image_button_t *)obj->src;
    if (button != NULL) {
        gfx_image_resource_close(&button->released_resource);
        gfx_image_resource_close(&button->pressed_resource);
        gfx_label_delete_state(obj, &button->label);
        free(button);
        obj->src = NULL;
    }
    return GFX_OK;
}

static void gfx_image_button_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_image_button_t *button;
    bool should_press;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    button = (gfx_image_button_t *)obj->src;
    if (!button->touch_feedback_enabled) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        if (!button->pressed) {
            button->pressed = true;
            gfx_object_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_MOVE:
        should_press = gfx_image_button_contains_point(obj, event->x, event->y);
        if (button->pressed != should_press) {
            button->pressed = should_press;
            gfx_object_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        if (button->pressed) {
            button->pressed = false;
            gfx_object_invalidate(obj);
        }
        break;
    default:
        break;
    }
}

static const gfx_widget_class_t s_gfx_image_button_widget_class = {
    .type = GFX_OBJ_TYPE_IMAGE_BUTTON,
    .name = "image_button",
    .draw = gfx_image_button_draw,
    .delete = gfx_image_button_delete_impl,
    .load = gfx_image_button_load_impl,
    .release = gfx_image_button_release_impl,
    .update = gfx_image_button_update,
    .touch_event = gfx_image_button_touch_event,
};

gfx_object_t *gfx_image_button_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_image_button_t *button;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create image button: display is NULL");
        return NULL;
    }

    button = calloc(1, sizeof(*button));
    if (button == NULL) {
        GFX_LOGE(TAG, "create image button: no mem for state");
        return NULL;
    }
    gfx_image_button_init_default_state(button);

    if (gfx_object_create_class_instance(disp, &s_gfx_image_button_widget_class,
                                         button, GFX_IMAGE_BUTTON_DEFAULT_WIDTH,
                                         GFX_IMAGE_BUTTON_DEFAULT_HEIGHT,
                                         "gfx_image_button_create", &obj) != GFX_OK) {
        free(button);
        GFX_LOGE(TAG, "create image button: no mem for object");
        return NULL;
    }
    return obj;
}

gfx_err_t gfx_image_button_set_src_released(gfx_object_t *obj, const gfx_image_src_t *src)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL && src != NULL, GFX_ERR_INVALID_ARG, TAG, "invalid released src");

    button = (gfx_image_button_t *)obj->src;
    GFX_RETURN_ON_ERROR(gfx_image_resource_set_source(&button->released_resource, src),
                        TAG, "set released image failed");
    gfx_object_mark_resource_dirty(obj);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_src_pressed(gfx_object_t *obj, const gfx_image_src_t *src)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL && src != NULL, GFX_ERR_INVALID_ARG, TAG, "invalid pressed src");

    button = (gfx_image_button_t *)obj->src;
    GFX_RETURN_ON_ERROR(gfx_image_resource_set_source(&button->pressed_resource, src),
                        TAG, "set pressed image failed");
    gfx_object_mark_resource_dirty(obj);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_text(gfx_object_t *obj, const char *text)
{
    gfx_image_button_t *button;
    const char *new_text = text != NULL ? text : "";
    char *dup_text;
    size_t len;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    button = (gfx_image_button_t *)obj->src;
    len = strlen(new_text) + 1U;
    dup_text = malloc(len);
    GFX_RETURN_ON_FALSE(dup_text != NULL, GFX_ERR_NO_MEM, TAG, "no mem for text");
    memcpy(dup_text, new_text, len);

    free(button->label.text.text);
    button->label.text.text = dup_text;
    button->label.text.text_width = 0;
    button->label.scroll.offset = 0;
    button->label.snap.offset = 0;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_font(gfx_object_t *obj, gfx_font_t font)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");
    button = (gfx_image_button_t *)obj->src;
    return gfx_label_set_font_source(obj, &button->label, font);
}

gfx_err_t gfx_image_button_set_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    ((gfx_image_button_t *)obj->src)->label.style.color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_text_align(gfx_object_t *obj, gfx_text_align_t align)
{
    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    ((gfx_image_button_t *)obj->src)->label.style.text_align = align;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_text_padding(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    button = (gfx_image_button_t *)obj->src;
    button->text_pad_x = pad_x;
    button->text_pad_y = pad_y;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_touch_feedback_enabled(gfx_object_t *obj, bool enabled)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    button = (gfx_image_button_t *)obj->src;
    button->touch_feedback_enabled = enabled;
    if (!enabled && button->pressed) {
        button->pressed = false;
        gfx_object_invalidate(obj);
    }
    return GFX_OK;
}

gfx_err_t gfx_image_button_set_pressed(gfx_object_t *obj, bool pressed)
{
    gfx_image_button_t *button;

    CHECK_OBJ_TYPE_IMAGE_BUTTON(obj);
    GFX_RETURN_ON_FALSE(obj->src != NULL, GFX_ERR_INVALID_STATE, TAG, "state is NULL");

    button = (gfx_image_button_t *)obj->src;
    if (button->pressed == pressed) {
        return GFX_OK;
    }

    button->pressed = pressed;
    gfx_object_invalidate(obj);
    return GFX_OK;
}
