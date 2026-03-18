/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "common.h"

static const char *TAG = "test_anim";

typedef struct {
    int asset_id;
    const char *name;
    bool auto_mirror;
    uint32_t observe_ms;
} test_anim_case_t;

static void test_anim_apply_layout(gfx_obj_t *anim_obj, const char *name, bool auto_mirror)
{
    if (strstr(name, "MI_1_EYE") != NULL) {
        gfx_obj_set_pos(anim_obj, 20, 10);
    } else {
        gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    }

    gfx_obj_set_size(anim_obj, 200, 150);
    gfx_anim_set_auto_mirror(anim_obj, auto_mirror);
    gfx_anim_set_segment(anim_obj, 0, 90, 50, true);
}

static void test_anim_run_case(mmap_assets_handle_t assets_handle, const test_anim_case_t *test_case)
{
    const void *anim_data = NULL;
    size_t anim_size = 0;
    gfx_obj_t *anim_obj = NULL;

    test_app_log_step(TAG, test_case->name);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    anim_obj = gfx_anim_create(disp_default);
    TEST_ASSERT_NOT_NULL(anim_obj);

    anim_data = mmap_assets_get_mem(assets_handle, test_case->asset_id);
    anim_size = mmap_assets_get_size(assets_handle, test_case->asset_id);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_obj, anim_data, anim_size));

    test_anim_apply_layout(anim_obj, test_case->name, test_case->auto_mirror);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    test_app_unlock();

    test_app_wait_for_observe(test_case->observe_ms);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);
    gfx_obj_delete(anim_obj);
    test_app_unlock();

    test_app_wait_for_observe(600);
}

static void test_anim_run(mmap_assets_handle_t assets_handle)
{
    static const test_anim_case_t s_cases[] = {
        {MMAP_TEST_ASSETS_MI_1_EYE_24BIT_AAF, "AAF 24-bit / MI_1_EYE", true, 2800},
        {MMAP_TEST_ASSETS_MI_1_EYE_4BIT_AAF, "AAF 4-bit / MI_1_EYE", true, 2800},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_AAF, "AAF 8-bit Huffman / MI_1_EYE", true, 2800},
        {MMAP_TEST_ASSETS_MI_2_EYE_24BIT_AAF, "AAF 24-bit / MI_2_EYE", false, 2800},
        {MMAP_TEST_ASSETS_MI_2_EYE_4BIT_AAF, "AAF 4-bit / MI_2_EYE", false, 2800},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF, "AAF 8-bit / MI_2_EYE", false, 2800},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_AAF, "AAF 8-bit Huffman / MI_2_EYE", false, 2800},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_EAF, "EAF 8-bit / MI_1_EYE", true, 2800},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_HUFF_EAF, "EAF 8-bit Huffman / MI_1_EYE", true, 2800},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_HUFF_EAF, "EAF 8-bit Huffman / MI_2_EYE", false, 2800},
        {MMAP_TEST_ASSETS_TRANSPARENT_EAF, "EAF transparent", false, 3200},
        {MMAP_TEST_ASSETS_ONLY_HEATSHRINK_4BIT_EAF, "EAF heatshrink 4-bit", false, 3200},
    };

    test_app_log_case(TAG, "Animation decoder validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x101820));
    test_app_unlock();

    for (size_t i = 0; i < TEST_APP_ARRAY_SIZE(s_cases); ++i) {
        test_anim_run_case(assets_handle, &s_cases[i]);
    }
}

TEST_CASE("widget animation decoder matrix", "[widget][anim][matrix]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_anim_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
