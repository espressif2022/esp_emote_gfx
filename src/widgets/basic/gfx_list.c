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

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LIST
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "render/sw/gfx_sw_draw_priv.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/input.h"
#include "platform/gfx_platform.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/list_core.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_LIST(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LIST, TAG)
#define GFX_LIST_DEFAULT_WIDTH       260U
#define GFX_LIST_DEFAULT_HEIGHT      160U
#define GFX_LIST_DEFAULT_ITEM_HEIGHT  36U
#define GFX_LIST_DEFAULT_PAD_X         0U
#define GFX_LIST_DEFAULT_PAD_Y         4U
#define GFX_LIST_DEFAULT_DRAG_THRESHOLD GFX_LIST_CORE_DRAG_THRESHOLD
#define GFX_LIST_INERTIA_MIN_VELOCITY   GFX_LIST_CORE_INERTIA_MIN_V

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_label_t label;
    char **items;
    uint16_t item_count;
    uint16_t top_index;
    int32_t focused_index;
    int32_t selected_index;
    int32_t pressed_index;
    int32_t scroll_y;
    uint16_t page_index;
    uint16_t items_per_page;
    uint16_t item_height;
    uint16_t pad_x;
    uint16_t pad_y;
    struct {
        bool pressed;
        bool dragging;
        uint16_t start_x;
        uint16_t start_y;
        uint16_t last_y;
        int32_t start_scroll_y;
        uint32_t last_timestamp_ms;
        int32_t velocity_y;
    } touch;
    struct {
        bool active;
        uint32_t last_ms;
        int32_t velocity_y;
    } inertia;
    struct {
        bool snap_to_item;
        uint16_t drag_threshold;
    } behavior;
    struct {
        gfx_color_t bg_color;
        gfx_color_t focus_bg_color;
        gfx_color_t selected_bg_color;
        gfx_color_t pressed_bg_color;
        gfx_color_t text_color;
        gfx_color_t focus_text_color;
        gfx_color_t selected_text_color;
        gfx_color_t pressed_text_color;
        gfx_color_t border_color;
        uint16_t border_width;
    } style;
    gfx_list_focus_cb_t focus_cb;
    void *focus_user_data;
    gfx_list_select_cb_t select_cb;
    void *select_user_data;
    gfx_list_page_load_cb_t page_load_cb;
    void *page_load_user_data;
} gfx_list_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "list";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_list_init_default_state(gfx_list_t *list);
static gfx_err_t gfx_list_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static gfx_err_t gfx_list_update(gfx_object_t *obj);
static gfx_err_t gfx_list_delete_impl(gfx_object_t *obj);
static gfx_err_t gfx_list_load_impl(gfx_object_t *obj);
static void gfx_list_release_impl(gfx_object_t *obj);
static void gfx_list_touch_event(gfx_object_t *obj, const void *event_data);
static gfx_err_t gfx_list_draw_text(gfx_object_t *obj, gfx_list_t *list, const gfx_draw_ctx_t *ctx,
                                    const char *text, const gfx_area_t *row_area,
                                    const gfx_area_t *clip_area, gfx_color_t color);
static void gfx_list_free_items(gfx_list_t *list);
static gfx_err_t gfx_list_dup_text(const char *text, char **out_text);
static int32_t gfx_list_max_scroll_y(const gfx_object_t *obj, const gfx_list_t *list);
static int32_t gfx_list_clamp_scroll_y(const gfx_object_t *obj, const gfx_list_t *list, int32_t scroll_y);
static void gfx_list_set_scroll_y(gfx_object_t *obj, gfx_list_t *list, int32_t scroll_y);
static void gfx_list_set_scroll_y_raw(gfx_object_t *obj, gfx_list_t *list, int32_t scroll_y, bool allow_overscroll);
static void gfx_list_sync_top_index(gfx_list_t *list);
static uint16_t gfx_list_effective_items_per_page(const gfx_object_t *obj, const gfx_list_t *list);
static uint16_t gfx_list_page_count(const gfx_object_t *obj, const gfx_list_t *list);
static void gfx_list_sync_page_index(gfx_object_t *obj, gfx_list_t *list, bool emit);
static void gfx_list_set_selected_internal(gfx_object_t *obj, gfx_list_t *list, int32_t index, bool confirmed);
static void gfx_list_snap_scroll(gfx_object_t *obj, gfx_list_t *list);
static int32_t gfx_list_index_from_point(const gfx_area_t *obj_area, const gfx_list_t *list, uint16_t y);
static bool gfx_list_anim_step(gfx_object_t *obj, gfx_list_t *list);

static const gfx_widget_class_t s_gfx_list_widget_class = {
    .type = GFX_OBJ_TYPE_LIST,
    .name = "list",
    .draw = gfx_list_draw,
    .delete = gfx_list_delete_impl,
    .load = gfx_list_load_impl,
    .release = gfx_list_release_impl,
    .update = gfx_list_update,
    .touch_event = gfx_list_touch_event,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_list_init_default_state(gfx_list_t *list)
{
    memset(list, 0, sizeof(*list));

    list->label.style.opa = 0xFF;
    list->label.style.bg_enable = false;
    list->label.style.text_align = GFX_TEXT_ALIGN_LEFT;
    list->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    list->label.text.line_spacing = 0;

    list->focused_index = 0;
    list->selected_index = -1;
    list->pressed_index = -1;
    list->item_height = GFX_LIST_DEFAULT_ITEM_HEIGHT;
    list->pad_x = GFX_LIST_DEFAULT_PAD_X;
    list->pad_y = GFX_LIST_DEFAULT_PAD_Y;
    list->behavior.snap_to_item = true;
    list->behavior.drag_threshold = GFX_LIST_DEFAULT_DRAG_THRESHOLD;

    list->style.bg_color = GFX_COLOR_HEX(0x000000);
    list->style.focus_bg_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.selected_bg_color = GFX_COLOR_HEX(0x243447);
    list->style.pressed_bg_color = GFX_COLOR_HEX(0x33485F);
    list->style.text_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.focus_text_color = GFX_COLOR_HEX(0x000000);
    list->style.selected_text_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.pressed_text_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.border_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.border_width = 1;
}

static gfx_err_t gfx_list_dup_text(const char *text, char **out_text)
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

static void gfx_list_free_items(gfx_list_t *list)
{
    if (list == NULL) {
        return;
    }

    for (uint16_t i = 0; i < list->item_count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->item_count = 0;
    list->top_index = 0;
    list->page_index = 0;
    list->focused_index = 0;
    list->selected_index = -1;
    list->pressed_index = -1;
    list->scroll_y = 0;
    memset(&list->touch, 0, sizeof(list->touch));
    memset(&list->inertia, 0, sizeof(list->inertia));
}

static uint16_t gfx_list_visible_count(const gfx_object_t *obj, const gfx_list_t *list)
{
    uint16_t visible;

    if (obj == NULL || list == NULL || list->item_height == 0U) {
        return 0;
    }

    visible = (uint16_t)((obj->geometry.height + list->item_height - 1U) / list->item_height);
    return visible > 0U ? visible : 1U;
}

static uint16_t gfx_list_max_top_index(const gfx_object_t *obj, const gfx_list_t *list)
{
    uint16_t visible;

    if (list == NULL || list->item_count == 0U) {
        return 0;
    }

    visible = gfx_list_visible_count(obj, list);
    if (visible >= list->item_count) {
        return 0;
    }
    return (uint16_t)(list->item_count - visible);
}

static void gfx_list_clamp_top_index(gfx_object_t *obj, gfx_list_t *list)
{
    if (obj == NULL || list == NULL) {
        return;
    }

    gfx_list_set_scroll_y(obj, list, list->scroll_y);
}

static int32_t gfx_list_max_scroll_y(const gfx_object_t *obj, const gfx_list_t *list)
{
    if (obj == NULL || list == NULL) {
        return 0;
    }
    return gfx_list_core_max_scroll_y((int32_t)obj->geometry.height, list->item_height,
                                      list->item_count);
}

static int32_t gfx_list_clamp_scroll_y(const gfx_object_t *obj, const gfx_list_t *list, int32_t scroll_y)
{
    if (obj == NULL || list == NULL) {
        return scroll_y;
    }
    return gfx_list_core_clamp_scroll_y((int32_t)obj->geometry.height, list->item_height,
                                        list->item_count, scroll_y, false);
}

static int32_t gfx_list_clamp_scroll_y_overscroll(const gfx_object_t *obj, const gfx_list_t *list,
        int32_t scroll_y)
{
    if (obj == NULL || list == NULL) {
        return scroll_y;
    }
    return gfx_list_core_clamp_scroll_y((int32_t)obj->geometry.height, list->item_height,
                                        list->item_count, scroll_y, true);
}

static void gfx_list_sync_top_index(gfx_list_t *list)
{
    if (list == NULL || list->item_height == 0U) {
        return;
    }

    list->top_index = (uint16_t)(MAX(0, list->scroll_y) / (int32_t)list->item_height);
}

static uint16_t gfx_list_effective_items_per_page(const gfx_object_t *obj, const gfx_list_t *list)
{
    if (obj == NULL || list == NULL) {
        return 1;
    }
    if (list->items_per_page > 0U) {
        return list->items_per_page;
    }
    return gfx_list_visible_count(obj, list);
}

static uint16_t gfx_list_page_count(const gfx_object_t *obj, const gfx_list_t *list)
{
    uint16_t per_page;

    if (obj == NULL || list == NULL || list->item_count == 0U) {
        return 0;
    }

    per_page = gfx_list_effective_items_per_page(obj, list);
    if (per_page == 0U) {
        per_page = 1;
    }
    return (uint16_t)((list->item_count + per_page - 1U) / per_page);
}

static void gfx_list_sync_page_index(gfx_object_t *obj, gfx_list_t *list, bool emit)
{
    uint16_t per_page;
    uint16_t page_count;
    uint16_t next_page;

    if (obj == NULL || list == NULL) {
        return;
    }

    per_page = gfx_list_effective_items_per_page(obj, list);
    if (per_page == 0U) {
        per_page = 1;
    }
    page_count = gfx_list_page_count(obj, list);
    next_page = (uint16_t)(list->top_index / per_page);
    if (page_count > 0U && next_page >= page_count) {
        next_page = (uint16_t)(page_count - 1U);
    }

    if (list->page_index != next_page) {
        list->page_index = next_page;
        if (emit && list->page_load_cb != NULL) {
            list->page_load_cb(obj, next_page, per_page, list->page_load_user_data);
        }
    }
}

static void gfx_list_set_scroll_y(gfx_object_t *obj, gfx_list_t *list, int32_t scroll_y)
{
    gfx_list_set_scroll_y_raw(obj, list, scroll_y, false);
}

static void gfx_list_set_scroll_y_raw(gfx_object_t *obj, gfx_list_t *list, int32_t scroll_y, bool allow_overscroll)
{
    int32_t next_scroll_y;

    if (obj == NULL || list == NULL) {
        return;
    }

    next_scroll_y = allow_overscroll ? gfx_list_clamp_scroll_y_overscroll(obj, list, scroll_y) :
                    gfx_list_clamp_scroll_y(obj, list, scroll_y);
    if (list->scroll_y != next_scroll_y) {
        list->scroll_y = next_scroll_y;
        gfx_list_sync_top_index(list);
        gfx_list_sync_page_index(obj, list, true);
        gfx_object_invalidate(obj);
        return;
    }
    gfx_list_sync_top_index(list);
    gfx_list_sync_page_index(obj, list, false);
}

static uint32_t gfx_list_now_ms(void)
{
    return (uint32_t)(gfx_platform_time_us() / 1000);
}

static int32_t gfx_list_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static void gfx_list_stop_inertia(gfx_list_t *list)
{
    if (list == NULL) {
        return;
    }
    memset(&list->inertia, 0, sizeof(list->inertia));
}

static void gfx_list_start_inertia(gfx_list_t *list, int32_t velocity_y)
{
    if (list == NULL) {
        return;
    }
    if (gfx_list_abs_i32(velocity_y) < GFX_LIST_INERTIA_MIN_VELOCITY) {
        gfx_list_stop_inertia(list);
        return;
    }

    list->inertia.active = true;
    list->inertia.last_ms = gfx_list_now_ms();
    list->inertia.velocity_y = velocity_y;
}

static bool gfx_list_anim_step(gfx_object_t *obj, gfx_list_t *list)
{
    bool changed = false;
    bool animating;
    int32_t before;

    if (obj == NULL || list == NULL) {
        return false;
    }

    before = list->scroll_y;
    animating = gfx_list_core_anim_step((int32_t)obj->geometry.height, list->item_height,
                                        list->item_count, &list->scroll_y, &list->inertia.velocity_y,
                                        &list->inertia.last_ms, &list->inertia.active,
                                        list->touch.pressed, list->behavior.snap_to_item,
                                        gfx_list_now_ms(), &changed);
    if (list->scroll_y != before) {
        gfx_list_sync_top_index(list);
        gfx_list_sync_page_index(obj, list, true);
        changed = true;
    }
    if (changed) {
        gfx_object_invalidate(obj);
    }
    return animating || changed;
}

static void gfx_list_set_selected_internal(gfx_object_t *obj, gfx_list_t *list, int32_t index, bool confirmed)
{
    bool changed;

    if (obj == NULL || list == NULL) {
        return;
    }

    changed = list->selected_index != index;
    if (changed) {
        list->selected_index = index;
        gfx_object_invalidate(obj);
    }
    if (list->select_cb != NULL && (changed || confirmed)) {
        list->select_cb(obj, index, confirmed, list->select_user_data);
    }
}

static void gfx_list_snap_scroll(gfx_object_t *obj, gfx_list_t *list)
{
    int32_t snapped;

    if (obj == NULL || list == NULL || !list->behavior.snap_to_item || list->item_height == 0U) {
        return;
    }

    snapped = gfx_list_core_snap_scroll_y(list->scroll_y, list->item_height);
    gfx_list_set_scroll_y(obj, list, snapped);
}

static int32_t gfx_list_index_from_point(const gfx_area_t *obj_area, const gfx_list_t *list, uint16_t y)
{
    if (obj_area == NULL || list == NULL) {
        return -1;
    }
    return gfx_list_core_index_from_point((int32_t)obj_area->y1, (int32_t)y, list->scroll_y,
                                          list->item_height, list->item_count);
}

static gfx_err_t gfx_list_call_label_update(gfx_object_t *obj, gfx_list_t *list,
        const char *text, const gfx_area_t *text_area)
{
    list->label.text.text = (char *)(text ? text : "");
    list->label.text.text_width = 0;
    list->label.scroll.offset = 0;
    list->label.snap.offset = 0;
    free(list->label.render.mask);
    list->label.render.mask = NULL;
    list->label.render.mask_capacity = 0;

    return gfx_label_text_box_update(obj, &list->label, text_area);
}

static gfx_err_t gfx_list_draw_text(gfx_object_t *obj, gfx_list_t *list, const gfx_draw_ctx_t *ctx,
                                    const char *text, const gfx_area_t *row_area,
                                    const gfx_area_t *clip_area, gfx_color_t color)
{
    gfx_area_t text_area;
    gfx_err_t ret;

    if (row_area->x2 <= row_area->x1 || row_area->y2 <= row_area->y1 ||
            clip_area == NULL || clip_area->x2 <= clip_area->x1 || clip_area->y2 <= clip_area->y1) {
        return GFX_OK;
    }

    text_area.x1 = (gfx_coord_t)(row_area->x1 + (gfx_coord_t)list->pad_x);
    text_area.y1 = (gfx_coord_t)(row_area->y1 + (gfx_coord_t)list->pad_y);
    text_area.x2 = (gfx_coord_t)(row_area->x2 - (gfx_coord_t)list->pad_x);
    text_area.y2 = (gfx_coord_t)(row_area->y2 - (gfx_coord_t)list->pad_y);
    if (text_area.x2 <= text_area.x1 || text_area.y2 <= text_area.y1) {
        text_area = *row_area;
    }

    ret = gfx_list_call_label_update(obj, list, text, &text_area);
    if (ret != GFX_OK) {
        return ret;
    }

    list->label.style.color = color;
    list->label.style.bg_enable = false;

    return gfx_label_text_box_draw(obj, &list->label, ctx, &text_area, clip_area);
}

static gfx_err_t gfx_list_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_list_t *list;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(ctx, GFX_ERR_INVALID_ARG);

    list = (gfx_list_t *)obj->src;
    GFX_RETURN_IF_NULL(list, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip_area, list->style.bg_color, 0xFFU);

    gfx_list_clamp_top_index(obj, list);

    for (uint16_t i = list->top_index; i < list->item_count; i++) {
        gfx_area_t row_area;
        gfx_area_t draw_area;
        gfx_area_t row_clip;
        bool focused = ((int32_t)i == list->focused_index);
        bool selected = ((int32_t)i == list->selected_index);
        bool pressed = ((int32_t)i == list->pressed_index);
        gfx_color_t row_bg = list->style.bg_color;
        gfx_color_t text_color = list->style.text_color;

        row_area.x1 = obj_area.x1;
        row_area.y1 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)((int32_t)i * (int32_t)list->item_height - list->scroll_y));
        row_area.x2 = obj_area.x2;
        row_area.y2 = (gfx_coord_t)(row_area.y1 + (gfx_coord_t)list->item_height);
        if (row_area.y1 >= obj_area.y2) {
            break;
        }
        if (row_area.y2 <= obj_area.y1) {
            continue;
        }

        draw_area = row_area;
        if (draw_area.y2 > obj_area.y2) {
            draw_area.y2 = obj_area.y2;
        }
        if (draw_area.y1 < obj_area.y1) {
            draw_area.y1 = obj_area.y1;
        }

        if (selected) {
            row_bg = list->style.selected_bg_color;
            text_color = list->style.selected_text_color;
        }
        if (focused) {
            row_bg = list->style.focus_bg_color;
            text_color = list->style.focus_text_color;
        }
        if (pressed) {
            row_bg = list->style.pressed_bg_color;
            text_color = list->style.pressed_text_color;
        }

        if (!gfx_area_intersect_exclusive(&row_clip, &ctx->clip_area, &draw_area)) {
            continue;
        }

        gfx_render_surface_fill(obj->disp, &dst_surface, &row_clip, row_bg, 0xFFU);

        if (list->style.border_width > 0 && row_area.y2 > row_area.y1) {
            gfx_coord_t line_y = (gfx_coord_t)(draw_area.y2 - 1);
            for (uint16_t line = 0; line < list->style.border_width && line_y >= row_area.y1; line++) {
                gfx_sw_draw_hline_fmt(ctx->buf, ctx->stride, ctx->format,
                                      &ctx->buf_area, &ctx->clip_area,
                                      draw_area.x1, (gfx_coord_t)(draw_area.x2 - 1), line_y,
                                      list->style.border_color, 0xFF);
                line_y--;
            }
        }

        gfx_list_draw_text(obj, list, ctx, list->items[i], &row_area, &row_clip, text_color);
    }

    return GFX_OK;
}

static gfx_err_t gfx_list_update(gfx_object_t *obj)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    list = (gfx_list_t *)obj->src;
    if (list != NULL) {
        (void)gfx_list_anim_step(obj, list);
    }
    return GFX_OK;
}

static gfx_err_t gfx_list_delete_impl(gfx_object_t *obj)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);

    list = (gfx_list_t *)obj->src;
    if (list == NULL) {
        return GFX_OK;
    }

    gfx_list_free_items(list);
    free(list->label.render.mask);
    free(list->label.render.color_mask);
    free(list);
    return GFX_OK;
}

static gfx_err_t gfx_list_load_impl(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LIST(obj);
    return gfx_label_load_state(obj, &((gfx_list_t *)obj->src)->label);
}

static void gfx_list_release_impl(gfx_object_t *obj)
{
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_LIST) {
        return;
    }

    gfx_label_release_state(&((gfx_list_t *)obj->src)->label);
}

static void gfx_list_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_list_t *list;
    gfx_area_t obj_area;
    int32_t index;
    int32_t delta_y;
    int32_t abs_dx;
    int32_t abs_dy;
    bool was_pressed;
    bool was_dragging;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    list = (gfx_list_t *)obj->src;
    if (list->item_height == 0 || list->item_count == 0) {
        return;
    }

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return;
    }
    was_pressed = list->touch.pressed;
    was_dragging = list->touch.dragging;
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        list->touch.pressed = false;
        list->touch.dragging = false;
    }
    if ((gfx_coord_t)event->x < obj_area.x1 ||
            (gfx_coord_t)event->x >= obj_area.x2 ||
            (gfx_coord_t)event->y < obj_area.y1 ||
            (gfx_coord_t)event->y >= obj_area.y2) {
        if (event->type == GFX_TOUCH_EVENT_RELEASE && was_pressed) {
            list->pressed_index = -1;
            gfx_list_snap_scroll(obj, list);
            gfx_object_invalidate(obj);
        }
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        gfx_list_stop_inertia(list);
        list->touch.pressed = true;
        list->touch.dragging = false;
        list->touch.start_x = event->x;
        list->touch.start_y = event->y;
        list->touch.last_y = event->y;
        list->touch.start_scroll_y = list->scroll_y;
        list->touch.last_timestamp_ms = event->timestamp_ms;
        list->touch.velocity_y = 0;
        list->pressed_index = gfx_list_index_from_point(&obj_area, list, event->y);
        gfx_object_invalidate(obj);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && list->touch.pressed) {
        abs_dx = (int32_t)event->x - (int32_t)list->touch.start_x;
        abs_dy = (int32_t)event->y - (int32_t)list->touch.start_y;
        if (abs_dx < 0) {
            abs_dx = -abs_dx;
        }
        if (abs_dy < 0) {
            abs_dy = -abs_dy;
        }

        if (!list->touch.dragging &&
                (abs_dx >= (int32_t)list->behavior.drag_threshold ||
                 abs_dy >= (int32_t)list->behavior.drag_threshold)) {
            list->touch.dragging = true;
            list->pressed_index = -1;
        }

        if (list->touch.dragging) {
            uint32_t dt = event->timestamp_ms - list->touch.last_timestamp_ms;
            delta_y = (int32_t)list->touch.last_y - (int32_t)event->y;
            gfx_list_set_scroll_y_raw(obj, list, list->scroll_y + delta_y, true);
            if (dt > 0U) {
                list->touch.velocity_y = (delta_y * 1000) / (int32_t)dt;
            }
        }
        list->touch.last_y = event->y;
        list->touch.last_timestamp_ms = event->timestamp_ms;
        return;
    }

    if (event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }
    if (!was_pressed) {
        return;
    }

    list->pressed_index = -1;
    if (was_dragging) {
        gfx_list_start_inertia(list, list->touch.velocity_y);
        if (!list->inertia.active) {
            (void)gfx_list_anim_step(obj, list);
        }
        gfx_object_invalidate(obj);
        return;
    }

    index = gfx_list_index_from_point(&obj_area, list, event->y);
    if (index < 0 || index >= list->item_count) {
        return;
    }

    if (list->selected_index != index || list->focused_index != index) {
        list->focused_index = index;
        gfx_list_set_selected_internal(obj, list, index, true);
        gfx_object_invalidate(obj);
        if (list->focus_cb != NULL) {
            list->focus_cb(obj, index, list->focus_user_data);
        }
    } else {
        gfx_list_set_selected_internal(obj, list, index, true);
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_object_t *gfx_list_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_list_t *list;

    if (disp == NULL) {
        GFX_LOGE(TAG, "create list: display is NULL");
        return NULL;
    }

    list = calloc(1, sizeof(gfx_list_t));
    if (list == NULL) {
        GFX_LOGE(TAG, "create list: no mem for state");
        return NULL;
    }

    gfx_list_init_default_state(list);

    if (gfx_object_create_class_instance(disp, &s_gfx_list_widget_class,
                                         list, GFX_LIST_DEFAULT_WIDTH, GFX_LIST_DEFAULT_HEIGHT,
                                         "gfx_list_create", &obj) != GFX_OK) {
        free(list);
        GFX_LOGE(TAG, "create list: no mem for object");
        return NULL;
    }

    return obj;
}

gfx_err_t gfx_list_clear(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    gfx_list_free_items((gfx_list_t *)obj->src);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_add_item(gfx_object_t *obj, const char *text)
{
    gfx_list_t *list;
    char **new_items;
    char *dup_text = NULL;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    ret = gfx_list_dup_text(text, &dup_text);
    if (ret != GFX_OK) {
        return ret;
    }

    new_items = realloc(list->items, ((size_t)list->item_count + 1U) * sizeof(char *));
    if (new_items == NULL) {
        free(dup_text);
        return GFX_ERR_NO_MEM;
    }

    list->items = new_items;
    list->items[list->item_count] = dup_text;
    list->item_count++;
    if (list->item_count == 1U) {
        list->focused_index = 0;
    }
    gfx_list_set_scroll_y(obj, list, list->scroll_y);

    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count)
{
    gfx_list_t *list;
    char **new_items = NULL;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    if (item_count > 0U) {
        GFX_RETURN_IF_NULL(items, GFX_ERR_INVALID_ARG);
    }

    list = (gfx_list_t *)obj->src;
    if (item_count > 0U) {
        new_items = calloc(item_count, sizeof(char *));
        if (new_items == NULL) {
            return GFX_ERR_NO_MEM;
        }
    }

    for (uint16_t i = 0; i < item_count; i++) {
        ret = gfx_list_dup_text(items[i], &new_items[i]);
        if (ret != GFX_OK) {
            for (uint16_t j = 0; j < i; j++) {
                free(new_items[j]);
            }
            free(new_items);
            return ret;
        }
    }

    gfx_list_free_items(list);
    list->items = new_items;
    list->item_count = item_count;
    list->top_index = 0;
    list->page_index = 0;
    list->focused_index = (item_count > 0U) ? 0 : -1;
    list->selected_index = -1;
    list->pressed_index = -1;
    list->scroll_y = 0;
    memset(&list->touch, 0, sizeof(list->touch));

    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_focus(gfx_object_t *obj, int32_t index)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    if (index < -1 || index >= list->item_count) {
        return GFX_ERR_INVALID_ARG;
    }

    if (list->focused_index != index) {
        list->focused_index = index;
        if (index >= 0 && index < list->top_index) {
            gfx_list_set_scroll_y(obj, list, index * (int32_t)list->item_height);
        } else if (index >= 0) {
            uint16_t visible = gfx_list_visible_count(obj, list);
            if (visible > 0U && index >= (int32_t)(list->top_index + visible)) {
                gfx_list_set_scroll_y(obj, list,
                                      (index - (int32_t)visible + 1) * (int32_t)list->item_height);
            }
        }
        gfx_list_clamp_top_index(obj, list);
        gfx_object_invalidate(obj);
        if (list->focus_cb != NULL) {
            list->focus_cb(obj, index, list->focus_user_data);
        }
    }
    return GFX_OK;
}

int32_t gfx_list_get_focus(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST) {
        return -1;
    }
    GFX_RETURN_IF_NULL(obj->src, -1);
    return ((gfx_list_t *)obj->src)->focused_index;
}

gfx_err_t gfx_list_set_top_index(gfx_object_t *obj, uint16_t index)
{
    gfx_list_t *list;
    uint16_t max_top;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    max_top = gfx_list_max_top_index(obj, list);
    if (index > max_top) {
        index = max_top;
    }
    gfx_list_set_scroll_y(obj, list, (int32_t)index * (int32_t)list->item_height);
    return GFX_OK;
}

uint16_t gfx_list_get_top_index(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return ((gfx_list_t *)obj->src)->top_index;
}

gfx_err_t gfx_list_set_scroll_offset(gfx_object_t *obj, int32_t scroll_y)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    gfx_list_set_scroll_y(obj, (gfx_list_t *)obj->src, scroll_y);
    return GFX_OK;
}

int32_t gfx_list_get_scroll_offset(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return ((gfx_list_t *)obj->src)->scroll_y;
}

gfx_err_t gfx_list_set_selected(gfx_object_t *obj, int32_t index)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    if (index < -1 || index >= list->item_count) {
        return GFX_ERR_INVALID_ARG;
    }

    gfx_list_set_selected_internal(obj, list, index, false);
    return GFX_OK;
}

int32_t gfx_list_get_selected(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return -1;
    }
    return ((gfx_list_t *)obj->src)->selected_index;
}

gfx_err_t gfx_list_confirm(gfx_object_t *obj)
{
    gfx_list_t *list;
    int32_t index;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    index = list->selected_index >= 0 ? list->selected_index : list->focused_index;
    if (index < 0 || index >= list->item_count) {
        return GFX_ERR_INVALID_STATE;
    }

    gfx_list_set_selected_internal(obj, list, index, true);
    return GFX_OK;
}

gfx_err_t gfx_list_set_items_per_page(gfx_object_t *obj, uint16_t items_per_page)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->items_per_page = items_per_page;
    gfx_list_sync_page_index(obj, list, false);
    return GFX_OK;
}

uint16_t gfx_list_get_items_per_page(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return gfx_list_effective_items_per_page(obj, (gfx_list_t *)obj->src);
}

gfx_err_t gfx_list_set_page(gfx_object_t *obj, uint16_t page_index)
{
    gfx_list_t *list;
    uint16_t per_page;
    uint16_t page_count;
    uint16_t first_item;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    page_count = gfx_list_page_count(obj, list);
    if (page_count == 0U) {
        return page_index == 0U ? GFX_OK : GFX_ERR_INVALID_ARG;
    }
    if (page_index >= page_count) {
        page_index = (uint16_t)(page_count - 1U);
    }

    per_page = gfx_list_effective_items_per_page(obj, list);
    first_item = (uint16_t)(page_index * per_page);
    if (first_item >= list->item_count) {
        first_item = (uint16_t)(list->item_count - 1U);
    }

    if (list->page_index == page_index) {
        gfx_list_set_scroll_y(obj, list, (int32_t)first_item * (int32_t)list->item_height);
    } else {
        gfx_list_set_scroll_y(obj, list, (int32_t)first_item * (int32_t)list->item_height);
        if (list->page_index != page_index) {
            list->page_index = page_index;
            if (list->page_load_cb != NULL) {
                list->page_load_cb(obj, list->page_index, per_page, list->page_load_user_data);
            }
        }
    }
    return GFX_OK;
}

gfx_err_t gfx_list_prev_page(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return GFX_ERR_INVALID_ARG;
    }
    gfx_list_t *list = (gfx_list_t *)obj->src;
    return gfx_list_set_page(obj, list->page_index > 0U ? (uint16_t)(list->page_index - 1U) : 0U);
}

gfx_err_t gfx_list_next_page(gfx_object_t *obj)
{
    gfx_list_t *list;
    uint16_t page_count;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    page_count = gfx_list_page_count(obj, list);
    if (page_count == 0U) {
        return GFX_OK;
    }
    return gfx_list_set_page(obj, list->page_index + 1U < page_count ?
                             (uint16_t)(list->page_index + 1U) : (uint16_t)(page_count - 1U));
}

uint16_t gfx_list_get_page(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return ((gfx_list_t *)obj->src)->page_index;
}

uint16_t gfx_list_get_page_count(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return gfx_list_page_count(obj, (gfx_list_t *)obj->src);
}

uint16_t gfx_list_get_item_count(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST) {
        return 0;
    }
    GFX_RETURN_IF_NULL(obj->src, 0);
    return ((gfx_list_t *)obj->src)->item_count;
}

const char *gfx_list_get_item_text(gfx_object_t *obj, uint16_t index)
{
    gfx_list_t *list;

    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST) {
        return NULL;
    }
    GFX_RETURN_IF_NULL(obj->src, NULL);

    list = (gfx_list_t *)obj->src;
    if (index >= list->item_count) {
        return NULL;
    }
    return list->items[index];
}

gfx_err_t gfx_list_set_font(gfx_object_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    return gfx_label_set_font_source(obj, &((gfx_list_t *)obj->src)->label, font);
}

gfx_err_t gfx_list_set_item_height(gfx_object_t *obj, uint16_t height)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(height > 0U, GFX_ERR_INVALID_ARG, TAG, "item height must be > 0");

    ((gfx_list_t *)obj->src)->item_height = height;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_text_pad(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->pad_x = pad_x;
    list->pad_y = pad_y;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_snap_to_item(gfx_object_t *obj, bool enable)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->behavior.snap_to_item = enable;
    if (enable) {
        gfx_list_snap_scroll(obj, list);
    }
    return GFX_OK;
}

gfx_err_t gfx_list_set_drag_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->behavior.drag_threshold = threshold;
    return GFX_OK;
}

gfx_err_t gfx_list_set_focus_cb(gfx_object_t *obj, gfx_list_focus_cb_t cb, void *user_data)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->focus_cb = cb;
    list->focus_user_data = user_data;
    return GFX_OK;
}

gfx_err_t gfx_list_set_select_cb(gfx_object_t *obj, gfx_list_select_cb_t cb, void *user_data)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->select_cb = cb;
    list->select_user_data = user_data;
    return GFX_OK;
}

gfx_err_t gfx_list_set_page_load_cb(gfx_object_t *obj, gfx_list_page_load_cb_t cb, void *user_data)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->page_load_cb = cb;
    list->page_load_user_data = user_data;
    return GFX_OK;
}

gfx_err_t gfx_list_set_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_focus_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.focus_bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_selected_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.selected_bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_pressed_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.pressed_bg_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_focus_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.focus_text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_selected_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.selected_text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_pressed_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.pressed_text_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_border_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.border_color = color;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_list_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}
