/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_stickman_emote.h"
#include "common.h"

static const char *TAG = "test_stickman";

#include "stickman_fireman_export.inc"

typedef struct {
    gfx_obj_t *stickman_obj;
    gfx_timer_handle_t pose_timer;
    size_t current_seq_index;
} test_stickman_emote_scene_t;

static uint32_t test_stickman_action_hold_ms(const gfx_stickman_action_t *action)
{
    uint32_t period_ms = (s_stickman_export.layout != NULL && s_stickman_export.layout->timer_period_ms > 0U) ?
                         s_stickman_export.layout->timer_period_ms :
                         33U;
    uint32_t transition_ms = ((action != NULL && action->transition_ticks > 0U) ? action->transition_ticks : 6U) * period_ms;
    uint32_t multiplier = (action != NULL && action->step_count > 1U) ? 3U : 4U;
    uint32_t hold_ms = transition_ms * multiplier;

    return (hold_ms < 600U) ? 600U : hold_ms;
}

static void test_stickman_log_export_baseline(void)
{
    if (s_stickman_export.meta != NULL) {
        ESP_LOGI(TAG,
                 "stickman meta: version=%" PRIu32 " viewbox=(%ld,%ld,%ld,%ld) export=(%ldx%ld) scale=%.3f ofs=(%ld,%ld)",
                 s_stickman_export.meta->version,
                 (long)s_stickman_export.meta->design_viewbox_x,
                 (long)s_stickman_export.meta->design_viewbox_y,
                 (long)s_stickman_export.meta->design_viewbox_w,
                 (long)s_stickman_export.meta->design_viewbox_h,
                 (long)s_stickman_export.meta->export_width,
                 (long)s_stickman_export.meta->export_height,
                 (double)s_stickman_export.meta->export_scale,
                 (long)s_stickman_export.meta->export_offset_x,
                 (long)s_stickman_export.meta->export_offset_y);
    }

    if (s_stickman_export.layout != NULL) {
        ESP_LOGI(TAG,
                 "stickman layout: head_radius=%d stroke=%d mirror_x=%d ground_y=%d timer=%u damping=%d",
                 s_stickman_export.layout->head_radius,
                 s_stickman_export.layout->stroke_width,
                 s_stickman_export.layout->mirror_x,
                 s_stickman_export.layout->ground_y,
                 s_stickman_export.layout->timer_period_ms,
                 s_stickman_export.layout->damping_div);
    }
}

static void test_stickman_log_action(const gfx_stickman_action_t *action)
{
    if (action == NULL) {
        return;
    }

    ESP_LOGI(TAG,
             "action=%s group=%s step=%u/%u facing=%d transition_ticks=%u hold=%ums",
             action->name,
             action->group_name,
             action->step_index,
             action->step_count,
             action->facing,
             action->transition_ticks,
             test_stickman_action_hold_ms(action));
}

static void test_stickman_apply_sequence_action(test_stickman_emote_scene_t *scene,
                                                bool snap_now,
                                                bool reset_timer)
{
    uint16_t sequence_action_index;
    const gfx_stickman_action_t *action;

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    sequence_action_index = s_stickman_export.sequence[scene->current_seq_index];
    action = &s_stickman_export.actions[sequence_action_index];
    test_stickman_log_action(action);
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_stickman_emote_set_action_index(scene->stickman_obj, sequence_action_index, snap_now));

    if (reset_timer && scene->pose_timer != NULL) {
        gfx_timer_set_period(scene->pose_timer, test_stickman_action_hold_ms(action));
        gfx_timer_reset(scene->pose_timer);
    }
}

static void test_stickman_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_stickman_emote_scene_t *scene = (test_stickman_emote_scene_t *)user_data;

    (void)touch;

    if (scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE ||
            s_stickman_export.sequence_count == 0U) {
        return;
    }

    if (event->x < (BSP_LCD_H_RES / 2U)) {
        scene->current_seq_index = (scene->current_seq_index == 0U) ?
                                   (s_stickman_export.sequence_count - 1U) :
                                   (scene->current_seq_index - 1U);
        ESP_LOGI(TAG, "touch prev action");
    } else {
        scene->current_seq_index = (scene->current_seq_index + 1U) % s_stickman_export.sequence_count;
        ESP_LOGI(TAG, "touch next action");
    }

    test_stickman_apply_sequence_action(scene, false, true);
}

static void test_stickman_emote_timer_cb(void *user_data)
{
    test_stickman_emote_scene_t *scene = (test_stickman_emote_scene_t *)user_data;

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    scene->current_seq_index = (scene->current_seq_index + 1U) % s_stickman_export.sequence_count;
    test_stickman_apply_sequence_action(scene, false, true);
}

void test_stickman_emote_run(void)
{
    test_stickman_emote_scene_t scene = {0};
    const gfx_stickman_action_t *first_action = &s_stickman_export.actions[0];

    test_app_log_case(TAG, "Stickman Emote fireman action export test");
    test_stickman_log_export_baseline();

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.stickman_obj = gfx_stickman_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    TEST_ASSERT_NOT_NULL(scene.stickman_obj);

    gfx_obj_align(scene.stickman_obj, GFX_ALIGN_CENTER, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_export(scene.stickman_obj, &s_stickman_export));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_stroke_color(scene.stickman_obj, GFX_COLOR_HEX(0xFFFFFF)));

    scene.current_seq_index = 0U;
    test_stickman_apply_sequence_action(&scene, true, false);
    scene.pose_timer = gfx_timer_create(emote_handle, test_stickman_emote_timer_cb,
                                        test_stickman_action_hold_ms(first_action), &scene);
    TEST_ASSERT_NOT_NULL(scene.pose_timer);
    test_app_set_touch_event_cb(test_stickman_touch_event_cb, &scene);

    test_app_unlock();

    test_app_log_step(TAG, "White stickman on black background; tap left for previous action, right for next, auto-cycle stays active");
    test_app_wait_for_observe(1000 * 10000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_app_set_touch_event_cb(NULL, NULL);
    if (scene.pose_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.pose_timer);
    }
    gfx_obj_delete(scene.stickman_obj);
    test_app_unlock();
}

void test_stickman_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_stickman_emote_run();
    test_app_runtime_close(&runtime);
}
