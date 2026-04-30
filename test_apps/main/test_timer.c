/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_timer";

#define TEST_TIMER_DEFAULT_PERIOD_MS      200U
#define TEST_TIMER_FAST_PERIOD_MS         100U
#define TEST_TIMER_DEFAULT_OBSERVE_MS     2200U
#define TEST_TIMER_FAST_OBSERVE_MS        1400U
#define TEST_TIMER_PAUSE_OBSERVE_MS       700U
#define TEST_TIMER_RESUME_OBSERVE_MS      1200U
#define TEST_TIMER_RESET_OBSERVE_MS       700U
#define TEST_TIMER_COUNT_TOLERANCE        2U
#define TEST_TIMER_NO_TICK_TOLERANCE      0U

typedef struct {
    volatile uint32_t count;
    volatile int64_t first_tick_us;
    volatile int64_t last_tick_us;
} test_timer_counter_t;

typedef struct {
    gfx_obj_t *status_label;
    gfx_timer_handle_t gfx_timer;
    esp_timer_handle_t ref_timer;
    test_timer_counter_t gfx_counter;
    test_timer_counter_t ref_counter;
} test_timer_scene_t;

typedef struct {
    const char *name;
    uint32_t observe_ms;
    uint32_t max_delta;
} test_timer_phase_t;

static void test_timer_counter_reset(test_timer_counter_t *counter)
{
    if (counter == NULL) {
        return;
    }

    counter->count = 0;
    counter->first_tick_us = 0;
    counter->last_tick_us = 0;
}

static void test_timer_record_tick(test_timer_counter_t *counter)
{
    int64_t now_us = esp_timer_get_time();

    if (counter->count == 0) {
        counter->first_tick_us = now_us;
    }

    counter->count++;
    counter->last_tick_us = now_us;
}

static void test_timer_gfx_cb(void *user_data)
{
    test_timer_counter_t *counter = (test_timer_counter_t *)user_data;

    test_timer_record_tick(counter);
}

static void test_timer_ref_cb(void *user_data)
{
    test_timer_counter_t *counter = (test_timer_counter_t *)user_data;

    test_timer_record_tick(counter);
}

static void test_timer_update_status(test_timer_scene_t *scene, const char *title)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->status_label);

    gfx_label_set_text_fmt(scene->status_label,
                           "%s\nG:%" PRIu32 "  R:%" PRIu32,
                           title ? title : "timer",
                           (uint32_t)scene->gfx_counter.count,
                           (uint32_t)scene->ref_counter.count);
}

static void test_timer_expect_close_counts(const test_timer_phase_t *phase,
        const test_timer_counter_t *gfx_counter,
        const test_timer_counter_t *ref_counter)
{
    uint32_t gfx_count = (uint32_t)gfx_counter->count;
    uint32_t ref_count = (uint32_t)ref_counter->count;
    uint32_t delta = (gfx_count >= ref_count) ? (gfx_count - ref_count) : (ref_count - gfx_count);

    ESP_LOGI(TAG,
             "[%s] gfx=%" PRIu32 ", ref=%" PRIu32 ", delta=%" PRIu32,
             phase->name,
             gfx_count,
             ref_count,
             delta);

    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(phase->max_delta,
            delta,
            phase->name);
}

static void test_timer_wait_and_validate(test_timer_scene_t *scene, const test_timer_phase_t *phase)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(phase);

    test_app_log_step(TAG, phase->name);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_timer_update_status(scene, phase->name);
    test_app_unlock();

    test_app_wait_for_observe(phase->observe_ms);
    test_timer_expect_close_counts(phase, &scene->gfx_counter, &scene->ref_counter);
}

static void test_timer_scene_cleanup(test_timer_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->ref_timer != NULL) {
        esp_timer_stop(scene->ref_timer);
        esp_timer_delete(scene->ref_timer);
        scene->ref_timer = NULL;
    }
    if (scene->gfx_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->gfx_timer);
        scene->gfx_timer = NULL;
    }
    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
}

static void test_timer_create_scene(test_timer_scene_t *scene)
{
    const esp_timer_create_args_t ref_timer_args = {
        .callback = test_timer_ref_cb,
        .arg = (void *) &scene->ref_counter,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "timer_ref",
        .skip_unhandled_events = false,
    };

    TEST_ASSERT_NOT_NULL(scene);
    test_timer_counter_reset(&scene->gfx_counter);
    test_timer_counter_reset(&scene->ref_counter);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene->status_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene->status_label);
    gfx_obj_set_size(scene->status_label, 260, 56);
    gfx_obj_align(scene->status_label, GFX_ALIGN_CENTER, 0, 0);
    gfx_label_set_font(scene->status_label, (gfx_font_t)&font_puhui_16_4);
    gfx_label_set_text_align(scene->status_label, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(scene->status_label, GFX_LABEL_LONG_WRAP);
    gfx_label_set_text(scene->status_label, "Timer validation");
    gfx_label_set_color(scene->status_label, GFX_COLOR_HEX(0xFFFFFF));

    scene->gfx_timer = gfx_timer_create(emote_handle,
                                        test_timer_gfx_cb,
                                        TEST_TIMER_DEFAULT_PERIOD_MS,
                                        (void *)&scene->gfx_counter);
    TEST_ASSERT_NOT_NULL(scene->gfx_timer);
    test_app_unlock();

    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_create(&ref_timer_args, &scene->ref_timer));
    TEST_ASSERT_NOT_NULL(scene->ref_timer);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene->ref_timer, TEST_TIMER_DEFAULT_PERIOD_MS * 1000ULL));
}

static void test_timer_run(void)
{
    static const test_timer_phase_t s_default_phase = {
        .name = "default period 200 ms",
        .observe_ms = TEST_TIMER_DEFAULT_OBSERVE_MS,
        .max_delta = TEST_TIMER_COUNT_TOLERANCE,
    };
    static const test_timer_phase_t s_fast_phase = {
        .name = "period switch 100 ms",
        .observe_ms = TEST_TIMER_FAST_OBSERVE_MS,
        .max_delta = TEST_TIMER_COUNT_TOLERANCE,
    };
    static const test_timer_phase_t s_paused_phase = {
        .name = "pause gate",
        .observe_ms = TEST_TIMER_PAUSE_OBSERVE_MS,
        .max_delta = TEST_TIMER_NO_TICK_TOLERANCE,
    };
    static const test_timer_phase_t s_resume_phase = {
        .name = "resume 100 ms",
        .observe_ms = TEST_TIMER_RESUME_OBSERVE_MS,
        .max_delta = TEST_TIMER_COUNT_TOLERANCE,
    };
    static const test_timer_phase_t s_reset_phase = {
        .name = "reset 100 ms",
        .observe_ms = TEST_TIMER_RESET_OBSERVE_MS,
        .max_delta = TEST_TIMER_COUNT_TOLERANCE,
    };

    test_timer_scene_t scene = {0};

    test_app_log_case(TAG, "Timer API closed-loop validation");
    test_timer_create_scene(&scene);

    test_timer_wait_and_validate(&scene, &s_default_phase);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_set_period(scene.gfx_timer, TEST_TIMER_FAST_PERIOD_MS);
    test_app_unlock();
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_and_validate(&scene, &s_fast_phase);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_pause(scene.gfx_timer);
    test_app_unlock();
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    test_timer_wait_and_validate(&scene, &s_paused_phase);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_resume(scene.gfx_timer);
    test_app_unlock();
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_and_validate(&scene, &s_resume_phase);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_reset(scene.gfx_timer);
    test_app_unlock();
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_and_validate(&scene, &s_reset_phase);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_timer_update_status(&scene, "timer validation done");
    test_timer_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("timer: api validate case", "[timer]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_timer_run();
    test_app_runtime_close(&runtime);
}
