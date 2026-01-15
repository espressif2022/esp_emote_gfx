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

static const char *TAG = "test_image";

static void test_image_function(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc;

    ESP_LOGI(TAG, "=== Testing Image Function ===");

    gfx_emote_lock(emote_handle);

    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_obj_t *img_obj_c_array = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj_c_array);

    gfx_img_set_src(img_obj_c_array, (void *)&icon_rgb565);
    gfx_obj_set_pos(img_obj_c_array, 100, 100);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    //test different pos
    ESP_LOGI(TAG, "--- Test different pos with set_pos ---");
    gfx_emote_lock(emote_handle);
    gfx_obj_set_pos(img_obj_c_array, 200, 100);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    ESP_LOGI(TAG, "--- Test different pos with align ---");
    gfx_emote_lock(emote_handle);
    gfx_obj_align(img_obj_c_array, GFX_ALIGN_CENTER, 0, 0);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj_c_array);

    gfx_obj_t *img_obj_bin = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj_bin);

    esp_err_t ret = load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565A8_BIN, &img_dsc);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    gfx_img_set_src(img_obj_bin, (void *)&img_dsc);
    gfx_obj_set_pos(img_obj_bin, 100, 180);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    ESP_LOGI(TAG, "--- Test from big to small image ---");
    gfx_emote_lock(emote_handle);
    load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565A8_BIN, &img_dsc);
    gfx_img_set_src(img_obj_bin, (void *)&img_dsc);
    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(6 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj_bin);

    ESP_LOGI(TAG, "--- Testing multiple images with different formats ---");
    gfx_obj_t *img_obj1 = gfx_img_create(disp_default);
    gfx_obj_t *img_obj2 = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj1);
    TEST_ASSERT_NOT_NULL(img_obj2);

    gfx_img_set_src(img_obj1, (void *)&icon_rgb565A8); // C_ARRAY format

    ret = load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565_BIN, &img_dsc);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    gfx_img_set_src(img_obj2, (void *)&img_dsc); // BIN format

    gfx_obj_set_pos(img_obj1, 150, 100);
    gfx_obj_set_pos(img_obj2, 150, 180);

    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj1);
    gfx_obj_delete(img_obj2);
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test function obj image", "")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = display_and_graphics_init("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_image_function(assets_handle);

    display_and_graphics_clean(assets_handle);
}
