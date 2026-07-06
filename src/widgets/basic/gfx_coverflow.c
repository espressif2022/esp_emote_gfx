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
#include "gfx/widgets/coverflow.h"
#include "gfx/widgets/mesh_image.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "widgets/img/gfx_image_resource_priv.h"
#include "widgets/label/gfx_label_draw_priv.h"
#include "widgets/label/gfx_label_priv.h"

#define CHECK_OBJ_TYPE_COVERFLOW(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_COVERFLOW, TAG)
#define GFX_COVERFLOW_DEFAULT_WIDTH 320U
#define GFX_COVERFLOW_DEFAULT_HEIGHT 180U
#define GFX_COVERFLOW_DEFAULT_DRAG_THRESHOLD 6U
#define GFX_COVERFLOW_DEFAULT_PAGE_THRESHOLD 42U
#define GFX_COVERFLOW_DEFAULT_CENTER_ZOOM 100U
#define GFX_COVERFLOW_DEFAULT_SIDE_ZOOM 64U
#define GFX_COVERFLOW_DEFAULT_SPACING 34U
#define GFX_COVERFLOW_DEFAULT_SIDE_DIM_OPA 80U
#define GFX_COVERFLOW_POS_Q 1024
#define GFX_COVERFLOW_VISIBLE_SIDE_COUNT 2
#define GFX_COVERFLOW_TWEEN_MS 180U

typedef struct {
    int32_t index;
    int32_t pos_q;
    uint16_t zoom;
    gfx_opa_t dim_opa;
    gfx_area_t area;
    bool centerish;
} gfx_coverflow_card_state_t;

typedef struct {
    uint16_t center_zoom;
    uint16_t side_zoom;
    uint16_t spacing_pct;
    gfx_opa_t side_dim_opa;
} gfx_coverflow_effect_t;

typedef struct {
    int32_t pos_q;
    uint16_t zoom;
    gfx_opa_t dim_opa;
    bool centerish;
} gfx_coverflow_effect_state_t;

typedef struct {
    gfx_label_t label;
    char **items;
    const gfx_image_dsc_t **images;
    gfx_image_resource_t *image_resources;
    gfx_coverflow_card_dsc_t *cards;
    uint16_t item_count;
    int32_t selected_index;
    int32_t tween_target_index;
    bool tween_commit_pending;
    bool use_images;
    bool use_cards;
    uint16_t drag_threshold;
    uint16_t page_threshold;
    int32_t drag_offset;
    struct {
        bool pressed;
        bool dragging;
        uint16_t start_x;
        uint16_t start_y;
    } touch;
    gfx_tween_t *tween;
    struct {
        gfx_color_t bg_color;
        gfx_color_t center_color;
        gfx_color_t side_color;
        gfx_color_t text_color;
        gfx_color_t border_color;
        uint16_t border_width;
        uint16_t center_zoom;
        uint16_t side_zoom;
        uint16_t spacing_pct;
        gfx_opa_t side_dim_opa;
    } style;
    gfx_coverflow_changed_cb_t changed_cb;
    void *changed_user_data;
} gfx_coverflow_t;

static const char *const TAG = "coverflow";

static gfx_err_t gfx_coverflow_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx);
static gfx_err_t gfx_coverflow_update(gfx_object_t *obj);
static gfx_err_t gfx_coverflow_delete_impl(gfx_object_t *obj);
static gfx_err_t gfx_coverflow_load_impl(gfx_object_t *obj);
static void gfx_coverflow_release_impl(gfx_object_t *obj);
static void gfx_coverflow_touch_event(gfx_object_t *obj, const void *event_data);
static void gfx_coverflow_layout_cards(gfx_object_t *obj, gfx_coverflow_t *flow);

static const gfx_widget_class_t s_gfx_coverflow_widget_class = {
    .type = GFX_OBJ_TYPE_COVERFLOW,
    .name = "coverflow",
    .draw = gfx_coverflow_draw,
    .delete = gfx_coverflow_delete_impl,
    .load = gfx_coverflow_load_impl,
    .release = gfx_coverflow_release_impl,
    .update = gfx_coverflow_update,
    .touch_event = gfx_coverflow_touch_event,
};

static void gfx_coverflow_init_default_state(gfx_coverflow_t *flow)
{
    memset(flow, 0, sizeof(*flow));
    flow->label.style.opa = 0xFF;
    flow->label.style.bg_enable = false;
    flow->label.style.text_align = GFX_TEXT_ALIGN_CENTER;
    flow->label.text.long_mode = GFX_LABEL_LONG_CLIP;
    flow->drag_threshold = GFX_COVERFLOW_DEFAULT_DRAG_THRESHOLD;
    flow->page_threshold = GFX_COVERFLOW_DEFAULT_PAGE_THRESHOLD;
    flow->style.bg_color = GFX_COLOR_HEX(0x101418);
    flow->style.center_color = GFX_COLOR_HEX(0x245C8F);
    flow->style.side_color = GFX_COLOR_HEX(0x1F2A35);
    flow->style.text_color = GFX_COLOR_HEX(0xF3F7FA);
    flow->style.border_color = GFX_COLOR_HEX(0x76B7E8);
    flow->style.border_width = 1;
    flow->style.center_zoom = GFX_COVERFLOW_DEFAULT_CENTER_ZOOM;
    flow->style.side_zoom = GFX_COVERFLOW_DEFAULT_SIDE_ZOOM;
    flow->style.spacing_pct = GFX_COVERFLOW_DEFAULT_SPACING;
    flow->style.side_dim_opa = GFX_COVERFLOW_DEFAULT_SIDE_DIM_OPA;
}

static gfx_err_t gfx_coverflow_dup_text(const char *text, char **out_text)
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

static void gfx_coverflow_free_items(gfx_coverflow_t *flow)
{
    if (flow == NULL) {
        return;
    }
    gfx_tween_stop(flow->tween, false);
    if (flow->items != NULL) {
        for (uint16_t i = 0; i < flow->item_count; i++) {
            free(flow->items[i]);
        }
    }
    free(flow->items);
    free(flow->images);
    if (flow->image_resources != NULL) {
        for (uint16_t i = 0; i < flow->item_count; i++) {
            gfx_image_resource_close(&flow->image_resources[i]);
        }
    }
    free(flow->image_resources);
    free(flow->cards);
    flow->items = NULL;
    flow->images = NULL;
    flow->image_resources = NULL;
    flow->cards = NULL;
    flow->item_count = 0;
    flow->selected_index = 0;
    flow->tween_target_index = 0;
    flow->tween_commit_pending = false;
    flow->use_images = false;
    flow->use_cards = false;
    flow->drag_offset = 0;
    memset(&flow->touch, 0, sizeof(flow->touch));
}

static void gfx_coverflow_cancel_tween(gfx_coverflow_t *flow)
{
    if (flow == NULL) {
        return;
    }

    gfx_tween_stop(flow->tween, false);
    flow->tween_commit_pending = false;
    flow->tween_target_index = flow->selected_index;
}

static int32_t gfx_coverflow_clamp_index(const gfx_coverflow_t *flow, int32_t index)
{
    if (flow == NULL || flow->item_count == 0U) {
        return 0;
    }
    if (index < 0) {
        return 0;
    }
    if (index >= (int32_t)flow->item_count) {
        return (int32_t)flow->item_count - 1;
    }
    return index;
}

static void gfx_coverflow_set_selected_internal(gfx_object_t *obj, gfx_coverflow_t *flow, int32_t index, bool emit)
{
    int32_t next = gfx_coverflow_clamp_index(flow, index);
    if (flow == NULL) {
        return;
    }
    gfx_coverflow_cancel_tween(flow);
    flow->drag_offset = 0;
    if (flow->selected_index != next) {
        flow->selected_index = next;
        if (emit && flow->changed_cb != NULL) {
            flow->changed_cb(obj, next, flow->changed_user_data);
        }
    }
    if (flow->use_cards) {
        gfx_coverflow_layout_cards(obj, flow);
        gfx_object_invalidate_tree(obj);
    } else {
        gfx_object_invalidate(obj);
    }
}

static void gfx_coverflow_tween_value_cb(gfx_tween_t *tween, gfx_object_t *obj, int32_t value, void *user_data)
{
    gfx_coverflow_t *flow = (gfx_coverflow_t *)user_data;
    (void)tween;

    if (obj == NULL || flow == NULL) {
        return;
    }

    if (flow->drag_offset != value) {
        flow->drag_offset = value;
        gfx_object_invalidate(obj);
    }
}

static void gfx_coverflow_tween_done_cb(gfx_tween_t *tween, gfx_object_t *obj, void *user_data)
{
    gfx_coverflow_t *flow = (gfx_coverflow_t *)user_data;
    bool selection_changed = false;
    bool need_invalidate = false;
    (void)tween;

    if (obj == NULL || flow == NULL) {
        return;
    }

    if (flow->tween_commit_pending) {
        flow->tween_commit_pending = false;
        if (flow->selected_index != flow->tween_target_index) {
            flow->selected_index = flow->tween_target_index;
            selection_changed = true;
        }
    }

    if (flow->drag_offset != 0) {
        flow->drag_offset = 0;
        need_invalidate = true;
    }

    if (selection_changed && flow->changed_cb != NULL) {
        flow->changed_cb(obj, flow->selected_index, flow->changed_user_data);
    }

    if (selection_changed || need_invalidate) {
        gfx_object_invalidate(obj);
    }
}

static void gfx_coverflow_start_tween(gfx_object_t *obj, gfx_coverflow_t *flow, int32_t target_index,
                                      int32_t start_offset)
{
    int32_t spacing;
    int32_t end_offset = 0;

    if (obj == NULL || flow == NULL) {
        return;
    }

    target_index = gfx_coverflow_clamp_index(flow, target_index);
    spacing = ((int32_t)obj->geometry.width * (int32_t)flow->style.spacing_pct) / 100;
    if (spacing <= 0) {
        spacing = (int32_t)flow->page_threshold;
    }

    flow->tween_target_index = target_index;
    flow->tween_commit_pending = (target_index != flow->selected_index);

    if (flow->tween_commit_pending) {
        end_offset = (target_index > flow->selected_index) ? -spacing : spacing;
        if (start_offset == end_offset) {
            start_offset += (end_offset < 0) ? 1 : -1;
        }
    }

    if (flow->tween == NULL) {
        flow->tween = gfx_tween_create(obj);
    }
    if (flow->tween == NULL ||
            gfx_tween_start_i32(flow->tween, start_offset, end_offset, GFX_COVERFLOW_TWEEN_MS,
                                GFX_TWEEN_EASE_OUT_QUAD,
                                gfx_coverflow_tween_value_cb, gfx_coverflow_tween_done_cb, flow) != GFX_OK) {
        if (flow->tween_commit_pending) {
            flow->selected_index = flow->tween_target_index;
            flow->tween_commit_pending = false;
            if (flow->changed_cb != NULL) {
                flow->changed_cb(obj, flow->selected_index, flow->changed_user_data);
            }
        }
        flow->drag_offset = 0;
        gfx_object_invalidate(obj);
    }
}

static gfx_object_t *gfx_coverflow_card_nth_child(gfx_object_t *card, uint8_t index)
{
    gfx_object_child_t *node;

    if (card == NULL) {
        return NULL;
    }

    node = card->child_list;
    while (node != NULL && index > 0U) {
        node = node->next;
        index--;
    }

    return node != NULL ? (gfx_object_t *)node->src : NULL;
}

static gfx_object_t *gfx_coverflow_card_root(const gfx_coverflow_t *flow, int32_t index)
{
    if (flow == NULL || flow->cards == NULL || index < 0 || index >= (int32_t)flow->item_count) {
        return NULL;
    }

    return flow->cards[index].card;
}

static gfx_err_t gfx_coverflow_draw_text(gfx_object_t *obj, gfx_coverflow_t *flow,
        const gfx_draw_ctx_t *ctx, const char *text, const gfx_area_t *area, const gfx_area_t *clip)
{
    gfx_area_t text_area = {
        .x1 = (gfx_coord_t)(area->x1 + 10),
        .y1 = (gfx_coord_t)(area->y1 + 8),
        .x2 = (gfx_coord_t)(area->x2 - 10),
        .y2 = (gfx_coord_t)(area->y2 - 8),
    };
    gfx_err_t ret;

    flow->label.text.text = (char *)(text ? text : "");
    flow->label.text.text_width = 0;
    flow->label.scroll.offset = 0;
    flow->label.snap.offset = 0;
    free(flow->label.render.mask);
    flow->label.render.mask = NULL;
    flow->label.render.mask_capacity = 0;
    flow->label.style.color = flow->style.text_color;
    flow->label.style.bg_enable = false;

    ret = gfx_label_text_box_update(obj, &flow->label, &text_area);
    if (ret == GFX_OK) {
        ret = gfx_label_text_box_draw(obj, &flow->label, ctx, &text_area, clip);
    }
    return ret;
}

static void gfx_coverflow_draw_image_scaled(gfx_object_t *obj, const gfx_draw_ctx_t *ctx,
        const gfx_image_resource_t *resource, const gfx_area_t *card,
        const gfx_area_t *clip)
{
    gfx_color_format_t color_format;
    uint8_t src_pixel_size;
    gfx_coord_t src_stride;
    gfx_area_t draw_area;
    gfx_area_t dst_area;
    gfx_area_t dst_clip;
    gfx_area_t src_area;
    gfx_area_t local_clip;
    int32_t dst_w;
    int32_t dst_h;
    int32_t src_w;
    int32_t src_h;
    int32_t src_draw_w;
    int32_t src_draw_h;
    int32_t src_x0;
    int32_t src_y0;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (obj == NULL || ctx == NULL || resource == NULL ||
            resource->header.w == 0U || resource->header.h == 0U ||
            !gfx_image_resource_is_open(resource)) {
        return;
    }

    color_format = gfx_image_resource_format(resource);
    if (!gfx_color_format_is_image_supported(color_format)) {
        return;
    }

    src_pixel_size = gfx_image_resource_pixel_size(resource);
    if (src_pixel_size == 0U) {
        return;
    }

    src_stride = gfx_image_resource_stride_px(resource);
    if (!gfx_area_intersect_exclusive(&draw_area, clip, card)) {
        return;
    }

    dst_w = card->x2 - card->x1;
    dst_h = card->y2 - card->y1;
    src_w = (int32_t)resource->header.w;
    src_h = (int32_t)resource->header.h;
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) {
        return;
    }

    if ((int64_t)src_w * dst_h > (int64_t)src_h * dst_w) {
        src_draw_h = src_h;
        src_draw_w = (dst_w * src_h) / dst_h;
    } else {
        src_draw_w = src_w;
        src_draw_h = (dst_h * src_w) / dst_w;
    }
    if (src_draw_w <= 0) {
        src_draw_w = 1;
    }
    if (src_draw_h <= 0) {
        src_draw_h = 1;
    }
    src_x0 = (src_w - src_draw_w) / 2;
    src_y0 = (src_h - src_draw_h) / 2;

    const gfx_opa_t *alpha_base = NULL;
    if (gfx_color_format_has_plane_alpha(color_format)) {
        alpha_base = gfx_image_resource_alpha(resource);
        if (alpha_base == NULL) {
            return;
        }
    }

    dst_area.x1 = card->x1 - ctx->buf_area.x1;
    dst_area.y1 = card->y1 - ctx->buf_area.y1;
    dst_area.x2 = card->x2 - ctx->buf_area.x1;
    dst_area.y2 = card->y2 - ctx->buf_area.y1;
    dst_clip.x1 = draw_area.x1 - ctx->buf_area.x1;
    dst_clip.y1 = draw_area.y1 - ctx->buf_area.y1;
    dst_clip.x2 = draw_area.x2 - ctx->buf_area.x1;
    dst_clip.y2 = draw_area.y2 - ctx->buf_area.y1;
    local_clip.x1 = clip->x1 - ctx->buf_area.x1;
    local_clip.y1 = clip->y1 - ctx->buf_area.y1;
    local_clip.x2 = clip->x2 - ctx->buf_area.x1;
    local_clip.y2 = clip->y2 - ctx->buf_area.y1;
    if (!gfx_area_intersect_exclusive(&dst_clip, &dst_clip, &local_clip)) {
        return;
    }

    src_area.x1 = (gfx_coord_t)src_x0;
    src_area.y1 = (gfx_coord_t)src_y0;
    src_area.x2 = (gfx_coord_t)(src_x0 + src_draw_w);
    src_area.y2 = (gfx_coord_t)(src_y0 + src_draw_h);

    gfx_render_image_t render_src = {
        .pixels = gfx_image_resource_pixels(resource),
        .stride = src_stride,
        .format = color_format,
        .alpha = alpha_base,
        .alpha_stride = gfx_image_resource_alpha_stride(resource),
    };
    if (gfx_render_surface_scale_image(obj->disp, &dst_surface, card, &render_src, &src_area, 0xFFU)) {
        return;
    }

    gfx_sw_blend_img_scale_draw_fmt(ctx->buf, ctx->stride, ctx->format,
                                    gfx_image_resource_pixels(resource), src_stride,
                                    alpha_base, gfx_image_resource_alpha_stride(resource),
                                    &dst_area, &dst_clip, &src_area,
                                    color_format, 0xFF);
}

static int32_t gfx_coverflow_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t gfx_coverflow_clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static gfx_opa_t gfx_coverflow_effect_dim_from_zoom(const gfx_coverflow_effect_t *effect, uint16_t zoom)
{
    if (effect == NULL) {
        return 0;
    }
    if (effect->center_zoom <= effect->side_zoom || zoom >= effect->center_zoom) {
        return 0;
    }
    if (zoom <= effect->side_zoom) {
        return effect->side_dim_opa;
    }

    return (gfx_opa_t)(((uint32_t)(effect->center_zoom - zoom) * effect->side_dim_opa) /
                       (uint32_t)(effect->center_zoom - effect->side_zoom));
}

static void gfx_coverflow_effect_calc(const gfx_coverflow_effect_t *effect, int32_t rel_slot,
                                      int32_t drag_offset, int32_t spacing,
                                      gfx_coverflow_effect_state_t *state)
{
    int32_t pos_q;
    int32_t progress_q;
    int32_t abs_pos_q;
    int32_t zoom_delta;
    int32_t zoom;

    if (effect == NULL || state == NULL) {
        return;
    }

    pos_q = rel_slot * GFX_COVERFLOW_POS_Q;
    progress_q = spacing > 0 ? (drag_offset * GFX_COVERFLOW_POS_Q) / spacing : 0;
    pos_q += progress_q;
    abs_pos_q = gfx_coverflow_abs_i32(pos_q);
    if (abs_pos_q > GFX_COVERFLOW_POS_Q) {
        abs_pos_q = GFX_COVERFLOW_POS_Q;
    }

    zoom_delta = (int32_t)effect->center_zoom - (int32_t)effect->side_zoom;
    zoom = (int32_t)effect->center_zoom -
           (zoom_delta * abs_pos_q + GFX_COVERFLOW_POS_Q / 2) / GFX_COVERFLOW_POS_Q;
    zoom = gfx_coverflow_clamp_i32(zoom, effect->side_zoom, effect->center_zoom);

    state->pos_q = pos_q;
    state->zoom = (uint16_t)zoom;
    state->dim_opa = gfx_coverflow_effect_dim_from_zoom(effect, (uint16_t)zoom);
    state->centerish = abs_pos_q < (GFX_COVERFLOW_POS_Q / 2);
}

static bool gfx_coverflow_effect_should_swap(const gfx_coverflow_card_state_t *a,
        const gfx_coverflow_card_state_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    return a->zoom > b->zoom;
}

static void gfx_coverflow_calc_card_state(const gfx_coverflow_t *flow, const gfx_area_t *obj_area,
        int32_t index, int32_t rel_slot,
        gfx_coverflow_card_state_t *state)
{
    gfx_coverflow_effect_t effect;
    gfx_coverflow_effect_state_t effect_state = {0};
    int32_t w = obj_area->x2 - obj_area->x1;
    int32_t h = obj_area->y2 - obj_area->y1;
    int32_t center_x = (obj_area->x1 + obj_area->x2) / 2;
    int32_t center_y = (obj_area->y1 + obj_area->y2) / 2;
    int32_t base_w = (w * 58) / 100;
    int32_t base_h = (h * 76) / 100;
    int32_t spacing = (w * flow->style.spacing_pct) / 100;
    int32_t card_w;
    int32_t card_h;
    int32_t card_cx;

    effect.center_zoom = flow->style.center_zoom;
    effect.side_zoom = flow->style.side_zoom;
    effect.spacing_pct = flow->style.spacing_pct;
    effect.side_dim_opa = flow->style.side_dim_opa;
    gfx_coverflow_effect_calc(&effect, rel_slot, flow->drag_offset, spacing, &effect_state);

    card_w = (base_w * effect_state.zoom) / 100;
    card_h = (base_h * effect_state.zoom) / 100;
    if (card_w < 1) {
        card_w = 1;
    }
    if (card_h < 1) {
        card_h = 1;
    }

    card_cx = center_x + (effect_state.pos_q * spacing) / GFX_COVERFLOW_POS_Q;
    state->index = index;
    state->pos_q = effect_state.pos_q;
    state->zoom = effect_state.zoom;
    state->dim_opa = effect_state.dim_opa;
    state->centerish = effect_state.centerish;
    state->area.x1 = (gfx_coord_t)(card_cx - card_w / 2);
    state->area.x2 = (gfx_coord_t)(card_cx + card_w / 2);
    state->area.y1 = (gfx_coord_t)(center_y - card_h / 2);
    state->area.y2 = (gfx_coord_t)(center_y + card_h / 2);
}

static void gfx_coverflow_draw_card_state(gfx_object_t *obj, gfx_coverflow_t *flow, const gfx_draw_ctx_t *ctx,
        const gfx_coverflow_card_state_t *state)
{
    gfx_area_t clip;
    gfx_color_t fill = state->centerish ? flow->style.center_color : flow->style.side_color;
    gfx_render_surface_t dst_surface = {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = ctx->clip_area,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    if (state == NULL || state->index < 0 || state->index >= (int32_t)flow->item_count) {
        return;
    }

    if (!gfx_area_intersect_exclusive(&clip, &ctx->clip_area, &state->area)) {
        return;
    }

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip, fill, 0xFFU);
    if (flow->style.border_width > 0U) {
        gfx_render_surface_rect_stroke(obj->disp, &dst_surface, &state->area,
                                       flow->style.border_width, flow->style.border_color, 0xFF);
    }
    if (flow->use_images) {
        if (flow->image_resources != NULL) {
            gfx_coverflow_draw_image_scaled(obj, ctx, &flow->image_resources[state->index], &state->area, &clip);
        }
    } else {
        (void)gfx_coverflow_draw_text(obj, flow, ctx, flow->items[state->index], &state->area, &clip);
    }

    if (state->dim_opa > 0U) {
        gfx_render_surface_fill(obj->disp, &dst_surface, &clip,
                                GFX_COLOR_HEX(0x000000), state->dim_opa);
    }
}

static void gfx_coverflow_apply_card_state(gfx_coverflow_t *flow, const gfx_area_t *flow_area,
        const gfx_coverflow_card_state_t *state, bool visible)
{
    gfx_object_t *card;
    gfx_object_t *image;
    gfx_object_t *title;
    gfx_coord_t card_x;
    gfx_coord_t card_y;
    int32_t card_w;
    int32_t card_h;
    int32_t title_h;
    int32_t pad;
    uint16_t width;
    uint16_t height;

    if (flow == NULL || flow_area == NULL || state == NULL || state->index < 0 ||
            state->index >= (int32_t)flow->item_count || flow->cards == NULL) {
        return;
    }

    card = gfx_coverflow_card_root(flow, state->index);
    if (card == NULL) {
        return;
    }

    if (card->state.is_visible != visible) {
        (void)gfx_object_set_visible(card, visible);
    }
    if (!visible) {
        return;
    }

    width = (uint16_t)MAX(0, state->area.x2 - state->area.x1);
    height = (uint16_t)MAX(0, state->area.y2 - state->area.y1);
    card_x = (gfx_coord_t)(state->area.x1 - flow_area->x1);
    card_y = (gfx_coord_t)(state->area.y1 - flow_area->y1);
    if (card->local_geometry.x != card_x || card->local_geometry.y != card_y) {
        (void)gfx_object_set_pos(card, card_x, card_y);
    }
    if (card->geometry.width != width || card->geometry.height != height) {
        (void)gfx_object_set_size(card, width, height);
    }

    image = flow->cards[state->index].image_slot;
    title = flow->cards[state->index].title_slot;
    card_w = state->area.x2 - state->area.x1;
    card_h = state->area.y2 - state->area.y1;
    pad = state->centerish ? 10 : 6;
    title_h = state->centerish ? 34 : 26;
    if (title_h > card_h / 2) {
        title_h = card_h / 2;
    }

    if (image != NULL) {
        uint16_t image_w = (uint16_t)MAX(0, card_w - pad * 2);
        uint16_t image_h = (uint16_t)MAX(0, card_h - title_h - pad * 2);
        gfx_coord_t image_x = (gfx_coord_t)pad;
        gfx_coord_t image_y = (gfx_coord_t)pad;
        if (image->state.is_visible != visible) {
            (void)gfx_object_set_visible(image, visible);
        }
        if (image->local_geometry.x != image_x || image->local_geometry.y != image_y) {
            (void)gfx_object_set_pos(image, image_x, image_y);
        }
        if (image->type == GFX_OBJ_TYPE_MESH_IMAGE &&
                (image->geometry.width != image_w || image->geometry.height != image_h)) {
            (void)gfx_mesh_img_set_rect(image, image_w, image_h);
        } else if (image->type != GFX_OBJ_TYPE_MESH_IMAGE &&
                   (image->geometry.width != image_w || image->geometry.height != image_h)) {
            (void)gfx_object_set_size(image, image_w, image_h);
        }
    }

    if (title != NULL) {
        gfx_coord_t title_x = (gfx_coord_t)pad;
        gfx_coord_t title_y = (gfx_coord_t)(card_h - title_h - pad / 2);
        uint16_t title_w = (uint16_t)MAX(0, card_w - pad * 2);
        uint16_t title_h_u16 = (uint16_t)MAX(0, title_h);
        if (title->state.is_visible != visible) {
            (void)gfx_object_set_visible(title, visible);
        }
        if (title->local_geometry.x != title_x || title->local_geometry.y != title_y) {
            (void)gfx_object_set_pos(title, title_x, title_y);
        }
        if (title->geometry.width != title_w || title->geometry.height != title_h_u16) {
            (void)gfx_object_set_size(title, title_w, title_h_u16);
        }
    }
}

static void gfx_coverflow_hide_card_items(gfx_coverflow_t *flow, const gfx_coverflow_card_state_t *visible_cards,
        uint8_t visible_count)
{
    if (flow == NULL || flow->cards == NULL) {
        return;
    }

    for (uint16_t i = 0; i < flow->item_count; i++) {
        bool visible = false;
        for (uint8_t j = 0; j < visible_count; j++) {
            if (visible_cards[j].index == (int32_t)i) {
                visible = true;
                break;
            }
        }
        gfx_object_t *card = gfx_coverflow_card_root(flow, i);
        if (!visible && card != NULL && card->state.is_visible) {
            (void)gfx_object_set_visible(card, false);
        }
    }
}

static uint8_t gfx_coverflow_collect_card_states(gfx_object_t *obj, gfx_coverflow_t *flow,
        const gfx_area_t *obj_area,
        gfx_coverflow_card_state_t *cards, uint8_t max_cards,
        bool sort_by_zoom)
{
    uint8_t card_count = 0;

    if (obj == NULL || flow == NULL || obj_area == NULL ||
            cards == NULL || max_cards == 0U || flow->item_count == 0U) {
        return 0;
    }

    for (int32_t slot = -GFX_COVERFLOW_VISIBLE_SIDE_COUNT; slot <= GFX_COVERFLOW_VISIBLE_SIDE_COUNT; slot++) {
        int32_t index = flow->selected_index + slot;
        if (index < 0 || index >= (int32_t)flow->item_count || card_count >= max_cards) {
            continue;
        }
        gfx_coverflow_calc_card_state(flow, obj_area, index, slot, &cards[card_count]);
        card_count++;
    }

    if (sort_by_zoom) {
        for (uint8_t i = 0; i < card_count; i++) {
            for (uint8_t j = (uint8_t)(i + 1U); j < card_count; j++) {
                if (gfx_coverflow_effect_should_swap(&cards[i], &cards[j])) {
                    gfx_coverflow_card_state_t tmp = cards[i];
                    cards[i] = cards[j];
                    cards[j] = tmp;
                }
            }
        }
    }

    return card_count;
}

static void gfx_coverflow_layout_cards(gfx_object_t *obj, gfx_coverflow_t *flow)
{
    gfx_coverflow_card_state_t cards[GFX_COVERFLOW_VISIBLE_SIDE_COUNT * 2 + 1];
    gfx_area_t obj_area;
    uint8_t card_count;

    if (obj == NULL || flow == NULL || !flow->use_cards) {
        return;
    }

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return;
    }

    card_count = gfx_coverflow_collect_card_states(obj, flow, &obj_area, cards,
                 (uint8_t)(sizeof(cards) / sizeof(cards[0])), false);
    gfx_coverflow_hide_card_items(flow, cards, card_count);
    for (uint8_t i = 0; i < card_count; i++) {
        gfx_coverflow_apply_card_state(flow, &obj_area, &cards[i], true);
    }
}

static gfx_err_t gfx_coverflow_draw(gfx_object_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_coverflow_t *flow;
    gfx_area_t clip;
    gfx_draw_ctx_t clipped_ctx;
    gfx_coverflow_card_state_t cards[GFX_COVERFLOW_VISIBLE_SIDE_COUNT * 2 + 1];
    uint8_t card_count = 0;
    gfx_area_t obj_area;
    gfx_render_surface_t dst_surface;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(ctx, GFX_ERR_INVALID_ARG);
    flow = (gfx_coverflow_t *)obj->src;
    GFX_RETURN_IF_NULL(flow, GFX_ERR_INVALID_STATE);

    if (!gfx_object_get_abs_area_exclusive(obj, &obj_area)) {
        return GFX_OK;
    }
    if (!gfx_area_intersect_exclusive(&clip, &ctx->clip_area, &obj_area)) {
        return GFX_OK;
    }

    clipped_ctx = *ctx;
    clipped_ctx.clip_area = clip;
    dst_surface = (gfx_render_surface_t) {
        .buf = ctx->buf,
        .buf_area = ctx->buf_area,
        .clip_area = clip,
        .stride = ctx->stride,
        .format = ctx->format,
    };

    gfx_render_surface_fill(obj->disp, &dst_surface, &clip, flow->style.bg_color, 0xFFU);

    if (flow->item_count == 0U) {
        return GFX_OK;
    }

    if (flow->use_cards) {
        gfx_coverflow_layout_cards(obj, flow);
    }

    card_count = gfx_coverflow_collect_card_states(obj, flow, &obj_area, cards,
                 (uint8_t)(sizeof(cards) / sizeof(cards[0])), true);

    if (flow->use_cards) {
        for (uint8_t i = 0; i < card_count; i++) {
            if (flow->cards != NULL && cards[i].index >= 0 && cards[i].index < (int32_t)flow->item_count) {
                gfx_object_t *card = gfx_coverflow_card_root(flow, cards[i].index);
                if (card != NULL) {
                    gfx_render_draw_object_tree(obj->disp, card, &clipped_ctx);
                }
            }
        }
    } else {
        for (uint8_t i = 0; i < card_count; i++) {
            gfx_coverflow_draw_card_state(obj, flow, &clipped_ctx, &cards[i]);
        }
    }
    return GFX_OK;
}

static gfx_err_t gfx_coverflow_update(gfx_object_t *obj)
{
    gfx_coverflow_t *flow;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    flow = (gfx_coverflow_t *)obj->src;
    if (flow != NULL) {
        gfx_coverflow_layout_cards(obj, flow);
    }
    return GFX_OK;
}

static gfx_err_t gfx_coverflow_delete_impl(gfx_object_t *obj)
{
    gfx_coverflow_t *flow;
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    flow = (gfx_coverflow_t *)obj->src;
    if (flow == NULL) {
        return GFX_OK;
    }
    gfx_coverflow_free_items(flow);
    gfx_tween_delete(flow->tween);
    free(flow->label.render.mask);
    free(flow->label.render.color_mask);
    free(flow);
    return GFX_OK;
}

static gfx_err_t gfx_coverflow_load_impl(gfx_object_t *obj)
{
    gfx_coverflow_t *flow;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    flow = (gfx_coverflow_t *)obj->src;
    GFX_RETURN_ON_FALSE(flow != NULL, GFX_ERR_INVALID_STATE, TAG, "load: state is NULL");

    GFX_RETURN_ON_ERROR(gfx_label_load_state(obj, &flow->label), TAG, "load label failed");
    if (flow->use_images && flow->image_resources != NULL) {
        for (uint16_t i = 0; i < flow->item_count; i++) {
            GFX_RETURN_ON_ERROR(gfx_image_resource_open(&flow->image_resources[i]),
                                TAG, "load image resource failed");
        }
    }
    return GFX_OK;
}

static void gfx_coverflow_release_impl(gfx_object_t *obj)
{
    if (obj == NULL || obj->src == NULL || obj->type != GFX_OBJ_TYPE_COVERFLOW) {
        return;
    }

    gfx_coverflow_t *flow = (gfx_coverflow_t *)obj->src;
    gfx_label_release_state(&flow->label);
    if (flow->image_resources != NULL) {
        for (uint16_t i = 0; i < flow->item_count; i++) {
            gfx_image_resource_close(&flow->image_resources[i]);
        }
    }
}

static void gfx_coverflow_touch_event(gfx_object_t *obj, const void *event_data)
{
    const gfx_touch_event_t *event = (const gfx_touch_event_t *)event_data;
    gfx_coverflow_t *flow;
    int32_t dx;
    int32_t dy;
    int32_t abs_dx;
    int32_t abs_dy;
    bool was_pressed;

    if (obj == NULL || event == NULL || obj->src == NULL) {
        return;
    }
    flow = (gfx_coverflow_t *)obj->src;
    if (flow->item_count == 0U) {
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
        gfx_coverflow_cancel_tween(flow);
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
            int32_t limit = ((int32_t)obj->geometry.width * (int32_t)flow->style.spacing_pct) / 100;
            if (limit <= 0) {
                limit = (int32_t)flow->page_threshold;
            }
            flow->drag_offset = gfx_coverflow_clamp_i32(dx, -limit, limit);
            gfx_object_invalidate(obj);
        }
        return;
    }
    if (event->type != GFX_TOUCH_EVENT_RELEASE || !was_pressed) {
        return;
    }
    if (flow->drag_offset <= -(int32_t)flow->page_threshold) {
        gfx_coverflow_start_tween(obj, flow, flow->selected_index + 1, flow->drag_offset);
    } else if (flow->drag_offset >= (int32_t)flow->page_threshold) {
        gfx_coverflow_start_tween(obj, flow, flow->selected_index - 1, flow->drag_offset);
    } else {
        gfx_coverflow_start_tween(obj, flow, flow->selected_index, flow->drag_offset);
    }
}

gfx_object_t *gfx_coverflow_create(gfx_display_t *disp)
{
    gfx_object_t *obj;
    gfx_coverflow_t *flow;
    if (disp == NULL) {
        return NULL;
    }
    flow = calloc(1, sizeof(gfx_coverflow_t));
    if (flow == NULL) {
        return NULL;
    }
    gfx_coverflow_init_default_state(flow);
    if (gfx_object_create_class_instance(disp, &s_gfx_coverflow_widget_class,
                                         flow, GFX_COVERFLOW_DEFAULT_WIDTH, GFX_COVERFLOW_DEFAULT_HEIGHT,
                                         "gfx_coverflow_create", &obj) != GFX_OK) {
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

gfx_err_t gfx_coverflow_clear(gfx_object_t *obj)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    gfx_coverflow_free_items((gfx_coverflow_t *)obj->src);
    gfx_object_set_manual_child_draw(obj, false);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_add_item(gfx_object_t *obj, const char *text)
{
    gfx_coverflow_t *flow;
    char **new_items;
    char *dup_text = NULL;
    gfx_err_t ret;
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    flow = (gfx_coverflow_t *)obj->src;
    if (flow->use_images || flow->use_cards) {
        gfx_coverflow_free_items(flow);
        gfx_object_set_manual_child_draw(obj, false);
    }
    ret = gfx_coverflow_dup_text(text, &dup_text);
    if (ret != GFX_OK) {
        return ret;
    }
    new_items = realloc(flow->items, ((size_t)flow->item_count + 1U) * sizeof(char *));
    if (new_items == NULL) {
        free(dup_text);
        return GFX_ERR_NO_MEM;
    }
    flow->items = new_items;
    flow->items[flow->item_count++] = dup_text;
    flow->images = NULL;
    flow->image_resources = NULL;
    flow->cards = NULL;
    flow->use_images = false;
    flow->use_cards = false;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_items(gfx_object_t *obj, const char *const *items, uint16_t item_count)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(item_count == 0U || items != NULL, GFX_ERR_INVALID_ARG, TAG, "items is NULL");
    GFX_RETURN_ON_ERROR(gfx_coverflow_clear(obj), TAG, "clear failed");
    for (uint16_t i = 0; i < item_count; i++) {
        GFX_RETURN_ON_ERROR(gfx_coverflow_add_item(obj, items[i]), TAG, "add item failed");
    }
    ((gfx_coverflow_t *)obj->src)->selected_index = item_count > 0U ? 0 : -1;
    ((gfx_coverflow_t *)obj->src)->tween_target_index = item_count > 0U ? 0 : -1;
    ((gfx_coverflow_t *)obj->src)->tween_commit_pending = false;
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_image_items(gfx_object_t *obj, const gfx_image_dsc_t *const *images,
                                        uint16_t item_count)
{
    gfx_image_src_t *sources = NULL;
    gfx_err_t ret;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_ON_FALSE(item_count == 0U || images != NULL, GFX_ERR_INVALID_ARG, TAG, "images is NULL");

    if (item_count > 0U) {
        sources = calloc(item_count, sizeof(sources[0]));
        if (sources == NULL) {
            return GFX_ERR_NO_MEM;
        }
        for (uint16_t i = 0; i < item_count; i++) {
            sources[i] = (gfx_image_src_t) {
                .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
                .data = images[i],
            };
        }
    }

    ret = gfx_coverflow_set_image_sources(obj, sources, item_count);
    free(sources);
    return ret;
}

gfx_err_t gfx_coverflow_set_image_sources(gfx_object_t *obj, const gfx_image_src_t *sources,
        uint16_t item_count)
{
    gfx_coverflow_t *flow;
    const gfx_image_dsc_t **image_copy = NULL;
    gfx_image_resource_t *resource_copy = NULL;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(item_count == 0U || sources != NULL, GFX_ERR_INVALID_ARG, TAG, "image sources is NULL");

    if (item_count > 0U) {
        image_copy = calloc(item_count, sizeof(image_copy[0]));
        if (image_copy == NULL) {
            return GFX_ERR_NO_MEM;
        }
        resource_copy = calloc(item_count, sizeof(resource_copy[0]));
        if (resource_copy == NULL) {
            free(image_copy);
            return GFX_ERR_NO_MEM;
        }
        for (uint16_t i = 0; i < item_count; i++) {
            gfx_err_t ret = gfx_image_resource_set_source(&resource_copy[i], &sources[i]);
            if (ret != GFX_OK) {
                for (uint16_t j = 0; j < i; j++) {
                    gfx_image_resource_close(&resource_copy[j]);
                }
                free(resource_copy);
                free(image_copy);
                return ret;
            }
            if (sources[i].type == GFX_IMAGE_SRC_TYPE_IMAGE_DSC) {
                image_copy[i] = (const gfx_image_dsc_t *)sources[i].data;
            }
        }
    }

    flow = (gfx_coverflow_t *)obj->src;
    gfx_coverflow_free_items(flow);
    flow->images = image_copy;
    flow->image_resources = resource_copy;
    flow->item_count = item_count;
    flow->selected_index = item_count > 0U ? 0 : -1;
    flow->tween_target_index = flow->selected_index;
    flow->tween_commit_pending = false;
    flow->use_images = true;
    flow->use_cards = false;
    gfx_object_set_manual_child_draw(obj, false);
    gfx_object_mark_resource_dirty(obj);
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_card_descriptors(gfx_object_t *obj, const gfx_coverflow_card_dsc_t *cards,
        uint16_t item_count)
{
    gfx_coverflow_t *flow;
    gfx_coverflow_card_dsc_t *card_copy = NULL;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(item_count == 0U || cards != NULL, GFX_ERR_INVALID_ARG, TAG, "cards is NULL");

    if (item_count > 0U) {
        card_copy = calloc(item_count, sizeof(card_copy[0]));
        if (card_copy == NULL) {
            return GFX_ERR_NO_MEM;
        }
        memcpy(card_copy, cards, (size_t)item_count * sizeof(card_copy[0]));
    }

    flow = (gfx_coverflow_t *)obj->src;
    gfx_coverflow_free_items(flow);
    flow->cards = card_copy;
    flow->item_count = item_count;
    flow->selected_index = item_count > 0U ? 0 : -1;
    flow->tween_target_index = flow->selected_index;
    flow->tween_commit_pending = false;
    flow->use_cards = true;

    gfx_object_set_manual_child_draw(obj, true);
    for (uint16_t i = 0; i < item_count; i++) {
        gfx_object_t *card = flow->cards[i].card;
        if (card == NULL) {
            continue;
        }
        if (flow->cards[i].image_slot == NULL) {
            flow->cards[i].image_slot = gfx_coverflow_card_nth_child(card, 0);
        }
        if (flow->cards[i].title_slot == NULL) {
            flow->cards[i].title_slot = gfx_coverflow_card_nth_child(card, 1);
        }
        if (gfx_object_get_parent(card) != obj) {
            GFX_RETURN_ON_ERROR(gfx_object_add_child(obj, card), TAG, "add card child failed");
        }
        gfx_object_set_input_passthrough_tree(card, true);
        (void)gfx_object_set_visible(card, false);
    }

    gfx_coverflow_layout_cards(obj, flow);
    gfx_object_invalidate_tree(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_card_items(gfx_object_t *obj, gfx_object_t *const *cards, uint16_t item_count)
{
    gfx_coverflow_card_dsc_t *descriptors = NULL;
    gfx_err_t ret;

    GFX_RETURN_ON_FALSE(item_count == 0U || cards != NULL, GFX_ERR_INVALID_ARG, TAG, "cards is NULL");
    if (item_count > 0U) {
        descriptors = calloc(item_count, sizeof(descriptors[0]));
        if (descriptors == NULL) {
            return GFX_ERR_NO_MEM;
        }
        for (uint16_t i = 0; i < item_count; i++) {
            descriptors[i].card = cards[i];
        }
    }

    ret = gfx_coverflow_set_card_descriptors(obj, descriptors, item_count);
    free(descriptors);
    return ret;
}

gfx_err_t gfx_coverflow_set_selected(gfx_object_t *obj, int32_t index)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    gfx_coverflow_set_selected_internal(obj, (gfx_coverflow_t *)obj->src, index, true);
    return GFX_OK;
}

int32_t gfx_coverflow_get_selected(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_COVERFLOW || obj->src == NULL) {
        return -1;
    }
    return ((gfx_coverflow_t *)obj->src)->selected_index;
}

uint16_t gfx_coverflow_get_item_count(gfx_object_t *obj)
{
    if (obj == NULL || obj->type != GFX_OBJ_TYPE_COVERFLOW || obj->src == NULL) {
        return 0;
    }
    return ((gfx_coverflow_t *)obj->src)->item_count;
}

gfx_err_t gfx_coverflow_set_font(gfx_object_t *obj, gfx_font_t font)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    return gfx_label_set_font_source(obj, &((gfx_coverflow_t *)obj->src)->label, font);
}

gfx_err_t gfx_coverflow_set_drag_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_coverflow_t *)obj->src)->drag_threshold = threshold;
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_page_threshold(gfx_object_t *obj, uint16_t threshold)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_coverflow_t *)obj->src)->page_threshold = threshold;
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_changed_cb(gfx_object_t *obj, gfx_coverflow_changed_cb_t cb, void *user_data)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_coverflow_t *)obj->src)->changed_cb = cb;
    ((gfx_coverflow_t *)obj->src)->changed_user_data = user_data;
    return GFX_OK;
}

#define GFX_COVERFLOW_SET_STYLE_FIELD(fn_name, field_name) \
    gfx_err_t fn_name(gfx_object_t *obj, gfx_color_t color) \
    { \
        CHECK_OBJ_TYPE_COVERFLOW(obj); \
        GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE); \
        ((gfx_coverflow_t *)obj->src)->style.field_name = color; \
        gfx_object_invalidate(obj); \
        return GFX_OK; \
    }

GFX_COVERFLOW_SET_STYLE_FIELD(gfx_coverflow_set_bg_color, bg_color)
GFX_COVERFLOW_SET_STYLE_FIELD(gfx_coverflow_set_center_color, center_color)
GFX_COVERFLOW_SET_STYLE_FIELD(gfx_coverflow_set_side_color, side_color)
GFX_COVERFLOW_SET_STYLE_FIELD(gfx_coverflow_set_text_color, text_color)
GFX_COVERFLOW_SET_STYLE_FIELD(gfx_coverflow_set_border_color, border_color)

gfx_err_t gfx_coverflow_set_border_width(gfx_object_t *obj, uint16_t width)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    ((gfx_coverflow_t *)obj->src)->style.border_width = width;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_zoom(gfx_object_t *obj, uint16_t center_zoom, uint16_t side_zoom)
{
    gfx_coverflow_t *flow;

    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(center_zoom > 0U && side_zoom > 0U && center_zoom >= side_zoom,
                        GFX_ERR_INVALID_ARG, TAG, "invalid zoom");

    flow = (gfx_coverflow_t *)obj->src;
    flow->style.center_zoom = center_zoom;
    flow->style.side_zoom = side_zoom;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_spacing(gfx_object_t *obj, uint16_t spacing_pct)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);
    GFX_RETURN_ON_FALSE(spacing_pct > 0U, GFX_ERR_INVALID_ARG, TAG, "invalid spacing");

    ((gfx_coverflow_t *)obj->src)->style.spacing_pct = spacing_pct;
    gfx_object_invalidate(obj);
    return GFX_OK;
}

gfx_err_t gfx_coverflow_set_side_dim(gfx_object_t *obj, gfx_opa_t opa)
{
    CHECK_OBJ_TYPE_COVERFLOW(obj);
    GFX_RETURN_IF_NULL(obj->src, GFX_ERR_INVALID_STATE);

    ((gfx_coverflow_t *)obj->src)->style.side_dim_opa = opa;
    gfx_object_invalidate(obj);
    return GFX_OK;
}
