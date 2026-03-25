/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_mesh";

/* Bulge (fisheye) */
#define TEST_MESH_BULGE_GRID_COLS        16U
#define TEST_MESH_BULGE_GRID_ROWS        12U
#define TEST_MESH_BULGE_POINT_COUNT      ((TEST_MESH_BULGE_GRID_COLS + 1U) * (TEST_MESH_BULGE_GRID_ROWS + 1U))
#define TEST_MESH_BULGE_FOCUS_LIMIT_X    96
#define TEST_MESH_BULGE_FOCUS_LIMIT_Y    72
#define TEST_MESH_BULGE_STRENGTH_LIMIT   140
#define TEST_MESH_BULGE_RADIUS           112
#define TEST_MESH_BULGE_INTENSITY_SCALE  320000

/* Drag deform */
#define TEST_MESH_DRAG_GRID_COLS         12U
#define TEST_MESH_DRAG_GRID_ROWS         8U
#define TEST_MESH_DRAG_DRAG_LIMIT_X      100
#define TEST_MESH_DRAG_DRAG_LIMIT_Y      100
#define TEST_MESH_DRAG_SHEAR_DIV         3000
#define TEST_MESH_DRAG_SQUEEZE_DIV       8000

#define TEST_MESH_TIMER_PERIOD_MS        33U
#define TEST_MESH_MAX_POINT_COUNT        TEST_MESH_BULGE_POINT_COUNT

typedef enum {
    TEST_MESH_MODE_BULGE = 0,
    TEST_MESH_MODE_DRAG,
} test_mesh_mode_t;

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_obj_t *title_label;
    gfx_obj_t *hint_label;
    gfx_obj_t *mode_btn;
    gfx_timer_handle_t anim_timer;
    test_mesh_mode_t mode;

    uint32_t grid_cols;
    uint32_t grid_rows;
    uint32_t point_count;
    uint32_t center_point_idx;

    gfx_mesh_img_point_t base_points[TEST_MESH_MAX_POINT_COUNT];

    int16_t mesh_center_x;
    int16_t mesh_center_y;
    int16_t press_x;
    int16_t press_y;

    int16_t target_focus_x;
    int16_t target_focus_y;
    int16_t current_focus_x;
    int16_t current_focus_y;
    int16_t target_strength;
    int16_t current_strength;

    int16_t current_drag_x;
    int16_t current_drag_y;
    int16_t target_drag_x;
    int16_t target_drag_y;

    bool dragging;
} test_mesh_scene_t;

static int16_t test_mesh_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static int16_t test_mesh_follow_axis_bulge(int16_t current, int16_t target)
{
    int32_t diff = (int32_t)target - current;
    int32_t step;

    if (diff == 0) {
        return current;
    }
    step = diff / 4;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(current + step);
}

static int16_t test_mesh_follow_axis_drag(int16_t current, int16_t target)
{
    int32_t diff = (int32_t)target - current;
    int32_t step;

    if (diff == 0) {
        return current;
    }
    step = diff / 3;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(current + step);
}

static int32_t test_mesh_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mesh_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int32_t test_mesh_edge_dense_coord(uint32_t idx, uint32_t segments, int32_t max_coord)
{
    uint64_t denom;
    uint64_t value_num;

    if (segments == 0U || max_coord <= 0) {
        return 0;
    }

    denom = (uint64_t)segments * (uint64_t)segments;
    if ((idx * 2U) <= segments) {
        value_num = 2ULL * (uint64_t)idx * (uint64_t)idx * (uint64_t)max_coord;
        return (int32_t)((value_num + denom / 2U) / denom);
    }

    idx = segments - idx;
    value_num = 2ULL * (uint64_t)idx * (uint64_t)idx * (uint64_t)max_coord;
    return max_coord - (int32_t)((value_num + denom / 2U) / denom);
}

static void test_mesh_apply_dense_edge_grid(test_mesh_scene_t *scene)
{
    gfx_mesh_img_point_t remapped[TEST_MESH_MAX_POINT_COUNT];
    int32_t max_x = 0;
    int32_t max_y = 0;
    size_t idx = 0;
    uint32_t rows = scene->grid_rows;
    uint32_t cols = scene->grid_cols;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);

    for (size_t i = 0; i < scene->point_count; i++) {
        gfx_mesh_img_point_t point;
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &point));
        if (point.x > max_x) {
            max_x = point.x;
        }
        if (point.y > max_y) {
            max_y = point.y;
        }
    }

    for (uint32_t row = 0; row <= rows; row++) {
        int32_t y = test_mesh_edge_dense_coord(row, rows, max_y);
        for (uint32_t col = 0; col <= cols; col++) {
            int32_t x = test_mesh_edge_dense_coord(col, cols, max_x);
            remapped[idx].x = (gfx_coord_t)x;
            remapped[idx].y = (gfx_coord_t)y;
            idx++;
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_rest_points(scene->mesh_obj, remapped, scene->point_count));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_reset_points(scene->mesh_obj));
}

static void test_mesh_capture_base_points(test_mesh_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(scene->point_count, gfx_mesh_img_get_point_count(scene->mesh_obj));

    for (size_t i = 0; i < scene->point_count; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &scene->base_points[i]));
    }

    scene->mesh_center_x = scene->base_points[scene->center_point_idx].x;
    scene->mesh_center_y = scene->base_points[scene->center_point_idx].y;
}

static esp_err_t test_mesh_apply_bulge_pose(test_mesh_scene_t *scene, int16_t focus_ofs_x, int16_t focus_ofs_y, int16_t strength)
{
    int32_t focus_x;
    int32_t focus_y;
    gfx_mesh_img_point_t pose_points[TEST_MESH_MAX_POINT_COUNT];
    const int32_t radius2 = TEST_MESH_BULGE_RADIUS * TEST_MESH_BULGE_RADIUS;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "bulge pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "bulge pose: object is NULL");

    focus_x = (int32_t)scene->mesh_center_x + focus_ofs_x;
    focus_y = (int32_t)scene->mesh_center_y + focus_ofs_y;

    for (size_t i = 0; i < scene->point_count; i++) {
        int32_t base_x = scene->base_points[i].x;
        int32_t base_y = scene->base_points[i].y;
        int32_t rel_x = base_x - focus_x;
        int32_t rel_y = base_y - focus_y;
        int32_t dist2 = rel_x * rel_x + rel_y * rel_y;
        int32_t influence = 0;
        int32_t delta_x = 0;
        int32_t delta_y = 0;

        if (dist2 < radius2) {
            influence = (int32_t)(((int64_t)(radius2 - dist2) * 1000) / radius2);
            delta_x = (int32_t)(((int64_t)rel_x * strength * influence) / TEST_MESH_BULGE_INTENSITY_SCALE);
            delta_y = (int32_t)(((int64_t)rel_y * strength * influence) / TEST_MESH_BULGE_INTENSITY_SCALE);
        }

        pose_points[i].x = (gfx_coord_t)(base_x + delta_x);
        pose_points[i].y = (gfx_coord_t)(base_y + delta_y);
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_points(scene->mesh_obj, pose_points, scene->point_count), TAG, "bulge pose: set points failed");
    return ESP_OK;
}

static esp_err_t test_mesh_apply_drag_pose(test_mesh_scene_t *scene, int16_t drag_x, int16_t drag_y)
{
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;
    int32_t drag_mag;
    gfx_mesh_img_point_t pose_points[TEST_MESH_MAX_POINT_COUNT];

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "drag pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "drag pose: object is NULL");

    center_x = scene->base_points[scene->center_point_idx].x;
    center_y = scene->base_points[scene->center_point_idx].y;

    for (size_t i = 0; i < scene->point_count; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;

        max_abs_x = test_mesh_max_i32(max_abs_x, test_mesh_abs_i32(rel_x));
        max_abs_y = test_mesh_max_i32(max_abs_y, test_mesh_abs_i32(rel_y));
    }

    drag_mag = test_mesh_abs_i32(drag_x) + test_mesh_abs_i32(drag_y);

    for (size_t i = 0; i < scene->point_count; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;
        int32_t nx = (rel_x * 1000) / max_abs_x;
        int32_t ny = (rel_y * 1000) / max_abs_y;

        int32_t soft_x = 1000 - (nx * nx) / 1000;
        int32_t soft_y = 1000 - (ny * ny) / 1000;
        int32_t softness = (soft_x * soft_y) / 1000;

        int32_t dx = ((int32_t)drag_x * softness) / TEST_MESH_DRAG_SHEAR_DIV;
        int32_t dy = ((int32_t)drag_y * softness) / TEST_MESH_DRAG_SHEAR_DIV;

        if (drag_mag > 0) {
            int32_t alignment = (nx * (int32_t)drag_x + ny * (int32_t)drag_y) / drag_mag;
            int32_t squeeze = (-alignment * softness) / 1000;
            dx += (squeeze * (int32_t)drag_x) / TEST_MESH_DRAG_SQUEEZE_DIV;
            dy += (squeeze * (int32_t)drag_y) / TEST_MESH_DRAG_SQUEEZE_DIV;
        }

        pose_points[i].x = (gfx_coord_t)(scene->base_points[i].x + dx);
        pose_points[i].y = (gfx_coord_t)(scene->base_points[i].y + dy);
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_points(scene->mesh_obj, pose_points, scene->point_count), TAG, "drag pose: set points failed");
    gfx_obj_align(scene->mesh_obj, GFX_ALIGN_CENTER, drag_x, drag_y);
    return ESP_OK;
}

static void test_mesh_reset_interaction_state(test_mesh_scene_t *scene)
{
    scene->dragging = false;
    scene->press_x = 0;
    scene->press_y = 0;

    scene->target_focus_x = 0;
    scene->target_focus_y = 0;
    scene->current_focus_x = 0;
    scene->current_focus_y = 0;
    scene->target_strength = 0;
    scene->current_strength = 0;

    scene->current_drag_x = 0;
    scene->current_drag_y = 0;
    scene->target_drag_x = 0;
    scene->target_drag_y = 0;
}

static void test_mesh_touch_bulge(gfx_obj_t *obj, const gfx_touch_event_t *event, test_mesh_scene_t *scene)
{
    int16_t disp_center_x;
    int16_t disp_center_y;

    (void)obj;

    disp_center_x = (int16_t)(gfx_disp_get_hor_res(disp_default) / 2);
    disp_center_y = (int16_t)(gfx_disp_get_ver_res(disp_default) / 2);

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        scene->dragging = true;
        scene->press_x = (int16_t)event->x;
        scene->press_y = (int16_t)event->y;
        scene->target_focus_x = test_mesh_clamp_i16((int32_t)event->x - disp_center_x,
                                                    -TEST_MESH_BULGE_FOCUS_LIMIT_X, TEST_MESH_BULGE_FOCUS_LIMIT_X);
        scene->target_focus_y = test_mesh_clamp_i16((int32_t)event->y - disp_center_y,
                                                    -TEST_MESH_BULGE_FOCUS_LIMIT_Y, TEST_MESH_BULGE_FOCUS_LIMIT_Y);
        scene->target_strength = 0;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && scene->dragging) {
        scene->target_focus_x = test_mesh_clamp_i16((int32_t)event->x - disp_center_x,
                                                    -TEST_MESH_BULGE_FOCUS_LIMIT_X, TEST_MESH_BULGE_FOCUS_LIMIT_X);
        scene->target_focus_y = test_mesh_clamp_i16((int32_t)event->y - disp_center_y,
                                                    -TEST_MESH_BULGE_FOCUS_LIMIT_Y, TEST_MESH_BULGE_FOCUS_LIMIT_Y);
        scene->target_strength = test_mesh_clamp_i16((int32_t)scene->press_y - event->y,
                                                     -TEST_MESH_BULGE_STRENGTH_LIMIT, TEST_MESH_BULGE_STRENGTH_LIMIT);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->dragging = false;
        scene->target_focus_x = 0;
        scene->target_focus_y = 0;
        scene->target_strength = 0;
    }
}

static void test_mesh_touch_drag(gfx_obj_t *obj, const gfx_touch_event_t *event, test_mesh_scene_t *scene)
{
    (void)obj;

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        scene->dragging = true;
        scene->press_x = (int16_t)event->x;
        scene->press_y = (int16_t)event->y;
        scene->target_drag_x = 0;
        scene->target_drag_y = 0;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && scene->dragging) {
        scene->target_drag_x = test_mesh_clamp_i16((int32_t)event->x - scene->press_x,
                                                   -TEST_MESH_DRAG_DRAG_LIMIT_X, TEST_MESH_DRAG_DRAG_LIMIT_X);
        scene->target_drag_y = test_mesh_clamp_i16((int32_t)event->y - scene->press_y,
                                                   -TEST_MESH_DRAG_DRAG_LIMIT_Y, TEST_MESH_DRAG_DRAG_LIMIT_Y);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->dragging = false;
        scene->target_drag_x = 0;
        scene->target_drag_y = 0;
    }
}

static void test_mesh_mesh_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_scene_t *scene = (test_mesh_scene_t *)user_data;

    if (scene == NULL || event == NULL || scene->mesh_obj == NULL) {
        return;
    }

    if (scene->mode == TEST_MESH_MODE_BULGE) {
        test_mesh_touch_bulge(obj, event, scene);
    } else {
        test_mesh_touch_drag(obj, event, scene);
    }
}

static void test_mesh_anim_cb(void *user_data)
{
    test_mesh_scene_t *scene = (test_mesh_scene_t *)user_data;

    if (scene == NULL || scene->mesh_obj == NULL) {
        return;
    }

    if (scene->mode == TEST_MESH_MODE_BULGE) {
        int16_t next_focus_x = test_mesh_follow_axis_bulge(scene->current_focus_x, scene->target_focus_x);
        int16_t next_focus_y = test_mesh_follow_axis_bulge(scene->current_focus_y, scene->target_focus_y);
        int16_t next_strength = test_mesh_follow_axis_bulge(scene->current_strength, scene->target_strength);

        if (next_focus_x == scene->current_focus_x && next_focus_y == scene->current_focus_y &&
                next_strength == scene->current_strength) {
            return;
        }

        scene->current_focus_x = next_focus_x;
        scene->current_focus_y = next_focus_y;
        scene->current_strength = next_strength;
        test_mesh_apply_bulge_pose(scene, next_focus_x, next_focus_y, next_strength);

        if (scene->hint_label != NULL) {
            gfx_label_set_text_fmt(scene->hint_label, " strength:%d ", (int)next_strength);
        }
    } else {
        int16_t next_drag_x = test_mesh_follow_axis_drag(scene->current_drag_x, scene->target_drag_x);
        int16_t next_drag_y = test_mesh_follow_axis_drag(scene->current_drag_y, scene->target_drag_y);

        if (!scene->dragging && test_mesh_abs_i32(next_drag_x) <= 1 && test_mesh_abs_i32(next_drag_y) <= 1 &&
                scene->target_drag_x == 0 && scene->target_drag_y == 0) {
            next_drag_x = 0;
            next_drag_y = 0;
        }

        if (next_drag_x == scene->current_drag_x && next_drag_y == scene->current_drag_y) {
            return;
        }

        scene->current_drag_x = next_drag_x;
        scene->current_drag_y = next_drag_y;
        test_mesh_apply_drag_pose(scene, next_drag_x, next_drag_y);
    }
}

static void test_mesh_update_mode_ui(test_mesh_scene_t *scene)
{
    if (scene->mode == TEST_MESH_MODE_BULGE) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene->title_label, " Bulge "));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene->hint_label, " strength:0 "));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(scene->mode_btn, "Mode: Drag"));
    } else {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene->title_label, " Drag "));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene->hint_label, " drag to pull + stretch "));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(scene->mode_btn, "Mode: Bulge"));
    }
}

static void test_mesh_mode_btn_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data);

static void test_mesh_rebuild_mesh(test_mesh_scene_t *scene, test_mesh_mode_t new_mode)
{
    TEST_ASSERT_NOT_NULL(scene);

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }

    if (scene->mesh_obj != NULL) {
        gfx_obj_delete(scene->mesh_obj);
        scene->mesh_obj = NULL;
    }

    if (new_mode == TEST_MESH_MODE_BULGE) {
        scene->grid_cols = TEST_MESH_BULGE_GRID_COLS;
        scene->grid_rows = TEST_MESH_BULGE_GRID_ROWS;
    } else {
        scene->grid_cols = TEST_MESH_DRAG_GRID_COLS;
        scene->grid_rows = TEST_MESH_DRAG_GRID_ROWS;
    }

    scene->mode = new_mode;
    scene->point_count = (scene->grid_cols + 1U) * (scene->grid_rows + 1U);
    scene->center_point_idx = ((scene->grid_rows / 2U) * (scene->grid_cols + 1U)) + (scene->grid_cols / 2U);

    test_mesh_reset_interaction_state(scene);

    scene->mesh_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene->mesh_obj, (void *)&simple_face));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene->mesh_obj, scene->grid_cols, scene->grid_rows));
    test_mesh_apply_dense_edge_grid(scene);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene->mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene->mesh_obj, test_mesh_mesh_touch_cb, scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene->mesh_obj, GFX_ALIGN_CENTER, 0, 0));

    test_mesh_capture_base_points(scene);

    if (scene->mode == TEST_MESH_MODE_BULGE) {
        TEST_ASSERT_EQUAL(ESP_OK, test_mesh_apply_bulge_pose(scene, 0, 0, 0));
    } else {
        TEST_ASSERT_EQUAL(ESP_OK, test_mesh_apply_drag_pose(scene, 0, 0));
    }

    test_mesh_update_mode_ui(scene);

    scene->anim_timer = gfx_timer_create(emote_handle, test_mesh_anim_cb, TEST_MESH_TIMER_PERIOD_MS, scene);
    TEST_ASSERT_NOT_NULL(scene->anim_timer);
}

static void test_mesh_mode_btn_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_scene_t *scene = (test_mesh_scene_t *)user_data;

    (void)obj;

    if (scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    test_mesh_mode_t next = (scene->mode == TEST_MESH_MODE_BULGE) ? TEST_MESH_MODE_DRAG : TEST_MESH_MODE_BULGE;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_rebuild_mesh(scene, next);
    test_app_unlock();

    ESP_LOGI(TAG, "Switched mode to %s", (next == TEST_MESH_MODE_BULGE) ? "Bulge" : "Drag");
}

static void test_mesh_scene_cleanup(test_mesh_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }

    if (scene->mesh_obj != NULL) {
        gfx_obj_delete(scene->mesh_obj);
        scene->mesh_obj = NULL;
    }
    if (scene->title_label != NULL) {
        gfx_obj_delete(scene->title_label);
        scene->title_label = NULL;
    }
    if (scene->hint_label != NULL) {
        gfx_obj_delete(scene->hint_label);
        scene->hint_label = NULL;
    }
    if (scene->mode_btn != NULL) {
        gfx_obj_delete(scene->mode_btn);
        scene->mode_btn = NULL;
    }
}

static void test_mesh_bulge_drag_run(void)
{
    test_mesh_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh bulge / drag (mode button)");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x02040A));

    scene.title_label = gfx_label_create(disp_default);
    scene.hint_label = gfx_label_create(disp_default);
    scene.mode_btn = gfx_button_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.hint_label);
    TEST_ASSERT_NOT_NULL(scene.mode_btn);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 200, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, -70, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.mode_btn, 128, 36));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mode_btn, GFX_ALIGN_TOP_MID, 90, 6));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_font(scene.mode_btn, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(scene.mode_btn, GFX_COLOR_HEX(0x2A6DF4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(scene.mode_btn, GFX_COLOR_HEX(0x163D87)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(scene.mode_btn, GFX_COLOR_HEX(0xDCE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_width(scene.mode_btn, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.mode_btn, test_mesh_mode_btn_cb, &scene));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 280, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));

    test_mesh_rebuild_mesh(&scene, TEST_MESH_MODE_BULGE);
    test_app_unlock();

    test_app_log_step(TAG, "Bulge: move center, drag up/down; Drag: pull/stretch. Tap Mode to switch.");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_scene_cleanup(&scene);
    test_app_unlock();
}

void test_mesh_bulge_drag_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_mesh_bulge_drag_run();
    test_app_runtime_close(&runtime);
}

void test_mesh_bulge_run_case(void)
{
    test_mesh_bulge_drag_run_case();
}

void test_mesh_drag_run_case(void)
{
    test_mesh_bulge_drag_run_case();
}
