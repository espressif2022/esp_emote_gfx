/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "common/gfx_comm.h"
#include "core/display/gfx_refr_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/stickman/gfx_stickman_emote_priv.h"

#define CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_STICKMAN_EMOTE, TAG)
#define GFX_STICKMAN_POINT_COUNT 12U

typedef struct {
    float x;
    float y;
} gfx_stickman_screen_point_t;

static const char *TAG = "stickman_emote";

static esp_err_t gfx_stickman_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_stickman_emote_delete_impl(gfx_obj_t *obj);
static void gfx_stickman_emote_anim_cb(void *user_data);

static const gfx_widget_class_t s_gfx_stickman_emote_widget_class = {
    .type = GFX_OBJ_TYPE_STICKMAN_EMOTE,
    .name = "stickman_emote",
    .draw = gfx_stickman_emote_draw,
    .delete = gfx_stickman_emote_delete_impl,
    .update = NULL,
    .touch_event = NULL,
};

static gfx_stickman_point_t *gfx_stickman_pose_points(gfx_stickman_pose_t *pose)
{
    return &pose->head_center;
}

static const gfx_stickman_point_t *gfx_stickman_pose_points_const(const gfx_stickman_pose_t *pose)
{
    return &pose->head_center;
}

static int16_t gfx_stickman_ease_value(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }

    if (div < 1) {
        div = 1;
    }

    step = diff / div;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }

    return (int16_t)(cur + step);
}

static void gfx_stickman_plot_pixel(const gfx_draw_ctx_t *ctx,
                                    const gfx_area_t *clip_area,
                                    int32_t x,
                                    int32_t y,
                                    uint16_t native_color)
{
    uint16_t *row;

    if (ctx == NULL) {
        return;
    }
    if (clip_area == NULL) {
        clip_area = &ctx->clip_area;
    }
    if (x < clip_area->x1 || x >= clip_area->x2 ||
            y < clip_area->y1 || y >= clip_area->y2) {
        return;
    }

    row = (uint16_t *)ctx->buf + (y - ctx->buf_area.y1) * ctx->stride + (x - ctx->buf_area.x1);
    *row = native_color;
}

static void gfx_stickman_draw_filled_circle(const gfx_draw_ctx_t *ctx,
                                            const gfx_area_t *clip_area,
                                            float cx,
                                            float cy,
                                            float radius,
                                            uint16_t native_color)
{
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    float radius_sq;

    if (ctx == NULL || radius <= 0.0f) {
        return;
    }

    x1 = (int32_t)floorf(cx - radius);
    y1 = (int32_t)floorf(cy - radius);
    x2 = (int32_t)ceilf(cx + radius);
    y2 = (int32_t)ceilf(cy + radius);
    radius_sq = radius * radius;

    for (int32_t y = y1; y <= y2; y++) {
        for (int32_t x = x1; x <= x2; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float dy = ((float)y + 0.5f) - cy;
            if ((dx * dx + dy * dy) <= radius_sq) {
                gfx_stickman_plot_pixel(ctx, clip_area, x, y, native_color);
            }
        }
    }
}

static void gfx_stickman_draw_circle_ring(const gfx_draw_ctx_t *ctx,
                                          const gfx_area_t *clip_area,
                                          float cx,
                                          float cy,
                                          float radius,
                                          float thickness,
                                          uint16_t native_color)
{
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    float outer_r;
    float inner_r;
    float outer_sq;
    float inner_sq;

    if (ctx == NULL || radius <= 0.0f || thickness <= 0.0f) {
        return;
    }

    outer_r = radius + (thickness * 0.5f);
    inner_r = radius - (thickness * 0.5f);
    if (inner_r < 0.0f) {
        inner_r = 0.0f;
    }
    outer_sq = outer_r * outer_r;
    inner_sq = inner_r * inner_r;
    x1 = (int32_t)floorf(cx - outer_r);
    y1 = (int32_t)floorf(cy - outer_r);
    x2 = (int32_t)ceilf(cx + outer_r);
    y2 = (int32_t)ceilf(cy + outer_r);

    for (int32_t y = y1; y <= y2; y++) {
        for (int32_t x = x1; x <= x2; x++) {
            float dx = ((float)x + 0.5f) - cx;
            float dy = ((float)y + 0.5f) - cy;
            float dist_sq = dx * dx + dy * dy;
            if (dist_sq <= outer_sq && dist_sq >= inner_sq) {
                gfx_stickman_plot_pixel(ctx, clip_area, x, y, native_color);
            }
        }
    }
}

static void gfx_stickman_draw_segment(const gfx_draw_ctx_t *ctx,
                                      const gfx_area_t *clip_area,
                                      gfx_color_t color,
                                      float thickness,
                                      const gfx_stickman_screen_point_t *a,
                                      const gfx_stickman_screen_point_t *b,
                                      uint16_t native_color)
{
    int32_t vx[4];
    int32_t vy[4];
    float dx;
    float dy;
    float len;
    float radius;
    float nx;
    float ny;

    if (ctx == NULL || a == NULL || b == NULL || thickness <= 0.0f) {
        return;
    }

    dx = b->x - a->x;
    dy = b->y - a->y;
    len = sqrtf(dx * dx + dy * dy);
    radius = thickness * 0.5f;

    if (len <= 0.001f) {
        gfx_stickman_draw_filled_circle(ctx, clip_area, a->x, a->y, radius, native_color);
        return;
    }

    nx = (-dy / len) * radius;
    ny = (dx / len) * radius;

    vx[0] = (int32_t)lroundf((a->x + nx) * 256.0f);
    vy[0] = (int32_t)lroundf((a->y + ny) * 256.0f);
    vx[1] = (int32_t)lroundf((b->x + nx) * 256.0f);
    vy[1] = (int32_t)lroundf((b->y + ny) * 256.0f);
    vx[2] = (int32_t)lroundf((b->x - nx) * 256.0f);
    vy[2] = (int32_t)lroundf((b->y - ny) * 256.0f);
    vx[3] = (int32_t)lroundf((a->x - nx) * 256.0f);
    vy[3] = (int32_t)lroundf((a->y - ny) * 256.0f);

    gfx_sw_blend_polygon_fill((gfx_color_t *)ctx->buf, ctx->stride,
                              &ctx->buf_area, clip_area,
                              color, vx, vy, 4, ctx->swap);
    gfx_stickman_draw_filled_circle(ctx, clip_area, a->x, a->y, radius, native_color);
    gfx_stickman_draw_filled_circle(ctx, clip_area, b->x, b->y, radius, native_color);
}

static void gfx_stickman_transform_point(const gfx_stickman_export_t *export_data,
                                         const gfx_stickman_point_t *src,
                                         gfx_coord_t obj_x,
                                         gfx_coord_t obj_y,
                                         uint16_t obj_w,
                                         uint16_t obj_h,
                                         gfx_stickman_screen_point_t *out)
{
    const gfx_parametric_export_meta_t *meta = export_data->meta;
    float scale;
    float render_w;
    float render_h;
    float offset_x;
    float offset_y;

    scale = fminf((float)obj_w / (float)meta->design_viewbox_w,
                  (float)obj_h / (float)meta->design_viewbox_h);
    render_w = (float)meta->design_viewbox_w * scale;
    render_h = (float)meta->design_viewbox_h * scale;
    offset_x = (float)obj_x + floorf(((float)obj_w - render_w) * 0.5f) - ((float)meta->design_viewbox_x * scale);
    offset_y = (float)obj_y + floorf(((float)obj_h - render_h) * 0.5f) - ((float)meta->design_viewbox_y * scale);

    out->x = offset_x + ((float)src->x * scale);
    out->y = offset_y + ((float)src->y * scale);
}

static float gfx_stickman_transform_scalar(const gfx_stickman_export_t *export_data,
                                           uint16_t obj_w,
                                           uint16_t obj_h,
                                           float value)
{
    const gfx_parametric_export_meta_t *meta = export_data->meta;
    float scale = fminf((float)obj_w / (float)meta->design_viewbox_w,
                        (float)obj_h / (float)meta->design_viewbox_h);
    return value * scale;
}

static void gfx_stickman_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_stickman_emote_t *stickman;

    if (obj == NULL || obj->src == NULL) {
        return;
    }

    stickman = (gfx_stickman_emote_t *)obj->src;
    gfx_stickman_emote_update_pose(obj, stickman);
}

gfx_obj_t *gfx_stickman_emote_create(gfx_disp_t *disp, uint16_t w, uint16_t h)
{
    gfx_obj_t *obj = NULL;
    gfx_stickman_emote_t *stickman = NULL;

    if (disp == NULL) {
        return NULL;
    }

    stickman = calloc(1, sizeof(*stickman));
    if (stickman == NULL) {
        return NULL;
    }

    stickman->cfg.display_w = w;
    stickman->cfg.display_h = h;
    stickman->cfg.timer_period_ms = GFX_STICKMAN_DEFAULT_TIMER_PERIOD_MS;
    stickman->cfg.damping_div = GFX_STICKMAN_DEFAULT_DAMPING_DIV;
    stickman->stroke_color = GFX_COLOR_HEX(0x111111);

    if (gfx_obj_create_class_instance(disp, &s_gfx_stickman_emote_widget_class,
                                      stickman, w, h, "gfx_stickman_emote_create", &obj) != ESP_OK) {
        free(stickman);
        return NULL;
    }

    stickman->anim_timer = gfx_timer_create(disp->ctx, gfx_stickman_emote_anim_cb,
                                            stickman->cfg.timer_period_ms, obj);
    return obj;
}

esp_err_t gfx_stickman_emote_set_export(gfx_obj_t *obj, const gfx_stickman_export_t *export_data)
{
    gfx_stickman_emote_t *stickman;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    ESP_RETURN_ON_ERROR(gfx_stickman_emote_validate_export(export_data), TAG, "invalid stickman export");

    stickman->export_data = export_data;
    stickman->cfg.timer_period_ms = (export_data->layout->timer_period_ms > 0U) ?
                                    export_data->layout->timer_period_ms :
                                    GFX_STICKMAN_DEFAULT_TIMER_PERIOD_MS;
    stickman->cfg.damping_div = (export_data->layout->damping_div > 0) ?
                                export_data->layout->damping_div :
                                GFX_STICKMAN_DEFAULT_DAMPING_DIV;
    if (stickman->anim_timer != NULL) {
        gfx_timer_set_period(stickman->anim_timer, stickman->cfg.timer_period_ms);
    }

    return gfx_stickman_emote_set_action_index(obj, 0U, true);
}

esp_err_t gfx_stickman_emote_set_action_name(gfx_obj_t *obj, const char *name, bool snap_now)
{
    gfx_stickman_emote_t *stickman;
    size_t action_index;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(stickman != NULL && stickman->export_data != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "stickman export is not ready");
    ESP_RETURN_ON_ERROR(gfx_stickman_emote_find_action_index(stickman->export_data, name, &action_index),
                        TAG, "action not found");
    return gfx_stickman_emote_set_action_index(obj, action_index, snap_now);
}

esp_err_t gfx_stickman_emote_set_action_index(gfx_obj_t *obj, size_t action_index, bool snap_now)
{
    gfx_stickman_emote_t *stickman;
    gfx_stickman_pose_t next_pose;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    ESP_RETURN_ON_FALSE(stickman != NULL && stickman->export_data != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "stickman export is not ready");
    ESP_RETURN_ON_ERROR(gfx_stickman_emote_resolve_action_pose(stickman->export_data, action_index, &next_pose),
                        TAG, "resolve action pose failed");

    stickman->pose_tgt = next_pose;
    if (snap_now) {
        stickman->pose_cur = next_pose;
    }
    stickman->action_idx = action_index;

    if (snap_now) {
        gfx_obj_invalidate(obj);
        return ESP_OK;
    }

    gfx_stickman_emote_update_pose(obj, stickman);
    return ESP_OK;
}

esp_err_t gfx_stickman_emote_set_stroke_color(gfx_obj_t *obj, gfx_color_t color)
{
    gfx_stickman_emote_t *stickman;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    stickman->stroke_color = color;
    gfx_obj_invalidate(obj);
    return ESP_OK;
}

esp_err_t gfx_stickman_emote_validate_export(const gfx_stickman_export_t *export_data)
{
    ESP_RETURN_ON_FALSE(export_data != NULL, ESP_ERR_INVALID_ARG, TAG, "export is NULL");
    ESP_RETURN_ON_FALSE(export_data->meta != NULL, ESP_ERR_INVALID_ARG, TAG, "export meta is NULL");
    ESP_RETURN_ON_FALSE(export_data->layout != NULL, ESP_ERR_INVALID_ARG, TAG, "layout is NULL");
    ESP_RETURN_ON_FALSE(export_data->poses != NULL && export_data->pose_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "poses are empty");
    ESP_RETURN_ON_FALSE(export_data->actions != NULL && export_data->action_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "actions are empty");
    ESP_RETURN_ON_FALSE(export_data->meta->version == GFX_STICKMAN_EXPORT_SCHEMA_VERSION,
                        ESP_ERR_INVALID_ARG, TAG, "schema version mismatch");
    ESP_RETURN_ON_FALSE(export_data->meta->design_viewbox_w > 0 && export_data->meta->design_viewbox_h > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid design viewbox");
    ESP_RETURN_ON_FALSE(export_data->layout->head_radius > 0 && export_data->layout->stroke_width > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid stickman layout");

    for (size_t i = 0; i < export_data->action_count; i++) {
        const gfx_stickman_action_t *action = &export_data->actions[i];
        ESP_RETURN_ON_FALSE(action->name != NULL, ESP_ERR_INVALID_ARG, TAG, "action name missing");
        ESP_RETURN_ON_FALSE(action->pose_index < export_data->pose_count,
                            ESP_ERR_INVALID_ARG, TAG, "action pose index out of range");
    }

    if (export_data->sequence != NULL) {
        for (size_t i = 0; i < export_data->sequence_count; i++) {
            ESP_RETURN_ON_FALSE(export_data->sequence[i] < export_data->action_count,
                                ESP_ERR_INVALID_ARG, TAG, "sequence action index out of range");
        }
    }

    return ESP_OK;
}

esp_err_t gfx_stickman_emote_find_action_index(const gfx_stickman_export_t *export_data,
                                               const char *name,
                                               size_t *index_out)
{
    ESP_RETURN_ON_FALSE(export_data != NULL && name != NULL && index_out != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid args");

    for (size_t i = 0; i < export_data->action_count; i++) {
        if (strcmp(export_data->actions[i].name, name) == 0) {
            *index_out = i;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_stickman_emote_resolve_action_pose(const gfx_stickman_export_t *export_data,
                                                 size_t action_index,
                                                 gfx_stickman_pose_t *pose_out)
{
    const gfx_stickman_action_t *action;
    const gfx_stickman_pose_t *src_pose;
    const gfx_stickman_layout_t *layout;
    const gfx_stickman_point_t *src_points;
    gfx_stickman_point_t *dst_points;

    ESP_RETURN_ON_FALSE(export_data != NULL && pose_out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(action_index < export_data->action_count, ESP_ERR_INVALID_ARG, TAG, "action index out of range");

    action = &export_data->actions[action_index];
    src_pose = &export_data->poses[action->pose_index];
    layout = export_data->layout;
    src_points = gfx_stickman_pose_points_const(src_pose);
    dst_points = gfx_stickman_pose_points(pose_out);

    for (size_t i = 0; i < GFX_STICKMAN_POINT_COUNT; i++) {
        dst_points[i] = src_points[i];
        if (action->facing < 0) {
            dst_points[i].x = (int16_t)((layout->mirror_x * 2) - src_points[i].x);
        }
    }

    return ESP_OK;
}

bool gfx_stickman_emote_update_pose(gfx_obj_t *obj, gfx_stickman_emote_t *stickman)
{
    gfx_stickman_point_t *cur_points;
    const gfx_stickman_point_t *tgt_points;
    bool changed = false;
    int16_t div;

    if (obj == NULL || stickman == NULL || stickman->export_data == NULL) {
        return false;
    }

    cur_points = gfx_stickman_pose_points(&stickman->pose_cur);
    tgt_points = gfx_stickman_pose_points_const(&stickman->pose_tgt);
    div = (stickman->cfg.damping_div > 0) ? stickman->cfg.damping_div : GFX_STICKMAN_DEFAULT_DAMPING_DIV;

    for (size_t i = 0; i < GFX_STICKMAN_POINT_COUNT; i++) {
        int16_t next_x = gfx_stickman_ease_value(cur_points[i].x, tgt_points[i].x, div);
        int16_t next_y = gfx_stickman_ease_value(cur_points[i].y, tgt_points[i].y, div);
        if (next_x != cur_points[i].x || next_y != cur_points[i].y) {
            changed = true;
            cur_points[i].x = next_x;
            cur_points[i].y = next_y;
        }
    }

    if (changed) {
        gfx_obj_invalidate(obj);
    }
    return changed;
}

static esp_err_t gfx_stickman_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    gfx_stickman_emote_t *stickman;
    const gfx_stickman_export_t *export_data;
    const gfx_stickman_layout_t *layout;
    const gfx_stickman_pose_t *pose;
    const gfx_stickman_point_t *pts;
    gfx_stickman_screen_point_t screen_pts[GFX_STICKMAN_POINT_COUNT];
    gfx_area_t obj_area;
    gfx_area_t clip_area;
    uint16_t native_color;
    float stroke_width;
    float head_radius;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    GFX_RETURN_IF_NULL(ctx, ESP_ERR_INVALID_ARG);

    stickman = (gfx_stickman_emote_t *)obj->src;
    GFX_RETURN_IF_NULL(stickman, ESP_ERR_INVALID_STATE);
    export_data = stickman->export_data;
    if (export_data == NULL) {
        return ESP_OK;
    }

    gfx_obj_calc_pos_in_parent(obj);
    obj_area.x1 = obj->geometry.x;
    obj_area.y1 = obj->geometry.y;
    obj_area.x2 = obj->geometry.x + obj->geometry.width;
    obj_area.y2 = obj->geometry.y + obj->geometry.height;
    if (!gfx_area_intersect_exclusive(&clip_area, &ctx->clip_area, &obj_area)) {
        return ESP_OK;
    }

    layout = export_data->layout;
    pose = &stickman->pose_cur;
    pts = gfx_stickman_pose_points_const(pose);
    native_color = gfx_color_to_native_u16(stickman->stroke_color, ctx->swap);
    stroke_width = gfx_stickman_transform_scalar(export_data, obj->geometry.width, obj->geometry.height,
                                                 (float)layout->stroke_width);
    if (stroke_width < 1.0f) {
        stroke_width = 1.0f;
    }
    head_radius = gfx_stickman_transform_scalar(export_data, obj->geometry.width, obj->geometry.height,
                                                (float)layout->head_radius);
    if (head_radius < stroke_width) {
        head_radius = stroke_width;
    }

    for (size_t i = 0; i < GFX_STICKMAN_POINT_COUNT; i++) {
        gfx_stickman_transform_point(export_data, &pts[i], obj->geometry.x, obj->geometry.y,
                                     obj->geometry.width, obj->geometry.height, &screen_pts[i]);
    }

    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[1], &screen_pts[2], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[2], &screen_pts[3], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[2], &screen_pts[4], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[4], &screen_pts[5], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[2], &screen_pts[6], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[6], &screen_pts[7], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[3], &screen_pts[8], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[8], &screen_pts[9], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[3], &screen_pts[10], native_color);
    gfx_stickman_draw_segment(ctx, &clip_area, stickman->stroke_color, stroke_width, &screen_pts[10], &screen_pts[11], native_color);
    gfx_stickman_draw_circle_ring(ctx, &clip_area, screen_pts[0].x, screen_pts[0].y, head_radius, stroke_width, native_color);

    obj->state.dirty = false;
    return ESP_OK;
}

static esp_err_t gfx_stickman_emote_delete_impl(gfx_obj_t *obj)
{
    gfx_stickman_emote_t *stickman;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    if (stickman != NULL) {
        if (stickman->anim_timer != NULL) {
            gfx_timer_delete(obj->disp->ctx, stickman->anim_timer);
        }
        free(stickman);
        obj->src = NULL;
    }
    return ESP_OK;
}
