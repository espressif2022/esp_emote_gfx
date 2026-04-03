/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#include "esp_check.h"
#include "esp_random.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_dragon_emote.h"
#include "common.h"

static const char *TAG = "test_dragon_emote";

#include "dragon_emote_assets.inc"

typedef struct {
    gfx_obj_t *dragon_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t pose_timer;
    size_t current_pose_index;
    uint32_t current_color_hex;
} test_dragon_emote_scene_t;

static const uint32_t s_dragon_color_palette[] = {
    0xFF6B4A, // Original Accent
    0x2CC55E, // Green
    0xFFA600, // Orange
    0x22B7D5, // Cyan
    0x8B5CF6, // Purple
    0xFFDD52, // Yellow
};

static void test_dragon_emote_pose_timer_cb(void *user_data)
{
    test_dragon_emote_scene_t *scene = (test_dragon_emote_scene_t *)user_data;
    if (scene == NULL || scene->dragon_obj == NULL) return;

    scene->current_pose_index = (scene->current_pose_index + 1) % s_dragon_assets.sequence_count;
    scene->current_color_hex = s_dragon_color_palette[esp_random() % (sizeof(s_dragon_color_palette)/sizeof(uint32_t))];

    gfx_dragon_emote_set_color(scene->dragon_obj, GFX_COLOR_HEX(scene->current_color_hex));
    gfx_dragon_emote_set_pose_name(scene->dragon_obj, s_dragon_assets.sequence[scene->current_pose_index].name, false);

    if (scene->title_label) {
        gfx_label_set_text_fmt(scene->title_label, "Pose: %s  #%06" PRIX32,
                              s_dragon_assets.sequence[scene->current_pose_index].name,
                              scene->current_color_hex);
    }

    ESP_LOGI(TAG, "Switched to pose: %s", s_dragon_assets.sequence[scene->current_pose_index].name);
}

void test_dragon_emote_run(void)
{
    test_dragon_emote_scene_t scene = {0};

    test_app_log_case(TAG, "Dragon Emote (龙牙) skeletal animation test");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x050B12));

    scene.dragon_obj = gfx_dragon_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.dragon_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);

    gfx_obj_align(scene.dragon_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_dragon_emote_set_assets(scene.dragon_obj, &s_dragon_assets);
    
    gfx_obj_set_size(scene.title_label, 320, 30);
    gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 10);
    gfx_label_set_text(scene.title_label, "Dragon Emote Test");
    gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xE0E0E0));

    scene.current_pose_index = 0;
    gfx_dragon_emote_set_pose_name(scene.dragon_obj, s_dragon_assets.sequence[0].name, true);

    scene.pose_timer = gfx_timer_create(emote_handle, test_dragon_emote_pose_timer_cb, 2000, &scene);
    
    test_app_unlock();

    test_app_log_step(TAG, "Observe 15 poses cycling every 2 seconds with skeletal coordination");
    test_app_wait_for_observe(1000 * 60);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    if (scene.pose_timer) gfx_timer_delete(emote_handle, scene.pose_timer);
    gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.dragon_obj);
    test_app_unlock();
}

// TEST_CASE("Dragon Emote skeletal test", "[dragon][emote]")
void test_dragon_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));
    test_dragon_emote_run();
    test_app_runtime_close(&runtime);
}

