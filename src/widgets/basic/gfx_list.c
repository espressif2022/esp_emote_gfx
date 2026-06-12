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
#define GFX_LOG_MODULE GFX_LOG_MODULE_LIST
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "render/sw/gfx_sw_draw_priv.h"
#include "core/object/gfx_object_priv.h"
#include "core/gfx_touch.h"
#include "gfx/widgets/list.h"
#include "fonts/gfx_font_priv.h"
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

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    gfx_label_t label;
    char **items;
    uint16_t item_count;
    uint16_t top_index;
    int32_t focused_index;
    uint16_t item_height;
    uint16_t pad_x;
    uint16_t pad_y;
    bool touch_pressed;
    uint16_t touch_start_y;
    uint16_t touch_last_y;
    struct {
        gfx_color_t bg_color;
        gfx_color_t focus_bg_color;
        gfx_color_t text_color;
        gfx_color_t focus_text_color;
        gfx_color_t border_color;
        uint16_t border_width;
    } style;
    gfx_list_focus_cb_t focus_cb;
    void *focus_user_data;
} gfx_list_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "list";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_list_init_default_state(gfx_list_t *list);
static esp_err_t gfx_list_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_list_update(gfx_object_t *obj);
static esp_err_t gfx_list_delete_impl(gfx_object_t *obj);
static void gfx_list_touch_event(gfx_object_t *obj, const void *event_data);
static esp_err_t gfx_list_draw_text(gfx_object_t *obj, gfx_list_t *list, const gfx_draw_ctx_t *ctx,
                                    const char *text, const gfx_area_t *row_area, gfx_color_t color);
static void gfx_list_free_items(gfx_list_t *list);
static esp_err_t gfx_list_dup_text(const char *text, char **out_text);
static esp_err_t gfx_list_set_font_adapter(gfx_list_t *list, gfx_font_t font);

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
    list->label.style.bg_enable = false;
    list->label.style.text_align = GFX_TEXT_ALIGN_LEFT;
    list->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    list->label.text.line_spacing = 0;

    list->focused_index = 0;
    list->item_height = GFX_LIST_DEFAULT_ITEM_HEIGHT;
    list->pad_x = GFX_LIST_DEFAULT_PAD_X;
    list->pad_y = GFX_LIST_DEFAULT_PAD_Y;

    list->style.bg_color = GFX_COLOR_HEX(0x000000);
    list->style.focus_bg_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.text_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.focus_text_color = GFX_COLOR_HEX(0x000000);
    list->style.border_color = GFX_COLOR_HEX(0xFFFFFF);
    list->style.border_width = 1;
}

static esp_err_t gfx_list_dup_text(const char *text, char **out_text)
{
    const char *src = text ? text : "";
    size_t len = strlen(src) + 1U;
    char *dup;

    GFX_RETURN_IF_NULL(out_text, ESP_ERR_INVALID_ARG);

    dup = malloc(len);
    if (dup == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(dup, src, len);
    *out_text = dup;
    return ESP_OK;
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
    list->focused_index = 0;
}

static uint16_t gfx_list_visible_count(const gfx_object_t *obj, const gfx_list_t *list)
{
    uint16_t visible;

    if (obj == NULL || list == NULL || list->item_height == 0U) {
        return 0;
    }

    visible = (uint16_t)(obj->geometry.height / list->item_height);
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
    uint16_t max_top;

    if (obj == NULL || list == NULL) {
        return;
    }

    max_top = gfx_list_max_top_index(obj, list);
    if (list->top_index > max_top) {
        list->top_index = max_top;
    }
}

static void gfx_list_scroll_by_items(gfx_object_t *obj, gfx_list_t *list, int32_t delta)
{
    int32_t next;
    uint16_t max_top;

    if (obj == NULL || list == NULL || delta == 0) {
        return;
    }

    max_top = gfx_list_max_top_index(obj, list);
    next = (int32_t)list->top_index + delta;
    if (next < 0) {
        next = 0;
    }
    if (next > max_top) {
        next = max_top;
    }
    if (list->top_index != (uint16_t)next) {
        list->top_index = (uint16_t)next;
        gfx_object_invalidate(obj);
    }
}

static esp_err_t gfx_list_set_font_adapter(gfx_list_t *list, gfx_font_t font)
{
    gfx_font_handle_t font_handle;

    GFX_RETURN_IF_NULL(list, ESP_ERR_INVALID_ARG);

    if (list->label.font.handle != NULL) {
        gfx_label_clear_glyph_cache(&list->label);
        free(list->label.font.handle);
        list->label.font.handle = NULL;
    }

    if (font == NULL) {
        return ESP_OK;
    }

    font_handle = calloc(1, sizeof(gfx_font_adapter_t));
    if (font_handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = gfx_font_init_adapter(font_handle, font);
    if (ret != ESP_OK) {
        free(font_handle);
        return ret;
    }

    list->label.font.handle = font_handle;
    list->label.text.text_width = 0;
    return ESP_OK;
}

static esp_err_t gfx_list_call_label_update(gfx_object_t *obj, gfx_list_t *list,
        const char *text, const gfx_area_t *text_area)
{
    uint8_t original_type;
    void *original_src;
    gfx_area_t original_geometry;
    uint8_t original_align_type;
    gfx_coord_t original_align_x_ofs;
    gfx_coord_t original_align_y_ofs;
    gfx_object_t *original_align_target;
    bool original_align_enabled;
    esp_err_t ret;

    original_type = obj->type;
    original_src = obj->src;
    original_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    original_align_type = obj->align.type;
    original_align_x_ofs = obj->align.x_ofs;
    original_align_y_ofs = obj->align.y_ofs;
    original_align_target = obj->align.target;
    original_align_enabled = obj->align.enabled;

    list->label.text.text = (char *)(text ? text : "");
    list->label.text.text_width = 0;
    list->label.scroll.offset = 0;
    list->label.snap.offset = 0;
    free(list->label.render.mask);
    list->label.render.mask = NULL;
    list->label.render.mask_capacity = 0;

    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->src = &list->label;
    obj->geometry.x = text_area->x1;
    obj->geometry.y = text_area->y1;
    obj->geometry.width = (uint16_t)MAX(0, text_area->x2 - text_area->x1);
    obj->geometry.height = (uint16_t)MAX(0, text_area->y2 - text_area->y1);
    obj->align.enabled = false;
    obj->state.dirty = true;

    ret = gfx_label_update_impl(obj);

    obj->type = original_type;
    obj->src = original_src;
    obj->geometry.x = original_geometry.x1;
    obj->geometry.y = original_geometry.y1;
    obj->geometry.width = (uint16_t)original_geometry.x2;
    obj->geometry.height = (uint16_t)original_geometry.y2;
    obj->align.type = original_align_type;
    obj->align.x_ofs = original_align_x_ofs;
    obj->align.y_ofs = original_align_y_ofs;
    obj->align.target = original_align_target;
    obj->align.enabled = original_align_enabled;

    return ret;
}

static esp_err_t gfx_list_draw_text(gfx_object_t *obj, gfx_list_t *list, const gfx_draw_ctx_t *ctx,
                                    const char *text, const gfx_area_t *row_area, gfx_color_t color)
{
    uint8_t original_type;
    void *original_src;
    gfx_area_t original_geometry;
    uint8_t original_align_type;
    gfx_coord_t original_align_x_ofs;
    gfx_coord_t original_align_y_ofs;
    gfx_object_t *original_align_target;
    bool original_align_enabled;
    gfx_area_t text_area;
    esp_err_t ret;

    if (row_area->x2 <= row_area->x1 || row_area->y2 <= row_area->y1) {
        return ESP_OK;
    }

    text_area.x1 = (gfx_coord_t)(row_area->x1 + (gfx_coord_t)list->pad_x);
    text_area.y1 = (gfx_coord_t)(row_area->y1 + (gfx_coord_t)list->pad_y);
    text_area.x2 = (gfx_coord_t)(row_area->x2 - (gfx_coord_t)list->pad_x);
    text_area.y2 = (gfx_coord_t)(row_area->y2 - (gfx_coord_t)list->pad_y);
    if (text_area.x2 <= text_area.x1 || text_area.y2 <= text_area.y1) {
        text_area = *row_area;
    }

    ret = gfx_list_call_label_update(obj, list, text, &text_area);
    if (ret != ESP_OK) {
        return ret;
    }

    original_type = obj->type;
    original_src = obj->src;
    original_geometry = (gfx_area_t) {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    original_align_type = obj->align.type;
    original_align_x_ofs = obj->align.x_ofs;
    original_align_y_ofs = obj->align.y_ofs;
    original_align_target = obj->align.target;
    original_align_enabled = obj->align.enabled;

    list->label.style.color = color;
    list->label.style.bg_enable = false;

    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->src = &list->label;
    obj->geometry.x = text_area.x1;
    obj->geometry.y = text_area.y1;
    obj->geometry.width = (uint16_t)MAX(0, text_area.x2 - text_area.x1);
    obj->geometry.height = (uint16_t)MAX(0, text_area.y2 - text_area.y1);
    obj->align.enabled = false;

    ret = gfx_label_draw(obj, ctx);

    obj->type = original_type;
    obj->src = original_src;
    obj->geometry.x = original_geometry.x1;
    obj->geometry.y = original_geometry.y1;
    obj->geometry.width = (uint16_t)original_geometry.x2;
    obj->geometry.height = (uint16_t)original_geometry.y2;
    obj->align.type = original_align_type;
    obj->align.x_ofs = original_align_x_ofs;
    obj->align.y_ofs = original_align_y_ofs;
    obj->align.target = original_align_target;
    obj->align.enabled = original_align_enabled;

    return ret;
}

static esp_err_t gfx_list_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_list_t *list;
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    gfx_area_t fill_area;
    gfx_color_t *dest_pixels;
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(ctx, ESP_ERR_INVALID_ARG);

    list = (gfx_list_t *)obj->src;
    GFX_RETURN_IF_NULL(list, ESP_ERR_INVALID_STATE);

    gfx_object_calc_pos_in_parent(obj);

    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;

    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return ESP_OK;
    }

    dest_pixels = (gfx_color_t *)ctx->buf;
    fill_area.x1 = clip_area.x1 - ctx->buf_area.x1;
    fill_area.y1 = clip_area.y1 - ctx->buf_area.y1;
    fill_area.x2 = clip_area.x2 - ctx->buf_area.x1;
    fill_area.y2 = clip_area.y2 - ctx->buf_area.y1;
    gfx_sw_blend_fill_area_color(dest_pixels, ctx->stride, &fill_area, list->style.bg_color, ctx->swap);

    gfx_list_clamp_top_index(obj, list);

    for (uint16_t i = list->top_index; i < list->item_count; i++) {
        gfx_area_t row_area;
        gfx_area_t row_clip;
        bool focused = ((int32_t)i == list->focused_index);
        gfx_color_t row_bg = focused ? list->style.focus_bg_color : list->style.bg_color;
        gfx_color_t text_color = focused ? list->style.focus_text_color : list->style.text_color;
        uint16_t visible_row = (uint16_t)(i - list->top_index);

        row_area.x1 = obj_area.x1;
        row_area.y1 = (gfx_coord_t)(obj_area.y1 + (gfx_coord_t)(visible_row * list->item_height));
        row_area.x2 = obj_area.x2;
        row_area.y2 = (gfx_coord_t)(row_area.y1 + (gfx_coord_t)list->item_height);
        if (row_area.y1 >= obj_area.y2) {
            break;
        }
        if (row_area.y2 > obj_area.y2) {
            row_area.y2 = obj_area.y2;
        }

        if (gfx_area_intersect_exclusive(&row_clip, &ctx->clip_area, &row_area)) {
            fill_area.x1 = row_clip.x1 - ctx->buf_area.x1;
            fill_area.y1 = row_clip.y1 - ctx->buf_area.y1;
            fill_area.x2 = row_clip.x2 - ctx->buf_area.x1;
            fill_area.y2 = row_clip.y2 - ctx->buf_area.y1;
            gfx_sw_blend_fill_area_color(dest_pixels, ctx->stride, &fill_area, row_bg, ctx->swap);
        }

        if (list->style.border_width > 0 && row_area.y2 > row_area.y1) {
            gfx_coord_t line_y = (gfx_coord_t)(row_area.y2 - 1);
            for (uint16_t line = 0; line < list->style.border_width && line_y >= row_area.y1; line++) {
                gfx_sw_draw_hline(dest_pixels, ctx->stride, &ctx->buf_area, &ctx->clip_area,
                                  row_area.x1, (gfx_coord_t)(row_area.x2 - 1), line_y,
                                  list->style.border_color, 0xFF, ctx->swap);
                line_y--;
            }
        }

        gfx_list_draw_text(obj, list, ctx, list->items[i], &row_area, text_color);
    }

    return ESP_OK;
}

static esp_err_t gfx_list_update(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LIST(obj);
    gfx_object_calc_pos_in_parent(obj);
    return ESP_OK;
}

static esp_err_t gfx_list_delete_impl(gfx_object_t *obj)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);

    list = (gfx_list_t *)obj->src;
    if (list == NULL) {
        return ESP_OK;
    }

    gfx_list_free_items(list);
    gfx_label_clear_glyph_cache(&list->label);
    free(list->label.font.handle);
    free(list->label.render.mask);
    free(list);
    return ESP_OK;
}

static void gfx_list_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_list_t *list;
    int32_t index;
    int32_t delta_y;
    int32_t rows;
    bool was_pressed;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }

    list = (gfx_list_t *)obj->src;
    if (list->item_height == 0 || list->item_count == 0) {
        return;
    }

    gfx_object_calc_pos_in_parent(obj);
    was_pressed = list->touch_pressed;
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        list->touch_pressed = false;
    }
    if ((gfx_coord_t)event->x < obj->geometry.x ||
            (gfx_coord_t)event->x >= obj->geometry.x + (gfx_coord_t)obj->geometry.width ||
            (gfx_coord_t)event->y < obj->geometry.y ||
            (gfx_coord_t)event->y >= obj->geometry.y + (gfx_coord_t)obj->geometry.height) {
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        list->touch_pressed = true;
        list->touch_start_y = event->y;
        list->touch_last_y = event->y;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && list->touch_pressed) {
        delta_y = (int32_t)list->touch_last_y - (int32_t)event->y;
        rows = delta_y / (int32_t)list->item_height;
        if (rows != 0) {
            gfx_list_scroll_by_items(obj, list, rows);
            list->touch_last_y = (uint16_t)((int32_t)list->touch_last_y - rows * (int32_t)list->item_height);
        }
        return;
    }

    if (event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }
    if (!was_pressed) {
        return;
    }

    delta_y = (int32_t)event->y - (int32_t)list->touch_start_y;
    if (delta_y < 0) {
        delta_y = -delta_y;
    }
    if (delta_y >= (int32_t)(list->item_height / 2U)) {
        return;
    }

    index = (int32_t)list->top_index +
            (((gfx_coord_t)event->y - obj->geometry.y) / (gfx_coord_t)list->item_height);
    if (index < 0 || index >= list->item_count) {
        return;
    }

    if (list->focused_index != index) {
        list->focused_index = index;
        gfx_object_invalidate(obj);
        if (list->focus_cb != NULL) {
            list->focus_cb(obj, index, list->focus_user_data);
        }
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
                                         "gfx_list_create", &obj) != ESP_OK) {
        free(list);
        GFX_LOGE(TAG, "create list: no mem for object");
        return NULL;
    }

    return obj;
}

esp_err_t gfx_list_clear(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    gfx_list_free_items((gfx_list_t *)obj->src);
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_add_item(gfx_object_t *obj, const char *text)
{
    gfx_list_t *list;
    char **new_items;
    char *dup_text = NULL;
    esp_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    ret = gfx_list_dup_text(text, &dup_text);
    if (ret != ESP_OK) {
        return ret;
    }

    new_items = realloc(list->items, ((size_t)list->item_count + 1U) * sizeof(char *));
    if (new_items == NULL) {
        free(dup_text);
        return ESP_ERR_NO_MEM;
    }

    list->items = new_items;
    list->items[list->item_count] = dup_text;
    list->item_count++;
    if (list->item_count == 1U) {
        list->focused_index = 0;
    }

    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count)
{
    gfx_list_t *list;
    char **new_items = NULL;
    esp_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    if (item_count > 0U) {
        GFX_RETURN_IF_NULL(items, ESP_ERR_INVALID_ARG);
    }

    list = (gfx_list_t *)obj->src;
    if (item_count > 0U) {
        new_items = calloc(item_count, sizeof(char *));
        if (new_items == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    for (uint16_t i = 0; i < item_count; i++) {
        ret = gfx_list_dup_text(items[i], &new_items[i]);
        if (ret != ESP_OK) {
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
    list->focused_index = (item_count > 0U) ? 0 : -1;

    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_focus(gfx_object_t *obj, int32_t index)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    if (index < -1 || index >= list->item_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (list->focused_index != index) {
        list->focused_index = index;
        if (index >= 0 && index < list->top_index) {
            list->top_index = (uint16_t)index;
        } else if (index >= 0) {
            uint16_t visible = gfx_list_visible_count(obj, list);
            if (visible > 0U && index >= (int32_t)(list->top_index + visible)) {
                list->top_index = (uint16_t)(index - (int32_t)visible + 1);
            }
        }
        gfx_list_clamp_top_index(obj, list);
        gfx_object_invalidate(obj);
        if (list->focus_cb != NULL) {
            list->focus_cb(obj, index, list->focus_user_data);
        }
    }
    return ESP_OK;
}

int32_t gfx_list_get_focus(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST) {
        return -1;
    }
    GFX_RETURN_IF_NULL(obj->src, -1);
    return ((gfx_list_t *)obj->src)->focused_index;
}

esp_err_t gfx_list_set_top_index(gfx_object_t *obj, uint16_t index)
{
    gfx_list_t *list;
    uint16_t max_top;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    max_top = gfx_list_max_top_index(obj, list);
    if (index > max_top) {
        index = max_top;
    }
    if (list->top_index != index) {
        list->top_index = index;
        gfx_object_invalidate(obj);
    }
    return ESP_OK;
}

uint16_t gfx_list_get_top_index(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_LIST || obj->src == NULL) {
        return 0;
    }
    return ((gfx_list_t *)obj->src)->top_index;
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

esp_err_t gfx_list_set_font(gfx_object_t *obj, gfx_font_t font)
{
    esp_err_t ret;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    ret = gfx_list_set_font_adapter((gfx_list_t *)obj->src, font);
    if (ret == ESP_OK) {
        gfx_object_invalidate(obj);
    }
    return ret;
}

esp_err_t gfx_list_set_item_height(gfx_object_t *obj, uint16_t height)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE(height > 0U, ESP_ERR_INVALID_ARG, TAG, "item height must be > 0");

    ((gfx_list_t *)obj->src)->item_height = height;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_text_pad(gfx_object_t *obj, uint16_t pad_x, uint16_t pad_y)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->pad_x = pad_x;
    list->pad_y = pad_y;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_focus_cb(gfx_object_t *obj, gfx_list_focus_cb_t cb, void *user_data)
{
    gfx_list_t *list;

    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);

    list = (gfx_list_t *)obj->src;
    list->focus_cb = cb;
    list->focus_user_data = user_data;
    return ESP_OK;
}

esp_err_t gfx_list_set_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.bg_color = color;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_focus_bg_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.focus_bg_color = color;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.text_color = color;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_focus_text_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.focus_text_color = color;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_border_color(gfx_object_t *obj, gfx_color_t color)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.border_color = color;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_list_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_LIST(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_list_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return ESP_OK;
}
