/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_LIST
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/gfx_touch.h"
#include "core/object/gfx_object_priv.h"
#include "gfx/tween.h"
#include "gfx/widgets/pageflow.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "render/sw/gfx_sw_draw_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

#define CHECK_OBJ_TYPE_PAGEFLOW(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_PAGEFLOW, TAG)
#define GFX_PAGEFLOW_DEFAULT_WIDTH 300U
#define GFX_PAGEFLOW_DEFAULT_HEIGHT 180U
#define GFX_PAGEFLOW_DEFAULT_DRAG_THRESHOLD 6U
#define GFX_PAGEFLOW_DEFAULT_PAGE_THRESHOLD 48U
#define GFX_PAGEFLOW_PAD_X 12U
#define GFX_PAGEFLOW_PAD_Y 8U
#define GFX_PAGEFLOW_TWEEN_MS 180U

typedef struct {
    gfx_label_t label;
    char **pages;
    const gfx_image_dsc_t *const *images;
    uint16_t page_count;
    int32_t page_index;
    bool use_images;
    gfx_pageflow_dir_t dir;
    uint16_t drag_threshold;
    uint16_t page_threshold;
    int32_t drag_offset;
    gfx_tween_t *tween;
    struct {
        bool pressed;
        bool dragging;
        uint16_t start_x;
        uint16_t start_y;
        uint16_t last_x;
        uint16_t last_y;
    } touch;
    struct {
        gfx_color_t bg_color;
        gfx_color_t page_color;
        gfx_color_t text_color;
        gfx_color_t border_color;
        uint16_t border_width;
    } style;
    gfx_pageflow_changed_cb_t changed_cb;
    void *changed_user_data;
} gfx_pageflow_t;

static const char *const TAG = "pageflow";

static esp_err_t gfx_pageflow_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_pageflow_update(gfx_object_t *obj);
static esp_err_t gfx_pageflow_delete_impl(gfx_object_t *obj);
static esp_err_t gfx_pageflow_load_impl(gfx_object_t *obj);
static void gfx_pageflow_release_impl(gfx_object_t *obj);
static void gfx_pageflow_touch_event(gfx_object_t *obj, const void *event_data);

static const gfx_widget_class_t s_gfx_pageflow_widget_class = {
    .type = GFX_OBJ_TYPE_PAGEFLOW,
    .name = "pageflow",
    .draw = gfx_pageflow_draw,
    .delete = gfx_pageflow_delete_impl,
    .load = gfx_pageflow_load_impl,
    .release = gfx_pageflow_release_impl,
    .update = gfx_pageflow_update,
    .touch_event = gfx_pageflow_touch_event,
};

static void gfx_pageflow_init_default_state(gfx_pageflow_t *flow)
{
    memset(flow, 0, sizeof(*flow));
    flow->label.style.opa = 0xFF;
    flow->label.style.bg_enable = false;
    flow->label.style.text_align = GFX_TEXT_ALIGN_CENTER;
    flow->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    flow->dir = GFX_PAGEFLOW_DIR_HORIZONTAL;
    flow->drag_threshold = GFX_PAGEFLOW_DEFAULT_DRAG_THRESHOLD;
    flow->page_threshold = GFX_PAGEFLOW_DEFAULT_PAGE_THRESHOLD;
    flow->style.bg_color = GFX_COLOR_HEX(0x101418);
    flow->style.page_color = GFX_COLOR_HEX(0x1F2A35);
    flow->style.text_color = GFX_COLOR_HEX(0xF3F7FA);
    flow->style.border_color = GFX_COLOR_HEX(0x3F5163);
    flow->style.border_width = 1;
}

static esp_err_t gfx_pageflow_dup_text(const char *text, char **out_text)
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

static void gfx_pageflow_free_pages(gfx_pageflow_t *flow)
{
    if (flow == NULL) {
        return;
    }
    gfx_tween_stop(flow->tween, false);
    if (flow->pages != NULL) {
        for (uint16_t i = 0; i < flow->page_count; i++) {
            free(flow->pages[i]);
        }
    }
    free(flow->pages);
    flow->pages = NULL;
    flow->images = NULL;
    flow->page_count = 0;
    flow->page_index = 0;
    flow->use_images = false;
    flow->drag_offset = 0;
    memset(&flow->touch, 0, sizeof(flow->touch));
}

static int32_t gfx_pageflow_clamp_page(const gfx_pageflow_t *flow, int32_t page)
{
    if (flow == NULL || flow->page_count == 0U) {
        return 0;
    }
    if (page < 0) {
        return 0;
    }
    if (page >= (int32_t)flow->page_count) {
        return (int32_t)flow->page_count - 1;
    }
    return page;
}

static void gfx_pageflow_set_page_internal(gfx_object_t *obj, gfx_pageflow_t *flow, int32_t page, bool emit)
{
    int32_t next = gfx_pageflow_clamp_page(flow, page);
    if (flow == NULL) {
        return;
    }
    gfx_tween_stop(flow->tween, false);
    flow->drag_offset = 0;
    if (flow->page_index != next) {
        flow->page_index = next;
        if (emit && flow->changed_cb != NULL) {
            flow->changed_cb(obj, next, flow->changed_user_data);
        }
    }
    gfx_object_invalidate(obj);
}

static void gfx_pageflow_tween_value_cb(gfx_tween_t *tween, gfx_object_t *obj, int32_t value, void *user_data)
{
    gfx_pageflow_t *flow = (gfx_pageflow_t *)user_data;
    (void)tween;

    if (obj == NULL || flow == NULL) {
        return;
    }

    if (flow->drag_offset != value) {
        flow->drag_offset = value;
        gfx_object_invalidate(obj);
    }
}

static void gfx_pageflow_start_tween(gfx_object_t *obj, gfx_pageflow_t *flow, int32_t target_page,
                                     int32_t start_offset)
{
    int32_t next;

    if (obj == NULL || flow == NULL) {
        return;
    }

    next = gfx_pageflow_clamp_page(flow, target_page);
    if (next != flow->page_index) {
        int32_t span = (flow->dir == GFX_PAGEFLOW_DIR_HORIZONTAL) ? (int32_t)obj->geometry.width :
                       (int32_t)obj->geometry.height;
        if (span <= 0) {
            span = (int32_t)flow->page_threshold;
        }
        start_offset = (next > flow->page_index) ? span + start_offset : start_offset - span;
        flow->page_index = next;
        if (flow->changed_cb != NULL) {
            flow->changed_cb(obj, next, flow->changed_user_data);
        }
    }

    if (flow->tween == NULL) {
        flow->tween = gfx_tween_create(obj);
    }
    if (flow->tween == NULL ||
            gfx_tween_start_i32(flow->tween, start_offset, 0, GFX_PAGEFLOW_TWEEN_MS,
                                GFX_TWEEN_EASE_OUT_QUAD,
                                gfx_pageflow_tween_value_cb, NULL, flow) != ESP_OK) {
        flow->drag_offset = 0;
        gfx_object_invalidate(obj);
    }
}

static esp_err_t gfx_pageflow_call_label_draw(gfx_object_t *obj, gfx_pageflow_t *flow,
        const gfx_draw_ctx_t *ctx, const char *text, const gfx_area_t *area, const gfx_area_t *clip)
{
    uint8_t original_type = obj->type;
    void *original_src = obj->src;
    gfx_area_t original_geometry = {
        .x1 = obj->geometry.x,
        .y1 = obj->geometry.y,
        .x2 = obj->geometry.width,
        .y2 = obj->geometry.height,
    };
    uint8_t original_align_type = obj->align.type;
    gfx_coord_t original_align_x_ofs = obj->align.x_ofs;
    gfx_coord_t original_align_y_ofs = obj->align.y_ofs;
    gfx_object_t *original_align_target = obj->align.target;
    bool original_align_enabled = obj->align.enabled;
    gfx_draw_ctx_t label_ctx = *ctx;
    gfx_area_t text_area = {
        .x1 = (gfx_coord_t)(area->x1 + GFX_PAGEFLOW_PAD_X),
        .y1 = (gfx_coord_t)(area->y1 + GFX_PAGEFLOW_PAD_Y),
        .x2 = (gfx_coord_t)(area->x2 - GFX_PAGEFLOW_PAD_X),
        .y2 = (gfx_coord_t)(area->y2 - GFX_PAGEFLOW_PAD_Y),
    };
    esp_err_t ret;

    if (text_area.x2 <= text_area.x1 || text_area.y2 <= text_area.y1) {
        text_area = *area;
    }

    flow->label.text.text = (char *)(text ? text : "");
    flow->label.text.text_width = 0;
    flow->label.scroll.offset = 0;
    flow->label.snap.offset = 0;
    free(flow->label.render.mask);
    flow->label.render.mask = NULL;
    flow->label.render.mask_capacity = 0;
    flow->label.style.color = flow->style.text_color;
    flow->label.style.bg_enable = false;

    obj->type = GFX_OBJ_TYPE_LABEL;
    obj->src = &flow->label;
    obj->geometry.x = text_area.x1;
    obj->geometry.y = text_area.y1;
    obj->geometry.width = (uint16_t)MAX(0, text_area.x2 - text_area.x1);
    obj->geometry.height = (uint16_t)MAX(0, text_area.y2 - text_area.y1);
    obj->align.enabled = false;
    obj->state.dirty = true;

    ret = gfx_label_update_impl(obj);
    if (ret == ESP_OK) {
        label_ctx.clip_area = *clip;
        ret = gfx_label_draw(obj, &label_ctx);
    }

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

static void gfx_pageflow_draw_image(gfx_object_t *obj, const gfx_draw_ctx_t *ctx,
                                    const gfx_image_dsc_t *image, const gfx_area_t *page_area,
                                    const gfx_area_t *clip)
{
    gfx_color_format_t color_format;
    uint8_t src_pixel_size;
    gfx_coord_t src_stride;
    gfx_area_t image_area;
    gfx_area_t draw_area;
    gfx_area_t draw_local;
    const uint8_t *src_pixels;
    const gfx_opa_t *alpha_mask = NULL;
    gfx_coord_t alpha_stride = 0;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (obj == NULL || ctx == NULL || image == NULL || image->data == NULL ||
            image->header.magic != GFX_IMAGE_HEADER_MAGIC ||
            image->header.w == 0U || image->header.h == 0U) {
        return;
    }

    color_format = (gfx_color_format_t)image->header.cf;
    if (!gfx_color_format_is_image_supported(color_format)) {
        return;
    }

    src_pixel_size = gfx_color_format_get_size(color_format);
    if (src_pixel_size == 0U) {
        return;
    }

    src_stride = (image->header.stride > 0U)
                 ? (gfx_coord_t)(image->header.stride / src_pixel_size)
                 : (gfx_coord_t)image->header.w;

    image_area.x1 = page_area->x1;
    image_area.y1 = page_area->y1;
    image_area.x2 = (gfx_coord_t)(page_area->x1 + image->header.w);
    image_area.y2 = (gfx_coord_t)(page_area->y1 + image->header.h);
    if (!gfx_area_intersect_exclusive(&draw_area, clip, &image_area)) {
        return;
    }

    draw_local.x1 = draw_area.x1 - ctx->buf_area.x1;
    draw_local.y1 = draw_area.y1 - ctx->buf_area.y1;
    draw_local.x2 = draw_area.x2 - ctx->buf_area.x1;
    draw_local.y2 = draw_area.y2 - ctx->buf_area.y1;

    gfx_coord_t src_x = (gfx_coord_t)(draw_area.x1 - image_area.x1);
    gfx_coord_t src_y = (gfx_coord_t)(draw_area.y1 - image_area.y1);
    src_pixels = image->data +
                 ((size_t)src_y * src_stride +
                  (size_t)src_x) * src_pixel_size;
    if (gfx_color_format_has_plane_alpha(color_format)) {
        const uint8_t *alpha_base = image->data + (size_t)src_stride * image->header.h * src_pixel_size;
        alpha_mask = GFX_BUFFER_OFFSET_8BPP(alpha_base,
                                            draw_area.y1 - image_area.y1,
                                            image->header.w,
                                            draw_area.x1 - image_area.x1);
        alpha_stride = (gfx_coord_t)image->header.w;
    }

    gfx_render_image_t render_src = {
        .pixels = src_pixels,
        .stride = src_stride,
        .format = color_format,
        .alpha = alpha_mask,
        .alpha_stride = alpha_stride,
    };
    gfx_render_image_t backend_src = render_src;
    backend_src.pixels = image->data;
    if (gfx_color_format_has_plane_alpha(color_format)) {
        backend_src.alpha = (gfx_opa_t *)(image->data + (size_t)src_stride * image->header.h * src_pixel_size);
    }
    if (gfx_render_surface_blit_image(obj->disp, &dst_surface, &draw_area, &backend_src,
                                      src_x,
                                      src_y,
                                      0xFFU)) {
        return;
    }

    gfx_sw_blend_img_draw_fmt(ctx->buf, ctx->stride, ctx->format, src_pixels, src_stride, alpha_mask,
                              alpha_stride, &draw_local, color_format, 0xFFU);
}

static void gfx_pageflow_draw_one(gfx_object_t *obj, gfx_pageflow_t *flow, const gfx_draw_ctx_t *ctx,
                                  const gfx_area_t *obj_area, int32_t page_index, int32_t slot)
{
    gfx_area_t page_area = *obj_area;
    gfx_area_t draw_area;
    gfx_area_t clip;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };
    if (page_index < 0 || page_index >= (int32_t)flow->page_count) {
        return;
    }

    if (flow->dir == GFX_PAGEFLOW_DIR_HORIZONTAL) {
        int32_t x_ofs = slot * (int32_t)(obj_area->x2 - obj_area->x1) + flow->drag_offset;
        page_area.x1 = (gfx_coord_t)(page_area.x1 + x_ofs);
        page_area.x2 = (gfx_coord_t)(page_area.x2 + x_ofs);
    } else {
        int32_t y_ofs = slot * (int32_t)(obj_area->y2 - obj_area->y1) + flow->drag_offset;
        page_area.y1 = (gfx_coord_t)(page_area.y1 + y_ofs);
        page_area.y2 = (gfx_coord_t)(page_area.y2 + y_ofs);
    }

    if (!gfx_area_intersect_exclusive(&draw_area, obj_area, &page_area) ||
            !gfx_area_intersect_exclusive(&clip, &ctx->clip_area, &draw_area)) {
        return;
    }

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip, flow->style.page_color, 0xFFU);
    if (flow->style.border_width > 0U) {
        gfx_sw_draw_rect_stroke_fmt(ctx->buf, ctx->stride, ctx->format, &ctx->buf_area, &ctx->clip_area,
                                    &page_area, flow->style.border_width, flow->style.border_color, 0xFF);
    }
    if (flow->use_images) {
        gfx_pageflow_draw_image(obj, ctx, flow->images[page_index], &page_area, &clip);
    } else {
        (void)gfx_pageflow_call_label_draw(obj, flow, ctx, flow->pages[page_index], &page_area, &clip);
    }
}

static esp_err_t gfx_pageflow_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_pageflow_t *flow;
    gfx_area_t obj_area;
    gfx_area_t clip;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(ctx, ESP_ERR_INVALID_ARG);
    flow = (gfx_pageflow_t *)obj->src;
    GFX_RETURN_IF_NULL(flow, ESP_ERR_INVALID_STATE);

    gfx_object_calc_pos_in_parent(obj);
    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;
    if (!gfx_area_intersect_exclusive(&clip, &ctx->clip_area, &obj_area)) {
        return ESP_OK;
    }

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip, flow->style.bg_color, 0xFFU);

    if (flow->page_count == 0U) {
        return ESP_OK;
    }
    gfx_pageflow_draw_one(obj, flow, ctx, &obj_area, flow->page_index - 1, -1);
    gfx_pageflow_draw_one(obj, flow, ctx, &obj_area, flow->page_index, 0);
    gfx_pageflow_draw_one(obj, flow, ctx, &obj_area, flow->page_index + 1, 1);
    return ESP_OK;
}

static esp_err_t gfx_pageflow_update(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    gfx_object_calc_pos_in_parent(obj);
    return ESP_OK;
}

static esp_err_t gfx_pageflow_delete_impl(gfx_object_t *obj)
{
    gfx_pageflow_t *flow;
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    flow = (gfx_pageflow_t *)obj->src;
    if (flow == NULL) {
        return ESP_OK;
    }
    gfx_pageflow_free_pages(flow);
    gfx_tween_delete(flow->tween);
    free(flow->label.render.mask);
    free(flow->label.render.color_mask);
    free(flow);
    return ESP_OK;
}

static esp_err_t gfx_pageflow_load_impl(gfx_object_t *obj)
{
    uint8_t original_type = obj->type;
    esp_err_t ret;
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    obj->type = GFX_OBJ_TYPE_LABEL;
    ret = gfx_label_load_impl(obj);
    obj->type = original_type;
    return ret;
}

static void gfx_pageflow_release_impl(gfx_object_t *obj)
{
    uint8_t original_type;
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_PAGEFLOW) {
        return;
    }
    original_type = obj->type;
    obj->type = GFX_OBJ_TYPE_LABEL;
    gfx_label_release_impl(obj);
    obj->type = original_type;
}

static void gfx_pageflow_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_pageflow_t *flow;
    int32_t dx;
    int32_t dy;
    int32_t main_delta;
    int32_t abs_dx;
    int32_t abs_dy;
    bool was_pressed;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }
    flow = (gfx_pageflow_t *)obj->src;
    if (flow->page_count == 0U) {
        return;
    }
    was_pressed = flow->touch.pressed;
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        flow->touch.pressed = false;
        flow->touch.dragging = false;
    }
    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        flow->touch.pressed = true;
        flow->touch.dragging = false;
        flow->touch.start_x = event->x;
        flow->touch.start_y = event->y;
        flow->touch.last_x = event->x;
        flow->touch.last_y = event->y;
        gfx_tween_stop(flow->tween, false);
        flow->drag_offset = 0;
        return;
    }
    if (event->type == GFX_TOUCH_EVENT_MOVE && flow->touch.pressed) {
        dx = (int32_t)event->x - (int32_t)flow->touch.start_x;
        dy = (int32_t)event->y - (int32_t)flow->touch.start_y;
        abs_dx = dx < 0 ? -dx : dx;
        abs_dy = dy < 0 ? -dy : dy;
        if (!flow->touch.dragging && (abs_dx >= flow->drag_threshold || abs_dy >= flow->drag_threshold)) {
            flow->touch.dragging = true;
        }
        if (flow->touch.dragging) {
            flow->drag_offset = flow->dir == GFX_PAGEFLOW_DIR_HORIZONTAL ? dx : dy;
            gfx_object_invalidate(obj);
        }
        return;
    }
    if (event->type != GFX_TOUCH_EVENT_RELEASE || !was_pressed) {
        return;
    }
    main_delta = flow->drag_offset;
    if (main_delta <= -(int32_t)flow->page_threshold) {
        gfx_pageflow_start_tween(obj, flow, flow->page_index + 1, main_delta);
    } else if (main_delta >= (int32_t)flow->page_threshold) {
        gfx_pageflow_start_tween(obj, flow, flow->page_index - 1, main_delta);
    } else {
        gfx_pageflow_start_tween(obj, flow, flow->page_index, main_delta);
    }
}

gfx_object_t *gfx_pageflow_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_pageflow_t *flow;
    if (disp == NULL) {
        return NULL;
    }
    flow = calloc(1, sizeof(gfx_pageflow_t));
    if (flow == NULL) {
        return NULL;
    }
    gfx_pageflow_init_default_state(flow);
    if (gfx_object_create_class_instance(disp, &s_gfx_pageflow_widget_class,
                                         flow, GFX_PAGEFLOW_DEFAULT_WIDTH, GFX_PAGEFLOW_DEFAULT_HEIGHT,
                                         "gfx_pageflow_create", &obj) != ESP_OK) {
        free(flow);
        return NULL;
    }
    flow->tween = gfx_tween_create(obj);
    if (flow->tween == NULL) {
        (void)gfx_object_delete(obj);
        return NULL;
    }
    return obj;
}

gfx_err_t gfx_pageflow_clear(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    gfx_pageflow_free_pages((gfx_pageflow_t *)obj->src);
    gfx_object_invalidate(obj);
    return ESP_OK;
}

gfx_err_t gfx_pageflow_add_page(gfx_object_t *obj, const char *text)
{
    gfx_pageflow_t *flow;
    char **new_pages;
    char *dup_text = NULL;
    esp_err_t ret;
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    flow = (gfx_pageflow_t *)obj->src;
    if (flow->use_images) {
        gfx_pageflow_free_pages(flow);
    }
    ret = gfx_pageflow_dup_text(text, &dup_text);
    if (ret != ESP_OK) {
        return ret;
    }
    new_pages = realloc(flow->pages, ((size_t)flow->page_count + 1U) * sizeof(char *));
    if (new_pages == NULL) {
        free(dup_text);
        return ESP_ERR_NO_MEM;
    }
    flow->pages = new_pages;
    flow->pages[flow->page_count++] = dup_text;
    flow->images = NULL;
    flow->use_images = false;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_pages(gfx_object_t *obj, const char *const *pages, uint16_t page_count)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE(page_count == 0U || pages != NULL, ESP_ERR_INVALID_ARG, TAG, "pages is NULL");
    ESP_RETURN_ON_ERROR(gfx_pageflow_clear(obj), TAG, "clear failed");
    for (uint16_t i = 0; i < page_count; i++) {
        ESP_RETURN_ON_ERROR(gfx_pageflow_add_page(obj, pages[i]), TAG, "add page failed");
    }
    ((gfx_pageflow_t *)obj->src)->page_index = page_count > 0U ? 0 : -1;
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_image_pages(gfx_object_t *obj, const gfx_image_dsc_t *const *images,
                                       uint16_t page_count)
{
    gfx_pageflow_t *flow;
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ESP_RETURN_ON_FALSE(page_count == 0U || images != NULL, ESP_ERR_INVALID_ARG, TAG, "images is NULL");

    flow = (gfx_pageflow_t *)obj->src;
    gfx_pageflow_free_pages(flow);
    flow->images = images;
    flow->page_count = page_count;
    flow->page_index = page_count > 0U ? 0 : -1;
    flow->use_images = true;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_page(gfx_object_t *obj, int32_t page_index)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    gfx_pageflow_set_page_internal(obj, (gfx_pageflow_t *)obj->src, page_index, true);
    return ESP_OK;
}

int32_t gfx_pageflow_get_page(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_PAGEFLOW || obj->src == NULL) {
        return -1;
    }
    return ((gfx_pageflow_t *)obj->src)->page_index;
}

uint16_t gfx_pageflow_get_page_count(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_PAGEFLOW || obj->src == NULL) {
        return 0;
    }
    return ((gfx_pageflow_t *)obj->src)->page_count;
}

gfx_err_t gfx_pageflow_set_font(gfx_object_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    return gfx_label_set_font_source(obj, &((gfx_pageflow_t *)obj->src)->label, font);
}

gfx_err_t gfx_pageflow_set_direction(gfx_object_t *obj, gfx_pageflow_dir_t dir)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_pageflow_t *)obj->src)->dir = dir;
    gfx_object_invalidate(obj);
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_drag_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_pageflow_t *)obj->src)->drag_threshold = threshold;
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_page_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_pageflow_t *)obj->src)->page_threshold = threshold;
    return ESP_OK;
}

gfx_err_t gfx_pageflow_set_changed_cb(gfx_object_t *obj, gfx_pageflow_changed_cb_t cb, void *user_data)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_pageflow_t *)obj->src)->changed_cb = cb;
    ((gfx_pageflow_t *)obj->src)->changed_user_data = user_data;
    return ESP_OK;
}

#define GFX_PAGEFLOW_SET_STYLE_FIELD(fn_name, field_name) \
    gfx_err_t fn_name(gfx_object_t *obj, gfx_color_t color) \
    { \
        CHECK_OBJ_TYPE_PAGEFLOW(obj); \
        GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE); \
        ((gfx_pageflow_t *)obj->src)->style.field_name = color; \
        gfx_object_invalidate(obj); \
        return ESP_OK; \
    }

GFX_PAGEFLOW_SET_STYLE_FIELD(gfx_pageflow_set_bg_color, bg_color)
GFX_PAGEFLOW_SET_STYLE_FIELD(gfx_pageflow_set_page_color, page_color)
GFX_PAGEFLOW_SET_STYLE_FIELD(gfx_pageflow_set_text_color, text_color)
GFX_PAGEFLOW_SET_STYLE_FIELD(gfx_pageflow_set_border_color, border_color)

gfx_err_t gfx_pageflow_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_PAGEFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, ESP_ERR_INVALID_STATE);
    ((gfx_pageflow_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return ESP_OK;
}
