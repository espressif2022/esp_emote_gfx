/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_random.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_lobster_emote.h"
#include "common.h"

static const char *TAG = "test_lobster_emote";

#include "lobster_emote_assets.inc"

typedef struct {
    gfx_obj_t *lobster_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t state_timer;
    size_t current_state_index;
    uint32_t current_color_hex;
} test_lobster_emote_scene_t;

static const uint32_t s_lobster_color_palette[] = {
    0xFF6B4A,
    0xF97316,
    0xFB7185,
    0x22C55E,
    0x38BDF8,
    0xFACC15,
};

static void test_lobster_emote_state_timer_cb(void *user_data)
{
    test_lobster_emote_scene_t *scene = (test_lobster_emote_scene_t *)user_data;

    if (scene == NULL || scene->lobster_obj == NULL) {
        return;
    }

    scene->current_state_index = (scene->current_state_index + 1U) % s_lobster_assets.sequence_count;
    // scene->current_color_hex = s_lobster_color_palette[esp_random() % (sizeof(s_lobster_color_palette) / sizeof(s_lobster_color_palette[0]))];
    scene->current_color_hex = 0xFF6B4A;
    
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_emote_set_color(scene->lobster_obj, GFX_COLOR_HEX(scene->current_color_hex)));
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_lobster_emote_set_state_name(scene->lobster_obj,
                                                       s_lobster_assets.sequence[scene->current_state_index].name,
                                                       false));

    if (scene->title_label != NULL) {
        gfx_label_set_text_fmt(scene->title_label, "%s  #%06" PRIX32,
                               s_lobster_assets.sequence[scene->current_state_index].name_cn,
                               scene->current_color_hex);
    }

    ESP_LOGI(TAG, "switch state: %s", s_lobster_assets.sequence[scene->current_state_index].name);
}

void test_lobster_emote_run(void)
{
    test_lobster_emote_scene_t scene = {0};

    test_app_log_case(TAG, "Lobster emote widget test");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x081018));

    scene.lobster_obj = gfx_lobster_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.lobster_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.lobster_obj, GFX_ALIGN_CENTER, 0, 10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_emote_set_assets(scene.lobster_obj, &s_lobster_assets));
    scene.current_color_hex = s_lobster_color_palette[0];
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_emote_set_color(scene.lobster_obj, GFX_COLOR_HEX(scene.current_color_hex)));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 320, 30));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, "Lobster Emote Test"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xF8FAFC)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x11243A)));

    scene.current_state_index = 0;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_emote_set_state_name(scene.lobster_obj, s_lobster_assets.sequence[0].name, true));
    scene.state_timer = gfx_timer_create(emote_handle, test_lobster_emote_state_timer_cb, 1800, &scene);

    test_app_unlock();

    test_app_log_step(TAG, "Observe eye motion, blink, and alternating claw swings across lobster states");
    test_app_wait_for_observe(1000 * 60);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    if (scene.state_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.state_timer);
    }
    gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.lobster_obj);
    test_app_unlock();
}

void test_lobster_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));
    test_lobster_emote_run();
    test_app_runtime_close(&runtime);
}
