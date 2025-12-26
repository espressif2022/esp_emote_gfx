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
#include "widget/gfx_qrcode.h"

static const char *TAG = "test_qrcode";

static void test_qrcode_function(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing QR Code Function ===");

    gfx_emote_lock(emote_handle);

    // Test 1: Basic QR Code creation and data setting
    ESP_LOGI(TAG, "--- Test 1: Basic QR Code ---");
    gfx_obj_t *qrcode_obj1 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj1);

    esp_err_t ret = gfx_qrcode_set_data(qrcode_obj1, "https://www.espressif.com");
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = gfx_qrcode_set_size(qrcode_obj1, 150);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    gfx_qrcode_set_color(qrcode_obj1, GFX_COLOR_HEX(0x000000));
    gfx_qrcode_set_bg_color(qrcode_obj1, GFX_COLOR_HEX(0xFFFFFF));
    gfx_obj_align(qrcode_obj1, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    // Test 2: Different error correction levels
    ESP_LOGI(TAG, "--- Test 2: Different ECC Levels ---");
    gfx_emote_lock(emote_handle);

    gfx_obj_t *qrcode_obj2 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj2);

    gfx_qrcode_set_data(qrcode_obj2, "Hello, QR Code!");
    gfx_qrcode_set_size(qrcode_obj2, 120);
    gfx_qrcode_set_ecc(qrcode_obj2, GFX_QRCODE_ECC_LOW);
    gfx_qrcode_set_color(qrcode_obj2, GFX_COLOR_HEX(0xFF0000));
    gfx_qrcode_set_bg_color(qrcode_obj2, GFX_COLOR_HEX(0xFFFFFF));
    gfx_obj_align(qrcode_obj2, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_ecc(qrcode_obj2, GFX_QRCODE_ECC_MEDIUM);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_ecc(qrcode_obj2, GFX_QRCODE_ECC_QUARTILE);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_ecc(qrcode_obj2, GFX_QRCODE_ECC_HIGH);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    // Test 3: Different sizes
    ESP_LOGI(TAG, "--- Test 3: Different Sizes ---");
    gfx_emote_lock(emote_handle);

    gfx_obj_t *qrcode_obj3 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj3);

    gfx_qrcode_set_data(qrcode_obj3, "Size Test");
    gfx_qrcode_set_size(qrcode_obj3, 100);
    gfx_qrcode_set_color(qrcode_obj3, GFX_COLOR_HEX(0x0000FF));
    gfx_qrcode_set_bg_color(qrcode_obj3, GFX_COLOR_HEX(0xFFFF00));
    gfx_obj_align(qrcode_obj3, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_size(qrcode_obj3, 180);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_size(qrcode_obj3, 80);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    // Test 4: Different colors
    ESP_LOGI(TAG, "--- Test 4: Different Colors ---");
    gfx_emote_lock(emote_handle);

    gfx_obj_t *qrcode_obj4 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj4);

    gfx_qrcode_set_data(qrcode_obj4, "Color Test");
    gfx_qrcode_set_size(qrcode_obj4, 130);
    gfx_qrcode_set_color(qrcode_obj4, GFX_COLOR_HEX(0x00FF00));
    gfx_qrcode_set_bg_color(qrcode_obj4, GFX_COLOR_HEX(0x000000));
    gfx_obj_align(qrcode_obj4, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_qrcode_set_color(qrcode_obj4, GFX_COLOR_HEX(0xFF00FF));
    gfx_qrcode_set_bg_color(qrcode_obj4, GFX_COLOR_HEX(0x00FFFF));
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    // Test 5: Alignment and positioning
    ESP_LOGI(TAG, "--- Test 5: Alignment and Positioning ---");
    gfx_emote_lock(emote_handle);

    gfx_obj_t *qrcode_obj5 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj5);

    gfx_qrcode_set_data(qrcode_obj5, "Alignment Test");
    gfx_qrcode_set_size(qrcode_obj5, 100);
    gfx_qrcode_set_ecc(qrcode_obj5, GFX_QRCODE_ECC_HIGH);
    gfx_obj_align(qrcode_obj5, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_align(qrcode_obj5, GFX_ALIGN_TOP_LEFT, 0, 0);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_align(qrcode_obj5, GFX_ALIGN_BOTTOM_RIGHT, 0, 0);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    // Test 6: Long text data
    ESP_LOGI(TAG, "--- Test 6: Long Text Data ---");
    gfx_emote_lock(emote_handle);

    gfx_obj_t *qrcode_obj6 = gfx_qrcode_create(emote_handle);
    TEST_ASSERT_NOT_NULL(qrcode_obj6);

    const char *long_text = "This is a longer text to test QR code";
    gfx_qrcode_set_data(qrcode_obj6, long_text);
    gfx_qrcode_set_size(qrcode_obj6, 200);
    gfx_qrcode_set_ecc(qrcode_obj6, GFX_QRCODE_ECC_HIGH);
    gfx_qrcode_set_color(qrcode_obj6, GFX_COLOR_HEX(0x000000));
    gfx_qrcode_set_bg_color(qrcode_obj6, GFX_COLOR_HEX(0xFFFFFF));
    gfx_obj_align(qrcode_obj6, GFX_ALIGN_CENTER, 0, 0);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    // Cleanup
    ESP_LOGI(TAG, "--- Cleanup ---");
    gfx_emote_lock(emote_handle);
    gfx_obj_delete(qrcode_obj1);
    gfx_obj_delete(qrcode_obj2);
    gfx_obj_delete(qrcode_obj3);
    gfx_obj_delete(qrcode_obj4);
    gfx_obj_delete(qrcode_obj5);
    gfx_obj_delete(qrcode_obj6);
    gfx_emote_unlock(emote_handle);

    ESP_LOGI(TAG, "=== QR Code Function Testing Completed ===");
}

TEST_CASE("test qrcode function", "[qrcode]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_qrcode_function(assets_handle);

    test_cleanup_display_and_graphics(assets_handle);
}

