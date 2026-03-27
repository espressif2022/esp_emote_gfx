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
}

static void test_anim_show_case(mmap_assets_handle_t assets_handle, gfx_obj_t *anim_obj, const test_anim_case_t *test_case)
{
    const void *anim_data = NULL;
    size_t anim_size = 0;
    gfx_anim_src_t anim_src;

    test_app_log_step(TAG, test_case->name);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);

    anim_data = mmap_assets_get_mem(assets_handle, test_case->asset_id);
    anim_size = mmap_assets_get_size(assets_handle, test_case->asset_id);
    anim_src.type = GFX_ANIM_SRC_TYPE_MEMORY;
    anim_src.data = anim_data;
    anim_src.data_len = anim_size;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src_desc(anim_obj, &anim_src));
    test_anim_apply_layout(anim_obj, test_case->name, test_case->auto_mirror);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_obj, 0, 0xFFFFFFFF, 50, true));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    test_app_unlock();
}

static void test_anim_run(mmap_assets_handle_t assets_handle)
{
    static const test_anim_case_t s_cases[] = {
        {MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF, "AAF 24-bit / MI_1_EYE", true, 2800},
        {MMAP_ASSETS_TEST_MI_1_EYE_4BIT_AAF, "AAF 4-bit / MI_1_EYE", true, 2800},
        {MMAP_ASSETS_TEST_MI_1_EYE_8BIT_HUFF_AAF, "AAF 8-bit Huffman / MI_1_EYE", true, 2800},
        {MMAP_ASSETS_TEST_MI_2_EYE_24BIT_AAF, "AAF 24-bit / MI_2_EYE", false, 2800},
        {MMAP_ASSETS_TEST_MI_2_EYE_4BIT_AAF, "AAF 4-bit / MI_2_EYE", false, 2800},
        {MMAP_ASSETS_TEST_MI_2_EYE_8BIT_AAF, "AAF 8-bit / MI_2_EYE", false, 2800},
        {MMAP_ASSETS_TEST_MI_2_EYE_8BIT_HUFF_AAF, "AAF 8-bit Huffman / MI_2_EYE", false, 2800},
        {MMAP_ASSETS_TEST_MI_1_EYE_8BIT_EAF, "EAF 8-bit / MI_1_EYE", true, 2800},
        {MMAP_ASSETS_TEST_MI_1_EYE_8BIT_HUFF_EAF, "EAF 8-bit Huffman / MI_1_EYE", true, 2800},
        {MMAP_ASSETS_TEST_MI_2_EYE_8BIT_HUFF_EAF, "EAF 8-bit Huffman / MI_2_EYE", false, 2800},
        {MMAP_ASSETS_TEST_TRANSPARENT_EAF, "EAF transparent", false, 3200},
        {MMAP_ASSETS_TEST_ONLY_HEATSHRINK_4BIT_EAF, "EAF heatshrink 4-bit", false, 3200},
    };

    test_app_log_case(TAG, "Animation decoder validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x101820));
    gfx_obj_t *anim_obj = gfx_anim_create(disp_default);
    TEST_ASSERT_NOT_NULL(anim_obj);
    test_app_unlock();

    size_t case_index = 0;
    while (1) {
        const test_anim_case_t *c = &s_cases[case_index];
        test_anim_show_case(assets_handle, anim_obj, c);
        test_app_wait_for_observe(c->observe_ms);
        if (gfx_anim_play_left_to_tail(anim_obj) == ESP_OK) {
            test_app_log_step(TAG, "drain remaining segments done");
        }
        case_index = (case_index + 1) % TEST_APP_ARRAY_SIZE(s_cases);
    }
}

// TEST_CASE("widget animation decoder matrix", "[widget][anim][matrix]")
void test_anim_run_case_matrix(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_anim_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
