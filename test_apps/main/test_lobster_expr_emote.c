/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp_check.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_lobster_emote.h"
#include "common.h"

static const char *TAG = "test_lobster_expr";

#include "lobster_emote_export.inc"
#include "lobster_test_mesh_tex.inc"
#include "lobster_test_body_tex.inc"

typedef struct {
    gfx_obj_t *antenna_obj;
    gfx_obj_t *bg_obj;
    gfx_obj_t *eyes_obj;
    // gfx_obj_t *title_label;
    gfx_timer_handle_t pose_timer;
    size_t current_expr_index;
    bool dragging;
    int16_t drag_start_x;
    int16_t drag_start_y;
    int16_t drag_ofs_start_x;
    int16_t drag_ofs_start_y;
    int16_t drag_ofs_x;
    int16_t drag_ofs_y;
} test_lobster_expr_emote_scene_t;

static bool test_lobster_expr_is_baseline_state(const char *name)
{
    if (name == NULL) {
        return false;
    }

    return (strcmp(name, "00 neutral") == 0) ||
           (strcmp(name, "07 happy") == 0) ||
           (strcmp(name, "16 surprise") == 0) ||
           (strcmp(name, "21 angry") == 0);
}

static void test_lobster_expr_log_export_baseline(void)
{
    if (s_lobster_expr_assets.export_meta != NULL) {
        ESP_LOGI(TAG,
                 "lobster baseline meta: version=%" PRIu32 " viewbox=(%ld,%ld,%ld,%ld) export=(%ldx%ld) scale=%.3f ofs=(%ld,%ld)",
                 s_lobster_expr_assets.export_meta->version,
                 (long)s_lobster_expr_assets.export_meta->design_viewbox_x,
                 (long)s_lobster_expr_assets.export_meta->design_viewbox_y,
                 (long)s_lobster_expr_assets.export_meta->design_viewbox_w,
                 (long)s_lobster_expr_assets.export_meta->design_viewbox_h,
                 (long)s_lobster_expr_assets.export_meta->export_width,
                 (long)s_lobster_expr_assets.export_meta->export_height,
                 (double)s_lobster_expr_assets.export_meta->export_scale,
                 (long)s_lobster_expr_assets.export_meta->export_offset_x,
                 (long)s_lobster_expr_assets.export_meta->export_offset_y);
    }
    if (s_lobster_expr_assets.layout != NULL) {
        ESP_LOGI(TAG,
                 "lobster baseline layout: eyeL=(%ld,%ld) eyeR=(%ld,%ld) pupilL=(%ld,%ld) pupilR=(%ld,%ld) mouth=(%ld,%ld) antL=(%ld,%ld) antR=(%ld,%ld)",
                 (long)s_lobster_expr_assets.layout->eye_left_cx,
                 (long)s_lobster_expr_assets.layout->eye_left_cy,
                 (long)s_lobster_expr_assets.layout->eye_right_cx,
                 (long)s_lobster_expr_assets.layout->eye_right_cy,
                 (long)s_lobster_expr_assets.layout->pupil_left_cx,
                 (long)s_lobster_expr_assets.layout->pupil_left_cy,
                 (long)s_lobster_expr_assets.layout->pupil_right_cx,
                 (long)s_lobster_expr_assets.layout->pupil_right_cy,
                 (long)s_lobster_expr_assets.layout->mouth_cx,
                 (long)s_lobster_expr_assets.layout->mouth_cy,
                 (long)s_lobster_expr_assets.layout->antenna_left_cx,
                 (long)s_lobster_expr_assets.layout->antenna_left_cy,
                 (long)s_lobster_expr_assets.layout->antenna_right_cx,
                 (long)s_lobster_expr_assets.layout->antenna_right_cy);
    }
}

static void test_lobster_expr_log_state_baseline(const gfx_lobster_emote_state_t *state)
{
    if (state == NULL || !test_lobster_expr_is_baseline_state(state->name)) {
        return;
    }

    ESP_LOGI(TAG,
             "weights=(sm:%d hp:%d sd:%d su:%d an:%d) look=(%d,%d) pupil=%d hold=%" PRIu32,
             state->w_smile,
             state->w_happy,
             state->w_sad,
             state->w_surprise,
             state->w_angry,
             state->w_look_x,
             state->w_look_y,
             (int)state->pupil_shape,
             state->hold_ticks);
}

static uint32_t test_lobster_expr_hold_to_ms(const gfx_lobster_emote_state_t *state)
{
    uint32_t hold_ticks = 30U;
    uint32_t period_ms = 33U;

    if (state != NULL && state->hold_ticks > 0U) {
        hold_ticks = state->hold_ticks;
    }
    if (s_lobster_expr_assets.semantics != NULL && s_lobster_expr_assets.semantics->timer_period_ms > 0U) {
        period_ms = s_lobster_expr_assets.semantics->timer_period_ms;
    }
    return hold_ticks * period_ms;
}

static void test_lobster_expr_apply_drag(test_lobster_expr_emote_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->antenna_obj) {
        gfx_obj_align(scene->antenna_obj, GFX_ALIGN_CENTER, scene->drag_ofs_x, scene->drag_ofs_y);
    }
    if (scene->bg_obj) {
        gfx_obj_align(scene->bg_obj, GFX_ALIGN_CENTER, scene->drag_ofs_x, scene->drag_ofs_y);
    }
    if (scene->eyes_obj) {
        gfx_obj_align(scene->eyes_obj, GFX_ALIGN_CENTER, scene->drag_ofs_x, scene->drag_ofs_y);
    }
}

static void test_lobster_expr_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_lobster_expr_emote_scene_t *scene = (test_lobster_expr_emote_scene_t *)user_data;
    (void)touch;

    if (scene == NULL || event == NULL) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        scene->dragging = true;
        scene->drag_start_x = event->x;
        scene->drag_start_y = event->y;
        scene->drag_ofs_start_x = scene->drag_ofs_x;
        scene->drag_ofs_start_y = scene->drag_ofs_y;
        break;
    case GFX_TOUCH_EVENT_MOVE:
        if (scene->dragging) {
            scene->drag_ofs_x = scene->drag_ofs_start_x + ((int16_t)event->x - scene->drag_start_x);
            scene->drag_ofs_y = scene->drag_ofs_start_y + ((int16_t)event->y - scene->drag_start_y);
            test_lobster_expr_apply_drag(scene);
        }
        break;
    case GFX_TOUCH_EVENT_RELEASE:
    default:
        scene->dragging = false;
        break;
    }
}

static void test_lobster_expr_emote_timer_cb(void *user_data)
{
    test_lobster_expr_emote_scene_t *scene = (test_lobster_expr_emote_scene_t *)user_data;
    const gfx_lobster_emote_state_t *state;
    if (scene == NULL || scene->antenna_obj == NULL || scene->eyes_obj == NULL) return;

    scene->current_expr_index = (scene->current_expr_index + 1U) % s_lobster_expr_assets.sequence_count;
    state = &s_lobster_expr_assets.sequence[scene->current_expr_index];
    ESP_LOGI(TAG, "Switched to: %s (hold: %dms)", state->name, test_lobster_expr_hold_to_ms(state));

    gfx_lobster_emote_set_state_name(scene->antenna_obj, state->name, false);
    gfx_lobster_emote_set_state_name(scene->eyes_obj, state->name, false);

    // if (scene->title_label) {
    //     gfx_label_set_text_fmt(scene->title_label, "Lobster: %s (hold: %dms)",
    //         state->name, test_lobster_expr_hold_to_ms(state));
    // }

    test_lobster_expr_log_state_baseline(state);
    if (scene->pose_timer != NULL) {
        gfx_timer_set_period(scene->pose_timer, test_lobster_expr_hold_to_ms(state));
        gfx_timer_reset(scene->pose_timer);
    }
}

void test_lobster_expr_emote_run(void)
{
    test_lobster_expr_emote_scene_t scene = {0};

    test_app_log_case(TAG, "Lobster Emote (龙虾) expression animation test");
    ESP_LOGI(TAG, "asset mode: expr inc + readback mesh/body (lobster_test_*_tex.inc)");
    test_lobster_expr_log_export_baseline();

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x1D1B25));

    scene.antenna_obj = gfx_lobster_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    scene.bg_obj = gfx_img_create(disp_default);
    scene.eyes_obj = gfx_lobster_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    // scene.title_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.antenna_obj);
    TEST_ASSERT_NOT_NULL(scene.bg_obj);
    TEST_ASSERT_NOT_NULL(scene.eyes_obj);
    // TEST_ASSERT_NOT_NULL(scene.title_label);

    gfx_obj_align(scene.antenna_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_lobster_emote_set_assets(scene.antenna_obj, &s_lobster_expr_assets);
    gfx_lobster_emote_set_color(scene.antenna_obj, GFX_COLOR_HEX(0xF46144));
    gfx_lobster_emote_set_layer_mask(scene.antenna_obj, GFX_LOBSTER_EMOTE_LAYER_ANTENNA);

    gfx_img_set_src_desc(scene.bg_obj, &(gfx_img_src_t) {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &s_lobster_test_body_tex,
    });
    gfx_obj_align(scene.bg_obj, GFX_ALIGN_CENTER, 0, 0);

    gfx_obj_align(scene.eyes_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_lobster_emote_set_assets(scene.eyes_obj, &s_lobster_expr_assets);
    gfx_lobster_emote_set_color(scene.eyes_obj, GFX_COLOR_HEX(0xF46144));
    gfx_lobster_emote_set_layer_mask(scene.eyes_obj, GFX_LOBSTER_EMOTE_LAYER_EYE_WHITE | GFX_LOBSTER_EMOTE_LAYER_PUPIL | GFX_LOBSTER_EMOTE_LAYER_MOUTH);

    /* RGB565A8: eye white ellipse + pupil arc from ip_svg_parts_readback.html (see gen_lobster_readback_textures.py) */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_lobster_emote_set_mesh_textures(scene.eyes_obj, &s_lobster_test_eye_tex, &s_lobster_test_pupil_tex));
    ESP_LOGI(TAG, "mesh textures (readback eye): eye %ux%u pupil %ux%u RGB565A8",
             (unsigned)s_lobster_test_eye_tex.header.w, (unsigned)s_lobster_test_eye_tex.header.h,
             (unsigned)s_lobster_test_pupil_tex.header.w, (unsigned)s_lobster_test_pupil_tex.header.h);
    ESP_LOGI(TAG, "bg image (readback body, no eyes/antenna): %ux%u RGB565A8",
             (unsigned)s_lobster_test_body_tex.header.w, (unsigned)s_lobster_test_body_tex.header.h);

    // gfx_obj_set_size(scene.title_label, 320, 30);
    // gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 10);
    // gfx_label_set_text(scene.title_label, "Lobster Emote Test");
    // gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xE0E0E0));

    scene.current_expr_index = 0;
    gfx_lobster_emote_set_state_name(scene.antenna_obj, s_lobster_expr_assets.sequence[0].name, true);
    gfx_lobster_emote_set_state_name(scene.eyes_obj, s_lobster_expr_assets.sequence[0].name, true);
    test_lobster_expr_log_state_baseline(&s_lobster_expr_assets.sequence[0]);
    test_app_set_touch_event_cb(test_lobster_expr_touch_event_cb, &scene);

    scene.pose_timer = gfx_timer_create(emote_handle, test_lobster_expr_emote_timer_cb,
                                        test_lobster_expr_hold_to_ms(&s_lobster_expr_assets.sequence[0]), &scene);
    
    test_app_unlock();

    test_app_log_step(TAG, "Observe lobster: readback body BG + eye mesh textures, expressions cycling with antenna");
    test_app_wait_for_observe(1000 * 10000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    if (scene.pose_timer) gfx_timer_delete(emote_handle, scene.pose_timer);
    test_app_set_touch_event_cb(NULL, NULL);
    // gfx_obj_delete(scene.title_label);
    gfx_obj_delete(scene.eyes_obj);
    gfx_obj_delete(scene.bg_obj);
    gfx_obj_delete(scene.antenna_obj);
    test_app_unlock();
}

// TEST_CASE("Lobster expression emote test", "[lobster][emote]")
void test_lobster_expr_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));
    test_lobster_expr_emote_run();
    test_app_runtime_close(&runtime);
}
