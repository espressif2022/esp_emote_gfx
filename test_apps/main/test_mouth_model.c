/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_random.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_mouth_model";

#include "test_mouth_model_keyframes.inc"

typedef struct {
    gfx_obj_t *face_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t expr_timer;
    gfx_face_emote_cfg_t cfg;
    bool use_chinese;
    gfx_face_emote_mix_t current_mix;
} test_mouth_model_scene_t;

static const gfx_face_emote_assets_t s_face_assets = {
    .ref_eye = s_ref_eye,
    .ref_eye_count = TEST_APP_ARRAY_SIZE(s_ref_eye),
    .ref_brow = s_ref_brow,
    .ref_brow_count = TEST_APP_ARRAY_SIZE(s_ref_brow),
    .ref_mouth = s_ref_mouth,
    .ref_mouth_count = TEST_APP_ARRAY_SIZE(s_ref_mouth),
    .sequence = s_face_sequence,
    .sequence_count = TEST_APP_ARRAY_SIZE(s_face_sequence),
};

static int16_t test_mouth_model_random_signed(int16_t limit)
{
    uint32_t span = (uint32_t)(limit * 2) + 1U;
    return (int16_t)((int32_t)(esp_random() % span) - limit);
}

static int16_t test_mouth_model_random_percent(void)
{
    return (int16_t)(esp_random() % 101U);
}

static void test_mouth_model_randomize_mix(gfx_face_emote_mix_t *mix)
{
    if (mix == NULL) {
        return;
    }

    mix->w_smile = test_mouth_model_random_percent();
    mix->w_happy = test_mouth_model_random_percent();
    mix->w_sad = test_mouth_model_random_percent();
    mix->w_surprise = test_mouth_model_random_percent();
    mix->w_angry = test_mouth_model_random_percent();
    mix->look_x = test_mouth_model_random_signed(22);
    mix->look_y = test_mouth_model_random_signed(22);
}

static void test_mouth_model_apply_mix(test_mouth_model_scene_t *scene, bool snap_now)
{
    if (scene == NULL || scene->face_obj == NULL) {
        return;
    }

    gfx_face_emote_set_mix(scene->face_obj, &scene->current_mix, snap_now);
    ESP_LOGI(TAG, "set mix sm=%d hp=%d sd=%d su=%d an=%d lx=%d ly=%d",
             scene->current_mix.w_smile, scene->current_mix.w_happy,
             scene->current_mix.w_sad, scene->current_mix.w_surprise,
             scene->current_mix.w_angry, scene->current_mix.look_x,
             scene->current_mix.look_y);
}

static void test_mouth_model_update_title(test_mouth_model_scene_t *scene)
{
    if (scene == NULL || scene->title_label == NULL) {
        return;
    }

    gfx_label_set_text_fmt(scene->title_label, "  mix h%02d s%02d u%02d a%02d",
                           scene->current_mix.w_happy, scene->current_mix.w_smile,
                           scene->current_mix.w_surprise, scene->current_mix.w_angry);
}

static void test_mouth_model_expr_timer_cb(void *user_data)
{
    test_mouth_model_scene_t *scene = (test_mouth_model_scene_t *)user_data;

    if (scene == NULL || scene->face_obj == NULL) {
        return;
    }

    test_mouth_model_randomize_mix(&scene->current_mix);
    test_mouth_model_apply_mix(scene, false);
    test_mouth_model_update_title(scene);
}

static void test_mouth_model_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_mouth_model_scene_t *scene = (test_mouth_model_scene_t *)user_data;
    gfx_coord_t face_x = 0;
    gfx_coord_t face_y = 0;
    int16_t look_x;
    int16_t look_y;

    (void)touch;

    if (scene == NULL || scene->face_obj == NULL || event == NULL) {
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        gfx_face_emote_set_manual_look(scene->face_obj, false, 0, 0);
        test_mouth_model_apply_mix(scene, false);
        return;
    }

    gfx_obj_get_pos(scene->face_obj, &face_x, &face_y);
    look_x = (int16_t)(((int32_t)event->x - ((int32_t)face_x + (scene->cfg.display_w / 2))) * 80 / (scene->cfg.display_w / 2));
    look_y = (int16_t)(((int32_t)event->y - ((int32_t)face_y + (scene->cfg.display_h / 2))) * 80 / (scene->cfg.display_h / 2));
    gfx_face_emote_set_manual_look(scene->face_obj, true, look_x, look_y);
}

static void test_mouth_model_run(void)
{
    test_mouth_model_scene_t scene = {0};

    test_app_log_case(TAG, "Standard face emote widget");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.face_obj = gfx_face_emote_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.face_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);

    gfx_face_emote_cfg_init(&scene.cfg);
    scene.use_chinese = true;

    TEST_ASSERT_EQUAL(ESP_OK, gfx_face_emote_set_config(scene.face_obj, &scene.cfg));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.face_obj, GFX_ALIGN_CENTER, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_face_emote_set_assets(scene.face_obj, &s_face_assets));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    test_mouth_model_randomize_mix(&scene.current_mix);
    test_mouth_model_apply_mix(&scene, true);
    test_mouth_model_update_title(&scene);
    scene.expr_timer = gfx_timer_create(emote_handle, test_mouth_model_expr_timer_cb, 1000U, &scene);
    TEST_ASSERT_NOT_NULL(scene.expr_timer);
    test_app_set_touch_event_cb(test_mouth_model_touch_event_cb, &scene);

    test_app_unlock();

    test_app_log_step(TAG, "Touch anywhere to steer gaze; random mix updates every 1 second");
    test_app_wait_for_observe(1000U * 10000U);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_app_set_touch_event_cb(NULL, NULL);
    if (scene.expr_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.expr_timer);
    }
    gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.face_obj);
    test_app_unlock();
}

void test_mouth_model_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_mouth_model_run();
    test_app_runtime_close(&runtime);
}
