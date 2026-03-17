/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_timer";

static void test_timer_run(void)
{
    test_app_log_case(TAG, "Timer API Demo");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_handle_t timer = gfx_timer_create(emote_handle, clock_tm_callback, 1000, NULL);
    TEST_ASSERT_NOT_NULL(timer);
    test_app_unlock();

    test_app_wait_ms(3000);

    test_app_log_step(TAG, "Set period to 500 ms");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_set_period(timer, 500);
    test_app_unlock();

    test_app_wait_ms(3000);

    test_app_log_step(TAG, "Set repeat count");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_set_repeat_count(timer, 5);
    test_app_unlock();

    test_app_wait_ms(3000);

    test_app_log_step(TAG, "Pause timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_pause(timer);
    test_app_unlock();

    test_app_wait_ms(3000);

    test_app_log_step(TAG, "Resume timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_resume(timer);
    test_app_unlock();

    test_app_wait_ms(3000);

    test_app_log_step(TAG, "Reset timer");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_reset(timer);
    test_app_unlock();

    test_app_wait_ms(3000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_delete(emote_handle, timer);
    test_app_unlock();
}

TEST_CASE("gfx demo: timer api", "[demo][timer]")
{
    test_app_runtime_t runtime;
    esp_err_t ret = test_app_runtime_open(&runtime);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_timer_run();

    test_app_runtime_close(&runtime);
}
