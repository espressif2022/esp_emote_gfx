/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "harness/gfx_test_core_fixture.h"

static const char *const TAG = "test_timer_logic";

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
    gfx_test_core_ctx_t *core;
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
    test_timer_record_tick((test_timer_counter_t *)user_data);
}

static void test_timer_ref_cb(void *user_data)
{
    test_timer_record_tick((test_timer_counter_t *)user_data);
}

static void test_timer_expect_close_counts(const test_timer_phase_t *phase,
        const test_timer_counter_t *gfx_counter,
        const test_timer_counter_t *ref_counter)
{
    uint32_t gfx_count = (uint32_t)gfx_counter->count;
    uint32_t ref_count = (uint32_t)ref_counter->count;
    uint32_t delta = (gfx_count >= ref_count) ? (gfx_count - ref_count) : (ref_count - gfx_count);

    ESP_LOGI(TAG, "[%s] gfx=%" PRIu32 ", ref=%" PRIu32 ", delta=%" PRIu32,
             phase->name, gfx_count, ref_count, delta);

    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(phase->max_delta, delta, phase->name);
}

static void test_timer_wait_phase(const test_timer_phase_t *phase)
{
    ESP_LOGI(TAG, "--- %s ---", phase->name ? phase->name : "phase");
    vTaskDelay(pdMS_TO_TICKS(phase->observe_ms));
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
    if (scene->gfx_timer != NULL && scene->core != NULL) {
        gfx_timer_delete(scene->core->handle, scene->gfx_timer);
        scene->gfx_timer = NULL;
    }
}

static void test_timer_create_scene(gfx_test_core_ctx_t *core, test_timer_scene_t *scene)
{
    const esp_timer_create_args_t ref_timer_args = {
        .callback = test_timer_ref_cb,
        .arg = (void *) &scene->ref_counter,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "timer_ref",
        .skip_unhandled_events = false,
    };

    TEST_ASSERT_NOT_NULL(core);
    TEST_ASSERT_NOT_NULL(scene);

    scene->core = core;
    test_timer_counter_reset(&scene->gfx_counter);
    test_timer_counter_reset(&scene->ref_counter);

    scene->gfx_timer = gfx_timer_create(core->handle,
                                        test_timer_gfx_cb,
                                        TEST_TIMER_DEFAULT_PERIOD_MS,
                                        (void *)&scene->gfx_counter);
    TEST_ASSERT_NOT_NULL(scene->gfx_timer);

    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_create(&ref_timer_args, &scene->ref_timer));
    TEST_ASSERT_NOT_NULL(scene->ref_timer);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene->ref_timer, TEST_TIMER_DEFAULT_PERIOD_MS * 1000ULL));
}

static void test_timer_run(gfx_test_core_ctx_t *core)
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

    test_timer_create_scene(core, &scene);

    test_timer_wait_phase(&s_default_phase);
    test_timer_expect_close_counts(&s_default_phase, &scene.gfx_counter, &scene.ref_counter);

    gfx_timer_set_period(scene.gfx_timer, TEST_TIMER_FAST_PERIOD_MS);
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_phase(&s_fast_phase);
    test_timer_expect_close_counts(&s_fast_phase, &scene.gfx_counter, &scene.ref_counter);

    gfx_timer_pause(scene.gfx_timer);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    test_timer_wait_phase(&s_paused_phase);
    test_timer_expect_close_counts(&s_paused_phase, &scene.gfx_counter, &scene.ref_counter);

    gfx_timer_resume(scene.gfx_timer);
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_phase(&s_resume_phase);
    test_timer_expect_close_counts(&s_resume_phase, &scene.gfx_counter, &scene.ref_counter);

    gfx_timer_reset(scene.gfx_timer);
    test_timer_counter_reset(&scene.gfx_counter);
    test_timer_counter_reset(&scene.ref_counter);
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_stop(scene.ref_timer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_timer_start_periodic(scene.ref_timer, TEST_TIMER_FAST_PERIOD_MS * 1000ULL));
    test_timer_wait_phase(&s_reset_phase);
    test_timer_expect_close_counts(&s_reset_phase, &scene.gfx_counter, &scene.ref_counter);

    test_timer_scene_cleanup(&scene);
}

TEST_CASE("timer: gfx timer tracks esp_timer reference", "[unit][timer]")
{
    gfx_test_core_ctx_t core = {0};

    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_core_open(&core));
    test_timer_run(&core);
    gfx_test_core_close(&core);
}
