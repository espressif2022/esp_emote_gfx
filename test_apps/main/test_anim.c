/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_anim";
static int32_t s_drag_offset_x = 0;
static int32_t s_drag_offset_y = 0;
static bool s_drag_active = false;
static gfx_obj_t *s_drag_obj = NULL;

enum {
    TEST_ANIM_EVENT_NEXT = BIT0,
};

static EventGroupHandle_t s_anim_events;

static void test_anim_next_btn_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    (void)obj;
    (void)user_data;
    if (event != NULL && event->type == GFX_TOUCH_EVENT_RELEASE && s_anim_events != NULL) {
        xEventGroupSetBits(s_anim_events, TEST_ANIM_EVENT_NEXT);
        ESP_LOGW(TAG, "Next");
    }
}

static void test_anim_image_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    gfx_coord_t obj_x = 0;
    gfx_coord_t obj_y = 0;

    (void)user_data;

    if (obj == NULL || event == NULL) {
        return;
    }

    gfx_obj_get_pos(obj, &obj_x, &obj_y);

    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        s_drag_offset_x = (int32_t)event->x - obj_x;
        s_drag_offset_y = (int32_t)event->y - obj_y;
        s_drag_active = true;
        s_drag_obj = obj;
    }

    if (s_drag_active && s_drag_obj == obj) {
        gfx_obj_set_pos(obj, (int32_t)event->x - s_drag_offset_x, (int32_t)event->y - s_drag_offset_y);
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE && s_drag_obj == obj) {
        s_drag_active = false;
        s_drag_obj = NULL;
    }
}

typedef struct {
    int asset_id;
    const char *name;
    bool auto_mirror;
    uint32_t observe_ms;
} test_anim_case_t;

typedef struct {
    char prefix[16];
    int value_a;
    int value_b;
    int value_c;
    bool valid;
} test_anim_case_numbers_t;

typedef struct {
    gfx_obj_t *img_primary;
    gfx_obj_t *img_secondary;
} test_anim_image_scene_t;

static void test_anim_image_scene_cleanup(test_anim_image_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->img_primary != NULL) {
        gfx_obj_delete(scene->img_primary);
        scene->img_primary = NULL;
    }

    if (scene->img_secondary != NULL) {
        gfx_obj_delete(scene->img_secondary);
        scene->img_secondary = NULL;
    }
}

static test_anim_case_numbers_t test_anim_parse_case_numbers(const char *name)
{
    test_anim_case_numbers_t numbers = {0};

    if (name == NULL) {
        return numbers;
    }

    if (sscanf(name, "%15[^_]_%d_%d_%d", numbers.prefix, &numbers.value_a, &numbers.value_b, &numbers.value_c) == 4) {
        numbers.valid = true;
    }

    return numbers;
}

static void test_anim_log_case_numbers(const test_anim_case_numbers_t *numbers, const char *name)
{
    if (numbers == NULL) {
        return;
    }

    if (numbers->valid) {
        ESP_LOGI(TAG, "case parsed: %s -> %d, %d, %d",
                 numbers->prefix, numbers->value_a, numbers->value_b, numbers->value_c);
    } else {
        ESP_LOGW(TAG, "case name format mismatch: %s", name);
    }
}

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
    test_anim_case_numbers_t numbers;
    gfx_anim_segment_t segments[3];

    test_app_log_step(TAG, test_case->name);
    numbers = test_anim_parse_case_numbers(test_case->name);
    test_anim_log_case_numbers(&numbers, test_case->name);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);

    anim_data = mmap_assets_get_mem(assets_handle, test_case->asset_id);
    anim_size = mmap_assets_get_size(assets_handle, test_case->asset_id);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_obj, anim_data, anim_size));
    test_anim_apply_layout(anim_obj, test_case->name, test_case->auto_mirror);

    if (numbers.valid) {
        segments[0].start = 0;
        segments[0].end = (uint32_t)numbers.value_a - 1;
        segments[0].fps = 50;
        segments[0].play_count = 1;
        segments[0].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[1].start = (uint32_t)numbers.value_a;
        segments[1].end = (uint32_t)numbers.value_b - 1;
        segments[1].fps = 50;
        segments[1].play_count = 5;
        // segments[1].end_action = GFX_ANIM_SEGMENT_ACTION_PAUSE;
        segments[1].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[2].start = (uint32_t)numbers.value_b;
        segments[2].end = (uint32_t)numbers.value_c - 1;
        segments[2].fps = 50;
        segments[2].play_count = 1;
        segments[2].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segments(anim_obj, segments, TEST_APP_ARRAY_SIZE(segments)));
    } else {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_obj, 0, 0xFFFFFFFF, 50, true));
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    test_app_unlock();
}


static void test_anim_show_case_image(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc = {0};
    test_anim_image_scene_t scene = {0};

    test_app_log_case(TAG, "Image widget show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x101820));

    scene.img_primary = gfx_img_create(disp_default);
    scene.img_secondary = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.img_primary);
    TEST_ASSERT_NOT_NULL(scene.img_secondary);

    gfx_img_set_src(scene.img_primary, (void *)&icon_rgb565);
    gfx_obj_set_pos(scene.img_primary, 80, 90);
    gfx_obj_align(scene.img_primary, GFX_ALIGN_CENTER, -100, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.img_primary, test_anim_image_touch_cb, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565A8_BIN, &img_dsc));
    gfx_img_set_src(scene.img_secondary, (void *)&img_dsc);
    gfx_obj_align(scene.img_secondary, GFX_ALIGN_CENTER, 100, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.img_secondary, test_anim_image_touch_cb, NULL));
    test_app_unlock();

    test_app_wait_for_observe(1000 * 1000);
}

static void test_anim_run(mmap_assets_handle_t assets_handle)
{
#if 0
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
#else
    static const test_anim_case_t s_cases[] = {
        {MMAP_TEST_ASSETS_A1_017_041_075_EAF, "A1_017_041_075", false, 28000},
        {MMAP_TEST_ASSETS_A2_018_039_063_EAF, "A2_018_039_063", false, 28000},
        {MMAP_TEST_ASSETS_A3_019_048_075_EAF, "A3_019_048_075", false, 28000},
        {MMAP_TEST_ASSETS_A4_019_083_100_EAF, "A4_019_083_100", false, 28000},
        {MMAP_TEST_ASSETS_A5_022_053_100_EAF, "A5_022_053_100", false, 28000},
    };
#endif
    size_t case_index = 0;

    test_app_log_case(TAG, "Animation decoder validation");
    s_anim_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_anim_events);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x101820));
    gfx_obj_t *anim_obj = gfx_anim_create(disp_default);
    gfx_obj_t *next_btn = gfx_button_create(disp_default);
    TEST_ASSERT_NOT_NULL(anim_obj);
    TEST_ASSERT_NOT_NULL(next_btn);

    gfx_obj_set_size(next_btn, 100, 40);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(next_btn, "Next"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_font(next_btn, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(next_btn, GFX_COLOR_HEX(0x2A6DF4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(next_btn, GFX_COLOR_HEX(0x163D87)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(next_btn, GFX_COLOR_HEX(0xDCE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_width(next_btn, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(next_btn, test_anim_next_btn_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(next_btn, GFX_ALIGN_TOP_MID, 0, 0));
    test_app_unlock();

    while (1) {
        test_anim_show_case(assets_handle, anim_obj, &s_cases[case_index]);

        while (1) {
            xEventGroupWaitBits(s_anim_events,
                                TEST_ANIM_EVENT_NEXT,
                                pdTRUE,
                                pdFALSE,
                                portMAX_DELAY);

            if (gfx_anim_drain_plan_blocking(anim_obj) == ESP_OK) {
                ESP_LOGW(TAG, "Play remaining done");
            }

            case_index = (case_index + 1) % TEST_APP_ARRAY_SIZE(s_cases);
            break;
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);
    gfx_obj_delete(next_btn);
    gfx_obj_delete(anim_obj);
    test_app_unlock();

    vEventGroupDelete(s_anim_events);
    s_anim_events = NULL;
}

// TEST_CASE("widget animation decoder matrix", "[widget][anim][matrix]")
void test_anim_run_case_matrix(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    // test_anim_run(runtime.assets_handle);
    test_anim_show_case_image(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
