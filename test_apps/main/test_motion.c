/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * Test: motion widget rig_active only.
 *
 * - Full-screen lobster motion preview
 * - Tap: switch to next action
 * - Hold/drag in move action: lobster swims toward the finger position
 * - Release: resumes autonomous swim
 */

#include <stddef.h>
#include <string.h>

#include "esp_random.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_motion_scene.h"
#include "common.h"

static const char *TAG = "test_motion";

#include "claw_motion.inc"

static const char *const s_motion_action_names[] = {
    [CLAW_MOTION_ACTION_POS_MOVE]     = "pos_move",
    [CLAW_MOTION_ACTION_POS_DOWN]     = "pos_down",
    [CLAW_MOTION_ACTION_POS_THINKING] = "pos_thinking",
    [CLAW_MOTION_ACTION_POS_HAPPY]    = "pos_happy",
};

typedef struct {
    gfx_motion_player_t      rt;
    uint16_t              seq_index;
    const gfx_motion_asset_t *asset;
    const char           *dbg_label;
    uint16_t              disp_w;
    uint16_t              disp_h;
    gfx_coord_t           canvas_home_x;
    gfx_coord_t           canvas_home_y;
    gfx_coord_t           move_y;
    uint16_t              canvas_w;
    uint16_t              canvas_h;
    uint16_t              move_step_px;
    uint16_t              move_hold_ticks;
    uint16_t              move_respawn_pad_px;
    gfx_coord_t           move_target_x;
    gfx_coord_t           move_target_y;
    gfx_timer_handle_t    motion_timer;
    bool                  touch_dragging;
    bool                  touch_moved;
    bool                  move_target_active;
    uint16_t              touch_press_x;
    uint16_t              touch_press_y;
    uint16_t              active_action;
} test_motion_slot_t;

static test_motion_slot_t s_motion_slot;

static void s_slot_force_apply(test_motion_slot_t *slot);
static void s_slot_apply_action(test_motion_slot_t *slot, uint16_t action_idx, bool snap);

#define MOTION_TOUCH_DRAG_THRESHOLD_PX 12
#define MOTION_MOVE_ACTION_IDX         CLAW_MOTION_ACTION_POS_MOVE
#define MOTION_MOVE_STEP_PX_MIN        0U
#define MOTION_MOVE_STEP_PX_MAX        2U
#define MOTION_MOVE_HOLD_TICKS_MIN     18U
#define MOTION_MOVE_HOLD_TICKS_MAX     48U
#define MOTION_MOVE_CHASE_STEP_MIN     3U
#define MOTION_MOVE_CHASE_STEP_MAX     10U
#define MOTION_MOVE_Y_JITTER_PX        24

static uint16_t s_random_u16_range(uint16_t min_value, uint16_t max_value)
{
    uint32_t span;

    if (max_value <= min_value) {
        return min_value;
    }

    span = (uint32_t)(max_value - min_value + 1U);
    return (uint16_t)(min_value + (esp_random() % span));
}

static gfx_coord_t s_step_coord_toward(gfx_coord_t cur, gfx_coord_t tgt, gfx_coord_t step)
{
    if (cur < tgt) {
        gfx_coord_t next = (gfx_coord_t)(cur + step);
        return (next > tgt) ? tgt : next;
    }
    if (cur > tgt) {
        gfx_coord_t next = (gfx_coord_t)(cur - step);
        return (next < tgt) ? tgt : next;
    }
    return cur;
}

static gfx_coord_t s_abs_coord_diff(gfx_coord_t a, gfx_coord_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

static int16_t s_random_i16_range(int16_t min_value, int16_t max_value)
{
    uint32_t span;

    if (max_value <= min_value) {
        return min_value;
    }

    span = (uint32_t)((int32_t)max_value - (int32_t)min_value + 1);
    return (int16_t)(min_value + (int16_t)(esp_random() % span));
}

static void s_slot_randomize_move_motion(test_motion_slot_t *slot)
{
    if (slot == NULL) {
        return;
    }

    slot->move_step_px = s_random_u16_range(MOTION_MOVE_STEP_PX_MIN, MOTION_MOVE_STEP_PX_MAX);
    slot->move_hold_ticks = (slot->move_step_px == 0U)
                            ? s_random_u16_range(MOTION_MOVE_HOLD_TICKS_MIN, MOTION_MOVE_HOLD_TICKS_MAX)
                            : 0U;
    slot->move_respawn_pad_px = s_random_u16_range(0U, (uint16_t)(slot->disp_w / 32U));
    slot->move_y = (gfx_coord_t)(slot->canvas_home_y +
                                 s_random_i16_range(-MOTION_MOVE_Y_JITTER_PX, MOTION_MOVE_Y_JITTER_PX));
}

static void s_slot_set_canvas(test_motion_slot_t *slot, gfx_coord_t x, gfx_coord_t y, bool force_apply)
{
    if (slot == NULL) {
        return;
    }

    if (gfx_motion_player_set_canvas(&slot->rt, x, y, slot->canvas_w, slot->canvas_h) != ESP_OK) {
        ESP_LOGW(TAG, "[%s] set canvas failed", slot->dbg_label ? slot->dbg_label : "?");
        return;
    }

    if (force_apply) {
        s_slot_force_apply(slot);
    }
}

static bool s_slot_is_move_action(const test_motion_slot_t *slot)
{
    return slot != NULL && slot->active_action == MOTION_MOVE_ACTION_IDX;
}

static void s_slot_reset_canvas_home(test_motion_slot_t *slot, bool force_apply)
{
    if (slot == NULL) {
        return;
    }

    slot->move_target_active = false;
    slot->move_y = slot->canvas_home_y;
    s_slot_set_canvas(slot, slot->canvas_home_x, slot->canvas_home_y, force_apply);
}

static void s_slot_spawn_move_from_right(test_motion_slot_t *slot, bool force_apply)
{
    if (slot == NULL) {
        return;
    }

    s_slot_randomize_move_motion(slot);
    slot->move_target_active = false;
    s_slot_set_canvas(slot,
                      (gfx_coord_t)(slot->disp_w + slot->move_respawn_pad_px),
                      slot->move_y,
                      force_apply);
}

static void s_slot_set_move_target_from_touch(test_motion_slot_t *slot, uint16_t touch_x, uint16_t touch_y)
{
    gfx_coord_t target_x;
    gfx_coord_t target_y;

    if (slot == NULL || !s_slot_is_move_action(slot)) {
        return;
    }

    target_x = (gfx_coord_t)((int32_t)touch_x - ((int32_t)slot->canvas_w / 2));
    target_y = (gfx_coord_t)((int32_t)touch_y - ((int32_t)slot->canvas_h / 2));
    if (target_x < -((gfx_coord_t)slot->canvas_w)) {
        target_x = -((gfx_coord_t)slot->canvas_w);
    }
    if (target_x > (gfx_coord_t)slot->disp_w) {
        target_x = (gfx_coord_t)slot->disp_w;
    }
    if (target_y < -((gfx_coord_t)slot->canvas_h)) {
        target_y = -((gfx_coord_t)slot->canvas_h);
    }
    if (target_y > (gfx_coord_t)slot->disp_h) {
        target_y = (gfx_coord_t)slot->disp_h;
    }

    slot->move_target_active = true;
    slot->move_target_x = target_x;
    slot->move_target_y = target_y;
    slot->move_y = target_y;

    if (slot->motion_timer != NULL) {
        gfx_timer_reset(slot->motion_timer);
    }
}

static void s_slot_force_apply(test_motion_slot_t *slot)
{
    esp_err_t err;

    if (slot == NULL) {
        return;
    }

    err = gfx_motion_player_sync(&slot->rt);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[%s] touch force-apply failed (%d)",
                 slot->dbg_label ? slot->dbg_label : "?", (int)err);
    }
}

static uint16_t s_sequence_action_at(const gfx_motion_asset_t *asset, uint16_t seq_index)
{
    if (asset == NULL || asset->sequence_count == 0U || asset->sequence == NULL) {
        return 0U;
    }
    return asset->sequence[seq_index % asset->sequence_count];
}

static const char *s_action_name_of(const gfx_motion_asset_t *asset, uint16_t action_idx)
{
    if (asset == &claw_motion_scene_asset &&
            action_idx < (uint16_t)(sizeof(s_motion_action_names) / sizeof(s_motion_action_names[0]))) {
        return s_motion_action_names[action_idx];
    }
    return "motion_action_unknown";
}

static void s_log_action_switch(const test_motion_slot_t *slot, uint16_t seq_index, uint16_t action_idx)
{
    const char *label = (slot != NULL && slot->dbg_label != NULL) ? slot->dbg_label : "?";
    const char *action_name = (slot != NULL && slot->asset != NULL)
                              ? s_action_name_of(slot->asset, action_idx)
                              : "action_unknown";

    ESP_LOGW(TAG, "[%s] sequence -> seq_idx=%u action_idx=%u action=%s",
             label, (unsigned)seq_index, (unsigned)action_idx, action_name);
}

static uint16_t s_sequence_index_of_action(const gfx_motion_asset_t *asset, uint16_t action_idx)
{
    if (asset == NULL || asset->sequence == NULL || asset->sequence_count == 0U) {
        return 0U;
    }
    for (uint16_t i = 0; i < asset->sequence_count; i++) {
        if (asset->sequence[i] == action_idx) {
            return i;
        }
    }
    return 0U;
}

static void s_slot_apply_action(test_motion_slot_t *slot, uint16_t action_idx, bool snap)
{
    if (slot == NULL || slot->asset == NULL || slot->asset->action_count == 0U) {
        return;
    }
    action_idx = (uint16_t)(action_idx % slot->asset->action_count);
    slot->seq_index = s_sequence_index_of_action(slot->asset, action_idx);
    slot->active_action = action_idx;
    s_log_action_switch(slot, slot->seq_index, action_idx);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_motion_player_set_action(&slot->rt, action_idx, snap));

    if (action_idx == MOTION_MOVE_ACTION_IDX) {
        s_slot_spawn_move_from_right(slot, false);
    } else {
        s_slot_reset_canvas_home(slot, false);
    }

    if (slot->motion_timer != NULL) {
        gfx_timer_reset(slot->motion_timer);
    }
    s_slot_force_apply(slot);
}

static bool s_slot_supports_touch_action_switch(const test_motion_slot_t *slot)
{
    return slot != NULL && slot->asset != NULL && slot->asset->action_count > 1U;
}

static void s_slot_cycle_next_action(test_motion_slot_t *slot, bool snap)
{
    uint16_t action_idx;

    if (slot == NULL || slot->asset == NULL || slot->asset->action_count == 0U) {
        return;
    }

    if (slot->asset->sequence_count > 0U && slot->asset->sequence != NULL) {
        slot->seq_index = (uint16_t)((slot->seq_index + 1U) % slot->asset->sequence_count);
        action_idx = s_sequence_action_at(slot->asset, slot->seq_index);
    } else {
        action_idx = (uint16_t)((slot->active_action + 1U) % slot->asset->action_count);
    }

    s_slot_apply_action(slot, action_idx, snap);
}

static void s_slot_motion_timer_cb(void *user_data)
{
    test_motion_slot_t *slot = (test_motion_slot_t *)user_data;
    gfx_coord_t next_x;
    gfx_coord_t next_y;
    gfx_coord_t chase_step;
    gfx_coord_t dx;
    gfx_coord_t dy;
    gfx_coord_t dist_hint;

    if (slot == NULL || !s_slot_is_move_action(slot)) {
        return;
    }

    if (slot->move_target_active) {
        dx = s_abs_coord_diff(slot->rt.canvas_x, slot->move_target_x);
        dy = s_abs_coord_diff(slot->rt.canvas_y, slot->move_target_y);
        dist_hint = (gfx_coord_t)(dx + (dy / 2));
        chase_step = (gfx_coord_t)(MOTION_MOVE_CHASE_STEP_MIN + (dist_hint / 18));
        if (chase_step < MOTION_MOVE_CHASE_STEP_MIN) {
            chase_step = MOTION_MOVE_CHASE_STEP_MIN;
        }
        if (chase_step > MOTION_MOVE_CHASE_STEP_MAX) {
            chase_step = MOTION_MOVE_CHASE_STEP_MAX;
        }
        next_x = s_step_coord_toward(slot->rt.canvas_x, slot->move_target_x, chase_step);
        next_y = s_step_coord_toward(slot->rt.canvas_y, slot->move_target_y, chase_step);
        s_slot_set_canvas(slot, next_x, next_y, true);
        return;
    }

    if (slot->move_hold_ticks > 0U) {
        slot->move_hold_ticks--;
        if (slot->move_hold_ticks == 0U && slot->move_step_px == 0U) {
            slot->move_step_px = s_random_u16_range(1U, MOTION_MOVE_STEP_PX_MAX);
        }
        return;
    }

    next_x = (gfx_coord_t)(slot->rt.canvas_x - (gfx_coord_t)slot->move_step_px);
    if (next_x <= -(gfx_coord_t)slot->canvas_w) {
        s_slot_spawn_move_from_right(slot, true);
        return;
    }

    s_slot_set_canvas(slot, next_x, slot->move_y, true);
}

static void s_slot_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    test_motion_slot_t *slot = (test_motion_slot_t *)user_data;
    int32_t dx;
    int32_t dy;
    int32_t abs_dx;
    int32_t abs_dy;

    (void)touch;

    if (slot == NULL || event == NULL) {
        return;
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        slot->touch_dragging = true;
        slot->touch_moved = false;
        slot->touch_press_x = event->x;
        slot->touch_press_y = event->y;
        s_slot_set_move_target_from_touch(slot, event->x, event->y);
        s_slot_force_apply(slot);
        if (slot->rt.motion.timer != NULL) {
            gfx_timer_reset(slot->rt.motion.timer);
        }
        break;

    case GFX_TOUCH_EVENT_MOVE:
        if (!slot->touch_dragging) {
            break;
        }
        dx = (int32_t)event->x - slot->touch_press_x;
        dy = (int32_t)event->y - slot->touch_press_y;
        abs_dx = dx >= 0 ? dx : -dx;
        abs_dy = dy >= 0 ? dy : -dy;
        if (abs_dx >= MOTION_TOUCH_DRAG_THRESHOLD_PX || abs_dy >= MOTION_TOUCH_DRAG_THRESHOLD_PX) {
            slot->touch_moved = true;
        }
        s_slot_set_move_target_from_touch(slot, event->x, event->y);
        s_slot_force_apply(slot);
        if (slot->rt.motion.timer != NULL) {
            gfx_timer_reset(slot->rt.motion.timer);
        }
        break;

    case GFX_TOUCH_EVENT_RELEASE:
    default:
        if (slot->touch_dragging) {
            slot->touch_dragging = false;
            slot->move_target_active = false;
            if (!slot->touch_moved && s_slot_supports_touch_action_switch(slot)) {
                s_slot_cycle_next_action(slot, true);
            }
            if (slot->rt.motion.timer != NULL) {
                gfx_timer_reset(slot->rt.motion.timer);
            }
        }
        break;
    }
}

static void test_motion_widget_run(void)
{
    uint16_t disp_w;
    uint16_t disp_h;

    test_app_log_case(TAG, "gfx_motion_player: lobster full-screen interactive preview");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    disp_w = (uint16_t)gfx_disp_get_hor_res(disp_default);
    disp_h = (uint16_t)gfx_disp_get_ver_res(disp_default);

    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x181818));
    gfx_disp_refresh_all(disp_default);

    memset(&s_motion_slot, 0, sizeof(s_motion_slot));
    s_motion_slot.asset     = &claw_motion_scene_asset;
    s_motion_slot.dbg_label = "rig_active";
    s_motion_slot.disp_w    = disp_w;
    s_motion_slot.disp_h    = disp_h;
    s_motion_slot.canvas_home_x = 0;
    s_motion_slot.canvas_home_y = 0;
    s_motion_slot.move_y = 0;
    s_motion_slot.move_target_x = 0;
    s_motion_slot.move_target_y = 0;
    s_motion_slot.canvas_w = (uint16_t)(disp_w * 10 / 10);
    s_motion_slot.canvas_h = (uint16_t)(disp_h * 10 / 10);
    s_motion_slot.move_step_px = MOTION_MOVE_STEP_PX_MIN;
    s_motion_slot.move_hold_ticks = 0U;
    s_motion_slot.move_respawn_pad_px = 0U;

    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_motion_player_init(&s_motion_slot.rt, disp_default, &claw_motion_scene_asset));
    TEST_ASSERT_EQUAL(ESP_OK,
                      gfx_motion_player_set_canvas(&s_motion_slot.rt,
                              s_motion_slot.canvas_home_x,
                              s_motion_slot.canvas_home_y,
                              s_motion_slot.canvas_w,
                              s_motion_slot.canvas_h));
    s_motion_slot.motion_timer = gfx_timer_create(emote_handle,
                                 s_slot_motion_timer_cb,
                                 claw_motion_layout.timer_period_ms,
                                 &s_motion_slot);
    TEST_ASSERT_NOT_NULL(s_motion_slot.motion_timer);

    ESP_LOGI(TAG, "disp_w=%u disp_h=%u", disp_w, disp_h);

    s_motion_slot.seq_index = 0;
    s_slot_apply_action(&s_motion_slot, s_sequence_action_at(&claw_motion_scene_asset, 0), true);
    test_app_set_touch_event_cb(s_slot_touch_event_cb, &s_motion_slot);

    test_app_unlock();

    test_app_log_step(TAG,
                      "Full display: rig_active - tap switches action, hold/drag in move action to guide swim, release resumes autonomous swim");
    // test_app_wait_for_observe(1000 * 100);
    test_app_wait_for_observe(1000 * 10000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_app_set_touch_event_cb(NULL, NULL);
    if (s_motion_slot.motion_timer != NULL) {
        gfx_timer_delete(emote_handle, s_motion_slot.motion_timer);
        s_motion_slot.motion_timer = NULL;
    }
    gfx_motion_player_deinit(&s_motion_slot.rt);
    test_app_unlock();
}

TEST_CASE("motion: rig pose preview", "[widget][motion]")
{
    test_app_runtime_t runtime;

    test_app_mem_log_snapshot(TAG, "entry");
    test_app_mem_monitor_start(TEST_APP_MEM_MONITOR_PERIOD_MS);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_motion_widget_run();
    test_app_mem_monitor_stop();
    test_app_runtime_close(&runtime);
}
