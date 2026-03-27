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

#include "esp_check.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_BUTTON
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/draw/gfx_sw_draw_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/gfx_touch.h"
#include "widget/gfx_button.h"
#include "widget/label/gfx_label_priv.h"

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
    } style;

    struct {
        bool pressed;
    } state;
} gfx_button_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "button";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_button_init_default_state(gfx_button_t *button);
static void gfx_button_apply_label_geometry(gfx_obj_t *obj);
static bool gfx_button_contains_point(gfx_obj_t *obj, uint16_t x, uint16_t y);
static esp_err_t gfx_button_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_button_update(gfx_obj_t *obj);
static esp_err_t gfx_button_delete_impl(gfx_obj_t *obj);
static void gfx_button_touch_event(gfx_obj_t *obj, const void *event_data);
static esp_err_t gfx_button_call_label_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_button_call_label_update(gfx_obj_t *obj);
static esp_err_t gfx_button_call_label_delete(gfx_obj_t *obj);

static const gfx_widget_class_t s_gfx_button_widget_class = {
    .type = GFX_OBJ_TYPE_BUTTON,
    .name = "button",
    .draw = gfx_button_draw,
    .delete = gfx_button_delete_impl,
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
    button->state.pressed = false;
}

static void gfx_button_apply_label_geometry(gfx_obj_t *obj)
{
    gfx_button_t *button = (gfx_button_t *)obj->src;
    gfx_coord_t inner_x;
    gfx_coord_t inner_y;
    uint16_t inner_w;
    uint16_t inner_h;

    inner_x = obj->geometry.x + GFX_BUTTON_TEXT_PAD_X;
    inner_y = obj->geometry.y + GFX_BUTTON_TEXT_PAD_Y;
    inner_w = (obj->geometry.width > (GFX_BUTTON_TEXT_PAD_X * 2)) ? (obj->geometry.width - (GFX_BUTTON_TEXT_PAD_X * 2)) : obj->geometry.width;
    inner_h = (obj->geometry.height > (GFX_BUTTON_TEXT_PAD_Y * 2)) ? (obj->geometry.height - (GFX_BUTTON_TEXT_PAD_Y * 2)) : obj->geometry.height;

    if (inner_w == 0) {
        inner_w = obj->geometry.width;
    }
    if (inner_h == 0) {
        inner_h = obj->geometry.height;
    }

    obj->geometry.x = inner_x;
    obj->geometry.y = inner_y;
    obj->geometry.width = inner_w;
    obj->geometry.height = inner_h;
    button->label.style.bg_enable = false;
}

static bool gfx_button_contains_point(gfx_obj_t *obj, uint16_t x, uint16_t y)
{
    if (obj == NULL) {
        return false;
    }

    gfx_obj_calc_pos_in_parent(obj);
    return ((gfx_coord_t)x >= obj->geometry.x) &&
           ((gfx_coord_t)y >= obj->geometry.y) &&
           ((gfx_coord_t)x < (obj->geometry.x + (gfx_coord_t)obj->geometry.width)) &&
           ((gfx_coord_t)y < (obj->geometry.y + (gfx_coord_t)obj->geometry.height));
}

static esp_err_t gfx_button_call_label_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_draw_label(obj, ctx);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_button_call_label_update(gfx_obj_t *obj)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_update_impl(obj);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_button_call_label_delete(gfx_obj_t *obj)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_delete_impl(obj);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_button_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_button_t *button;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t fill_area;
    gfx_area_t saved_geometry;
    gfx_color_t fill_color;
    uint16_t fill_color_raw;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(ctx, ESP_ERR_INVALID_ARG);

    button = (gfx_button_t *)obj->src;
    GFX_RETURN_IF_NULL(button, ESP_ERR_INVALID_STATE);

    gfx_obj_calc_pos_in_parent(obj);

    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return ESP_OK;
    }

    fill_color = button->state.pressed ? button->style.bg_color_pressed : button->style.bg_color;
    fill_color_raw = gfx_color_to_native_u16(fill_color, ctx->swap);
    gfx_color_t *dest_pixels = (gfx_color_t *)ctx->buf;
    fill_area.x1 = clip_area.x1 - ctx->buf_area.x1;
    fill_area.y1 = clip_area.y1 - ctx->buf_area.y1;
    fill_area.x2 = clip_area.x2 - ctx->buf_area.x1;
    fill_area.y2 = clip_area.y2 - ctx->buf_area.y1;

    gfx_sw_blend_fill_area((uint16_t *)dest_pixels, ctx->stride, &fill_area, fill_color_raw);
    gfx_sw_draw_rect_stroke(dest_pixels,
                            ctx->stride,
                            &ctx->buf_area,
                            &ctx->clip_area,
                            &obj_area,
                            button->style.border_width,
                            button->style.border_color,
                            0xFF,
                            ctx->swap);

    saved_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    gfx_button_apply_label_geometry(obj);
    gfx_button_call_label_draw(obj, ctx);
    obj->geometry.x = saved_geometry.x1;
    obj->geometry.y = saved_geometry.y1;
    obj->geometry.width = (uint16_t)saved_geometry.x2;
    obj->geometry.height = (uint16_t)saved_geometry.y2;

    return ESP_OK;
}

static esp_err_t gfx_button_update(gfx_obj_t *obj)
{
    gfx_area_t saved_geometry;
    esp_err_t ret;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    gfx_obj_calc_pos_in_parent(obj);

    saved_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    gfx_button_apply_label_geometry(obj);
    ret = gfx_button_call_label_update(obj);
    obj->geometry.x = saved_geometry.x1;
    obj->geometry.y = saved_geometry.y1;
    obj->geometry.width = (uint16_t)saved_geometry.x2;
    obj->geometry.height = (uint16_t)saved_geometry.y2;

    return ret;
}

static esp_err_t gfx_button_delete_impl(gfx_obj_t *obj)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    return gfx_button_call_label_delete(obj);
}

static void gfx_button_touch_event(gfx_obj_t *obj, const void *event_data)
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
            gfx_obj_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_MOVE:
        should_press = gfx_button_contains_point(obj, event->x, event->y);
        if (button->state.pressed != should_press) {
            button->state.pressed = should_press;
            gfx_obj_invalidate(obj);
        }
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        if (button->state.pressed) {
            button->state.pressed = false;
            gfx_obj_invalidate(obj);
        }
        break;
    default:
        break;
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_button_create(gfx_disp_t *disp)
{
    gfx_obj_t *obj;
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

    if (gfx_obj_create_class_instance(disp, &s_gfx_button_widget_class,
                                      button, GFX_BUTTON_DEFAULT_WIDTH, GFX_BUTTON_DEFAULT_HEIGHT,
                                      "gfx_button_create", &obj) != ESP_OK) {
        free(button);
        GFX_LOGE(TAG, "create button: no mem for object");
        return NULL;
    }

    GFX_LOGD(TAG, "create button: object created");
    return obj;
}

esp_err_t gfx_button_set_text(gfx_obj_t *obj, const char *text)
{
    CHECK_OBJ_TYPE_BUTTON(obj);

    gfx_button_t *button = (gfx_button_t *)obj->src;
    const char *new_text = text ? text : "";
    char *dup_text = NULL;
    size_t len;

    GFX_RETURN_IF_NULL(button, ESP_ERR_INVALID_STATE);

    len = strlen(new_text) + 1;
    dup_text = malloc(len);
    if (dup_text == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(dup_text, new_text, len);

    free(button->label.text.text);
    button->label.text.text = dup_text;
    button->label.text.text_width = 0;
    button->label.scroll.offset = 0;
    button->label.snap.offset = 0;
    gfx_obj_invalidate(obj);

    return ESP_OK;
}

esp_err_t gfx_button_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...)
{
    char *buf = NULL;
    va_list args;
    va_list args_copy;
    int len;
    esp_err_t ret;

    CHECK_OBJ_TYPE_BUTTON(obj);
    ESP_RETURN_ON_FALSE(fmt != NULL, ESP_ERR_INVALID_ARG, TAG, "fmt is NULL");

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (len < 0) {
        va_end(args);
        return ESP_FAIL;
    }

    buf = malloc((size_t)len + 1U);
    if (buf == NULL) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }

    vsnprintf(buf, (size_t)len + 1U, fmt, args);
    va_end(args);

    ret = gfx_button_set_text(obj, buf);
    free(buf);
    return ret;
}

esp_err_t gfx_button_set_font(gfx_obj_t *obj, gfx_font_t font)
{
    gfx_button_t *button;

    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    button = (gfx_button_t *)obj->src;

    if (button->label.font.handle != NULL) {
        free(button->label.font.handle);
        button->label.font.handle = NULL;
    }

    gfx_label_clear_glyph_cache(&button->label);
    button->label.text.text_width = 0;

    if (font != NULL) {
        gfx_font_handle_t font_handle = calloc(1, sizeof(gfx_font_adapter_t));
        if (font_handle == NULL) {
            return ESP_ERR_NO_MEM;
        }

        if (gfx_is_lvgl_font(font)) {
            gfx_font_lv_init_adapter(font_handle, font);
        } else {
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
            gfx_font_ft_init_adapter(font_handle, font);
#else
            free(font_handle);
            GFX_LOGW(TAG, "set button font: freetype support is not enabled");
            return ESP_ERR_NOT_SUPPORTED;
#endif
        }

        button->label.font.handle = font_handle;
    }

    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_text_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->label.style.color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_bg_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.bg_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_bg_color_pressed(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.bg_color_pressed = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_border_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.border_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_border_width(gfx_obj_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->style.border_width = width;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_button_set_text_align(gfx_obj_t *obj, gfx_text_align_t align)
{
    CHECK_OBJ_TYPE_BUTTON(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_button_t *)obj->src)->label.style.text_align = align;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}
