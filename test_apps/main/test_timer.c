/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"

static const char *TAG = "test_timer";

typedef struct {
    gfx_obj_t *status_label;
    gfx_timer_handle_t timer;
} test_timer_scene_t;

static void test_timer_scene_cleanup(test_timer_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->timer != NULL) {
        gfx_timer_delete(emote_handle, scene->timer);
        scene->timer = NULL;
    }
    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
}

static void test_timer_run(void)
{
    test_timer_scene_t scene = {0};

    test_app_log_case(TAG, "Timer API validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene.status_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.status_label);
    gfx_obj_set_size(scene.status_label, 240, 48);
    gfx_obj_align(scene.status_label, GFX_ALIGN_CENTER, 0, 0);
    gfx_label_set_font(scene.status_label, (gfx_font_t)&font_puhui_16_4);
    gfx_label_set_text_align(scene.status_label, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(scene.status_label, GFX_LABEL_LONG_WRAP);
    gfx_label_set_text(scene.status_label, "Timer created\nperiod=1000 ms");

    scene.timer = gfx_timer_create(emote_handle, clock_tm_callback, 1000, scene.status_label);
    TEST_ASSERT_NOT_NULL(scene.timer);
    test_app_unlock();

    test_app_wait_for_observe(2500);

    test_app_log_step(TAG, "Set period to 500 ms");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.status_label, "Timer running\nperiod=500 ms");
    gfx_timer_set_period(scene.timer, 500);
    test_app_unlock();

    test_app_wait_for_observe(2500);

    test_app_log_step(TAG, "Limit repeat count to 5");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.status_label, "Timer running\nrepeat=5");
    gfx_timer_set_repeat_count(scene.timer, 5);
    test_app_unlock();

    test_app_wait_for_observe(2500);

    test_app_log_step(TAG, "Pause timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.status_label, "Timer paused");
    gfx_timer_pause(scene.timer);
    test_app_unlock();

    test_app_wait_for_observe(1800);

    test_app_log_step(TAG, "Resume timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.status_label, "Timer resumed");
    gfx_timer_resume(scene.timer);
    test_app_unlock();

    test_app_wait_for_observe(2000);

    test_app_log_step(TAG, "Reset timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.status_label, "Timer reset");
    gfx_timer_reset(scene.timer);
    test_app_unlock();

    test_app_wait_for_observe(2200);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_timer_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("gfx verify: timer api", "[verify][timer]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_timer_run();
    test_app_runtime_close(&runtime);
}
