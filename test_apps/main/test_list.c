/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"

static const char *TAG = "test_list";

#define TEST_LIST_PANEL_WIDTH   246
#define TEST_LIST_PANEL_HEIGHT  176
#define TEST_LIST_HEADER_HEIGHT 30
#define TEST_LIST_FOOTER_HEIGHT 28

typedef struct {
    gfx_obj_t *title_label;
    gfx_obj_t *list_obj;
    gfx_obj_t *status_label;
} test_list_scene_t;

static void test_list_scene_cleanup(test_list_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->title_label != NULL) {
        gfx_obj_delete(scene->title_label);
        scene->title_label = NULL;
    }
    if (scene->list_obj != NULL) {
        gfx_obj_delete(scene->list_obj);
        scene->list_obj = NULL;
    }
    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
}

static void test_list_run(void)
{
    static const char *items_main[] = {
        "Display Brightness",
        "Expression Profile",
        "Motion Intensity",
        "Audio Sensitivity",
        "Face Tracking Mode",
        "Eye Contact Strength",
        "System Diagnostics",
        "Factory Reset",
    };
    static const char *items_alt[] = {
        "Theme: Calm Blue",
        "Theme: Studio Gray",
        "Theme: Warm Sunset",
        "Theme: Night Vision",
        "Theme: High Contrast",
    };

    test_list_scene_t scene = {0};

    test_app_log_case(TAG, "List widget menu-style UI");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x0B1220));

    scene.title_label = gfx_label_create(disp_default);
    scene.list_obj = gfx_list_create(disp_default);
    scene.status_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.list_obj);
    TEST_ASSERT_NOT_NULL(scene.status_label);

    gfx_obj_set_size(scene.title_label, TEST_LIST_PANEL_WIDTH, TEST_LIST_HEADER_HEIGHT);
    gfx_obj_set_size(scene.list_obj, TEST_LIST_PANEL_WIDTH, TEST_LIST_PANEL_HEIGHT);
    gfx_obj_set_size(scene.status_label, TEST_LIST_PANEL_WIDTH, TEST_LIST_FOOTER_HEIGHT);

    gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 10);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align_to(scene.list_obj, scene.title_label, GFX_ALIGN_OUT_BOTTOM_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align_to(scene.status_label, scene.list_obj, GFX_ALIGN_OUT_BOTTOM_MID, 0, 8));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.status_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_font(scene.list_obj, (gfx_font_t)&font_puhui_16_4));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x1A253B)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " SETTINGS MENU "));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_LEFT));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xE9F1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_long_mode(scene.title_label, GFX_LABEL_LONG_CLIP));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_bg_color(scene.list_obj, GFX_COLOR_HEX(0x111A2C)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_border_color(scene.list_obj, GFX_COLOR_HEX(0x344764)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_border_width(scene.list_obj, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected_bg_color(scene.list_obj, GFX_COLOR_HEX(0x2354C4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_text_color(scene.list_obj, GFX_COLOR_HEX(0xDDE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_row_height(scene.list_obj, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_padding(scene.list_obj, 10, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_items(scene.list_obj, items_main, TEST_APP_ARRAY_SIZE(items_main)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene.list_obj, 1));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.status_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.status_label, GFX_COLOR_HEX(0x162238)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.status_label, GFX_COLOR_HEX(0xBFD3F7)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.status_label, GFX_TEXT_ALIGN_LEFT));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_fmt(scene.status_label, " hint: touch list rows  |  selected=%ld",
                                                     (long)gfx_list_get_selected(scene.list_obj)));
    test_app_unlock();

    test_app_wait_for_observe(2400);

    test_app_log_step(TAG, "Navigate menu + insert one runtime item");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene.list_obj, 3));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene.list_obj, 5));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_add_item(scene.list_obj, "Developer Overlay"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene.list_obj, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_fmt(scene.status_label, " apply profile -> item=%ld",
                                                     (long)gfx_list_get_selected(scene.list_obj)));
    test_app_unlock();

    test_app_wait_for_observe(2400);

    test_app_log_step(TAG, "Switch menu page style");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_bg_color(scene.list_obj, GFX_COLOR_HEX(0x1B1B1D)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_border_color(scene.list_obj, GFX_COLOR_HEX(0x4F5566)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected_bg_color(scene.list_obj, GFX_COLOR_HEX(0xAD6B1F)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_text_color(scene.list_obj, GFX_COLOR_HEX(0xF6EBDD)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_items(scene.list_obj, items_alt, TEST_APP_ARRAY_SIZE(items_alt)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene.list_obj, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.status_label, GFX_COLOR_HEX(0x2A2118)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.status_label, GFX_COLOR_HEX(0xFFE8C7)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.status_label, " profile page: warm theme preview "));
    test_app_unlock();

    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_list_scene_cleanup(&scene);
    test_app_unlock();
}

void test_list_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_list_run();
    test_app_runtime_close(&runtime);
}
