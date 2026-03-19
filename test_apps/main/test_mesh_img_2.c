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

static const char *TAG = "test_mesh_img_2";

#define TEST_MESH2_GRID_COLS       6U
#define TEST_MESH2_GRID_ROWS       4U
#define TEST_MESH2_POINT_COUNT     ((TEST_MESH2_GRID_COLS + 1U) * (TEST_MESH2_GRID_ROWS + 1U))
#define TEST_MESH2_TIMER_PERIOD_MS 33U
// #define TEST_MESH2_DRAG_LIMIT_X    60
// #define TEST_MESH2_DRAG_LIMIT_Y    50
#define TEST_MESH2_DRAG_LIMIT_X    100
#define TEST_MESH2_DRAG_LIMIT_Y    100
#define TEST_MESH2_CENTER_POINT_IDX (((TEST_MESH2_GRID_ROWS / 2U) * (TEST_MESH2_GRID_COLS + 1U)) + (TEST_MESH2_GRID_COLS / 2U))

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t base_points[TEST_MESH2_POINT_COUNT];
    int16_t current_drag_x;
    int16_t current_drag_y;
    int16_t target_drag_x;
    int16_t target_drag_y;
    int16_t press_x;
    int16_t press_y;
    bool dragging;
} test_mesh_img_2_scene_t;

static int32_t test_mesh_img_2_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mesh_img_2_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int16_t test_mesh_img_2_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static int16_t test_mesh_img_2_follow_axis(int16_t current, int16_t target)
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

static void test_mesh_img_2_capture_base_points(test_mesh_img_2_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MESH2_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mesh_obj));

    for (size_t i = 0; i < TEST_MESH2_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &scene->base_points[i]));
    }
}

static esp_err_t test_mesh_img_2_apply_drag_pose(test_mesh_img_2_scene_t *scene, int16_t drag_x, int16_t drag_y)
{
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;
    int32_t drag_mag;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply drag pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply drag pose: object is NULL");

    center_x = scene->base_points[TEST_MESH2_CENTER_POINT_IDX].x;
    center_y = scene->base_points[TEST_MESH2_CENTER_POINT_IDX].y;

    for (size_t i = 0; i < TEST_MESH2_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;

        max_abs_x = test_mesh_img_2_max_i32(max_abs_x, test_mesh_img_2_abs_i32(rel_x));
        max_abs_y = test_mesh_img_2_max_i32(max_abs_y, test_mesh_img_2_abs_i32(rel_y));
    }

    drag_mag = test_mesh_img_2_abs_i32(drag_x) + test_mesh_img_2_abs_i32(drag_y);

    for (size_t i = 0; i < TEST_MESH2_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;
        int32_t nx = (rel_x * 1000) / max_abs_x;
        int32_t ny = (rel_y * 1000) / max_abs_y;
        int32_t abs_nx = test_mesh_img_2_abs_i32(nx);
        int32_t abs_ny = test_mesh_img_2_abs_i32(ny);
        int32_t dx = 0;
        int32_t dy = 0;

        /*
         * Corner weight: 0 at center and along center axes (horizontal/vertical),
         * maximum (~1000) at the four diagonal corners.
         * This keeps the eyes and mouth (center band) stable while the
         * peripheral corners stretch outward.
         */
        int32_t corner_weight = (abs_nx * abs_ny) / 1000;

        /*
         * Radial diagonal stretch: each corner point pushes outward along its
         * own diagonal direction, driven by the overall drag magnitude.
         * Points on the center cross (corner_weight ≈ 0) are unaffected.
         */
        dx += (rel_x * drag_mag * corner_weight) / 500000;
        dy += (rel_y * drag_mag * corner_weight) / 500000;

        /*
         * Directional lean: corners on the side being dragged toward
         * stretch a bit more, giving a subtle directional "lean" to the
         * overall shape.
         */
        dx += ((int32_t)drag_x * corner_weight) / 2500;
        dy += ((int32_t)drag_y * corner_weight) / 2500;

        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_point(scene->mesh_obj, i,
                                                   (gfx_coord_t)(scene->base_points[i].x + dx),
                                                   (gfx_coord_t)(scene->base_points[i].y + dy)),
                            TAG, "apply drag pose: set point failed");
    }

    /* Object follows the finger position directly */
    gfx_obj_align(scene->mesh_obj, GFX_ALIGN_CENTER, drag_x, drag_y);
    return ESP_OK;
}

static void test_mesh_img_2_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_img_2_scene_t *scene = (test_mesh_img_2_scene_t *)user_data;

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
        scene->target_drag_x = test_mesh_img_2_clamp_i16((int32_t)event->x - scene->press_x,
                                                         -TEST_MESH2_DRAG_LIMIT_X, TEST_MESH2_DRAG_LIMIT_X);
        scene->target_drag_y = test_mesh_img_2_clamp_i16((int32_t)event->y - scene->press_y,
                                                         -TEST_MESH2_DRAG_LIMIT_Y, TEST_MESH2_DRAG_LIMIT_Y);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->dragging = false;
        scene->target_drag_x = 0;
        scene->target_drag_y = 0;
    }
}

static void test_mesh_img_2_anim_cb(void *user_data)
{
    test_mesh_img_2_scene_t *scene = (test_mesh_img_2_scene_t *)user_data;
    int16_t next_drag_x;
    int16_t next_drag_y;

    if (scene == NULL || scene->mesh_obj == NULL) {
        return;
    }

    next_drag_x = test_mesh_img_2_follow_axis(scene->current_drag_x, scene->target_drag_x);
    next_drag_y = test_mesh_img_2_follow_axis(scene->current_drag_y, scene->target_drag_y);

    if (!scene->dragging &&
            test_mesh_img_2_abs_i32(next_drag_x) <= 1 &&
            test_mesh_img_2_abs_i32(next_drag_y) <= 1 &&
            scene->target_drag_x == 0 && scene->target_drag_y == 0) {
        next_drag_x = 0;
        next_drag_y = 0;
    }

    if (next_drag_x == scene->current_drag_x && next_drag_y == scene->current_drag_y) {
        return;
    }

    scene->current_drag_x = next_drag_x;
    scene->current_drag_y = next_drag_y;
    test_mesh_img_2_apply_drag_pose(scene, next_drag_x, next_drag_y);
}

static void test_mesh_img_2_scene_cleanup(test_mesh_img_2_scene_t *scene)
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
}

static void test_mesh_img_2_run(void)
{
    test_mesh_img_2_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh image drag deform show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    // gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x0f172a));
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.mesh_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.mesh_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mesh_obj, (void *)&face_ui_simple));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mesh_obj, TEST_MESH2_GRID_COLS, TEST_MESH2_GRID_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, true));
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.mesh_obj, test_mesh_img_2_touch_cb, &scene));

    test_mesh_img_2_capture_base_points(&scene);
    TEST_ASSERT_EQUAL(ESP_OK, test_mesh_img_2_apply_drag_pose(&scene, 0, 0));
    gfx_obj_align(scene.mesh_obj, GFX_ALIGN_CENTER, 0, 0);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mesh_img_2_anim_cb, TEST_MESH2_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Drag the face UI to stretch it gently in each direction");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_img_2_scene_cleanup(&scene);
    test_app_unlock();
}

void test_mesh_img_2_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mesh_img_2_run();
    test_app_runtime_close(&runtime);
}
