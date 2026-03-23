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

static const char *TAG = "test_mesh_bulge";

#define TEST_MESH_BULGE_GRID_COLS         16U
#define TEST_MESH_BULGE_GRID_ROWS         12U
#define TEST_MESH_BULGE_POINT_COUNT       ((TEST_MESH_BULGE_GRID_COLS + 1U) * (TEST_MESH_BULGE_GRID_ROWS + 1U))
#define TEST_MESH_BULGE_TIMER_PERIOD_MS   33U
#define TEST_MESH_BULGE_FOCUS_LIMIT_X     96
#define TEST_MESH_BULGE_FOCUS_LIMIT_Y     72
#define TEST_MESH_BULGE_STRENGTH_LIMIT    140
#define TEST_MESH_BULGE_RADIUS            112
#define TEST_MESH_BULGE_INTENSITY_SCALE   320000
#define TEST_MESH_BULGE_CENTER_POINT_IDX  (((TEST_MESH_BULGE_GRID_ROWS / 2U) * (TEST_MESH_BULGE_GRID_COLS + 1U)) + (TEST_MESH_BULGE_GRID_COLS / 2U))

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_obj_t *title_label;
    gfx_obj_t *hint_label;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t base_points[TEST_MESH_BULGE_POINT_COUNT];
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
    bool dragging;
} test_mesh_bulge_scene_t;

static int16_t test_mesh_bulge_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static int16_t test_mesh_bulge_follow_axis(int16_t current, int16_t target)
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

static int32_t test_mesh_bulge_edge_dense_coord(uint32_t idx, uint32_t segments, int32_t max_coord)
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

static void test_mesh_bulge_apply_dense_edge_grid(test_mesh_bulge_scene_t *scene)
{
    gfx_mesh_img_point_t remapped[TEST_MESH_BULGE_POINT_COUNT];
    int32_t max_x = 0;
    int32_t max_y = 0;
    size_t idx = 0;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);

    for (size_t i = 0; i < TEST_MESH_BULGE_POINT_COUNT; i++) {
        gfx_mesh_img_point_t point;
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &point));
        if (point.x > max_x) {
            max_x = point.x;
        }
        if (point.y > max_y) {
            max_y = point.y;
        }
    }

    for (uint32_t row = 0; row <= TEST_MESH_BULGE_GRID_ROWS; row++) {
        int32_t y = test_mesh_bulge_edge_dense_coord(row, TEST_MESH_BULGE_GRID_ROWS, max_y);
        for (uint32_t col = 0; col <= TEST_MESH_BULGE_GRID_COLS; col++) {
            int32_t x = test_mesh_bulge_edge_dense_coord(col, TEST_MESH_BULGE_GRID_COLS, max_x);
            remapped[idx].x = (gfx_coord_t)x;
            remapped[idx].y = (gfx_coord_t)y;
            idx++;
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_rest_points(scene->mesh_obj, remapped, TEST_MESH_BULGE_POINT_COUNT));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_reset_points(scene->mesh_obj));
}

static void test_mesh_bulge_capture_base_points(test_mesh_bulge_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MESH_BULGE_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mesh_obj));

    for (size_t i = 0; i < TEST_MESH_BULGE_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mesh_obj, i, &scene->base_points[i]));
    }

    scene->mesh_center_x = scene->base_points[TEST_MESH_BULGE_CENTER_POINT_IDX].x;
    scene->mesh_center_y = scene->base_points[TEST_MESH_BULGE_CENTER_POINT_IDX].y;
}

static esp_err_t test_mesh_bulge_apply_pose(test_mesh_bulge_scene_t *scene, int16_t focus_ofs_x,
                                            int16_t focus_ofs_y, int16_t strength)
{
    int32_t focus_x;
    int32_t focus_y;
    gfx_mesh_img_point_t pose_points[TEST_MESH_BULGE_POINT_COUNT];
    const int32_t radius2 = TEST_MESH_BULGE_RADIUS * TEST_MESH_BULGE_RADIUS;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: object is NULL");

    focus_x = (int32_t)scene->mesh_center_x + focus_ofs_x;
    focus_y = (int32_t)scene->mesh_center_y + focus_ofs_y;

    for (size_t i = 0; i < TEST_MESH_BULGE_POINT_COUNT; i++) {
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

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_points(scene->mesh_obj, pose_points, TEST_MESH_BULGE_POINT_COUNT),
                        TAG, "apply pose: set points failed");

    return ESP_OK;
}

static void test_mesh_bulge_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_bulge_scene_t *scene = (test_mesh_bulge_scene_t *)user_data;
    int16_t disp_center_x;
    int16_t disp_center_y;

    (void)obj;

    if (scene == NULL || event == NULL) {
        return;
    }

    disp_center_x = (int16_t)(gfx_disp_get_hor_res(disp_default) / 2);
    disp_center_y = (int16_t)(gfx_disp_get_ver_res(disp_default) / 2);

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        scene->dragging = true;
        scene->press_x = (int16_t)event->x;
        scene->press_y = (int16_t)event->y;
        scene->target_focus_x = test_mesh_bulge_clamp_i16((int32_t)event->x - disp_center_x,
                                                          -TEST_MESH_BULGE_FOCUS_LIMIT_X, TEST_MESH_BULGE_FOCUS_LIMIT_X);
        scene->target_focus_y = test_mesh_bulge_clamp_i16((int32_t)event->y - disp_center_y,
                                                          -TEST_MESH_BULGE_FOCUS_LIMIT_Y, TEST_MESH_BULGE_FOCUS_LIMIT_Y);
        scene->target_strength = 0;
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_MOVE && scene->dragging) {
        scene->target_focus_x = test_mesh_bulge_clamp_i16((int32_t)event->x - disp_center_x,
                                                          -TEST_MESH_BULGE_FOCUS_LIMIT_X, TEST_MESH_BULGE_FOCUS_LIMIT_X);
        scene->target_focus_y = test_mesh_bulge_clamp_i16((int32_t)event->y - disp_center_y,
                                                          -TEST_MESH_BULGE_FOCUS_LIMIT_Y, TEST_MESH_BULGE_FOCUS_LIMIT_Y);
        scene->target_strength = test_mesh_bulge_clamp_i16((int32_t)scene->press_y - event->y,
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

static void test_mesh_bulge_anim_cb(void *user_data)
{
    test_mesh_bulge_scene_t *scene = (test_mesh_bulge_scene_t *)user_data;
    int16_t next_focus_x;
    int16_t next_focus_y;
    int16_t next_strength;

    if (scene == NULL || scene->mesh_obj == NULL) {
        return;
    }

    next_focus_x = test_mesh_bulge_follow_axis(scene->current_focus_x, scene->target_focus_x);
    next_focus_y = test_mesh_bulge_follow_axis(scene->current_focus_y, scene->target_focus_y);
    next_strength = test_mesh_bulge_follow_axis(scene->current_strength, scene->target_strength);

    if (next_focus_x == scene->current_focus_x &&
            next_focus_y == scene->current_focus_y &&
            next_strength == scene->current_strength) {
        return;
    }

    scene->current_focus_x = next_focus_x;
    scene->current_focus_y = next_focus_y;
    scene->current_strength = next_strength;
    test_mesh_bulge_apply_pose(scene, next_focus_x, next_focus_y, next_strength);

    if (scene->hint_label != NULL) {
        gfx_label_set_text_fmt(scene->hint_label, " strength:%d ", (int)next_strength);
    }
}

static void test_mesh_bulge_scene_cleanup(test_mesh_bulge_scene_t *scene)
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

static void test_mesh_bulge_run(void)
{
    test_mesh_bulge_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh bulge/pinch (fisheye-like) show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x02040A));

    scene.mesh_obj = gfx_mesh_img_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    scene.hint_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.mesh_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.hint_label);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mesh_obj, (void *)&simple_face));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mesh_obj, TEST_MESH_BULGE_GRID_COLS, TEST_MESH_BULGE_GRID_ROWS));
    test_mesh_bulge_apply_dense_edge_grid(&scene);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.mesh_obj, test_mesh_bulge_touch_cb, &scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mesh_obj, GFX_ALIGN_CENTER, 0, 0));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Bulge"));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 240, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.hint_label, " strength:0 "));

    test_mesh_bulge_capture_base_points(&scene);
    TEST_ASSERT_EQUAL(ESP_OK, test_mesh_bulge_apply_pose(&scene, 0, 0, 0));

    scene.anim_timer = gfx_timer_create(emote_handle, test_mesh_bulge_anim_cb, TEST_MESH_BULGE_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Touch to move fisheye center; drag up to bulge, drag down to pinch");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_bulge_scene_cleanup(&scene);
    test_app_unlock();
}

void test_mesh_bulge_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mesh_bulge_run();
    test_app_runtime_close(&runtime);
}

