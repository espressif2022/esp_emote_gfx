/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LIST
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "gfx/input.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/tween.h"
#include "gfx/widgets/wheel.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

#define CHECK_OBJ_TYPE_WHEEL(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_WHEEL, TAG)

#define GFX_WHEEL_DEFAULT_WIDTH          220U
#define GFX_WHEEL_DEFAULT_ITEM_HEIGHT     40U
#define GFX_WHEEL_DEFAULT_VISIBLE_ROWS     5U
#define GFX_WHEEL_DEFAULT_DRAG_THRESHOLD   6U
#define GFX_WHEEL_PAD_X                   10U
#define GFX_WHEEL_PAD_Y                    4U
#define GFX_WHEEL_TWEEN_MS                160U

typedef struct {
    gfx_label_t label;
    char **items;
    uint16_t item_count;
    int32_t selected_index;
    int32_t scroll_y;
    gfx_tween_t *tween;
    uint16_t item_height;
    uint8_t visible_rows;
    bool cyclic;
    uint16_t drag_threshold;

    struct {
        bool pressed;
        bool dragging;
        uint16_t start_x;
        uint16_t start_y;
        uint16_t last_y;
        uint32_t last_timestamp_ms;
        int32_t velocity_y;
    } touch;

    struct {
        gfx_color_t bg_color;
        gfx_color_t text_color;
        gfx_color_t center_bg_color;
        gfx_color_t center_text_color;
        gfx_color_t border_color;
        uint16_t border_width;
    } style;

    gfx_wheel_value_cb_t value_cb;
    void *value_user_data;
    gfx_wheel_confirm_cb_t confirm_cb;
    void *confirm_user_data;
} gfx_wheel_t;

static const char *const TAG = "wheel";

static void gfx_wheel_init_default_state(gfx_wheel_t *wheel);
static gfx_err_t gfx_wheel_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static gfx_err_t gfx_wheel_update(gfx_object_t *obj);
static gfx_err_t gfx_wheel_delete_impl(gfx_object_t *obj);
static gfx_err_t gfx_wheel_load_impl(gfx_object_t *obj);
static void gfx_wheel_release_impl(gfx_object_t *obj);
static void gfx_wheel_touch_event(gfx_object_t *obj, const void *event_data);

static const gfx_widget_class_t s_gfx_wheel_widget_class = {
    .type = GFX_OBJ_TYPE_WHEEL,
    .name = "wheel",
    .draw = gfx_wheel_draw,
    .delete = gfx_wheel_delete_impl,
    .load = gfx_wheel_load_impl,
    .release = gfx_wheel_release_impl,
    .update = gfx_wheel_update,
    .touch_event = gfx_wheel_touch_event,
};

static void gfx_wheel_init_default_state(gfx_wheel_t *wheel)
{
    memset(wheel, 0, sizeof(*wheel));

    wheel->label.style.opa = 0xFF;
    wheel->label.style.bg_enable = false;
    wheel->label.style.text_align = GFX_TEXT_ALIGN_CENTER;
    wheel->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    wheel->label.text.line_spacing = 0;

    wheel->selected_index = 0;
    wheel->item_height = GFX_WHEEL_DEFAULT_ITEM_HEIGHT;
    wheel->visible_rows = GFX_WHEEL_DEFAULT_VISIBLE_ROWS;
    wheel->drag_threshold = GFX_WHEEL_DEFAULT_DRAG_THRESHOLD;
    wheel->cyclic = false;

    wheel->style.bg_color = GFX_COLOR_HEX(0x101418);
    wheel->style.text_color = GFX_COLOR_HEX(0xA8B3BD);
    wheel->style.center_bg_color = GFX_COLOR_HEX(0xF1C40F);
    wheel->style.center_text_color = GFX_COLOR_HEX(0x101418);
    wheel->style.border_color = GFX_COLOR_HEX(0x3F5163);
    wheel->style.border_width = 1;
}

static gfx_err_t gfx_wheel_dup_text(const char *text, char **out_text)
{
    const char *src = text ? text : "";
    size_t len = strlen(src) + 1U;
    char *dup;

    GFX_RETURN_IF_NULL(out_text, GFX_ERR_INVALID_ARG);

    dup = malloc(len);
    if (dup == NULL) {
        return GFX_ERR_NO_MEM;
    }
    memcpy(dup, src, len);
    *out_text = dup;
    return GFX_OK;
}

static void gfx_wheel_free_items(gfx_wheel_t *wheel)
{
    if (wheel == NULL) {
        return;
    }

    gfx_tween_stop(wheel->tween, false);
    for (uint16_t i = 0; i < wheel->item_count; i++) {
        free(wheel->items[i]);
    }
    free(wheel->items);
    wheel->items = NULL;
    wheel->item_count = 0;
    wheel->selected_index = 0;
    wheel->scroll_y = 0;
    memset(&wheel->touch, 0, sizeof(wheel->touch));
}

static int32_t gfx_wheel_center_y(const gfx_area_t *obj_area)
{
    return obj_area ? ((int32_t)obj_area->y1 + (int32_t)obj_area->y2) / 2 : 0;
}

static int32_t gfx_wheel_center_scroll_y(const gfx_object_t *obj, const gfx_wheel_t *wheel, int32_t index)
{
    if (obj == NULL || wheel == NULL) {
        return 0;
    }

    return index * (int32_t)wheel->item_height - ((int32_t)obj->geometry.height - (int32_t)wheel->item_height) / 2;
}

static int32_t gfx_wheel_max_scroll_y(const gfx_object_t *obj, const gfx_wheel_t *wheel)
{
    int32_t max;

    if (obj == NULL || wheel == NULL || wheel->item_count == 0U) {
        return 0;
    }

    max = gfx_wheel_center_scroll_y(obj, wheel, (int32_t)wheel->item_count - 1);
    return max > 0 ? max : 0;
}

static int32_t gfx_wheel_clamp_index(const gfx_wheel_t *wheel, int32_t index)
{
    if (wheel == NULL || wheel->item_count == 0U) {
        return 0;
    }

    if (wheel->cyclic) {
        int32_t count = (int32_t)wheel->item_count;
        index %= count;
        return index < 0 ? index + count : index;
    }

    if (index < 0) {
        return 0;
    }
    if (index >= (int32_t)wheel->item_count) {
        return (int32_t)wheel->item_count - 1;
    }
    return index;
}

static int32_t gfx_wheel_clamp_scroll_y(const gfx_object_t *obj, const gfx_wheel_t *wheel, int32_t scroll_y)
{
    int32_t max_scroll_y;

    if (wheel == NULL || wheel->cyclic) {
        return scroll_y;
    }

    if (scroll_y < 0) {
        return 0;
    }
    max_scroll_y = gfx_wheel_max_scroll_y(obj, wheel);
    if (scroll_y > max_scroll_y) {
        return max_scroll_y;
    }
    return scroll_y;
}

static void gfx_wheel_set_scroll_y(gfx_object_t *obj, gfx_wheel_t *wheel, int32_t scroll_y)
{
    int32_t next;

    if (obj == NULL || wheel == NULL) {
        return;
    }

    next = gfx_wheel_clamp_scroll_y(obj, wheel, scroll_y);
    if (wheel->scroll_y != next) {
        wheel->scroll_y = next;
        gfx_object_invalidate(obj);
    }
}

static void gfx_wheel_tween_value_cb(gfx_tween_t *tween, gfx_object_t *obj, int32_t value, void *user_data)
{
    gfx_wheel_t *wheel = (gfx_wheel_t *)user_data;
    (void)tween;

    gfx_wheel_set_scroll_y(obj, wheel, value);
}

static int32_t gfx_wheel_index_from_scroll(const gfx_object_t *obj, const gfx_wheel_t *wheel)
{
    int32_t numerator;
    int32_t index;

    if (obj == NULL || wheel == NULL || wheel->item_height == 0U || wheel->item_count == 0U) {
        return -1;
    }

    numerator = wheel->scroll_y + ((int32_t)obj->geometry.height - (int32_t)wheel->item_height) / 2;
    if (numerator >= 0) {
        index = (numerator + (int32_t)wheel->item_height / 2) / (int32_t)wheel->item_height;
    } else {
        index = (numerator - (int32_t)wheel->item_height / 2) / (int32_t)wheel->item_height;
    }
    return gfx_wheel_clamp_index(wheel, index);
}

static int32_t gfx_wheel_raw_index_from_scroll(const gfx_object_t *obj, const gfx_wheel_t *wheel)
{
    int32_t numerator;

    if (obj == NULL || wheel == NULL || wheel->item_height == 0U || wheel->item_count == 0U) {
        return 0;
    }

    numerator = wheel->scroll_y + ((int32_t)obj->geometry.height - (int32_t)wheel->item_height) / 2;
    if (numerator >= 0) {
        return (numerator + (int32_t)wheel->item_height / 2) / (int32_t)wheel->item_height;
    }
    return (numerator - (int32_t)wheel->item_height / 2) / (int32_t)wheel->item_height;
}

static void gfx_wheel_emit_value(gfx_object_t *obj, gfx_wheel_t *wheel, int32_t index)
{
    if (wheel != NULL && wheel->value_cb != NULL) {
        wheel->value_cb(obj, index, wheel->value_user_data);
    }
}

static void gfx_wheel_emit_confirm(gfx_object_t *obj, gfx_wheel_t *wheel, int32_t index)
{
    if (wheel != NULL && wheel->confirm_cb != NULL) {
        wheel->confirm_cb(obj, index, wheel->confirm_user_data);
    }
}

static void gfx_wheel_set_selected_internal(gfx_object_t *obj, gfx_wheel_t *wheel, int32_t index, bool emit)
{
    int32_t next_index;

    if (obj == NULL || wheel == NULL || wheel->item_count == 0U) {
        return;
    }

    gfx_tween_stop(wheel->tween, false);
    next_index = gfx_wheel_clamp_index(wheel, index);
    if (wheel->selected_index != next_index) {
        wheel->selected_index = next_index;
        if (emit) {
            gfx_wheel_emit_value(obj, wheel, next_index);
        }
    }
    gfx_wheel_set_scroll_y(obj, wheel, gfx_wheel_center_scroll_y(obj, wheel, next_index));
    gfx_object_invalidate(obj);
}

static void gfx_wheel_snap(gfx_object_t *obj, gfx_wheel_t *wheel)
{
    int32_t index = gfx_wheel_index_from_scroll(obj, wheel);
    if (index >= 0) {
        int32_t next_index = gfx_wheel_clamp_index(wheel, index);
        int32_t target_scroll = gfx_wheel_center_scroll_y(obj, wheel, next_index);
        if (wheel->selected_index != next_index) {
            wheel->selected_index = next_index;
            gfx_wheel_emit_value(obj, wheel, next_index);
        }
        if (wheel->tween == NULL) {
            wheel->tween = gfx_tween_create(obj);
        }
        if (wheel->tween == NULL ||
                gfx_tween_start_i32(wheel->tween, wheel->scroll_y, target_scroll, GFX_WHEEL_TWEEN_MS,
                                    GFX_TWEEN_EASE_OUT_QUAD,
                                    gfx_wheel_tween_value_cb, NULL, wheel) != GFX_OK) {
            gfx_wheel_set_scroll_y(obj, wheel, target_scroll);
        }
    }
}

static gfx_err_t gfx_wheel_call_label_update(gfx_object_t *obj, gfx_wheel_t *wheel,
        const char *text, const gfx_area_t *text_area)
{
    wheel->label.text.text = (char *)(text ? text : "");
    wheel->label.text.text_width = 0;
    wheel->label.scroll.offset = 0;
    wheel->label.snap.offset = 0;
    free(wheel->label.render.mask);
    wheel->label.render.mask = NULL;
    wheel->label.render.mask_capacity = 0;

    return gfx_label_text_box_update(obj, &wheel->label, text_area);
}

static gfx_err_t gfx_wheel_draw_text(gfx_object_t *obj, gfx_wheel_t *wheel, const gfx_draw_ctx_t *ctx,
                                     const char *text, const gfx_area_t *row_area,
                                     const gfx_area_t *clip_area, gfx_color_t color)
{
    gfx_area_t text_area;
    gfx_err_t ret;

    if (row_area->x2 <= row_area->x1 || row_area->y2 <= row_area->y1 ||
            clip_area == NULL || clip_area->x2 <= clip_area->x1 || clip_area->y2 <= clip_area->y1) {
        return GFX_OK;
    }

    text_area.x1 = (gfx_coord_t)(row_area->x1 + GFX_WHEEL_PAD_X);
    text_area.y1 = (gfx_coord_t)(row_area->y1 + GFX_WHEEL_PAD_Y);
    text_area.x2 = (gfx_coord_t)(row_area->x2 - GFX_WHEEL_PAD_X);
    text_area.y2 = (gfx_coord_t)(row_area->y2 - GFX_WHEEL_PAD_Y);
    if (text_area.x2 <= text_area.x1 || text_area.y2 <= text_area.y1) {
        text_area = *row_area;
    }

    ret = gfx_wheel_call_label_update(obj, wheel, text, &text_area);
    if (ret != GFX_OK) {
        return ret;
    }

    wheel->label.style.color = color;
    wheel->label.style.bg_enable = false;

    return gfx_label_text_box_draw(obj, &wheel->label, ctx, &text_area, clip_area);
}

static int32_t gfx_wheel_draw_index_for_slot(const gfx_wheel_t *wheel, int32_t base_index, int32_t slot)
{
    int32_t index = base_index + slot;

    if (wheel == NULL || wheel->item_count == 0U) {
        return -1;
    }
    if (wheel->cyclic) {
        return gfx_wheel_clamp_index(wheel, index);
    }
    return (index >= 0 && index < (int32_t)wheel->item_count) ? index : -1;
}

static gfx_err_t gfx_wheel_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_wheel_t *wheel;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t center_area;
    int32_t center_raw_index;
    int32_t center_line_y;
    int32_t half_rows;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(ctx, GFX_ERR_INVALID_ARG);

    wheel = (gfx_wheel_t *)obj->src;
    GFX_RETURN_IF_NULL(wheel, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip_area, wheel->style.bg_color, 0xFFU);

    center_line_y = gfx_wheel_center_y(&obj_area);
    center_area.x1 = obj_area.x1;
    center_area.y1 = (gfx_coord_t)(center_line_y - (int32_t)wheel->item_height / 2);
    center_area.x2 = obj_area.x2;
    center_area.y2 = (gfx_coord_t)(center_area.y1 + (gfx_coord_t)wheel->item_height);
    if (center_area.y1 < obj_area.y1) {
        center_area.y1 = obj_area.y1;
    }
    if (center_area.y2 > obj_area.y2) {
        center_area.y2 = obj_area.y2;
    }
    if (gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &center_area)) {
        gfx_render_surface_fill(obj->disp, &dst_surface, &clip_area,
                                wheel->style.center_bg_color, 0xFFU);
    }

    if (wheel->style.border_width > 0U) {
        gfx_render_surface_rect_stroke(obj->disp, &dst_surface, &center_area,
                                       wheel->style.border_width, wheel->style.border_color, 0xFF);
        gfx_render_surface_rect_stroke(obj->disp, &dst_surface, &obj_area,
                                       wheel->style.border_width, wheel->style.border_color, 0xFF);
    }

    if (wheel->item_count == 0U || wheel->item_height == 0U) {
        return GFX_OK;
    }

    center_raw_index = gfx_wheel_raw_index_from_scroll(obj, wheel);
    half_rows = (int32_t)wheel->visible_rows / 2 + 2;
    for (int32_t slot = -half_rows; slot <= half_rows; slot++) {
        int32_t index = gfx_wheel_draw_index_for_slot(wheel, center_raw_index, slot);
        gfx_area_t row_area;
        gfx_area_t draw_area;
        gfx_area_t row_clip;
        gfx_color_t text_color;
        int32_t visual_index;

        if (index < 0) {
            continue;
        }

        visual_index = center_raw_index + slot;
        row_area.x1 = obj_area.x1;
        row_area.y1 = (gfx_coord_t)(obj_area.y1 + visual_index * (int32_t)wheel->item_height - wheel->scroll_y);
        row_area.x2 = obj_area.x2;
        row_area.y2 = (gfx_coord_t)(row_area.y1 + (gfx_coord_t)wheel->item_height);
        if (row_area.y1 >= obj_area.y2 || row_area.y2 <= obj_area.y1) {
            continue;
        }

        draw_area = row_area;
        if (draw_area.y1 < obj_area.y1) {
            draw_area.y1 = obj_area.y1;
        }
        if (draw_area.y2 > obj_area.y2) {
            draw_area.y2 = obj_area.y2;
        }
        if (!gfx_area_intersect_exclusive(&row_clip, &ctx->clip_area, &draw_area)) {
            continue;
        }

        text_color = (index == wheel->selected_index) ? wheel->style.center_text_color : wheel->style.text_color;
        gfx_wheel_draw_text(obj, wheel, ctx, wheel->items[index], &row_area, &row_clip, text_color);
    }

    return GFX_OK;
}

static gfx_err_t gfx_wheel_update(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    return GFX_OK;
}

static gfx_err_t gfx_wheel_delete_impl(gfx_object_t *obj)
{
    gfx_wheel_t *wheel;

    CHECK_OBJ_TYPE_WHEEL(obj);
    wheel = (gfx_wheel_t *)obj->src;
    if (wheel == NULL) {
        return GFX_OK;
    }

    gfx_wheel_free_items(wheel);
    gfx_tween_delete(wheel->tween);
    free(wheel->label.render.mask);
    free(wheel->label.render.color_mask);
    free(wheel);
    return GFX_OK;
}

static gfx_err_t gfx_wheel_load_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    return gfx_label_load_state(obj, &((gfx_wheel_t *)obj->src)->label);
}

static void gfx_wheel_release_impl(gfx_object_t *obj)
{
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_WHEEL) {
        return;
    }

    gfx_label_release_state(&((gfx_wheel_t *)obj->src)->label);
}

static void gfx_wheel_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_wheel_t *wheel;
    int32_t abs_dx;
    int32_t abs_dy;
    int32_t delta_y;
    bool was_pressed;
    bool was_dragging;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    wheel = (gfx_wheel_t *)obj->src;
    if (wheel->item_count == 0U || wheel->item_height == 0U) {
        return;
    }

    was_pressed = wheel->touch.pressed;
    was_dragging = wheel->touch.dragging;
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        wheel->touch.pressed = false;
        wheel->touch.dragging = false;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        wheel->touch.pressed = true;
        wheel->touch.dragging = false;
        wheel->touch.start_x = event->x;
        wheel->touch.start_y = event->y;
        wheel->touch.last_y = event->y;
        wheel->touch.last_timestamp_ms = event->timestamp_ms;
        wheel->touch.velocity_y = 0;
        gfx_tween_stop(wheel->tween, false);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && wheel->touch.pressed) {
        abs_dx = (int32_t)event->x - (int32_t)wheel->touch.start_x;
        abs_dy = (int32_t)event->y - (int32_t)wheel->touch.start_y;
        if (abs_dx < 0) {
            abs_dx = -abs_dx;
        }
        if (abs_dy < 0) {
            abs_dy = -abs_dy;
        }

        if (!wheel->touch.dragging &&
                (abs_dx >= (int32_t)wheel->drag_threshold || abs_dy >= (int32_t)wheel->drag_threshold)) {
            wheel->touch.dragging = true;
        }
        if (wheel->touch.dragging) {
            uint32_t dt = event->timestamp_ms - wheel->touch.last_timestamp_ms;
            delta_y = (int32_t)wheel->touch.last_y - (int32_t)event->y;
            gfx_wheel_set_scroll_y(obj, wheel, wheel->scroll_y + delta_y);
            if (dt > 0U) {
                wheel->touch.velocity_y = (delta_y * 1000) / (int32_t)dt;
            }
        }
        wheel->touch.last_y = event->y;
        wheel->touch.last_timestamp_ms = event->timestamp_ms;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && was_pressed) {
        gfx_wheel_snap(obj, wheel);
        if (!was_dragging) {
            gfx_wheel_emit_confirm(obj, wheel, wheel->selected_index);
        }
    }
}

gfx_object_t *gfx_wheel_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_wheel_t *wheel;
    uint16_t height = GFX_WHEEL_DEFAULT_VISIBLE_ROWS * GFX_WHEEL_DEFAULT_ITEM_HEIGHT;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create wheel: display is NULL");
        return NULL;
    }

    wheel = calloc(1, sizeof(gfx_wheel_t));
    if (wheel == NULL) {
        GFX_LOGE(TAG, "create wheel: no mem for state");
        return NULL;
    }
    gfx_wheel_init_default_state(wheel);

    if (gfx_object_create_class_instance(disp, &s_gfx_wheel_widget_class,
                                         wheel, GFX_WHEEL_DEFAULT_WIDTH, height,
                                         "gfx_wheel_create", &obj) != GFX_OK) {
        free(wheel);
        GFX_LOGE(TAG, "create wheel: no mem for object");
        return NULL;
    }
    wheel->tween = gfx_tween_create(obj);
    if (wheel->tween == NULL) {
        (void)gfx_object_delete(obj);
        return NULL;
    }

    return obj;
}

gfx_err_t gfx_wheel_clear(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    gfx_wheel_free_items((gfx_wheel_t *)obj->src);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_add_item(gfx_object_t *obj, const char *text)
{
    gfx_wheel_t *wheel;
    char **new_items;
    char *dup_text = NULL;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    wheel = (gfx_wheel_t *)obj->src;
    ret = gfx_wheel_dup_text(text, &dup_text);
    if (ret != GFX_OK) {
        return ret;
    }

    new_items = realloc(wheel->items, ((size_t)wheel->item_count + 1U) * sizeof(char *));
    if (new_items == NULL) {
        free(dup_text);
        return GFX_ERR_NO_MEM;
    }

    wheel->items = new_items;
    wheel->items[wheel->item_count] = dup_text;
    wheel->item_count++;
    gfx_wheel_set_selected_internal(obj, wheel, wheel->selected_index, false);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count)
{
    gfx_wheel_t *wheel;
    char **new_items = NULL;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    if (item_count > 0U) {
        GFX_RETURN_IF_NULL(items, GFX_ERR_INVALID_ARG);
    }

    wheel = (gfx_wheel_t *)obj->src;
    if (item_count > 0U) {
        new_items = calloc(item_count, sizeof(char *));
        if (new_items == NULL) {
            return GFX_ERR_NO_MEM;
        }
    }

    for (uint16_t i = 0; i < item_count; i++) {
        ret = gfx_wheel_dup_text(items[i], &new_items[i]);
        if (ret != GFX_OK) {
            for (uint16_t j = 0; j < i; j++) {
                free(new_items[j]);
            }
            free(new_items);
            return ret;
        }
    }

    gfx_wheel_free_items(wheel);
    wheel->items = new_items;
    wheel->item_count = item_count;
    wheel->selected_index = (item_count > 0U) ? 0 : -1;
    wheel->scroll_y = 0;
    gfx_wheel_set_selected_internal(obj, wheel, wheel->selected_index, false);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_selected(gfx_object_t *obj, int32_t index)
{
    gfx_wheel_t *wheel;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    wheel = (gfx_wheel_t *)obj->src;
    if (!wheel->cyclic && (index < 0 || index >= wheel->item_count)) {
        return GFX_ERR_INVALID_ARG;
    }
    gfx_wheel_set_selected_internal(obj, wheel, index, true);
    return GFX_OK;
}

gfx_err_t gfx_wheel_confirm(gfx_object_t *obj)
{
    gfx_wheel_t *wheel;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    wheel = (gfx_wheel_t *)obj->src;
    if (wheel->selected_index < 0 || wheel->selected_index >= (int32_t)wheel->item_count) {
        return GFX_ERR_INVALID_STATE;
    }
    gfx_wheel_emit_confirm(obj, wheel, wheel->selected_index);
    return GFX_OK;
}

int32_t gfx_wheel_get_selected(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_WHEEL || obj->src == NULL) {
        return -1;
    }
    return ((gfx_wheel_t *)obj->src)->selected_index;
}

uint16_t gfx_wheel_get_item_count(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_WHEEL || obj->src == NULL) {
        return 0;
    }
    return ((gfx_wheel_t *)obj->src)->item_count;
}

const char *gfx_wheel_get_item_text(gfx_object_t *obj, uint16_t index)
{
    gfx_wheel_t *wheel;

    if (obj == NULL || obj->type != GFX_OBJ_TYPE_WHEEL || obj->src == NULL) {
        return NULL;
    }

    wheel = (gfx_wheel_t *)obj->src;
    if (index >= wheel->item_count) {
        return NULL;
    }
    return wheel->items[index];
}

gfx_err_t gfx_wheel_set_font(gfx_object_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    return gfx_label_set_font_source(obj, &((gfx_wheel_t *)obj->src)->label, font);
}

gfx_err_t gfx_wheel_set_item_height(gfx_object_t *obj, uint16_t height)
{
    gfx_wheel_t *wheel;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(height > 0U, GFX_ERR_INVALID_ARG, TAG, "item height must be > 0");

    wheel = (gfx_wheel_t *)obj->src;
    wheel->item_height = height;
    gfx_wheel_set_selected_internal(obj, wheel, wheel->selected_index, false);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_visible_rows(gfx_object_t *obj, uint8_t rows)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(rows > 0U, GFX_ERR_INVALID_ARG, TAG, "visible rows must be > 0");
    ((gfx_wheel_t *)obj->src)->visible_rows = rows;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_cyclic(gfx_object_t *obj, bool cyclic)
{
    gfx_wheel_t *wheel;

    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    wheel = (gfx_wheel_t *)obj->src;
    wheel->cyclic = cyclic;
    gfx_wheel_set_selected_internal(obj, wheel, wheel->selected_index, false);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_drag_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->drag_threshold = threshold;
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_value_cb(gfx_object_t *obj, gfx_wheel_value_cb_t cb, void *user_data)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->value_cb = cb;
    ((gfx_wheel_t *)obj->src)->value_user_data = user_data;
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_confirm_cb(gfx_object_t *obj, gfx_wheel_confirm_cb_t cb, void *user_data)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->confirm_cb = cb;
    ((gfx_wheel_t *)obj->src)->confirm_user_data = user_data;
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_center_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.center_bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_center_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.center_text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_border_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.border_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_wheel_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_WHEEL(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_wheel_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}
