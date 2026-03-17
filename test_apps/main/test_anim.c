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
#include "common.h"

static const char *TAG = "test_anim";

static void test_anim_run(mmap_assets_handle_t assets_handle)
{
    test_app_log_case(TAG, "Animation Widget Demo");

    struct {
        int asset_id;
        const char *name;
    } test_cases[] = {
        {MMAP_TEST_ASSETS_MI_1_EYE_24BIT_AAF,  "MI_1_EYE 24-bit AAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_4BIT_AAF,   "MI_1_EYE 4-bit AAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_AAF, "MI_1_EYE 8-bit Huffman AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_24BIT_AAF,  "MI_2_EYE 24-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_4BIT_AAF,   "MI_2_EYE 4-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF,   "MI_2_EYE 8-bit AAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_AAF, "MI_2_EYE 8-bit Huffman AAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_EAF,   "MI_1_EYE 8-bit EAF"},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_EAF, "MI_1_EYE 8-bit Huffman EAF"},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_EAF, "MI_2_EYE 8-bit Huffman EAF"},
        {MMAP_TEST_ASSETS_TRANSPARENT_EAF,     "Transparent EAF"},
        {MMAP_TEST_ASSETS_ONLY_HEATSHRINK_4BIT_EAF, "Only Heatshrink 4-bit EAF"},
    };

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0xFF0000));
    test_app_unlock();

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        test_app_log_step(TAG, test_cases[i].name);

        TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());

        TEST_ASSERT_NOT_NULL(disp_default);
        gfx_obj_t *anim_obj = gfx_anim_create(disp_default);
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

        test_app_unlock();
        test_app_wait_ms(5000);

        TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
        gfx_anim_stop(anim_obj);
        test_app_unlock();

        test_app_wait_ms(2000);

        TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
        gfx_obj_delete(anim_obj);
        test_app_unlock();

        test_app_wait_ms(1000);
    }
}

TEST_CASE("gfx demo: animation widget", "[demo][anim]")
{
    test_app_runtime_t runtime;
    esp_err_t ret = test_app_runtime_open(&runtime);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_anim_run(runtime.assets_handle);

    test_app_runtime_close(&runtime);
}
