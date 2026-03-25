/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"

static const char *TAG = "test_button";

typedef struct {
    gfx_obj_t *button_obj;
    gfx_obj_t *status_label;
} test_button_scene_t;

static void test_button_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    gfx_obj_t *status_label = (gfx_obj_t *)user_data;

    (void)obj;

    if (status_label == NULL || event == NULL) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        gfx_label_set_text(status_label, "button state: pressed");
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        gfx_label_set_text(status_label, "button state: released");
        break;
    case GFX_TOUCH_EVENT_MOVE:
    default:
        break;
    }
}

static void test_button_scene_cleanup(test_button_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->button_obj != NULL) {
        gfx_obj_delete(scene->button_obj);
        scene->button_obj = NULL;
    }
    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
}

static void test_button_run(void)
{
    test_button_scene_t scene = {0};

    test_app_log_case(TAG, "Button widget validation (align_to)");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene.button_obj = gfx_button_create(disp_default);
    scene.status_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.button_obj);
    TEST_ASSERT_NOT_NULL(scene.status_label);

    gfx_obj_set_size(scene.button_obj, 180, 52);
    gfx_obj_align(scene.button_obj, GFX_ALIGN_CENTER, 0, -18);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(scene.button_obj, "Tap Button"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_font(scene.button_obj, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(scene.button_obj, GFX_COLOR_HEX(0x2A6DF4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(scene.button_obj, GFX_COLOR_HEX(0x163D87)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(scene.button_obj, GFX_COLOR_HEX(0xDCE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_width(scene.button_obj, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.button_obj, test_button_touch_cb, scene.status_label));

    gfx_obj_set_size(scene.status_label, 220, 32);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align_to(scene.status_label, scene.button_obj, GFX_ALIGN_OUT_BOTTOM_MID, 0, 12));
    gfx_label_set_font(scene.status_label, (gfx_font_t)&font_puhui_16_4);
    gfx_label_set_text_align(scene.status_label, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_color(scene.status_label, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text(scene.status_label, "button state: idle");
    test_app_unlock();

    test_app_wait_for_observe(5700);

    test_app_log_step(TAG, "Update button visual style");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(scene.button_obj, "Style Updated"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(scene.button_obj, GFX_COLOR_HEX(0x21825B)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(scene.button_obj, GFX_COLOR_HEX(0x175E42)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(scene.button_obj, GFX_COLOR_HEX(0xD7F7E8)));
    test_app_unlock();

    test_app_wait_for_observe(4000);

    test_app_log_step(TAG, "Move button and verify align_to follower");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_align(scene.button_obj, GFX_ALIGN_TOP_MID, 0, 36);
    gfx_label_set_text(scene.status_label, "automatically follows");
    test_app_unlock();

    test_app_wait_for_observe(4000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_button_scene_cleanup(&scene);
    test_app_unlock();
}

// TEST_CASE("widget button interaction with align_to", "[widget][button][align_to]")

void test_button_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_button_run();
    test_app_runtime_close(&runtime);
}
