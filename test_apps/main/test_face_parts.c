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

static const char *TAG = "test_face_parts";

#define TEST_FACE_PARTS_MOUTH_GRID_COLS        14U
#define TEST_FACE_PARTS_MOUTH_GRID_ROWS        6U
#define TEST_FACE_PARTS_MOUTH_POINT_COUNT      ((TEST_FACE_PARTS_MOUTH_GRID_COLS + 1U) * (TEST_FACE_PARTS_MOUTH_GRID_ROWS + 1U))
#define TEST_FACE_PARTS_TIMER_PERIOD_MS        33U
#define TEST_FACE_PARTS_GAZE_LIMIT_X           18
#define TEST_FACE_PARTS_GAZE_LIMIT_Y           10
#define TEST_FACE_PARTS_SMILE_LIMIT            36
#define TEST_FACE_PARTS_LEFT_EYE_X_OFS         (-48)
#define TEST_FACE_PARTS_RIGHT_EYE_X_OFS        48
#define TEST_FACE_PARTS_EYE_Y_OFS              (-24)
#define TEST_FACE_PARTS_MOUTH_Y_OFS            60
#define TEST_FACE_PARTS_HEAD_Y_OFS             8
#define TEST_FACE_PARTS_MOUTH_MODE_THRESH      10

typedef enum {
    TEST_FACE_PARTS_MOUTH_MODE_FLAT = 0,
    TEST_FACE_PARTS_MOUTH_MODE_SMILE,
    TEST_FACE_PARTS_MOUTH_MODE_O,
} test_face_parts_mouth_mode_t;

typedef struct {
    gfx_obj_t *head_obj;
    gfx_obj_t *left_eye_obj;
    gfx_obj_t *right_eye_obj;
    gfx_obj_t *mouth_obj;
    gfx_obj_t *title_label;
    gfx_obj_t *hint_label;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t mouth_base_points[TEST_FACE_PARTS_MOUTH_POINT_COUNT];
    int16_t current_gaze_x;
    int16_t current_gaze_y;
    int16_t target_gaze_x;
    int16_t target_gaze_y;
    int16_t current_smile;
    int16_t target_smile;
    uint16_t blink_tick;
    bool blink_closed;
    bool dragging;
    test_face_parts_mouth_mode_t mouth_mode;
} test_face_parts_scene_t;

static int16_t test_face_parts_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (int16_t)value;
}

static int32_t test_face_parts_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int16_t test_face_parts_follow_axis(int16_t current, int16_t target)
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

static bool test_face_parts_blink_closed(uint16_t tick)
{
    uint16_t phase = tick % 120U;
    return (phase >= 92U && phase < 100U);
}

static test_face_parts_mouth_mode_t test_face_parts_select_mouth_mode(int16_t smile)
{
    if (smile >= TEST_FACE_PARTS_MOUTH_MODE_THRESH) {
        return TEST_FACE_PARTS_MOUTH_MODE_SMILE;
    }
    if (smile <= -TEST_FACE_PARTS_MOUTH_MODE_THRESH) {
        return TEST_FACE_PARTS_MOUTH_MODE_O;
    }
    return TEST_FACE_PARTS_MOUTH_MODE_FLAT;
}

static const gfx_image_dsc_t *test_face_parts_get_mouth_src(test_face_parts_mouth_mode_t mode)
{
    switch (mode) {
    case TEST_FACE_PARTS_MOUTH_MODE_SMILE:
        return &face_parts_mouth;
    case TEST_FACE_PARTS_MOUTH_MODE_O:
        return &face_parts_mouth_o;
    case TEST_FACE_PARTS_MOUTH_MODE_FLAT:
    default:
        return &face_parts_mouth_flat;
    }
}

static void test_face_parts_capture_mouth_base_points(test_face_parts_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mouth_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_FACE_PARTS_MOUTH_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mouth_obj));

    for (size_t i = 0; i < TEST_FACE_PARTS_MOUTH_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mouth_obj, i, &scene->mouth_base_points[i]));
    }
}

static void test_face_parts_set_mouth_mode(test_face_parts_scene_t *scene, test_face_parts_mouth_mode_t mode)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mouth_obj);

    if (scene->mouth_mode == mode) {
        return;
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene->mouth_obj, (void *)test_face_parts_get_mouth_src(mode)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene->mouth_obj, TEST_FACE_PARTS_MOUTH_GRID_COLS, TEST_FACE_PARTS_MOUTH_GRID_ROWS));
    test_face_parts_capture_mouth_base_points(scene);
    scene->mouth_mode = mode;
}

static esp_err_t test_face_parts_apply_mouth_pose(test_face_parts_scene_t *scene, int16_t smile)
{
    gfx_mesh_img_point_t pose_points[TEST_FACE_PARTS_MOUTH_POINT_COUNT];
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;
    int32_t micro_smile = smile;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply mouth pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mouth_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply mouth pose: object is NULL");

    switch (scene->mouth_mode) {
    case TEST_FACE_PARTS_MOUTH_MODE_SMILE:
        micro_smile = smile - TEST_FACE_PARTS_MOUTH_MODE_THRESH;
        break;
    case TEST_FACE_PARTS_MOUTH_MODE_O:
        micro_smile = -(test_face_parts_abs_i32(smile) - TEST_FACE_PARTS_MOUTH_MODE_THRESH);
        break;
    case TEST_FACE_PARTS_MOUTH_MODE_FLAT:
    default:
        break;
    }

    center_x = scene->mouth_base_points[(TEST_FACE_PARTS_MOUTH_GRID_ROWS / 2U) * (TEST_FACE_PARTS_MOUTH_GRID_COLS + 1U) + (TEST_FACE_PARTS_MOUTH_GRID_COLS / 2U)].x;
    center_y = scene->mouth_base_points[(TEST_FACE_PARTS_MOUTH_GRID_ROWS / 2U) * (TEST_FACE_PARTS_MOUTH_GRID_COLS + 1U) + (TEST_FACE_PARTS_MOUTH_GRID_COLS / 2U)].y;

    for (size_t i = 0; i < TEST_FACE_PARTS_MOUTH_POINT_COUNT; i++) {
        int32_t rel_x = scene->mouth_base_points[i].x - center_x;
        int32_t rel_y = scene->mouth_base_points[i].y - center_y;
        if (test_face_parts_abs_i32(rel_x) > max_abs_x) {
            max_abs_x = test_face_parts_abs_i32(rel_x);
        }
        if (test_face_parts_abs_i32(rel_y) > max_abs_y) {
            max_abs_y = test_face_parts_abs_i32(rel_y);
        }
    }

    for (size_t i = 0; i < TEST_FACE_PARTS_MOUTH_POINT_COUNT; i++) {
        int32_t base_x = scene->mouth_base_points[i].x;
        int32_t base_y = scene->mouth_base_points[i].y;
        int32_t rel_x = base_x - center_x;
        int32_t rel_y = base_y - center_y;
        int32_t abs_nx = test_face_parts_abs_i32((rel_x * 1000) / max_abs_x);
        int32_t abs_ny = test_face_parts_abs_i32((rel_y * 1000) / max_abs_y);
        int32_t edge_weight = abs_nx;
        int32_t center_weight = 1000 - abs_nx;
        int32_t vertical_weight = 1000 - abs_ny;
        int32_t dx = 0;
        int32_t dy = 0;

        if (scene->mouth_mode == TEST_FACE_PARTS_MOUTH_MODE_SMILE) {
            dx = ((int32_t)micro_smile * rel_x * vertical_weight) / 120000;
            dy = -((int32_t)micro_smile * edge_weight * vertical_weight) / 22000;
            if (rel_y > 0) {
                dy += micro_smile / 8;
            }
        } else if (scene->mouth_mode == TEST_FACE_PARTS_MOUTH_MODE_O) {
            int32_t open = test_face_parts_abs_i32(micro_smile);
            dx = -((int32_t)open * center_weight * rel_x) / 220000;
            dy = ((int32_t)open * center_weight) / 40;
            if (rel_y < 0) {
                dy = -dy / 2;
            }
        } else {
            dy = ((int32_t)micro_smile * (500 - edge_weight)) / 32000;
        }

        pose_points[i].x = (gfx_coord_t)(base_x + dx);
        pose_points[i].y = (gfx_coord_t)(base_y + dy);
    }

    return gfx_mesh_img_set_points(scene->mouth_obj, pose_points, TEST_FACE_PARTS_MOUTH_POINT_COUNT);
}

static void test_face_parts_apply_eye_pose(test_face_parts_scene_t *scene)
{
    gfx_coord_t gaze_x = (gfx_coord_t)scene->current_gaze_x;
    gfx_coord_t gaze_y = (gfx_coord_t)scene->current_gaze_y;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->left_eye_obj);
    TEST_ASSERT_NOT_NULL(scene->right_eye_obj);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene->left_eye_obj,
                                            GFX_ALIGN_CENTER,
                                            TEST_FACE_PARTS_LEFT_EYE_X_OFS + gaze_x,
                                            TEST_FACE_PARTS_EYE_Y_OFS + gaze_y));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene->right_eye_obj,
                                            GFX_ALIGN_CENTER,
                                            TEST_FACE_PARTS_RIGHT_EYE_X_OFS + gaze_x,
                                            TEST_FACE_PARTS_EYE_Y_OFS + gaze_y));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_visible(scene->left_eye_obj, !scene->blink_closed));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_visible(scene->right_eye_obj, !scene->blink_closed));
}

static void test_face_parts_apply_scene_pose(test_face_parts_scene_t *scene)
{
    test_face_parts_mouth_mode_t mode;

    TEST_ASSERT_NOT_NULL(scene);
    mode = test_face_parts_select_mouth_mode(scene->current_smile);
    test_face_parts_set_mouth_mode(scene, mode);
    test_face_parts_apply_eye_pose(scene);
    TEST_ASSERT_EQUAL(ESP_OK, test_face_parts_apply_mouth_pose(scene, scene->current_smile));

    if (scene->hint_label != NULL) {
        const char *mode_name = (mode == TEST_FACE_PARTS_MOUTH_MODE_SMILE) ? "smile" :
                                (mode == TEST_FACE_PARTS_MOUTH_MODE_O) ? "o" : "flat";
        gfx_label_set_text_fmt(scene->hint_label, " gaze(%d,%d)  mouth:%s ",
                               (int)scene->current_gaze_x,
                               (int)scene->current_gaze_y,
                               mode_name);
    }
}

static void test_face_parts_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_face_parts_scene_t *scene = (test_face_parts_scene_t *)user_data;
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
    }

    if (event->type == GFX_TOUCH_EVENT_PRESS || (event->type == GFX_TOUCH_EVENT_MOVE && scene->dragging)) {
        scene->target_gaze_x = test_face_parts_clamp_i16(((int32_t)event->x - disp_center_x) / 4,
                                                         -TEST_FACE_PARTS_GAZE_LIMIT_X,
                                                         TEST_FACE_PARTS_GAZE_LIMIT_X);
        scene->target_gaze_y = test_face_parts_clamp_i16(((int32_t)event->y - (disp_center_y - 12)) / 6,
                                                         -TEST_FACE_PARTS_GAZE_LIMIT_Y,
                                                         TEST_FACE_PARTS_GAZE_LIMIT_Y);
        scene->target_smile = test_face_parts_clamp_i16(((int32_t)disp_center_y + 36 - event->y) / 2,
                                                        -TEST_FACE_PARTS_SMILE_LIMIT,
                                                        TEST_FACE_PARTS_SMILE_LIMIT);
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->dragging = false;
        scene->target_gaze_x = 0;
        scene->target_gaze_y = 0;
        scene->target_smile = 0;
    }
}

static void test_face_parts_anim_cb(void *user_data)
{
    test_face_parts_scene_t *scene = (test_face_parts_scene_t *)user_data;
    int16_t next_gaze_x;
    int16_t next_gaze_y;
    int16_t next_smile;
    bool next_blink_closed;

    if (scene == NULL) {
        return;
    }

    scene->blink_tick++;
    next_gaze_x = test_face_parts_follow_axis(scene->current_gaze_x, scene->target_gaze_x);
    next_gaze_y = test_face_parts_follow_axis(scene->current_gaze_y, scene->target_gaze_y);
    next_smile = test_face_parts_follow_axis(scene->current_smile, scene->target_smile);
    next_blink_closed = test_face_parts_blink_closed(scene->blink_tick);

    if (next_gaze_x == scene->current_gaze_x &&
            next_gaze_y == scene->current_gaze_y &&
            next_smile == scene->current_smile &&
            next_blink_closed == scene->blink_closed) {
        return;
    }

    scene->current_gaze_x = next_gaze_x;
    scene->current_gaze_y = next_gaze_y;
    scene->current_smile = next_smile;
    scene->blink_closed = next_blink_closed;
    test_face_parts_apply_scene_pose(scene);
}

static void test_face_parts_scene_cleanup(test_face_parts_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }
    if (scene->mouth_obj != NULL) {
        gfx_obj_delete(scene->mouth_obj);
        scene->mouth_obj = NULL;
    }
    if (scene->right_eye_obj != NULL) {
        gfx_obj_delete(scene->right_eye_obj);
        scene->right_eye_obj = NULL;
    }
    if (scene->left_eye_obj != NULL) {
        gfx_obj_delete(scene->left_eye_obj);
        scene->left_eye_obj = NULL;
    }
    if (scene->head_obj != NULL) {
        gfx_obj_delete(scene->head_obj);
        scene->head_obj = NULL;
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

static void test_face_parts_run(void)
{
    test_face_parts_scene_t scene = {0};

    test_app_log_case(TAG, "Face parts demo (head + two eyes + mouth)");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x07111F));

    scene.head_obj = gfx_img_create(disp_default);
    scene.left_eye_obj = gfx_img_create(disp_default);
    scene.right_eye_obj = gfx_img_create(disp_default);
    scene.mouth_obj = gfx_mesh_img_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    scene.hint_label = gfx_label_create(disp_default);
    scene.mouth_mode = TEST_FACE_PARTS_MOUTH_MODE_FLAT;
    TEST_ASSERT_NOT_NULL(scene.head_obj);
    TEST_ASSERT_NOT_NULL(scene.left_eye_obj);
    TEST_ASSERT_NOT_NULL(scene.right_eye_obj);
    TEST_ASSERT_NOT_NULL(scene.mouth_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.hint_label);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_img_set_src(scene.head_obj, (void *)&face_parts_head));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.head_obj, GFX_ALIGN_CENTER, 0, TEST_FACE_PARTS_HEAD_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_img_set_src(scene.left_eye_obj, (void *)&face_parts_eye));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_eye_obj, GFX_ALIGN_CENTER, TEST_FACE_PARTS_LEFT_EYE_X_OFS, TEST_FACE_PARTS_EYE_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_img_set_src(scene.right_eye_obj, (void *)&face_parts_eye));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_eye_obj, GFX_ALIGN_CENTER, TEST_FACE_PARTS_RIGHT_EYE_X_OFS, TEST_FACE_PARTS_EYE_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mouth_obj, (void *)&face_parts_mouth_flat));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mouth_obj, TEST_FACE_PARTS_MOUTH_GRID_COLS, TEST_FACE_PARTS_MOUTH_GRID_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mouth_obj, GFX_ALIGN_CENTER, 0, TEST_FACE_PARTS_MOUTH_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.head_obj, test_face_parts_touch_cb, &scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.left_eye_obj, test_face_parts_touch_cb, &scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.right_eye_obj, test_face_parts_touch_cb, &scene));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.mouth_obj, test_face_parts_touch_cb, &scene));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Parts Face"));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 290, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.hint_label, " gaze(0,0)  mouth:flat "));

    test_face_parts_capture_mouth_base_points(&scene);
    test_face_parts_apply_scene_pose(&scene);

    scene.anim_timer = gfx_timer_create(emote_handle, test_face_parts_anim_cb, TEST_FACE_PARTS_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Touch face parts to steer gaze and mouth; blink runs automatically");
    test_app_wait_for_observe(120000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_face_parts_scene_cleanup(&scene);
    test_app_unlock();
}

void test_face_parts_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_face_parts_run();
    test_app_runtime_close(&runtime);
}
