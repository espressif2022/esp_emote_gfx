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
#include "common.h"

static const char *TAG = "test_face_emote";

// #define TEST_APP_FACE_EMOTE_LAYOUT_REF_X  ((BSP_LCD_H_RES) * 2 / 3)
// #define TEST_APP_FACE_EMOTE_LAYOUT_REF_Y  ((BSP_LCD_V_RES) * 3 / 2)

#define TEST_APP_FACE_EMOTE_LAYOUT_REF_X  ((BSP_LCD_H_RES) * 2 / 3)
#define TEST_APP_FACE_EMOTE_LAYOUT_REF_Y  ((BSP_LCD_V_RES) * 3 / 3)
#define TEST_APP_FACE_EMOTE_USE_RANDOM_MIX 0

#include "face_emote_expr_assets.inc"

typedef struct {
    gfx_obj_t *face_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t expr_timer;
    gfx_face_emote_cfg_t cfg;
    bool use_chinese;
    gfx_face_emote_mix_t current_mix;
    gfx_color_t current_color;
    uint32_t current_color_hex;
    size_t current_expr_index;
} test_face_emote_scene_t;

static const gfx_face_emote_assets_t s_face_assets = {
    .ref_eye = s_ref_eye,
    .ref_eye_count = TEST_APP_ARRAY_SIZE(s_ref_eye),
    .ref_brow = s_ref_brow,
    .ref_brow_count = TEST_APP_ARRAY_SIZE(s_ref_brow),
    .ref_mouth = s_ref_mouth,
    .ref_mouth_count = TEST_APP_ARRAY_SIZE(s_ref_mouth),
    .sequence = s_face_sequence,
    .sequence_count = TEST_APP_ARRAY_SIZE(s_face_sequence),
};

static const uint32_t s_face_color_palette[] = {
    0x1D4ED8,
    0xFF6B6B,
    0x10B981,
    0xF59E0B,
    0xA855F7,
    0xF8FAFC,
};

static uint32_t test_face_emote_random_color_hex(void)
{
    size_t idx = esp_random() % TEST_APP_ARRAY_SIZE(s_face_color_palette);
    return s_face_color_palette[idx];
}

static const char *test_face_emote_get_expr_name(const test_face_emote_scene_t *scene, size_t index)
{
    const gfx_face_emote_expr_t *expr;

    if (scene == NULL || index >= s_face_assets.sequence_count) {
        return "unknown";
    }

    expr = &s_face_assets.sequence[index];
    if (scene->use_chinese && expr->name_cn != NULL) {
        return expr->name_cn;
    }
    if (expr->name != NULL) {
        return expr->name;
    }
    if (expr->name_cn != NULL) {
        return expr->name_cn;
    }
    return "unknown";
}

static void test_face_emote_apply_color(test_face_emote_scene_t *scene)
{
    if (scene == NULL || scene->face_obj == NULL) {
        return;
    }

    scene->current_color = GFX_COLOR_HEX(scene->current_color_hex);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_face_emote_set_color(scene->face_obj, scene->current_color));
    ESP_LOGI(TAG, "set color #%06" PRIX32, scene->current_color_hex);
}

#if TEST_APP_FACE_EMOTE_USE_RANDOM_MIX
static int16_t test_face_emote_random_signed(int16_t limit)
{
    uint32_t span = (uint32_t)(limit * 2) + 1U;
    return (int16_t)((int32_t)(esp_random() % span) - limit);
}

static int16_t test_face_emote_random_percent(void)
{
    return (int16_t)(esp_random() % 101U);
}

static void test_face_emote_randomize_mix(gfx_face_emote_mix_t *mix)
{
    if (mix == NULL) {
        return;
    }

    mix->w_smile = test_face_emote_random_percent();
    mix->w_happy = test_face_emote_random_percent();
    mix->w_sad = test_face_emote_random_percent();
    mix->w_surprise = test_face_emote_random_percent();
    mix->w_angry = test_face_emote_random_percent();
    mix->look_x = test_face_emote_random_signed(22);
    mix->look_y = test_face_emote_random_signed(22);
}

static void test_face_emote_apply_mix(test_face_emote_scene_t *scene, bool snap_now)
{
    if (scene == NULL || scene->face_obj == NULL) {
        return;
    }

    gfx_face_emote_set_mix(scene->face_obj, &scene->current_mix, snap_now);
    ESP_LOGI(TAG, "set mix sm=%d hp=%d sd=%d su=%d an=%d lx=%d ly=%d",
             scene->current_mix.w_smile, scene->current_mix.w_happy,
             scene->current_mix.w_sad, scene->current_mix.w_surprise,
             scene->current_mix.w_angry, scene->current_mix.look_x,
             scene->current_mix.look_y);
}
#endif

static void test_face_emote_update_title(test_face_emote_scene_t *scene)
{
    if (scene == NULL || scene->title_label == NULL) {
        return;
    }

#if TEST_APP_FACE_EMOTE_USE_RANDOM_MIX
    gfx_label_set_text_fmt(scene->title_label, "mix h%02d s%02d u%02d a%02d  #%06" PRIX32,
                           scene->current_mix.w_happy, scene->current_mix.w_smile,
                           scene->current_mix.w_surprise, scene->current_mix.w_angry,
                           scene->current_color_hex);
#else
    gfx_label_set_text_fmt(scene->title_label, "%s  #%06" PRIX32,
                           test_face_emote_get_expr_name(scene, scene->current_expr_index),
                           scene->current_color_hex);
#endif
}

static void test_face_emote_expr_timer_cb(void *user_data)
{
    test_face_emote_scene_t *scene = (test_face_emote_scene_t *)user_data;

    if (scene == NULL || scene->face_obj == NULL) {
        return;
    }

#if TEST_APP_FACE_EMOTE_USE_RANDOM_MIX
    test_face_emote_randomize_mix(&scene->current_mix);
    scene->current_color_hex = test_face_emote_random_color_hex();
    test_face_emote_apply_color(scene);
    test_face_emote_apply_mix(scene, false);
#else
    scene->current_expr_index = (scene->current_expr_index + 1U) % s_face_assets.sequence_count;
    scene->current_color_hex = test_face_emote_random_color_hex();
    test_face_emote_apply_color(scene);
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_face_emote_set_expression_name(scene->face_obj,
                                                         s_face_assets.sequence[scene->current_expr_index].name,
                                                         false));
    ESP_LOGI(TAG, "set expression %s (%s)",
             s_face_assets.sequence[scene->current_expr_index].name,
             test_face_emote_get_expr_name(scene, scene->current_expr_index));
#endif
    test_face_emote_update_title(scene);
}

static void test_face_emote_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_face_emote_scene_t *scene = (test_face_emote_scene_t *)user_data;
    gfx_coord_t face_x = 0;
    gfx_coord_t face_y = 0;
    int16_t look_x;
    int16_t look_y;

    (void)touch;

    if (scene == NULL || scene->face_obj == NULL || event == NULL) {
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        gfx_face_emote_set_manual_look(scene->face_obj, false, 0, 0);
#if TEST_APP_FACE_EMOTE_USE_RANDOM_MIX
        test_face_emote_apply_mix(scene, false);
#else
        TEST_ASSERT_EQUAL(ESP_OK,
                          gfx_face_emote_set_expression_name(scene->face_obj,
                                                             s_face_assets.sequence[scene->current_expr_index].name,
                                                             false));
#endif
        return;
    }

    gfx_obj_get_pos(scene->face_obj, &face_x, &face_y);
    look_x = (int16_t)(((int32_t)event->x - ((int32_t)face_x + (scene->cfg.display_w / 2))) * 80 / (scene->cfg.display_w / 2));
    look_y = (int16_t)(((int32_t)event->y - ((int32_t)face_y + (scene->cfg.display_h / 2))) * 80 / (scene->cfg.display_h / 2));
    gfx_face_emote_set_manual_look(scene->face_obj, true, look_x, look_y);
}

static void test_face_emote_run(void)
{
    test_face_emote_scene_t scene = {0};

    test_app_log_case(TAG, "Standard face emote widget");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.face_obj = gfx_face_emote_create(disp_default,
                                           TEST_APP_FACE_EMOTE_LAYOUT_REF_X,
                                           TEST_APP_FACE_EMOTE_LAYOUT_REF_Y);
    scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.face_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);

    gfx_face_emote_cfg_init(&scene.cfg,BSP_LCD_H_RES,BSP_LCD_V_RES,
                            TEST_APP_FACE_EMOTE_LAYOUT_REF_X,
                            TEST_APP_FACE_EMOTE_LAYOUT_REF_Y);

    ESP_LOGI(TAG, "layout_ref_x: %d, layout_ref_y: %d", TEST_APP_FACE_EMOTE_LAYOUT_REF_X, TEST_APP_FACE_EMOTE_LAYOUT_REF_Y);
    ESP_LOGI(TAG, "display_w: %d, display_h: %d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    scene.use_chinese = true;

    TEST_ASSERT_EQUAL(ESP_OK, gfx_face_emote_set_config(scene.face_obj, &scene.cfg));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.face_obj, GFX_ALIGN_CENTER, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_face_emote_set_assets(scene.face_obj, &s_face_assets));
    scene.current_color_hex = s_face_color_palette[0];
    test_face_emote_apply_color(&scene);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));

#if TEST_APP_FACE_EMOTE_USE_RANDOM_MIX
    test_face_emote_randomize_mix(&scene.current_mix);
    test_face_emote_apply_mix(&scene, true);
#else
    scene.current_expr_index = 0U;
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_face_emote_set_expression_name(scene.face_obj,
                                                         s_face_assets.sequence[scene.current_expr_index].name,
                                                         true));
#endif
    test_face_emote_update_title(&scene);
    scene.expr_timer = gfx_timer_create(emote_handle, test_face_emote_expr_timer_cb, 1000U, &scene);
    TEST_ASSERT_NOT_NULL(scene.expr_timer);
    test_app_set_touch_event_cb(test_face_emote_touch_event_cb, &scene);

    test_app_unlock();

    test_app_log_step(TAG, "Touch anywhere to steer gaze; face color and pose update every 1 second");
    test_app_wait_for_observe(1000U * 10000U);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_app_set_touch_event_cb(NULL, NULL);
    if (scene.expr_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.expr_timer);
    }
    gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.face_obj);
    test_app_unlock();
}

void test_face_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));
    test_face_emote_run();
    test_app_runtime_close(&runtime);
}

/** @deprecated Old entry name; use test_face_emote_run_case(). */
void test_mouth_model_run_case(void)
{
    test_face_emote_run_case();
}
