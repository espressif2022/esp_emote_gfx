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

static const char *TAG = "test_mesh_img";

#define TEST_MESH_GRID_COLS       4U
#define TEST_MESH_GRID_ROWS       4U
#define TEST_MESH_POINT_COUNT     ((TEST_MESH_GRID_COLS + 1U) * (TEST_MESH_GRID_ROWS + 1U))
#define TEST_MESH_TIMER_PERIOD_MS 33U
#define TEST_MESH_CENTER_POINT_IDX (((TEST_MESH_GRID_ROWS / 2U) * (TEST_MESH_GRID_COLS + 1U)) + (TEST_MESH_GRID_COLS / 2U))

typedef struct {
    int16_t offset_x;
    int16_t offset_y;
    int16_t gaze_x;
    int16_t gaze_y;
    int16_t stretch_x_permille;
    int16_t stretch_y_permille;
    int16_t top_lid_y;
    int16_t bottom_lid_y;
    int16_t side_inset_x;
    int16_t tilt_y_permille;
    int16_t center_lift_y;
} test_mesh_img_pose_t;

typedef struct {
    test_mesh_img_pose_t pose;
    uint16_t transition_ticks;
    uint16_t hold_ticks;
} test_mesh_img_keyframe_t;

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t base_points[TEST_MESH_POINT_COUNT];
    test_mesh_img_pose_t current_pose;
    test_mesh_img_pose_t transition_from_pose;
    size_t keyframe_idx;
    size_t next_keyframe_idx;
    uint16_t hold_tick;
    uint16_t transition_tick;
    bool is_transitioning;
} test_mesh_img_scene_t;

static const test_mesh_img_keyframe_t s_orb_keyframes[] = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 8U, 16U},
    {{-3, 0, -12, 0, -70, 20, 0, 0, 10, -50, 0}, 9U, 8U},
    {{0, -5, 0, -5, -110, 140, 0, 0, -4, 0, -6}, 8U, 6U},
    {{3, 0, 12, 0, -70, 20, 0, 0, 10, 50, 0}, 10U, 8U},
    {{0, 5, 0, 4, 120, -80, 0, 0, 8, 0, 4}, 8U, 5U},
    {{0, 0, 0, 0, 140, -180, 24, 14, 14, 0, 0}, 4U, 1U},
    {{0, 0, 0, 0, 220, -520, 54, 36, 26, 0, 0}, 3U, 1U},
    {{0, 0, 0, 0, 160, -220, 28, 16, 16, 0, 0}, 4U, 1U},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 5U, 10U},
    {{0, -4, 0, -2, -180, 220, 0, 0, -10, 0, -8}, 8U, 4U},
    {{0, -1, 0, -1, -60, 80, 0, 0, -4, 0, -2}, 5U, 5U},
    {{2, 0, 8, 0, -40, 10, 0, 0, 6, 30, 0}, 7U, 4U},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 6U, 14U},
};

static int32_t test_mesh_img_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mesh_img_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int32_t test_mesh_img_lerp_i32(int32_t from, int32_t to, uint16_t t_permille)
{
    return from + (((to - from) * (int32_t)t_permille) / 1000);
}

static uint16_t test_mesh_img_smoothstep_permille(uint16_t t_permille)
{
    int64_t t = t_permille;
    int64_t eased = (t * t * (3000 - 2 * t) + 500000) / 1000000;

    if (eased < 0) {
        eased = 0;
    } else if (eased > 1000) {
        eased = 1000;
    }

    return (uint16_t)eased;
}

static test_mesh_img_pose_t test_mesh_img_lerp_pose(const test_mesh_img_pose_t *from,
                                                    const test_mesh_img_pose_t *to,
                                                    uint16_t t_permille)
{
    test_mesh_img_pose_t pose = {0};

    pose.offset_x = (int16_t)test_mesh_img_lerp_i32(from->offset_x, to->offset_x, t_permille);
    pose.offset_y = (int16_t)test_mesh_img_lerp_i32(from->offset_y, to->offset_y, t_permille);
    pose.gaze_x = (int16_t)test_mesh_img_lerp_i32(from->gaze_x, to->gaze_x, t_permille);
    pose.gaze_y = (int16_t)test_mesh_img_lerp_i32(from->gaze_y, to->gaze_y, t_permille);
    pose.stretch_x_permille = (int16_t)test_mesh_img_lerp_i32(from->stretch_x_permille, to->stretch_x_permille, t_permille);
    pose.stretch_y_permille = (int16_t)test_mesh_img_lerp_i32(from->stretch_y_permille, to->stretch_y_permille, t_permille);
    pose.top_lid_y = (int16_t)test_mesh_img_lerp_i32(from->top_lid_y, to->top_lid_y, t_permille);
    pose.bottom_lid_y = (int16_t)test_mesh_img_lerp_i32(from->bottom_lid_y, to->bottom_lid_y, t_permille);
    pose.side_inset_x = (int16_t)test_mesh_img_lerp_i32(from->side_inset_x, to->side_inset_x, t_permille);
    pose.tilt_y_permille = (int16_t)test_mesh_img_lerp_i32(from->tilt_y_permille, to->tilt_y_permille, t_permille);
    pose.center_lift_y = (int16_t)test_mesh_img_lerp_i32(from->center_lift_y, to->center_lift_y, t_permille);
    return pose;
}

static void test_mesh_img_capture_base_points(test_mesh_img_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MESH_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mesh_obj));

    for (size_t i = 0; i < TEST_MESH_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &scene->base_points[i]));
    }
}

static esp_err_t test_mesh_img_apply_pose(test_mesh_img_scene_t *scene, const test_mesh_img_pose_t *pose)
{
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: object is NULL");
    ESP_RETURN_ON_FALSE(pose != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: pose is NULL");

    center_x = scene->base_points[TEST_MESH_CENTER_POINT_IDX].x;
    center_y = scene->base_points[TEST_MESH_CENTER_POINT_IDX].y;

    for (size_t i = 0; i < TEST_MESH_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;

        max_abs_x = test_mesh_img_max_i32(max_abs_x, test_mesh_img_abs_i32(rel_x));
        max_abs_y = test_mesh_img_max_i32(max_abs_y, test_mesh_img_abs_i32(rel_y));
    }

    for (size_t i = 0; i < TEST_MESH_POINT_COUNT; i++) {
        int32_t rel_x = scene->base_points[i].x - center_x;
        int32_t rel_y = scene->base_points[i].y - center_y;
        int32_t nx = (rel_x * 1000) / max_abs_x;
        int32_t ny = (rel_y * 1000) / max_abs_y;
        int32_t abs_nx = test_mesh_img_abs_i32(nx);
        int32_t abs_ny = test_mesh_img_abs_i32(ny);
        int32_t center_x_weight = 1000 - abs_nx;
        int32_t center_y_weight = 1000 - abs_ny;
        int32_t center_weight = (center_x_weight * center_y_weight) / 1000;
        int32_t top_weight = (ny < 0) ? -ny : 0;
        int32_t bottom_weight = (ny > 0) ? ny : 0;
        int32_t left_weight = (nx < 0) ? -nx : 0;
        int32_t right_weight = (nx > 0) ? nx : 0;
        int32_t dx = pose->offset_x;
        int32_t dy = pose->offset_y;

        dx += (pose->gaze_x * center_y_weight) / 1000;
        dy += (pose->gaze_y * center_x_weight) / 1000;

        dx += (rel_x * (int32_t)pose->stretch_x_permille * center_y_weight) / 1000000;
        dy += (rel_y * (int32_t)pose->stretch_y_permille * center_x_weight) / 1000000;

        dy += (pose->top_lid_y * top_weight * center_x_weight) / 1000000;
        dy -= (pose->bottom_lid_y * bottom_weight * center_x_weight) / 1000000;

        dx += (pose->side_inset_x * left_weight * center_y_weight) / 1000000;
        dx -= (pose->side_inset_x * right_weight * center_y_weight) / 1000000;

        dy += (rel_x * (int32_t)pose->tilt_y_permille * center_y_weight) / 1000000;
        dy += (pose->center_lift_y * center_weight) / 1000;

        // ESP_LOGI("", "set point[%d]: %d, %d",
        //     i,
        //     (gfx_coord_t)(scene->base_points[i].x + dx), 
        //     (gfx_coord_t)(scene->base_points[i].y + dy));

        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_point(scene->mesh_obj, i,
                                                   (gfx_coord_t)(scene->base_points[i].x + dx),
                                                   (gfx_coord_t)(scene->base_points[i].y + dy)),
                            TAG, "apply pose: set point failed");
    }

    scene->current_pose = *pose;
    return ESP_OK;
}

static void test_mesh_img_start_transition(test_mesh_img_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    scene->next_keyframe_idx = (scene->keyframe_idx + 1U) % TEST_APP_ARRAY_SIZE(s_orb_keyframes);
    scene->transition_from_pose = scene->current_pose;
    scene->transition_tick = 0U;
    scene->is_transitioning = true;
}

static void test_mesh_img_anim_cb(void *user_data)
{
    test_mesh_img_scene_t *scene = (test_mesh_img_scene_t *)user_data;
    const test_mesh_img_keyframe_t *target_keyframe;
    test_mesh_img_pose_t pose;
    uint16_t duration;
    uint16_t t_permille;
    uint16_t eased_permille;

    if (scene == NULL || scene->mesh_obj == NULL) {
        return;
    }

    if (!scene->is_transitioning) {
        scene->hold_tick++;
        if (scene->hold_tick < s_orb_keyframes[scene->keyframe_idx].hold_ticks) {
            return;
        }

        scene->hold_tick = 0U;
        test_mesh_img_start_transition(scene);
    }

    target_keyframe = &s_orb_keyframes[scene->next_keyframe_idx];
    duration = target_keyframe->transition_ticks;
    if (duration == 0U) {
        duration = 1U;
    }

    scene->transition_tick++;
    if (scene->transition_tick > duration) {
        scene->transition_tick = duration;
    }

    t_permille = (uint16_t)(((uint32_t)scene->transition_tick * 1000U) / duration);
    eased_permille = test_mesh_img_smoothstep_permille(t_permille);
    pose = test_mesh_img_lerp_pose(&scene->transition_from_pose, &target_keyframe->pose, eased_permille);

    if (test_mesh_img_apply_pose(scene, &pose) != ESP_OK) {
        scene->transition_tick = 0U;
        scene->hold_tick = 0U;
        scene->is_transitioning = false;
        return;
    }

    if (scene->transition_tick >= duration) {
        scene->keyframe_idx = scene->next_keyframe_idx;
        scene->transition_tick = 0U;
        scene->hold_tick = 0U;
        scene->is_transitioning = false;
    }
}

static void test_mesh_img_scene_cleanup(test_mesh_img_scene_t *scene)
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

static void test_mesh_img_run(void)
{
    test_mesh_img_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh image local deform show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x0f172a));

    scene.mesh_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.mesh_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mesh_obj, (void *)&orb_ball_center));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mesh_obj, TEST_MESH_GRID_COLS, TEST_MESH_GRID_ROWS));
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, true));

    test_mesh_img_capture_base_points(&scene);
    scene.current_pose = s_orb_keyframes[0].pose;
    scene.keyframe_idx = 0U;
    scene.next_keyframe_idx = 1U;
    TEST_ASSERT_EQUAL(ESP_OK, test_mesh_img_apply_pose(&scene, &scene.current_pose));
    gfx_obj_align(scene.mesh_obj, GFX_ALIGN_CENTER, 0, 0);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mesh_img_anim_cb, TEST_MESH_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Observe local mesh point deformation");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_img_scene_cleanup(&scene);
    test_app_unlock();
}

// TEST_CASE("widget mesh image deform", "[widget][image][mesh]")
void test_mesh_img_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mesh_img_run();
    test_app_runtime_close(&runtime);
}
