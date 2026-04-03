/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_random.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_lobster_face_emote.h"
#include "common.h"

static const char *TAG = "test_lobster_face";

#include "lobster_face_emote_assets.inc"

typedef struct {
    gfx_obj_t *lobster_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t expr_timer;
    size_t current_expr_index;
    uint32_t current_color_hex;
} test_lobster_face_scene_t;

static const uint32_t s_lobster_face_colors[] = {
    0xF46144, 0xFF6B4A, 0xFB7185, 0xF97316
};

static void test_lobster_face_timer_cb(void *user_data)
{
    test_lobster_face_scene_t *scene = (test_lobster_face_scene_t *)user_data;

    if (scene == NULL || scene->lobster_obj == NULL) {
        return;
    }

    scene->current_expr_index = (scene->current_expr_index + 1U) % s_lobster_face_assets.sequence_count;
    scene->current_color_hex = s_lobster_face_colors[esp_random() % (sizeof(s_lobster_face_colors) / sizeof(s_lobster_face_colors[0]))];

    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_face_emote_set_color(scene->lobster_obj, GFX_COLOR_HEX(scene->current_color_hex)));
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_lobster_face_emote_set_expression_name(scene->lobster_obj,
                                                                 s_lobster_face_assets.sequence[scene->current_expr_index].name,
                                                                 false));
    gfx_label_set_text_fmt(scene->title_label, "%s  #%06" PRIX32,
                           s_lobster_face_assets.sequence[scene->current_expr_index].name_cn,
                           scene->current_color_hex);
}

void test_lobster_face_emote_run(void)
{
    test_lobster_face_scene_t scene = {0};

    test_app_log_case(TAG, "Lobster face-style emote test");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x09131D));

    scene.lobster_obj = gfx_lobster_face_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.lobster_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.lobster_obj, GFX_ALIGN_CENTER, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_face_emote_set_assets(scene.lobster_obj, &s_lobster_face_assets));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 320, 28));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, "Lobster Face Emote"));

    scene.current_expr_index = 0;
    scene.current_color_hex = s_lobster_face_colors[0];
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_face_emote_set_expression_name(scene.lobster_obj, s_lobster_face_assets.sequence[0].name, true));
    scene.expr_timer = gfx_timer_create(emote_handle, test_lobster_face_timer_cb, 1800, &scene);
    test_app_unlock();

    test_app_log_step(TAG, "Observe lobster face-style expression blending on eyes and claws");
    test_app_wait_for_observe(1000 * 60);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    if (scene.expr_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.expr_timer);
    }
    gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.lobster_obj);
    test_app_unlock();
}

void test_lobster_face_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));
    test_lobster_face_emote_run();
    test_app_runtime_close(&runtime);
}
