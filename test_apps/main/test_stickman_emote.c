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
#include "widget/gfx_stickman_emote.h"
#include "common.h"

static const char *TAG = "test_stickman";

#include "stickman_fireman_export.inc"

typedef struct {
    gfx_obj_t *stickman_obj;
    gfx_timer_handle_t pose_timer;
    size_t current_seq_index;
    size_t current_action_index;
    bool current_action_valid;
    bool touch_active;
    int16_t current_ofs_x;
    int16_t current_ofs_y;
} test_stickman_emote_scene_t;

static uint32_t test_stickman_action_hold_ms(const gfx_stickman_action_t *action);
static void test_stickman_log_action(const gfx_stickman_action_t *action);

typedef struct {
    size_t action_index;
    int16_t ofs_x;
    int16_t ofs_y;
} test_stickman_touch_target_t;

static uint32_t test_stickman_abs_i32(int32_t value)
{
    return (value < 0) ? (uint32_t)(-value) : (uint32_t)value;
}

static int16_t test_stickman_clamp_i16(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        value = min_value;
    }
    if (value > max_value) {
        value = max_value;
    }
    return (int16_t)value;
}

static const gfx_stickman_action_t *test_stickman_find_action(const char *group_name,
                                                              int8_t facing,
                                                              uint8_t step_index,
                                                              size_t *index_out)
{
    if (group_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < s_stickman_export.action_count; i++) {
        const gfx_stickman_action_t *action = &s_stickman_export.actions[i];

        if (strcmp(action->group_name, group_name) != 0) {
            continue;
        }
        if (action->facing != facing) {
            continue;
        }
        if (action->step_index != step_index) {
            continue;
        }
        if (index_out != NULL) {
            *index_out = i;
        }
        return action;
    }

    return NULL;
}

static void test_stickman_apply_offset(test_stickman_emote_scene_t *scene, int16_t ofs_x, int16_t ofs_y)
{
    if (scene == NULL || scene->stickman_obj == NULL) {
        return;
    }
    if (scene->current_ofs_x == ofs_x && scene->current_ofs_y == ofs_y) {
        return;
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene->stickman_obj, GFX_ALIGN_CENTER, ofs_x, ofs_y));
    scene->current_ofs_x = ofs_x;
    scene->current_ofs_y = ofs_y;
}

static void test_stickman_apply_action_index(test_stickman_emote_scene_t *scene,
                                             size_t action_index,
                                             bool snap_now,
                                             bool reset_timer)
{
    const gfx_stickman_action_t *action;

    if (scene == NULL || scene->stickman_obj == NULL || action_index >= s_stickman_export.action_count) {
        return;
    }

    action = &s_stickman_export.actions[action_index];
    if (scene->current_action_valid && scene->current_action_index == action_index) {
        if (reset_timer && scene->pose_timer != NULL) {
            gfx_timer_set_period(scene->pose_timer, test_stickman_action_hold_ms(action));
            gfx_timer_reset(scene->pose_timer);
        }
        return;
    }

    test_stickman_log_action(action);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_stickman_emote_set_action_index(scene->stickman_obj, action_index, snap_now));

    scene->current_action_index = action_index;
    scene->current_action_valid = true;

    if (reset_timer && scene->pose_timer != NULL) {
        gfx_timer_set_period(scene->pose_timer, test_stickman_action_hold_ms(action));
        gfx_timer_reset(scene->pose_timer);
    }
}

static bool test_stickman_resolve_touch_target(uint16_t x, uint16_t y, test_stickman_touch_target_t *target_out)
{
    int32_t dx = (int32_t)x - (int32_t)(BSP_LCD_H_RES / 2U);
    int32_t dy = (int32_t)y - (int32_t)(BSP_LCD_V_RES / 2U);
    uint32_t abs_x = test_stickman_abs_i32(dx);
    uint32_t abs_y = test_stickman_abs_i32(dy);
    uint32_t min_dim = (BSP_LCD_H_RES < BSP_LCD_V_RES) ? BSP_LCD_H_RES : BSP_LCD_V_RES;
    uint32_t dead_zone = min_dim / 8U;
    uint32_t diagonal_gate;
    uint32_t walk_step2 = BSP_LCD_H_RES / 5U;
    uint32_t run_step1 = BSP_LCD_H_RES / 3U;
    uint32_t run_step2 = (BSP_LCD_H_RES * 9U) / 20U;
    uint32_t vertical_far = BSP_LCD_V_RES / 3U;
    uint32_t max_ofs_x = BSP_LCD_H_RES / 12U;
    uint32_t max_ofs_y = BSP_LCD_V_RES / 14U;
    const char *group_name = "calm";
    int8_t facing = 1;
    uint8_t step_index = 1;
    size_t action_index = 0U;
    bool diagonal_touch;
    int16_t ofs_x;
    int16_t ofs_y;

    if (target_out == NULL) {
        return false;
    }

    if (dead_zone < 24U) {
        dead_zone = 24U;
    }
    diagonal_gate = dead_zone + (dead_zone / 2U);
    if (walk_step2 <= dead_zone) {
        walk_step2 = dead_zone + 1U;
    }
    if (run_step1 <= walk_step2) {
        run_step1 = walk_step2 + 1U;
    }
    if (run_step2 <= run_step1) {
        run_step2 = run_step1 + 1U;
    }
    if (vertical_far <= dead_zone) {
        vertical_far = dead_zone + 1U;
    }
    if (max_ofs_x < 16U) {
        max_ofs_x = 16U;
    }
    if (max_ofs_y < 14U) {
        max_ofs_y = 14U;
    }

    ofs_x = test_stickman_clamp_i16(dx / 7, -(int32_t)max_ofs_x, (int32_t)max_ofs_x);
    ofs_y = test_stickman_clamp_i16(dy / 9, -(int32_t)max_ofs_y, (int32_t)max_ofs_y);
    diagonal_touch = (abs_x >= diagonal_gate) && (abs_y >= diagonal_gate) &&
                     (abs_x * 5U >= abs_y * 3U) && (abs_y * 5U >= abs_x * 3U);

    if (abs_x < dead_zone && abs_y < dead_zone) {
        group_name = "calm";
        ofs_x = 0;
        ofs_y = 0;
    } else if (diagonal_touch && dy < 0) {
        if (dx < 0) {
            group_name = "wave";
            ofs_x = test_stickman_clamp_i16(ofs_x - (int32_t)(max_ofs_x / 3U), -(int32_t)max_ofs_x, (int32_t)max_ofs_x);
            ofs_y = test_stickman_clamp_i16(ofs_y - (int32_t)(max_ofs_y / 3U), -(int32_t)max_ofs_y, (int32_t)max_ofs_y);
        } else {
            group_name = "point_r";
            ofs_x = test_stickman_clamp_i16(ofs_x + (int32_t)(max_ofs_x / 4U), -(int32_t)max_ofs_x, (int32_t)max_ofs_x);
            ofs_y = test_stickman_clamp_i16(ofs_y - (int32_t)(max_ofs_y / 4U), -(int32_t)max_ofs_y, (int32_t)max_ofs_y);
        }
    } else if (diagonal_touch && dy > 0) {
        if (dx < 0) {
            group_name = "hands_on_head";
        } else {
            group_name = "akimbo";
        }
    } else if (abs_x >= abs_y) {
        facing = (dx >= 0) ? 1 : -1;
        if (abs_x >= run_step1) {
            group_name = (facing > 0) ? "run_r" : "run_l";
            step_index = (abs_x >= run_step2) ? 2U : 1U;
            ofs_x = test_stickman_clamp_i16(ofs_x + ((facing > 0) ? (int32_t)(max_ofs_x / 4U) : -(int32_t)(max_ofs_x / 4U)),
                                            -(int32_t)max_ofs_x, (int32_t)max_ofs_x);
        } else {
            group_name = (facing > 0) ? "walk_r" : "walk_l";
            step_index = (abs_x >= walk_step2) ? 2U : 1U;
        }
    } else if (dy < 0) {
        group_name = (abs_y >= vertical_far) ? "jump" : "cheer";
        ofs_y = test_stickman_clamp_i16(ofs_y - (int32_t)(max_ofs_y / 4U), -(int32_t)max_ofs_y, (int32_t)max_ofs_y);
    } else {
        group_name = (abs_y >= vertical_far) ? "deep_squat_hold_head" : "half_squat";
        ofs_y = test_stickman_clamp_i16(ofs_y + (int32_t)(max_ofs_y / 5U), -(int32_t)max_ofs_y, (int32_t)max_ofs_y);
    }

    if (test_stickman_find_action(group_name, facing, step_index, &action_index) == NULL &&
            !(step_index > 1U && test_stickman_find_action(group_name, facing, 1U, &action_index) != NULL) &&
            !(test_stickman_find_action(group_name, 1, 1U, &action_index) != NULL)) {
        return false;
    }

    target_out->action_index = action_index;
    target_out->ofs_x = ofs_x;
    target_out->ofs_y = ofs_y;
    return true;
}

static void test_stickman_apply_touch_action(test_stickman_emote_scene_t *scene,
                                             uint16_t x,
                                             uint16_t y,
                                             bool snap_now)
{
    test_stickman_touch_target_t target;

    if (scene == NULL) {
        return;
    }
    if (!test_stickman_resolve_touch_target(x, y, &target)) {
        return;
    }

    test_stickman_apply_offset(scene, target.ofs_x, target.ofs_y);
    test_stickman_apply_action_index(scene, target.action_index, snap_now, false);
}

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

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    sequence_action_index = s_stickman_export.sequence[scene->current_seq_index];
    test_stickman_apply_offset(scene, 0, 0);
    test_stickman_apply_action_index(scene, sequence_action_index, snap_now, reset_timer);
}

static void test_stickman_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_stickman_emote_scene_t *scene = (test_stickman_emote_scene_t *)user_data;

    (void)touch;

    if (scene == NULL || event == NULL || s_stickman_export.sequence_count == 0U) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        scene->touch_active = true;
        if (scene->pose_timer != NULL) {
            gfx_timer_pause(scene->pose_timer);
        }
        test_stickman_apply_touch_action(scene, event->x, event->y, false);
        break;
    case GFX_TOUCH_EVENT_MOVE:
        if (scene->touch_active) {
            test_stickman_apply_touch_action(scene, event->x, event->y, false);
        }
        break;
    case GFX_TOUCH_EVENT_RELEASE:
    default:
        scene->touch_active = false;
        scene->current_seq_index = 0U;
        test_stickman_apply_sequence_action(scene, false, false);
        if (scene->pose_timer != NULL) {
            gfx_timer_set_period(scene->pose_timer, test_stickman_action_hold_ms(&s_stickman_export.actions[0]));
            gfx_timer_reset(scene->pose_timer);
            gfx_timer_resume(scene->pose_timer);
        }
        break;
    }
}

static void test_stickman_emote_timer_cb(void *user_data)
{
    test_stickman_emote_scene_t *scene = (test_stickman_emote_scene_t *)user_data;

    if (scene == NULL || scene->stickman_obj == NULL || s_stickman_export.sequence_count == 0U ||
            scene->touch_active) {
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

    test_app_log_step(TAG, "White stickman on black background; drag to steer walk/run, diagonals map to wave/point/hands-on-head/akimbo, release returns calm");
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
