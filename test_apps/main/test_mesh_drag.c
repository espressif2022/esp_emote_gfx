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

static const char *TAG = "test_mesh_drag";

#define TEST_MESH_DRAG_GRID_COLS       12U
#define TEST_MESH_DRAG_GRID_ROWS       8U
#define TEST_MESH_DRAG_POINT_COUNT     ((TEST_MESH_DRAG_GRID_COLS + 1U) * (TEST_MESH_DRAG_GRID_ROWS + 1U))
#define TEST_MESH_DRAG_TIMER_PERIOD_MS 33U
// #define TEST_MESH_DRAG_DRAG_LIMIT_X    60
// #define TEST_MESH_DRAG_DRAG_LIMIT_Y    50
#define TEST_MESH_DRAG_DRAG_LIMIT_X    100
#define TEST_MESH_DRAG_DRAG_LIMIT_Y    100
#define TEST_MESH_DRAG_CENTER_POINT_IDX (((TEST_MESH_DRAG_GRID_ROWS / 2U) * (TEST_MESH_DRAG_GRID_COLS + 1U)) + (TEST_MESH_DRAG_GRID_COLS / 2U))
#define TEST_MESH_DRAG_SHEAR_DIV       3000
#define TEST_MESH_DRAG_SQUEEZE_DIV     8000

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_obj_t *title_label;
    gfx_obj_t *hint_label;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t base_points[TEST_MESH_DRAG_POINT_COUNT];
    int16_t current_drag_x;
    int16_t current_drag_y;
    int16_t target_drag_x;
    int16_t target_drag_y;
    int16_t press_x;
    int16_t press_y;
    bool dragging;
} test_mesh_drag_scene_t;

static int32_t test_mesh_drag_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mesh_drag_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int16_t test_mesh_drag_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static int16_t test_mesh_drag_follow_axis(int16_t current, int16_t target)
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

static int32_t test_mesh_drag_edge_dense_coord(uint32_t idx, uint32_t segments, int32_t max_coord)
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

static void test_mesh_drag_apply_dense_edge_grid(test_mesh_drag_scene_t *scene)
{
    gfx_mesh_img_point_t remapped[TEST_MESH_DRAG_POINT_COUNT];
    int32_t max_x = 0;
    int32_t max_y = 0;
    size_t idx = 0;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);

    for (size_t i = 0; i < TEST_MESH_DRAG_POINT_COUNT; i++) {
        gfx_mesh_img_point_t point;
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &point));
        if (point.x > max_x) {
            max_x = point.x;
        }
        if (point.y > max_y) {
            max_y = point.y;
        }
    }

    for (uint32_t row = 0; row <= TEST_MESH_DRAG_GRID_ROWS; row++) {
        int32_t y = test_mesh_drag_edge_dense_coord(row, TEST_MESH_DRAG_GRID_ROWS, max_y);
        for (uint32_t col = 0; col <= TEST_MESH_DRAG_GRID_COLS; col++) {
            int32_t x = test_mesh_drag_edge_dense_coord(col, TEST_MESH_DRAG_GRID_COLS, max_x);
            remapped[idx].x = (gfx_coord_t)x;
            remapped[idx].y = (gfx_coord_t)y;
            idx++;
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_rest_points(scene->mesh_obj, remapped, TEST_MESH_DRAG_POINT_COUNT));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_reset_points(scene->mesh_obj));
}

static void test_mesh_drag_capture_base_points(test_mesh_drag_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MESH_DRAG_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mesh_obj));

    for (size_t i = 0; i < TEST_MESH_DRAG_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &scene->base_points[i]));
    }
}

static esp_err_t test_mesh_drag_apply_drag_pose(test_mesh_drag_scene_t *scene, int16_t drag_x, int16_t drag_y)
{
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;
    int32_t drag_mag;
    gfx_mesh_img_point_t pose_points[TEST_MESH_DRAG_POINT_COUNT];

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply drag pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply drag pose: object is NULL");

    center_x = scene->base_points[TEST_MESH_DRAG_CENTER_POINT_IDX].x;
    center_y = scene->base_points[TEST_MESH_DRAG_CENTER_POINT_IDX].y;

    for (size_t i = 0; i < TEST_MESH_DRAG_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;

        max_abs_x = test_mesh_drag_max_i32(max_abs_x, test_mesh_drag_abs_i32(rel_x));
        max_abs_y = test_mesh_drag_max_i32(max_abs_y, test_mesh_drag_abs_i32(rel_y));
    }

    drag_mag = test_mesh_drag_abs_i32(drag_x) + test_mesh_drag_abs_i32(drag_y);

    for (size_t i = 0; i < TEST_MESH_DRAG_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;
        int32_t nx = (rel_x * 1000) / max_abs_x;
        int32_t ny = (rel_y * 1000) / max_abs_y;

        /* Softness field: parabolic falloff, 0 at every edge, 1000 at center */
        int32_t soft_x = 1000 - (nx * nx) / 1000;
        int32_t soft_y = 1000 - (ny * ny) / 1000;
        int32_t softness = (soft_x * soft_y) / 1000;

        /* Layer 1 -- Soft shear: center follows drag, edges anchored */
        int32_t dx = ((int32_t)drag_x * softness) / TEST_MESH_DRAG_SHEAR_DIV;
        int32_t dy = ((int32_t)drag_y * softness) / TEST_MESH_DRAG_SHEAR_DIV;

        /* Layer 2 -- Squash-and-stretch along drag direction */
        if (drag_mag > 0) {
            int32_t alignment = (nx * (int32_t)drag_x + ny * (int32_t)drag_y) / drag_mag;
            int32_t squeeze = (-alignment * softness) / 1000;
            dx += (squeeze * (int32_t)drag_x) / TEST_MESH_DRAG_SQUEEZE_DIV;
            dy += (squeeze * (int32_t)drag_y) / TEST_MESH_DRAG_SQUEEZE_DIV;
        }

        pose_points[i].x = (gfx_coord_t)(scene->base_points[i].x + dx);
        pose_points[i].y = (gfx_coord_t)(scene->base_points[i].y + dy);
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_points(scene->mesh_obj, pose_points, TEST_MESH_DRAG_POINT_COUNT),
                        TAG, "apply drag pose: set points failed");

    /* Object follows the finger position directly */
    gfx_obj_align(scene->mesh_obj, GFX_ALIGN_CENTER, drag_x, drag_y);
    return ESP_OK;
}

static void test_mesh_drag_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_drag_scene_t *scene = (test_mesh_drag_scene_t *)user_data;

    (void)obj;

    if (scene == NULL || event == NULL) {
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        scene->dragging = true;
        scene->press_x = (int16_t)event->x;
        scene->press_y = (int16_t)event->y;
        scene->target_drag_x = 0;
        scene->target_drag_y = 0;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && scene->dragging) {
        scene->target_drag_x = test_mesh_drag_clamp_i16((int32_t)event->x - scene->press_x,
                                                         -TEST_MESH_DRAG_DRAG_LIMIT_X, TEST_MESH_DRAG_DRAG_LIMIT_X);
        scene->target_drag_y = test_mesh_drag_clamp_i16((int32_t)event->y - scene->press_y,
                                                         -TEST_MESH_DRAG_DRAG_LIMIT_Y, TEST_MESH_DRAG_DRAG_LIMIT_Y);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->dragging = false;
        scene->target_drag_x = 0;
        scene->target_drag_y = 0;
    }
}

static void test_mesh_drag_anim_cb(void *user_data)
{
    test_mesh_drag_scene_t *scene = (test_mesh_drag_scene_t *)user_data;
    int16_t next_drag_x;
    int16_t next_drag_y;

    if (scene == NULL || scene->mesh_obj == NULL) {
        return;
    }

    next_drag_x = test_mesh_drag_follow_axis(scene->current_drag_x, scene->target_drag_x);
    next_drag_y = test_mesh_drag_follow_axis(scene->current_drag_y, scene->target_drag_y);

    if (!scene->dragging &&
            test_mesh_drag_abs_i32(next_drag_x) <= 1 &&
            test_mesh_drag_abs_i32(next_drag_y) <= 1 &&
            scene->target_drag_x == 0 && scene->target_drag_y == 0) {
        next_drag_x = 0;
        next_drag_y = 0;
    }

    if (next_drag_x == scene->current_drag_x && next_drag_y == scene->current_drag_y) {
        return;
    }

    scene->current_drag_x = next_drag_x;
    scene->current_drag_y = next_drag_y;
    test_mesh_drag_apply_drag_pose(scene, next_drag_x, next_drag_y);
}

static void test_mesh_drag_scene_cleanup(test_mesh_drag_scene_t *scene)
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
}

static void test_mesh_drag_run(void)
{
    test_mesh_drag_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh image drag deform show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x02040A));

    scene.mesh_obj = gfx_mesh_img_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    scene.hint_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.mesh_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.hint_label);
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mesh_obj, (void *)&simple_face2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mesh_obj, (void *)&simple_face));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mesh_obj, TEST_MESH_DRAG_GRID_COLS, TEST_MESH_DRAG_GRID_ROWS));
    test_mesh_drag_apply_dense_edge_grid(&scene);
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.mesh_obj, test_mesh_drag_touch_cb, &scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mesh_obj, GFX_ALIGN_CENTER, 0, 0));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Drag"));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 260, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.hint_label, " drag to pull + stretch "));

    test_mesh_drag_capture_base_points(&scene);
    TEST_ASSERT_EQUAL(ESP_OK, test_mesh_drag_apply_drag_pose(&scene, 0, 0));

    scene.anim_timer = gfx_timer_create(emote_handle, test_mesh_drag_anim_cb, TEST_MESH_DRAG_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Drag the face UI to stretch it gently in each direction");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_drag_scene_cleanup(&scene);
    test_app_unlock();
}

void test_mesh_drag_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mesh_drag_run();
    test_app_runtime_close(&runtime);
}
