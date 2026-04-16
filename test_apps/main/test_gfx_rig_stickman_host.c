/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_stickman_emote.h"
#include "widget/gfx_stickman_rig_host.h"
#include "common.h"

static const char *TAG = "test_rig_stickman";

#include "stickman_fireman_export.inc"

typedef struct {
    gfx_obj_t *stickman_obj;
    gfx_stickman_rig_host_t rig_host;
    gfx_timer_handle_t pose_timer;
    size_t current_seq_index;
} test_rig_stickman_scene_t;

static uint32_t test_rig_stickman_action_hold_ms(const gfx_stickman_action_t *action)
{
    uint32_t period_ms = (s_stickman_export.layout != NULL && s_stickman_export.layout->timer_period_ms > 0U) ?
                         s_stickman_export.layout->timer_period_ms :
                         33U;
    uint32_t ticks = (action != NULL && action->transition_ticks > 0U) ? (uint32_t)action->transition_ticks : 1U;

    return ticks * period_ms;
}

static void test_rig_stickman_apply_sequence_action(test_rig_stickman_scene_t *scene, bool snap_now, bool reset_timer)
{
    size_t sequence_action_index;

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    sequence_action_index = s_stickman_export.sequence[scene->current_seq_index];
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene->stickman_obj, GFX_ALIGN_CENTER, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_action_index(scene->stickman_obj, sequence_action_index, snap_now));

    if (reset_timer && scene->pose_timer != NULL) {
        const gfx_stickman_action_t *action = &s_stickman_export.actions[sequence_action_index];
        gfx_timer_set_period(scene->pose_timer, test_rig_stickman_action_hold_ms(action));
        gfx_timer_reset(scene->pose_timer);
    }
}

static void test_rig_stickman_timer_cb(void *user_data)
{
    test_rig_stickman_scene_t *scene = (test_rig_stickman_scene_t *)user_data;

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    scene->current_seq_index = (scene->current_seq_index + 1U) % s_stickman_export.sequence_count;
    ESP_LOGI(TAG, "current_seq_index: %d", scene->current_seq_index);
    test_rig_stickman_apply_sequence_action(scene, false, true);
}

void test_gfx_rig_stickman_host_run(void)
{
    test_rig_stickman_scene_t scene = {0};
    const gfx_stickman_action_t *first_action = &s_stickman_export.actions[0];

    test_app_log_case(TAG, "Stickman + gfx_rig_host (internal anim_timer suspended while rig attached)");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.stickman_obj = gfx_stickman_emote_create(disp_default, BSP_LCD_H_RES, BSP_LCD_V_RES);
    TEST_ASSERT_NOT_NULL(scene.stickman_obj);
    gfx_obj_align(scene.stickman_obj, GFX_ALIGN_CENTER, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_export(scene.stickman_obj, &s_stickman_export));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_stroke_color(scene.stickman_obj, GFX_COLOR_HEX(0xFFFFFF)));

    gfx_stickman_rig_host_init(&scene.rig_host);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_rig_host_attach(&scene.rig_host, disp_default, scene.stickman_obj));

    scene.current_seq_index = 0U;
    test_rig_stickman_apply_sequence_action(&scene, true, false);
    scene.pose_timer = gfx_timer_create(emote_handle, test_rig_stickman_timer_cb,
                                        test_rig_stickman_action_hold_ms(first_action), &scene);
    TEST_ASSERT_NOT_NULL(scene.pose_timer);

    test_app_unlock();

    test_app_log_step(TAG, "Same fireman sequence as test_stickman_emote; pose is driven by gfx_rig_host");
    test_app_wait_for_observe(1000 * 10000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    if (scene.pose_timer != NULL) {
        gfx_timer_delete(emote_handle, scene.pose_timer);
    }
    gfx_stickman_rig_host_detach(&scene.rig_host);
    gfx_obj_delete(scene.stickman_obj);
    test_app_unlock();
}

void test_gfx_rig_stickman_host_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_gfx_rig_stickman_host_run();
    test_app_runtime_close(&runtime);
}
