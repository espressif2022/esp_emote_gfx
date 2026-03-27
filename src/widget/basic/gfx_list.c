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
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/draw/gfx_sw_draw_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/gfx_touch.h"
#include "widget/gfx_list.h"
#include "widget/label/gfx_label_priv.h"

/*********************
 *      DEFINES
 *********************/

#define CHECK_OBJ_TYPE_LIST(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_LIST, TAG)
#define GFX_LIST_DEFAULT_WIDTH           180
#define GFX_LIST_DEFAULT_HEIGHT          140
#define GFX_LIST_DEFAULT_PADDING_X         8
#define GFX_LIST_DEFAULT_PADDING_Y         6
#define GFX_LIST_DEFAULT_ROW_HEIGHT       22
#define GFX_LIST_DEFAULT_BORDER_WIDTH      1
#define GFX_LIST_EMPTY_TEXT        "(empty)"

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_label_t label; /* Must stay first so label internals can be reused safely. */

    struct {
        gfx_color_t bg_color;
        gfx_color_t selected_bg_color;
        gfx_color_t border_color;
        uint16_t border_width;
        uint16_t padding_x;
        uint16_t padding_y;
        uint16_t row_height;
    } style;

    struct {
        char **items;
        size_t count;
        size_t capacity;
        int32_t selected;
    } items;
} gfx_list_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "list";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_list_init_default_state(gfx_list_t *list);
static void gfx_list_apply_label_geometry(gfx_obj_t *obj);
static void gfx_list_free_items(gfx_list_t *list);
static esp_err_t gfx_list_items_reserve(gfx_list_t *list, size_t min_capacity);
static esp_err_t gfx_list_rebuild_text(gfx_obj_t *obj, gfx_list_t *list);
static int32_t gfx_list_get_row_by_y(gfx_obj_t *obj, const gfx_list_t *list, uint16_t y);
static esp_err_t gfx_list_set_selected_internal(gfx_obj_t *obj, gfx_list_t *list, int32_t index);
static esp_err_t gfx_list_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_list_update(gfx_obj_t *obj);
static esp_err_t gfx_list_delete_impl(gfx_obj_t *obj);
static void gfx_list_touch_event(gfx_obj_t *obj, const void *event_data);
static esp_err_t gfx_list_call_label_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_list_call_label_update(gfx_obj_t *obj);
static esp_err_t gfx_list_call_label_delete(gfx_obj_t *obj);

static const gfx_widget_class_t s_gfx_list_widget_class = {
    .type = GFX_OBJ_TYPE_LIST,
    .name = "list",
    .draw = gfx_list_draw,
    .delete = gfx_list_delete_impl,
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
    list->label.style.color = GFX_COLOR_HEX(0xF5F5F5);
    list->label.style.bg_color = GFX_COLOR_HEX(0x000000);
    list->label.style.bg_enable = false;
    list->label.style.text_align = GFX_TEXT_ALIGN_LEFT;
    list->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    list->label.text.line_spacing = 2;
    list->label.text.text_width = 0;

    list->label.scroll.offset = 0;
    list->label.scroll.step = 1;
    list->label.scroll.speed = 50;
    list->label.scroll.loop = true;
    list->label.scroll.scrolling = false;
    list->label.scroll.timer = NULL;

    list->label.snap.interval = 2000;
    list->label.snap.offset = 0;
    list->label.snap.loop = true;
    list->label.snap.timer = NULL;

    list->style.bg_color = GFX_COLOR_HEX(0x181818);
    list->style.selected_bg_color = GFX_COLOR_HEX(0x2A6DF4);
    list->style.border_color = GFX_COLOR_HEX(0x4A4A4A);
    list->style.border_width = GFX_LIST_DEFAULT_BORDER_WIDTH;
    list->style.padding_x = GFX_LIST_DEFAULT_PADDING_X;
    list->style.padding_y = GFX_LIST_DEFAULT_PADDING_Y;
    list->style.row_height = GFX_LIST_DEFAULT_ROW_HEIGHT;

    list->items.selected = -1;
}

static void gfx_list_apply_label_geometry(gfx_obj_t *obj)
{
    gfx_list_t *list = (gfx_list_t *)obj->src;
    gfx_coord_t inner_x;
    gfx_coord_t inner_y;
    uint16_t inner_w;
    uint16_t inner_h;

    inner_x = obj->geometry.x + (gfx_coord_t)list->style.padding_x;
    inner_y = obj->geometry.y + (gfx_coord_t)list->style.padding_y;
    inner_w = (obj->geometry.width > (uint16_t)(list->style.padding_x * 2U)) ? (obj->geometry.width - (uint16_t)(list->style.padding_x * 2U)) : obj->geometry.width;
    inner_h = (obj->geometry.height > (uint16_t)(list->style.padding_y * 2U)) ? (obj->geometry.height - (uint16_t)(list->style.padding_y * 2U)) : obj->geometry.height;

    obj->geometry.x = inner_x;
    obj->geometry.y = inner_y;
    obj->geometry.width = inner_w;
    obj->geometry.height = inner_h;
    list->label.style.bg_enable = false;
}

static void gfx_list_free_items(gfx_list_t *list)
{
    if (list == NULL) {
        return;
    }

    if (list->items.items != NULL) {
        for (size_t i = 0; i < list->items.count; i++) {
            free(list->items.items[i]);
            list->items.items[i] = NULL;
        }
        free(list->items.items);
        list->items.items = NULL;
    }
    list->items.count = 0;
    list->items.capacity = 0;
    list->items.selected = -1;
}

static esp_err_t gfx_list_items_reserve(gfx_list_t *list, size_t min_capacity)
{
    size_t new_capacity;
    char **new_items;

    if (min_capacity <= list->items.capacity) {
        return ESP_OK;
    }

    new_capacity = (list->items.capacity > 0U) ? list->items.capacity : 4U;
    while (new_capacity < min_capacity) {
        size_t prev = new_capacity;
        new_capacity *= 2U;
        if (new_capacity < prev) {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    new_items = realloc(list->items.items, new_capacity * sizeof(char *));
    if (new_items == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(new_items + list->items.capacity, 0, (new_capacity - list->items.capacity) * sizeof(char *));
    list->items.items = new_items;
    list->items.capacity = new_capacity;
    return ESP_OK;
}

static esp_err_t gfx_list_rebuild_text(gfx_obj_t *obj, gfx_list_t *list)
{
    char *buf;
    char *cursor;
    size_t total_len = 1U;

    if (list->items.count == 0U) {
        size_t empty_len = strlen(GFX_LIST_EMPTY_TEXT) + 1U;

        buf = malloc(empty_len);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(buf, GFX_LIST_EMPTY_TEXT, empty_len);
    } else {
        for (size_t i = 0; i < list->items.count; i++) {
            const char *item = (list->items.items[i] != NULL) ? list->items.items[i] : "";

            total_len += strlen(item);
            total_len += 2U; /* Prefix: "> " or "  " */
            if (i + 1U < list->items.count) {
                total_len += 1U; /* '\n' */
            }
        }

        buf = malloc(total_len);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }

        cursor = buf;
        for (size_t i = 0; i < list->items.count; i++) {
            const char *item = (list->items.items[i] != NULL) ? list->items.items[i] : "";
            size_t item_len = strlen(item);

            *cursor++ = ((int32_t)i == list->items.selected) ? '>' : ' ';
            *cursor++ = ' ';
            if (item_len > 0U) {
                memcpy(cursor, item, item_len);
                cursor += item_len;
            }
            if (i + 1U < list->items.count) {
                *cursor++ = '\n';
            }
        }
        *cursor = '\0';
    }

    free(list->label.text.text);
    list->label.text.text = buf;
    list->label.text.text_width = 0;
    list->label.scroll.offset = 0;
    list->label.snap.offset = 0;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

static int32_t gfx_list_get_row_by_y(gfx_obj_t *obj, const gfx_list_t *list, uint16_t y)
{
    gfx_coord_t row_top;
    gfx_coord_t row_bottom;
    int32_t row_index;

    if (list->items.count == 0U || list->style.row_height == 0U) {
        return -1;
    }

    row_top = obj->geometry.y + (gfx_coord_t)list->style.padding_y;
    row_bottom = row_top + (gfx_coord_t)(list->style.row_height * list->items.count);

    if ((gfx_coord_t)y < row_top || (gfx_coord_t)y >= row_bottom) {
        return -1;
    }

    row_index = ((gfx_coord_t)y - row_top) / (gfx_coord_t)list->style.row_height;
    if (row_index < 0 || row_index >= (int32_t)list->items.count) {
        return -1;
    }

    return row_index;
}

static esp_err_t gfx_list_set_selected_internal(gfx_obj_t *obj, gfx_list_t *list, int32_t index)
{
    if (index < -1 || index >= (int32_t)list->items.count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (list->items.selected == index) {
        return ESP_OK;
    }

    list->items.selected = index;
    return gfx_list_rebuild_text(obj, list);
}

static esp_err_t gfx_list_call_label_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_draw_label(obj, ctx);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_list_call_label_update(gfx_obj_t *obj)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_update_impl(obj);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_list_call_label_delete(gfx_obj_t *obj)
{
    int original_type = obj->type;
    esp_err_t ret;

    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_delete_impl(obj);
    obj->type = original_type;

    return ret;
}

static esp_err_t gfx_list_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_list_t *list;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t fill_area;
    gfx_area_t saved_geometry;
    uint16_t bg_color_raw;
    uint16_t selected_bg_color_raw;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(ctx, ESP_ERR_INVALID_ARG);

    list = (gfx_list_t *)obj->src;
    GFX_RETURN_IF_NULL(list, ESP_ERR_INVALID_STATE);

    gfx_obj_calc_pos_in_parent(obj);

    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return ESP_OK;
    }

    bg_color_raw = gfx_color_to_native_u16(list->style.bg_color, ctx->swap);
    selected_bg_color_raw = gfx_color_to_native_u16(list->style.selected_bg_color, ctx->swap);

    fill_area.x1 = clip_area.x1 - ctx->buf_area.x1;
    fill_area.y1 = clip_area.y1 - ctx->buf_area.y1;
    fill_area.x2 = clip_area.x2 - ctx->buf_area.x1;
    fill_area.y2 = clip_area.y2 - ctx->buf_area.y1;
    gfx_sw_blend_fill_area((uint16_t *)ctx->buf, ctx->stride, &fill_area, bg_color_raw);

    if (list->items.selected >= 0 && list->items.selected < (int32_t)list->items.count && list->style.row_height > 0U) {
        gfx_area_t selected_area;
        gfx_area_t selected_clip;

        selected_area.x1 = obj->geometry.x + (gfx_coord_t)list->style.padding_x;
        selected_area.x2 = obj->geometry.x + obj->geometry.width - (gfx_coord_t)list->style.padding_x;
        selected_area.y1 = obj->geometry.y + (gfx_coord_t)list->style.padding_y + (gfx_coord_t)(list->items.selected * (int32_t)list->style.row_height);
        selected_area.y2 = selected_area.y1 + (gfx_coord_t)list->style.row_height;

        if (selected_area.x2 > selected_area.x1 && selected_area.y2 > selected_area.y1 &&
                gfx_area_intersect_exclusive(&selected_clip, &ctx->clip_area, &selected_area)) {
            fill_area.x1 = selected_clip.x1 - ctx->buf_area.x1;
            fill_area.y1 = selected_clip.y1 - ctx->buf_area.y1;
            fill_area.x2 = selected_clip.x2 - ctx->buf_area.x1;
            fill_area.y2 = selected_clip.y2 - ctx->buf_area.y1;
            gfx_sw_blend_fill_area((uint16_t *)ctx->buf, ctx->stride, &fill_area, selected_bg_color_raw);
        }
    }

    gfx_sw_draw_rect_stroke((gfx_color_t *)ctx->buf,
                            ctx->stride,
                            &ctx->buf_area,
                            &ctx->clip_area,
                            &obj_area,
                            list->style.border_width,
                            list->style.border_color,
                            0xFF,
                            ctx->swap);

    saved_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    gfx_list_apply_label_geometry(obj);
    gfx_list_call_label_draw(obj, ctx);
    obj->geometry.x = saved_geometry.x1;
    obj->geometry.y = saved_geometry.y1;
    obj->geometry.width = (uint16_t)saved_geometry.x2;
    obj->geometry.height = (uint16_t)saved_geometry.y2;

    return ESP_OK;
}

static esp_err_t gfx_list_update(gfx_obj_t *obj)
{
    gfx_area_t saved_geometry;
    esp_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    gfx_obj_calc_pos_in_parent(obj);

    saved_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    gfx_list_apply_label_geometry(obj);
    ret = gfx_list_call_label_update(obj);
    obj->geometry.x = saved_geometry.x1;
    obj->geometry.y = saved_geometry.y1;
    obj->geometry.width = (uint16_t)saved_geometry.x2;
    obj->geometry.height = (uint16_t)saved_geometry.y2;

    return ret;
}

static esp_err_t gfx_list_delete_impl(gfx_obj_t *obj)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    gfx_list_free_items(list);
    return gfx_list_call_label_delete(obj);
}

static void gfx_list_touch_event(gfx_obj_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_list_t *list;
    int32_t row;

    if (obj == NULL || obj->src == NULL || event == NULL) {
        return;
    }

    if (event->type != GFX_TOUCH_EVENT_PRESS && event->type != GFX_TOUCH_EVENT_MOVE) {
        return;
    }

    list = (gfx_list_t *)obj->src;
    gfx_obj_calc_pos_in_parent(obj);

    row = gfx_list_get_row_by_y(obj, list, event->y);
    if (row >= 0) {
        gfx_list_set_selected_internal(obj, list, row);
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_obj_t *gfx_list_create(gfx_disp_t *disp)
{
    gfx_obj_t *obj;
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

    if (gfx_obj_create_class_instance(disp, &s_gfx_list_widget_class,
                                      list, GFX_LIST_DEFAULT_WIDTH, GFX_LIST_DEFAULT_HEIGHT,
                                      "gfx_list_create", &obj) != ESP_OK) {
        free(list);
        GFX_LOGE(TAG, "create list: no mem for object");
        return NULL;
    }

    if (gfx_list_rebuild_text(obj, list) != ESP_OK) {
        gfx_obj_delete(obj);
        GFX_LOGE(TAG, "create list: rebuild text failed");
        return NULL;
    }

    GFX_LOGD(TAG, "create list: object created");
    return obj;
}

esp_err_t gfx_list_set_items(gfx_obj_t *obj, const char *const *items, size_t count)
{
    gfx_list_t *list;
    size_t i;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE((count == 0U) || (items != NULL), ESP_ERR_INVALID_ARG, TAG, "items is NULL while count > 0");

    list = (gfx_list_t *)obj->src;
    gfx_list_free_items(list);

    if (count > 0U) {
        ESP_RETURN_ON_ERROR(gfx_list_items_reserve(list, count), TAG, "reserve list items failed");

        for (i = 0; i < count; i++) {
            const char *src = (items[i] != NULL) ? items[i] : "";
            size_t len = strlen(src) + 1U;
            char *dup = malloc(len);

            if (dup == NULL) {
                for (size_t j = 0; j < i; j++) {
                    free(list->items.items[j]);
                    list->items.items[j] = NULL;
                }
                list->items.count = 0;
                list->items.selected = -1;
                return ESP_ERR_NO_MEM;
            }

            memcpy(dup, src, len);
            list->items.items[i] = dup;
        }

        list->items.count = count;
        list->items.selected = 0;
    }

    return gfx_list_rebuild_text(obj, list);
}

esp_err_t gfx_list_add_item(gfx_obj_t *obj, const char *item)
{
    gfx_list_t *list;
    char *dup;
    size_t len;
    const char *src = (item != NULL) ? item : "";

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    ESP_RETURN_ON_ERROR(gfx_list_items_reserve(list, list->items.count + 1U), TAG, "reserve list item failed");

    len = strlen(src) + 1U;
    dup = malloc(len);
    if (dup == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(dup, src, len);

    list->items.items[list->items.count++] = dup;
    if (list->items.selected < 0) {
        list->items.selected = 0;
    }

    return gfx_list_rebuild_text(obj, list);
}

esp_err_t gfx_list_clear(gfx_obj_t *obj)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    gfx_list_free_items(list);
    return gfx_list_rebuild_text(obj, list);
}

esp_err_t gfx_list_set_selected(gfx_obj_t *obj, int32_t index)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    return gfx_list_set_selected_internal(obj, list, index);
}

int32_t gfx_list_get_selected(gfx_obj_t *obj)
{
    gfx_list_t *list;

    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return -1;
    }

    list = (gfx_list_t *)obj->src;
    return list->items.selected;
}

esp_err_t gfx_list_set_font(gfx_obj_t *obj, gfx_font_t font)
{
    int original_type;
    esp_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    original_type = obj->type;
    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_set_font(obj, font);
    obj->type = original_type;

    return ret;
}

esp_err_t gfx_list_set_text_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->label.style.color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_bg_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->style.bg_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_selected_bg_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->style.selected_bg_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_border_color(gfx_obj_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->style.border_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_border_width(gfx_obj_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE(width > 0U, ESP_ERR_INVALID_ARG, TAG, "border width must be > 0");

    ((gfx_list_t *)obj->src)->style.border_width = width;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_row_height(gfx_obj_t *obj, uint16_t row_height)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE(row_height > 0U, ESP_ERR_INVALID_ARG, TAG, "row_height must be > 0");

    list = (gfx_list_t *)obj->src;
    list->style.row_height = row_height;
    return gfx_list_rebuild_text(obj, list);
}

esp_err_t gfx_list_set_padding(gfx_obj_t *obj, uint16_t pad_x, uint16_t pad_y)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ((gfx_list_t *)obj->src)->style.padding_x = pad_x;
    ((gfx_list_t *)obj->src)->style.padding_y = pad_y;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}
