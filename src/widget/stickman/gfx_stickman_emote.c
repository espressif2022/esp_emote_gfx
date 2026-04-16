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
#include "core/display/gfx_disp_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "widget/stickman/gfx_stickman_emote_priv.h"

#define CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj) CHECK_OBJ_TYPE(obj, GFX_OBJ_TYPE_STICKMAN_EMOTE, TAG)
#define GFX_STICKMAN_POINT_COUNT 12U
#define GFX_STICKMAN_CIRCLE_SEGS_MIN 16U
#define GFX_STICKMAN_CIRCLE_SEGS_MAX 48U

typedef struct {
    int32_t x;
    int32_t y;
} gfx_stickman_screen_point_t;

typedef enum {
    GFX_STICKMAN_SEG_NECK = 0,
    GFX_STICKMAN_SEG_SPINE,
    GFX_STICKMAN_SEG_ARM_L_UPPER,
    GFX_STICKMAN_SEG_ARM_L_LOWER,
    GFX_STICKMAN_SEG_ARM_R_UPPER,
    GFX_STICKMAN_SEG_ARM_R_LOWER,
    GFX_STICKMAN_SEG_LEG_L_UPPER,
    GFX_STICKMAN_SEG_LEG_L_LOWER,
    GFX_STICKMAN_SEG_LEG_R_UPPER,
    GFX_STICKMAN_SEG_LEG_R_LOWER,
} gfx_stickman_segment_id_t;

static const char *TAG = "stickman_emote";

static const uint16_t s_default_pixel = 0x001F;
static const gfx_image_dsc_t s_default_img = {
    .header = {
        .magic = 0x19,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = 2,
    .data = (const uint8_t *)&s_default_pixel,
};

static const uint8_t s_segment_point_pairs[GFX_STICKMAN_SEGMENT_COUNT][2] = {
    [GFX_STICKMAN_SEG_NECK] = { 1, 2 },
    [GFX_STICKMAN_SEG_SPINE] = { 2, 3 },
    [GFX_STICKMAN_SEG_ARM_L_UPPER] = { 2, 4 },
    [GFX_STICKMAN_SEG_ARM_L_LOWER] = { 4, 5 },
    [GFX_STICKMAN_SEG_ARM_R_UPPER] = { 2, 6 },
    [GFX_STICKMAN_SEG_ARM_R_LOWER] = { 6, 7 },
    [GFX_STICKMAN_SEG_LEG_L_UPPER] = { 3, 8 },
    [GFX_STICKMAN_SEG_LEG_L_LOWER] = { 8, 9 },
    [GFX_STICKMAN_SEG_LEG_R_UPPER] = { 3, 10 },
    [GFX_STICKMAN_SEG_LEG_R_LOWER] = { 10, 11 },
};

static esp_err_t gfx_stickman_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx);
static esp_err_t gfx_stickman_emote_delete_impl(gfx_obj_t *obj);
static esp_err_t gfx_stickman_emote_update_impl(gfx_obj_t *obj);
static esp_err_t gfx_stickman_emote_apply_color(gfx_stickman_emote_t *stickman);
static esp_err_t gfx_stickman_apply_segment_mesh(gfx_obj_t *mesh_obj,
                                                 const gfx_stickman_screen_point_t *a,
                                                 const gfx_stickman_screen_point_t *b,
                                                 int32_t thickness_px);
static esp_err_t gfx_stickman_apply_head_ring_mesh(gfx_obj_t *mesh_obj,
                                                   const gfx_stickman_screen_point_t *center,
                                                   int32_t radius_px,
                                                   int32_t thickness_px,
                                                   uint8_t segs);

static const gfx_widget_class_t s_gfx_stickman_emote_widget_class = {
    .type = GFX_OBJ_TYPE_STICKMAN_EMOTE,
    .name = "stickman_emote",
    .draw = gfx_stickman_emote_draw,
    .delete = gfx_stickman_emote_delete_impl,
    .update = gfx_stickman_emote_update_impl,
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

static uint8_t gfx_stickman_circle_segments_for_radius(float radius)
{
    int32_t segs = (int32_t)lroundf(radius * 2.0f);

    if (segs < (int32_t)GFX_STICKMAN_CIRCLE_SEGS_MIN) {
        segs = GFX_STICKMAN_CIRCLE_SEGS_MIN;
    }
    if (segs > (int32_t)GFX_STICKMAN_CIRCLE_SEGS_MAX) {
        segs = GFX_STICKMAN_CIRCLE_SEGS_MAX;
    }
    return (uint8_t)segs;
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

    out->x = (int32_t)lroundf(offset_x + ((float)src->x * scale));
    out->y = (int32_t)lroundf(offset_y + ((float)src->y * scale));
}

static int32_t gfx_stickman_transform_scalar_px(const gfx_stickman_export_t *export_data,
                                                uint16_t obj_w,
                                                uint16_t obj_h,
                                                float value)
{
    const gfx_parametric_export_meta_t *meta = export_data->meta;
    float scale = fminf((float)obj_w / (float)meta->design_viewbox_w,
                        (float)obj_h / (float)meta->design_viewbox_h);
    int32_t px = (int32_t)lroundf(value * scale);

    return (px < 1) ? 1 : px;
}

static esp_err_t gfx_stickman_emote_apply_color(gfx_stickman_emote_t *stickman)
{
    gfx_img_src_t solid_src;
    bool swap;

    ESP_RETURN_ON_FALSE(stickman != NULL, ESP_ERR_INVALID_ARG, TAG, "stickman state is NULL");
    ESP_RETURN_ON_FALSE(stickman->head_obj != NULL && stickman->head_obj->disp != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "stickman display is not ready");

    swap = stickman->head_obj->disp->flags.swap;
    stickman->solid_pixel = gfx_color_to_native_u16(stickman->stroke_color, swap);
    stickman->solid_img.data = (const uint8_t *)&stickman->solid_pixel;

    solid_src.type = GFX_IMG_SRC_TYPE_IMAGE_DSC;
    solid_src.data = &stickman->solid_img;

    for (size_t i = 0; i < GFX_STICKMAN_SEGMENT_COUNT; i++) {
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(stickman->segment_objs[i], &solid_src),
                            TAG, "set stickman segment color failed");
    }
    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(stickman->head_obj, &solid_src),
                        TAG, "set stickman head color failed");
    return ESP_OK;
}

static esp_err_t gfx_stickman_apply_segment_mesh(gfx_obj_t *mesh_obj,
                                                 const gfx_stickman_screen_point_t *a,
                                                 const gfx_stickman_screen_point_t *b,
                                                 int32_t thickness_px)
{
    gfx_mesh_img_point_q8_t mesh_pts[4];
    float ax;
    float ay;
    float bx;
    float by;
    float dx;
    float dy;
    float len;
    float half;
    float tx;
    float ty;
    float nx;
    float ny;
    int32_t px[4];
    int32_t py[4];
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;

    ESP_RETURN_ON_FALSE(mesh_obj != NULL && a != NULL && b != NULL, ESP_ERR_INVALID_ARG, TAG, "segment mesh args are invalid");

    ax = (float)a->x;
    ay = (float)a->y;
    bx = (float)b->x;
    by = (float)b->y;
    dx = bx - ax;
    dy = by - ay;
    len = sqrtf(dx * dx + dy * dy);
    half = (float)thickness_px * 0.5f;

    if (len <= 0.001f) {
        px[0] = (int32_t)lroundf(ax - half);
        py[0] = (int32_t)lroundf(ay - half);
        px[1] = (int32_t)lroundf(ax + half);
        py[1] = (int32_t)lroundf(ay - half);
        px[2] = (int32_t)lroundf(ax - half);
        py[2] = (int32_t)lroundf(ay + half);
        px[3] = (int32_t)lroundf(ax + half);
        py[3] = (int32_t)lroundf(ay + half);
    } else {
        tx = (dx / len) * half;
        ty = (dy / len) * half;
        nx = (-dy / len) * half;
        ny = (dx / len) * half;

        px[0] = (int32_t)lroundf(ax - tx + nx);
        py[0] = (int32_t)lroundf(ay - ty + ny);
        px[1] = (int32_t)lroundf(bx + tx + nx);
        py[1] = (int32_t)lroundf(by + ty + ny);
        px[2] = (int32_t)lroundf(ax - tx - nx);
        py[2] = (int32_t)lroundf(ay - ty - ny);
        px[3] = (int32_t)lroundf(bx + tx - nx);
        py[3] = (int32_t)lroundf(by + ty - ny);
    }

    min_x = max_x = px[0];
    min_y = max_y = py[0];
    for (size_t i = 1; i < 4; i++) {
        min_x = MIN(min_x, px[i]);
        min_y = MIN(min_y, py[i]);
        max_x = MAX(max_x, px[i]);
        max_y = MAX(max_y, py[i]);
    }

    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    mesh_pts[0].x_q8 = (px[0] - min_x) << 8;
    mesh_pts[0].y_q8 = (py[0] - min_y) << 8;
    mesh_pts[1].x_q8 = (px[1] - min_x) << 8;
    mesh_pts[1].y_q8 = (py[1] - min_y) << 8;
    mesh_pts[2].x_q8 = (px[2] - min_x) << 8;
    mesh_pts[2].y_q8 = (py[2] - min_y) << 8;
    mesh_pts[3].x_q8 = (px[3] - min_x) << 8;
    mesh_pts[3].y_q8 = (py[3] - min_y) << 8;

    ESP_RETURN_ON_ERROR(gfx_obj_align(mesh_obj, GFX_ALIGN_TOP_LEFT, (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "align stickman segment failed");
    return gfx_mesh_img_set_points_q8(mesh_obj, mesh_pts, 4U);
}

static esp_err_t gfx_stickman_apply_head_ring_mesh(gfx_obj_t *mesh_obj,
                                                   const gfx_stickman_screen_point_t *center,
                                                   int32_t radius_px,
                                                   int32_t thickness_px,
                                                   uint8_t segs)
{
    gfx_mesh_img_point_q8_t mesh_pts[(GFX_STICKMAN_CIRCLE_SEGS_MAX + 1U) * 2U];
    float outer_r;
    float inner_r;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;

    ESP_RETURN_ON_FALSE(mesh_obj != NULL && center != NULL, ESP_ERR_INVALID_ARG, TAG, "head ring args are invalid");
    ESP_RETURN_ON_FALSE(segs >= GFX_STICKMAN_CIRCLE_SEGS_MIN && segs <= GFX_STICKMAN_CIRCLE_SEGS_MAX,
                        ESP_ERR_INVALID_ARG, TAG, "invalid head ring segment count");

    outer_r = (float)radius_px + ((float)thickness_px * 0.5f);
    inner_r = (float)radius_px - ((float)thickness_px * 0.5f);
    if (inner_r < 1.0f) {
        inner_r = 1.0f;
    }

    min_x = INT32_MAX;
    min_y = INT32_MAX;
    max_x = INT32_MIN;
    max_y = INT32_MIN;

    for (uint8_t i = 0; i <= segs; i++) {
        float angle = ((float)i * 2.0f * (float)M_PI) / (float)segs;
        int32_t outer_x = (int32_t)lroundf((float)center->x + (cosf(angle) * outer_r));
        int32_t outer_y = (int32_t)lroundf((float)center->y + (sinf(angle) * outer_r));
        int32_t inner_x = (int32_t)lroundf((float)center->x + (cosf(angle) * inner_r));
        int32_t inner_y = (int32_t)lroundf((float)center->y + (sinf(angle) * inner_r));

        min_x = MIN(min_x, MIN(outer_x, inner_x));
        min_y = MIN(min_y, MIN(outer_y, inner_y));
        max_x = MAX(max_x, MAX(outer_x, inner_x));
        max_y = MAX(max_y, MAX(outer_y, inner_y));

        mesh_pts[i].x_q8 = outer_x << 8;
        mesh_pts[i].y_q8 = outer_y << 8;
        mesh_pts[segs + 1U + i].x_q8 = inner_x << 8;
        mesh_pts[segs + 1U + i].y_q8 = inner_y << 8;
    }

    if (max_x <= min_x) {
        max_x = min_x + 1;
    }
    if (max_y <= min_y) {
        max_y = min_y + 1;
    }

    for (size_t i = 0; i < ((size_t)segs + 1U) * 2U; i++) {
        mesh_pts[i].x_q8 -= min_x << 8;
        mesh_pts[i].y_q8 -= min_y << 8;
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(mesh_obj, GFX_ALIGN_TOP_LEFT, (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "align stickman head failed");
    return gfx_mesh_img_set_points_q8(mesh_obj, mesh_pts, ((size_t)segs + 1U) * 2U);
}

esp_err_t gfx_stickman_emote_sync_meshes(gfx_obj_t *obj, gfx_stickman_emote_t *stickman)
{
    const gfx_stickman_export_t *export_data;
    const gfx_stickman_layout_t *layout;
    const gfx_stickman_point_t *pts;
    gfx_stickman_screen_point_t screen_pts[GFX_STICKMAN_POINT_COUNT];
    bool root_changed;
    int32_t stroke_width_px;
    int32_t head_radius_px;

    ESP_RETURN_ON_FALSE(obj != NULL && stickman != NULL, ESP_ERR_INVALID_ARG, TAG, "sync stickman args are invalid");
    export_data = stickman->export_data;
    if (export_data == NULL) {
        return ESP_OK;
    }

    gfx_obj_calc_pos_in_parent(obj);
    root_changed = stickman->mesh_dirty ||
                   obj->geometry.x != stickman->last_root_x ||
                   obj->geometry.y != stickman->last_root_y ||
                   obj->geometry.width != stickman->last_root_w ||
                   obj->geometry.height != stickman->last_root_h;
    if (!root_changed) {
        return ESP_OK;
    }

    layout = export_data->layout;
    pts = gfx_stickman_pose_points_const(&stickman->pose_cur);
    stroke_width_px = gfx_stickman_transform_scalar_px(export_data, obj->geometry.width, obj->geometry.height,
                                                       (float)layout->stroke_width);
    head_radius_px = gfx_stickman_transform_scalar_px(export_data, obj->geometry.width, obj->geometry.height,
                                                      (float)layout->head_radius);
    if (head_radius_px < stroke_width_px) {
        head_radius_px = stroke_width_px;
    }
    if (stickman->head_segs == 0U) {
        stickman->head_segs = gfx_stickman_circle_segments_for_radius((float)layout->head_radius);
    }

    for (size_t i = 0; i < GFX_STICKMAN_POINT_COUNT; i++) {
        gfx_stickman_transform_point(export_data, &pts[i],
                                     obj->geometry.x, obj->geometry.y,
                                     obj->geometry.width, obj->geometry.height,
                                     &screen_pts[i]);
    }

    for (size_t i = 0; i < GFX_STICKMAN_SEGMENT_COUNT; i++) {
        uint8_t start_idx = s_segment_point_pairs[i][0];
        uint8_t end_idx = s_segment_point_pairs[i][1];

        ESP_RETURN_ON_ERROR(gfx_stickman_apply_segment_mesh(stickman->segment_objs[i],
                                                            &screen_pts[start_idx],
                                                            &screen_pts[end_idx],
                                                            stroke_width_px),
                            TAG, "apply stickman segment mesh failed");
        ESP_RETURN_ON_ERROR(gfx_obj_set_visible(stickman->segment_objs[i], true),
                            TAG, "show stickman segment failed");
    }

    ESP_RETURN_ON_ERROR(gfx_stickman_apply_head_ring_mesh(stickman->head_obj,
                                                          &screen_pts[0],
                                                          head_radius_px,
                                                          stroke_width_px,
                                                          stickman->head_segs),
                        TAG, "apply stickman head mesh failed");
    ESP_RETURN_ON_ERROR(gfx_obj_set_visible(stickman->head_obj, true),
                        TAG, "show stickman head failed");

    stickman->last_root_x = obj->geometry.x;
    stickman->last_root_y = obj->geometry.y;
    stickman->last_root_w = obj->geometry.width;
    stickman->last_root_h = obj->geometry.height;
    stickman->mesh_dirty = false;
    return ESP_OK;
}

void gfx_stickman_emote_anim_cb(void *user_data)
{
    gfx_obj_t *obj = (gfx_obj_t *)user_data;
    gfx_stickman_emote_t *stickman;

    if (obj == NULL || obj->src == NULL) {
        return;
    }

    stickman = (gfx_stickman_emote_t *)obj->src;
    gfx_stickman_emote_update_pose(obj, stickman);
}

static esp_err_t gfx_stickman_emote_draw(gfx_obj_t *obj, const gfx_draw_ctx_t *ctx)
{
    (void)obj;
    (void)ctx;
    return ESP_OK;
}

static esp_err_t gfx_stickman_emote_update_impl(gfx_obj_t *obj)
{
    gfx_stickman_emote_t *stickman;

    CHECK_OBJ_TYPE_STICKMAN_EMOTE(obj);
    stickman = (gfx_stickman_emote_t *)obj->src;
    if (stickman == NULL || stickman->export_data == NULL) {
        return ESP_OK;
    }

    return gfx_stickman_emote_sync_meshes(obj, stickman);
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
    stickman->solid_pixel = s_default_pixel;
    stickman->solid_img = s_default_img;
    stickman->solid_img.data = (const uint8_t *)&stickman->solid_pixel;
    stickman->mesh_dirty = true;

    if (gfx_obj_create_class_instance(disp, &s_gfx_stickman_emote_widget_class,
                                      stickman, w, h, "gfx_stickman_emote_create", &obj) != ESP_OK) {
        free(stickman);
        return NULL;
    }

    for (size_t i = 0; i < GFX_STICKMAN_SEGMENT_COUNT; i++) {
        stickman->segment_objs[i] = gfx_mesh_img_create(disp);
        if (stickman->segment_objs[i] == NULL) {
            gfx_obj_delete(obj);
            return NULL;
        }
        gfx_mesh_img_set_grid(stickman->segment_objs[i], 1U, 1U);
        gfx_mesh_img_set_aa_inward(stickman->segment_objs[i], true);
        gfx_obj_set_visible(stickman->segment_objs[i], false);
    }

    stickman->head_obj = gfx_mesh_img_create(disp);
    if (stickman->head_obj == NULL) {
        gfx_obj_delete(obj);
        return NULL;
    }
    stickman->head_segs = gfx_stickman_circle_segments_for_radius(20.0f);
    gfx_mesh_img_set_grid(stickman->head_obj, stickman->head_segs, 1U);
    gfx_mesh_img_set_aa_inward(stickman->head_obj, true);
    gfx_mesh_img_set_wrap_cols(stickman->head_obj, true);
    gfx_obj_set_visible(stickman->head_obj, false);

    if (gfx_stickman_emote_apply_color(stickman) != ESP_OK) {
        gfx_obj_delete(obj);
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
    stickman->head_segs = gfx_stickman_circle_segments_for_radius((float)export_data->layout->head_radius);
    stickman->mesh_dirty = true;

    if (stickman->head_obj != NULL) {
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_grid(stickman->head_obj, stickman->head_segs, 1U),
                            TAG, "set stickman head grid failed");
    }
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
        stickman->mesh_dirty = true;
    }
    stickman->action_idx = action_index;

    if (snap_now) {
        return gfx_stickman_emote_sync_meshes(obj, stickman);
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
    return gfx_stickman_emote_apply_color(stickman);
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
        stickman->mesh_dirty = true;
    }
    if (changed || stickman->mesh_dirty) {
        if (gfx_stickman_emote_sync_meshes(obj, stickman) != ESP_OK) {
            GFX_LOGW(TAG, "sync stickman meshes failed");
        }
    }
    return changed;
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
        if (stickman->head_obj != NULL) {
            gfx_obj_delete(stickman->head_obj);
        }
        for (size_t i = 0; i < GFX_STICKMAN_SEGMENT_COUNT; i++) {
            if (stickman->segment_objs[i] != NULL) {
                gfx_obj_delete(stickman->segment_objs[i]);
            }
        }
        free(stickman);
        obj->src = NULL;
    }
    return ESP_OK;
}
