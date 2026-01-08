/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "test_common.h"

static const char *TAG = "test_timer";

static void test_timer_function(void)
{
    ESP_LOGI(TAG, "=== Testing Timer Function ===");

    gfx_emote_lock(emote_handle);
    gfx_timer_handle_t timer = gfx_timer_create(emote_handle, test_clock_tm_callback, 1000, label_tips);
    TEST_ASSERT_NOT_NULL(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_set_period(timer, 500);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_set_repeat_count(timer, 5);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_pause(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_resume(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_reset(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_timer_delete(emote_handle, timer);
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test timer function", "[timer]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_timer_function();

    test_cleanup_display_and_graphics(assets_handle);
}
