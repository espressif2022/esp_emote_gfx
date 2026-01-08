/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "test_common.h"

static const char *TAG = "test_anim";

static void test_animation_function(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Animation Function ===");

    // Define test cases for different bit depths and animation files
    struct {
        int asset_id;
        const char *name;
    } test_cases[] = {
        // AAF format animations
        {MMAP_TEST_ASSETS_MI_1_EYE_24BIT_AAF,  "MI_1_EYE 24-bit AAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_4BIT_AAF,   "MI_1_EYE 4-bit AAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_AAF, "MI_1_EYE 8-bit Huffman AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_24BIT_AAF,  "MI_2_EYE 24-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_4BIT_AAF,   "MI_2_EYE 4-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF,   "MI_2_EYE 8-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_AAF, "MI_2_EYE 8-bit Huffman AAF"},
        // EAF format animations
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_EAF,   "MI_1_EYE 8-bit EAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_EAF, "MI_1_EYE 8-bit Huffman EAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_EAF, "MI_2_EYE 8-bit Huffman EAF"},
        {MMAP_TEST_ASSETS_TRANSPARENT_EAF,     "Transparent EAF"},
    };

    gfx_emote_lock(emote_handle);
    gfx_emote_set_bg_color(emote_handle, GFX_COLOR_HEX(0xFF0000));
    gfx_emote_unlock(emote_handle);

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        ESP_LOGI(TAG, "--- Testing %s ---", test_cases[i].name);

        gfx_emote_lock(emote_handle);

        gfx_obj_t *anim_obj = gfx_anim_create(emote_handle);
        TEST_ASSERT_NOT_NULL(anim_obj);

        const void *anim_data = mmap_assets_get_mem(assets_handle, test_cases[i].asset_id);
        size_t anim_size = mmap_assets_get_size(assets_handle, test_cases[i].asset_id);
        esp_err_t ret = gfx_anim_set_src(anim_obj, anim_data, anim_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        if (strstr(test_cases[i].name, "MI_1_EYE")) {
            gfx_obj_set_pos(anim_obj, 20, 10);
            gfx_anim_set_auto_mirror(anim_obj, true);
        } else {
            gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
            gfx_anim_set_auto_mirror(anim_obj, false);
        }
        gfx_obj_set_size(anim_obj, 200, 150);
        gfx_anim_set_segment(anim_obj, 0, 90, 50, true);

        ret = gfx_anim_start(anim_obj);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        gfx_emote_unlock(emote_handle);

        vTaskDelay(pdMS_TO_TICKS(5000));  // 5 seconds per animation

        gfx_emote_lock(emote_handle);
        gfx_anim_stop(anim_obj);
        gfx_emote_unlock(emote_handle);

        vTaskDelay(pdMS_TO_TICKS(2 * 1000));

        gfx_emote_lock(emote_handle);
        gfx_obj_delete(anim_obj);
        gfx_emote_unlock(emote_handle);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "=== Animation Function Testing Completed ===");
}

TEST_CASE("test animation function", "[animation]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_animation_function(assets_handle);

    test_cleanup_display_and_graphics(assets_handle);
}
