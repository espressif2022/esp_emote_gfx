/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_check.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_mesh_emote";

#define TEST_MESH_EMOTE_EMOTE_GRID_COLS       4U
#define TEST_MESH_EMOTE_EMOTE_GRID_ROWS       4U
#define TEST_MESH_EMOTE_EMOTE_POINT_COUNT     ((TEST_MESH_EMOTE_EMOTE_GRID_COLS + 1U) * (TEST_MESH_EMOTE_EMOTE_GRID_ROWS + 1U))
#define TEST_MESH_EMOTE_EMOTE_TIMER_PERIOD_MS 33U
#define TEST_MESH_EMOTE_EMOTE_EYE_COUNT       2U
#define TEST_MESH_EMOTE_EMOTE_LEFT_EYE_X_OFS  (-90)
#define TEST_MESH_EMOTE_EMOTE_RIGHT_EYE_X_OFS 90
#define TEST_MESH_EMOTE_EMOTE_EYE_Y_OFS       0
#define TEST_MESH_EMOTE_EMOTE_LEFT_EYE_SIGN   (-1)
#define TEST_MESH_EMOTE_EMOTE_RIGHT_EYE_SIGN  1
#define TEST_MESH_EMOTE_EMOTE_CENTER_POINT_IDX (((TEST_MESH_EMOTE_EMOTE_GRID_ROWS / 2U) * (TEST_MESH_EMOTE_EMOTE_GRID_COLS + 1U)) + (TEST_MESH_EMOTE_EMOTE_GRID_COLS / 2U))

typedef struct {
    int16_t offset_x;
    int16_t offset_y;
    int16_t gaze_x;
    int16_t gaze_y;
    int16_t stretch_x_permille;
    int16_t stretch_y_permille;
    int16_t top_lid_y;
    int16_t bottom_lid_y;
    int16_t side_inset_x;
    int16_t tilt_y_permille;
    int16_t center_lift_y;
} test_mesh_emote_pose_t;

typedef struct {
    int16_t gaze_x;
    int16_t gaze_y;
    int16_t blink;
    int16_t squint;
    int16_t widen;
    int16_t smile;
    int16_t sleepy;
    int16_t sad;
    int16_t surprise;
    int16_t angry;
    int16_t shift_x;
    int16_t shift_y;
} test_mesh_emote_expr_t;

typedef struct {
    test_mesh_emote_expr_t expr;
    uint16_t transition_ticks;
    uint16_t hold_ticks;
    uint8_t easing_mode;
    uint8_t hold_motion_mode;
} test_mesh_emote_clip_t;

typedef struct {
    const char *name;
    const char *desc;
    const test_mesh_emote_clip_t *clips;
    size_t clip_count;
} test_mesh_emote_action_t;

typedef struct {
    gfx_obj_t *mesh_obj;
    gfx_mesh_img_point_t base_points[TEST_MESH_EMOTE_EMOTE_POINT_COUNT];
    gfx_obj_t *top_lid_obj;
    gfx_obj_t *bottom_lid_obj;
    int16_t align_x;
    int16_t align_y;
    int8_t side_sign;
} test_mesh_emote_eye_t;

typedef struct {
    test_mesh_emote_eye_t eyes[TEST_MESH_EMOTE_EMOTE_EYE_COUNT];
    gfx_obj_t *next_btn;
    gfx_timer_handle_t anim_timer;
    test_mesh_emote_expr_t current_expr;
    test_mesh_emote_expr_t transition_from_expr;
    test_mesh_emote_pose_t current_pose;
    size_t action_idx;
    size_t clip_idx;
    size_t next_action_idx;
    size_t next_clip_idx;
    uint16_t hold_tick;
    uint16_t transition_tick;
    uint32_t anim_tick;
    volatile bool pending_manual_next;
    bool wait_for_manual_next;
    bool is_transitioning;
} test_mesh_emote_scene_t;

enum {
    TEST_MESH_EMOTE_EMOTE_EASING_GENTLE = 0,
    TEST_MESH_EMOTE_EMOTE_EASING_SNAP,
    TEST_MESH_EMOTE_EMOTE_EASING_LINGER,
    TEST_MESH_EMOTE_EMOTE_EASING_SETTLE,
};

enum {
    TEST_MESH_EMOTE_EMOTE_HOLD_NONE = 0,
    TEST_MESH_EMOTE_EMOTE_HOLD_IDLE,
    TEST_MESH_EMOTE_EMOTE_HOLD_STARE,
    TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS,
    TEST_MESH_EMOTE_EMOTE_HOLD_THINK,
    TEST_MESH_EMOTE_EMOTE_HOLD_SLEEPY,
    TEST_MESH_EMOTE_EMOTE_HOLD_HAPPY,
};

#define TEST_MESH_EMOTE_EMOTE_CLIP_EXT(gx, gy, blink, squint, widen, smile, sleepy, sad, surprise, angry, sx, sy, trans, hold, easing, motion) \
    {{gx, gy, blink, squint, widen, smile, sleepy, sad, surprise, angry, sx, sy}, trans, hold, easing, motion}

#define TEST_MESH_EMOTE_EMOTE_CLIP(gx, gy, blink, squint, widen, smile, sleepy, sad, surprise, sx, sy, trans, hold, easing, motion) \
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(gx, gy, blink, squint, widen, smile, sleepy, sad, surprise, 0, sx, sy, trans, hold, easing, motion)

static const test_mesh_emote_clip_t s_action_gif_angry[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    /* Blink / Anticipation (like Frame 005) */
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 50, 20, 0, 0, 0, 0, 0, 20, 0, 0, 6U, 2U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    /* Compressed angry blink (like Frame 010) */
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 80, 40, 0, 0, 0, 0, 0, 60, 0, 0, 4U, 3U, TEST_MESH_EMOTE_EMOTE_EASING_LINGER, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    /* Eyes snap open into sharp angry shape (like Frame 015-030) */
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 0, 10, 15, 0, 0, 0, 0, 120, 0, 0, 6U, 25U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    /* Settle into a sustained angry glare (like Frame 045) */
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 10U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    /* Relax back to idle (like Frame 060) */
    TEST_MESH_EMOTE_EMOTE_CLIP_EXT(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_idle[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18U, 26U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_blank_stare[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10U, 10U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 10, 48, 0, 0, 0, 20, 0, 0, 8U, 10U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 6, 34, 0, 0, 0, 10, 0, 0, 12U, 30U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_STARE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 20, 8, 26, 0, 0, 0, 6, 0, 0, 6U, 2U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 2, 18, 0, 0, 0, 4, 0, 0, 8U, 10U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_STARE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14U, 22U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_look_left[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(-10, 0, 80, 10, 0, 0, 0, 0, 0, -1, 0, 6U, 3U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(-34, 0, 0, 30, 0, 0, 0, 0, 0, -4, 0, 12U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    TEST_MESH_EMOTE_EMOTE_CLIP(-22, 0, 0, 15, 0, 0, 0, 0, 0, -2, 0, 10U, 10U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_blink_soft[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 12U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 100, 15, 0, 0, 0, 0, 0, 0, 0, 5U, 3U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, -10, 0, 15, 0, 0, 0, 0, 0, 0, 8U, 6U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_look_right[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(12, 0, 80, 10, 0, 0, 0, 0, 0, 1, 0, 6U, 3U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(38, 0, 0, 30, 0, 0, 0, 0, 0, 4, 0, 12U, 22U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    TEST_MESH_EMOTE_EMOTE_CLIP(20, 0, 0, 15, 0, 0, 0, 0, 0, 2, 0, 10U, 10U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_think_up[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(6, -18, 0, 12, 12, 0, 0, 0, 0, 0, -2, 18U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_LINGER, TEST_MESH_EMOTE_EMOTE_HOLD_THINK),
    TEST_MESH_EMOTE_EMOTE_CLIP(10, -34, 0, 20, 24, 0, 0, 0, 0, 0, -4, 20U, 28U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_THINK),
    TEST_MESH_EMOTE_EMOTE_CLIP(10, -22, 72, 24, 0, 0, 0, 0, 0, 0, -2, 10U, 2U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(8, -30, 0, 10, 18, 0, 0, 0, 0, 0, -4, 12U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_THINK),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18U, 24U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_sleepy[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 8, 0, 5, 0, 0, 20, 18, 0, 0, 0, 24U, 28U, TEST_MESH_EMOTE_EMOTE_EASING_LINGER, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 18, 20, 10, 0, 0, 60, 0, 0, 0, 3, 22U, 28U, TEST_MESH_EMOTE_EMOTE_EASING_LINGER, TEST_MESH_EMOTE_EMOTE_HOLD_SLEEPY),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 22, 110, 15, 0, 0, 80, 0, 0, 0, 4, 10U, 5U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 16, 30, 8, 0, 0, 50, 0, 0, 0, 3, 14U, 20U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_SLEEPY),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18U, 24U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_clip_t s_action_surprise_happy[] = {
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12U, 14U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -8, 0, 0, 72, 0, 0, 0, 60, 0, -4, 10U, 8U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -4, 0, 0, 28, 0, 0, 0, 24, 0, -2, 12U, 18U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -4, 0, 28, 10, 60, 0, 0, 0, 0, -2, 18U, 18U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_HAPPY),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -2, 72, 34, 0, 30, 0, 0, 0, 0, -1, 8U, 2U, TEST_MESH_EMOTE_EMOTE_EASING_SNAP, TEST_MESH_EMOTE_EMOTE_HOLD_NONE),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -4, 0, 16, 8, 50, 0, 0, 0, 0, -2, 10U, 12U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_HAPPY),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, -3, 0, 22, 8, 70, 0, 0, 0, 0, -2, 16U, 26U, TEST_MESH_EMOTE_EMOTE_EASING_GENTLE, TEST_MESH_EMOTE_EMOTE_HOLD_HAPPY),
    TEST_MESH_EMOTE_EMOTE_CLIP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18U, 24U, TEST_MESH_EMOTE_EMOTE_EASING_SETTLE, TEST_MESH_EMOTE_EMOTE_HOLD_IDLE),
};

static const test_mesh_emote_action_t s_action_playlist[] = {
    {"idle", "待机 (正面凝视)", s_action_idle, TEST_APP_ARRAY_SIZE(s_action_idle)},
    {"gif_angry", "愤怒怒视", s_action_gif_angry, TEST_APP_ARRAY_SIZE(s_action_gif_angry)},
    {"blank_stare", "放空直视", s_action_blank_stare, TEST_APP_ARRAY_SIZE(s_action_blank_stare)},
    {"look_left", "向左看", s_action_look_left, TEST_APP_ARRAY_SIZE(s_action_look_left)},
    {"blink_soft", "慢眨眼", s_action_blink_soft, TEST_APP_ARRAY_SIZE(s_action_blink_soft)},
    {"look_right", "向右看", s_action_look_right, TEST_APP_ARRAY_SIZE(s_action_look_right)},
    {"think_up", "向上看 / 思考", s_action_think_up, TEST_APP_ARRAY_SIZE(s_action_think_up)},
    {"sleepy", "困倦 / 发呆", s_action_sleepy, TEST_APP_ARRAY_SIZE(s_action_sleepy)},
    {"surprise_happy", "惊讶到开心", s_action_surprise_happy, TEST_APP_ARRAY_SIZE(s_action_surprise_happy)},
};

#undef TEST_MESH_EMOTE_EMOTE_CLIP

static int32_t test_mesh_emote_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mesh_emote_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int16_t test_mesh_emote_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return (int16_t)value;
}

static int32_t test_mesh_emote_lerp_i32(int32_t from, int32_t to, uint16_t t_permille)
{
    return from + (((to - from) * (int32_t)t_permille) / 1000);
}

static uint16_t test_mesh_emote_ease_in_cubic_permille(uint16_t t_permille)
{
    int64_t t = t_permille;
    int64_t eased = (t * t * t + 500000) / 1000000;

    if (eased > 1000) {
        eased = 1000;
    }

    return (uint16_t)eased;
}

static uint16_t test_mesh_emote_ease_out_cubic_permille(uint16_t t_permille)
{
    int64_t inv_t = 1000 - t_permille;
    int64_t eased = 1000 - ((inv_t * inv_t * inv_t + 500000) / 1000000);

    if (eased < 0) {
        eased = 0;
    } else if (eased > 1000) {
        eased = 1000;
    }

    return (uint16_t)eased;
}

static uint16_t test_mesh_emote_ease_in_out_cubic_permille(uint16_t t_permille)
{
    int64_t t = t_permille;

    if (t <= 500) {
        return (uint16_t)((4 * t * t * t + 500000) / 1000000);
    }

    t = 1000 - t;
    return (uint16_t)(1000 - ((4 * t * t * t + 500000) / 1000000));
}

static uint16_t test_mesh_emote_smoothstep_permille(uint16_t t_permille)
{
    int64_t t = t_permille;
    int64_t eased = (t * t * (3000 - 2 * t) + 500000) / 1000000;

    if (eased < 0) {
        eased = 0;
    } else if (eased > 1000) {
        eased = 1000;
    }

    return (uint16_t)eased;
}

static uint16_t test_mesh_emote_apply_easing(uint8_t easing_mode, uint16_t t_permille)
{
    switch (easing_mode) {
    case TEST_MESH_EMOTE_EMOTE_EASING_SNAP:
        return test_mesh_emote_ease_out_cubic_permille(t_permille);
    case TEST_MESH_EMOTE_EMOTE_EASING_LINGER:
        return test_mesh_emote_ease_in_cubic_permille(t_permille);
    case TEST_MESH_EMOTE_EMOTE_EASING_SETTLE:
        return test_mesh_emote_ease_in_out_cubic_permille(t_permille);
    case TEST_MESH_EMOTE_EMOTE_EASING_GENTLE:
    default:
        return test_mesh_emote_smoothstep_permille(t_permille);
    }
}

static esp_err_t test_mesh_emote_apply_pose(test_mesh_emote_scene_t *scene, const test_mesh_emote_pose_t *pose);
static void test_mesh_emote_update_next_btn_text(test_mesh_emote_scene_t *scene);

static const test_mesh_emote_action_t *test_mesh_emote_get_action(size_t action_idx)
{
    return &s_action_playlist[action_idx % TEST_APP_ARRAY_SIZE(s_action_playlist)];
}

static size_t test_mesh_emote_get_manual_action_count(void)
{
    return (TEST_APP_ARRAY_SIZE(s_action_playlist) > 1U) ? (TEST_APP_ARRAY_SIZE(s_action_playlist) - 1U) : 1U;
}

static size_t test_mesh_emote_get_first_manual_action_idx(void)
{
    return (TEST_APP_ARRAY_SIZE(s_action_playlist) > 1U) ? 1U : 0U;
}

static size_t test_mesh_emote_get_next_manual_action_idx(size_t current_action_idx)
{
    size_t next_action_idx = (current_action_idx + 1U) % TEST_APP_ARRAY_SIZE(s_action_playlist);

    if ((TEST_APP_ARRAY_SIZE(s_action_playlist) > 1U) && (next_action_idx == 0U)) {
        next_action_idx = 1U;
    }

    return next_action_idx;
}

static size_t test_mesh_emote_get_manual_action_pos(size_t action_idx)
{
    if (TEST_APP_ARRAY_SIZE(s_action_playlist) <= 1U) {
        return 1U;
    }

    if (action_idx == 0U) {
        return 1U;
    }

    return action_idx;
}

static const test_mesh_emote_clip_t *test_mesh_emote_get_clip(size_t action_idx, size_t clip_idx)
{
    const test_mesh_emote_action_t *action = test_mesh_emote_get_action(action_idx);

    return &action->clips[clip_idx % action->clip_count];
}

static test_mesh_emote_expr_t test_mesh_emote_lerp_expr(const test_mesh_emote_expr_t *from,
                                                    const test_mesh_emote_expr_t *to,
                                                    uint16_t t_permille)
{
    test_mesh_emote_expr_t expr = {0};

    expr.gaze_x = (int16_t)test_mesh_emote_lerp_i32(from->gaze_x, to->gaze_x, t_permille);
    expr.gaze_y = (int16_t)test_mesh_emote_lerp_i32(from->gaze_y, to->gaze_y, t_permille);
    expr.blink = (int16_t)test_mesh_emote_lerp_i32(from->blink, to->blink, t_permille);
    expr.squint = (int16_t)test_mesh_emote_lerp_i32(from->squint, to->squint, t_permille);
    expr.widen = (int16_t)test_mesh_emote_lerp_i32(from->widen, to->widen, t_permille);
    expr.smile = (int16_t)test_mesh_emote_lerp_i32(from->smile, to->smile, t_permille);
    expr.sleepy = (int16_t)test_mesh_emote_lerp_i32(from->sleepy, to->sleepy, t_permille);
    expr.sad = (int16_t)test_mesh_emote_lerp_i32(from->sad, to->sad, t_permille);
    expr.surprise = (int16_t)test_mesh_emote_lerp_i32(from->surprise, to->surprise, t_permille);
    expr.angry = (int16_t)test_mesh_emote_lerp_i32(from->angry, to->angry, t_permille);
    expr.shift_x = (int16_t)test_mesh_emote_lerp_i32(from->shift_x, to->shift_x, t_permille);
    expr.shift_y = (int16_t)test_mesh_emote_lerp_i32(from->shift_y, to->shift_y, t_permille);
    return expr;
}

static test_mesh_emote_pose_t test_mesh_emote_expr_to_pose(const test_mesh_emote_expr_t *expr)
{
    test_mesh_emote_pose_t pose = {0};
    int32_t blink = expr->blink;
    int32_t squint = expr->squint;
    int32_t widen = expr->widen;
    int32_t smile = expr->smile;
    int32_t sleepy = expr->sleepy;
    int32_t sad = expr->sad;
    int32_t surprise = expr->surprise;
    int32_t angry = expr->angry;

    pose.offset_x = test_mesh_emote_clamp_i16((int32_t)expr->shift_x + (expr->gaze_x / 10), -40, 40);
    pose.offset_y = test_mesh_emote_clamp_i16((int32_t)expr->shift_y + (expr->gaze_y / 18) + (sleepy / 18) - (surprise / 28), -32, 32);
    pose.gaze_x = test_mesh_emote_clamp_i16(expr->gaze_x, -180, 180);
    pose.gaze_y = test_mesh_emote_clamp_i16(expr->gaze_y, -180, 180);

    pose.stretch_x_permille = test_mesh_emote_clamp_i16((squint / 2) - (widen / 3) + (surprise / 4) + (smile / 5),
                                                      -240, 240);
    pose.stretch_y_permille = test_mesh_emote_clamp_i16((widen / 2) + (surprise / 3) - blink - (sleepy / 2) - (squint / 3),
                                                      -240, 240);

    pose.top_lid_y = test_mesh_emote_clamp_i16(blink + (sleepy / 2) + (sad / 2) + angry - (widen / 3) - (surprise / 3),
                                             -48, 96);
    pose.bottom_lid_y = test_mesh_emote_clamp_i16((blink * 3 / 4) + (smile / 2) + (squint / 2) + (sleepy / 5) + (angry / 4) - (widen / 5),
                                                -48, 96);
    pose.side_inset_x = test_mesh_emote_clamp_i16((squint / 4) + (smile / 6) + (surprise / 8) + (angry / 4),
                                                -48, 48);
    pose.tilt_y_permille = test_mesh_emote_clamp_i16((expr->gaze_x / 2) + (sad / 2) - (smile / 4) - angry,
                                                   -120, 120);
    pose.center_lift_y = test_mesh_emote_clamp_i16(-(smile / 6) - (surprise / 10) - (sad / 3) + (sleepy / 5) - (expr->gaze_y / 16),
                                                 -28, 28);
    return pose;
}

static void test_mesh_emote_capture_base_points(test_mesh_emote_eye_t *eye)
{
    TEST_ASSERT_NOT_NULL(eye);
    TEST_ASSERT_NOT_NULL(eye->mesh_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MESH_EMOTE_EMOTE_POINT_COUNT, gfx_mesh_img_get_point_count(eye->mesh_obj));

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(eye->mesh_obj, i, &eye->base_points[i]));
    }
}

static void test_mesh_emote_align_eye(test_mesh_emote_eye_t *eye)
{
    if (eye == NULL || eye->mesh_obj == NULL) {
        return;
    }

    gfx_obj_align(eye->mesh_obj, GFX_ALIGN_CENTER, eye->align_x, eye->align_y);
}

static test_mesh_emote_pose_t test_mesh_emote_refine_pose_for_eye(const test_mesh_emote_eye_t *eye,
                                                              const test_mesh_emote_pose_t *pose)
{
    test_mesh_emote_pose_t refined_pose;
    int32_t lateral_gaze;
    int32_t vertical_gaze;
    int32_t lateral_focus;
    int32_t down_gaze;
    int32_t up_gaze;
    int32_t eye_bias;
    int32_t blink_bias;

    TEST_ASSERT_NOT_NULL(eye);
    TEST_ASSERT_NOT_NULL(pose);

    refined_pose = *pose;
    lateral_gaze = pose->gaze_x;
    vertical_gaze = pose->gaze_y;
    lateral_focus = test_mesh_emote_abs_i32(lateral_gaze);
    down_gaze = test_mesh_emote_max_i32(vertical_gaze, 0);
    up_gaze = test_mesh_emote_max_i32(-vertical_gaze, 0);
    eye_bias = (lateral_gaze * eye->side_sign) / 8;
    blink_bias = (lateral_gaze * eye->side_sign) / 12;

    refined_pose.gaze_x = test_mesh_emote_clamp_i16((int32_t)pose->gaze_x + eye_bias, -180, 180);
    refined_pose.offset_x = test_mesh_emote_clamp_i16((int32_t)pose->offset_x + (eye_bias / 3), -40, 40);

    refined_pose.stretch_x_permille = test_mesh_emote_clamp_i16((int32_t)pose->stretch_x_permille + (lateral_focus / 2),
                                                              -240, 240);
    refined_pose.side_inset_x = test_mesh_emote_clamp_i16((int32_t)pose->side_inset_x + (lateral_focus / 7) + (eye_bias / 2),
                                                        -48, 48);
    refined_pose.tilt_y_permille = test_mesh_emote_clamp_i16((int32_t)pose->tilt_y_permille + ((lateral_gaze * eye->side_sign) / 6),
                                                           -120, 120);

    refined_pose.top_lid_y = test_mesh_emote_clamp_i16((int32_t)pose->top_lid_y + (down_gaze / 3) - (up_gaze / 5) + (blink_bias / 2),
                                                     -48, 96);
    refined_pose.bottom_lid_y = test_mesh_emote_clamp_i16((int32_t)pose->bottom_lid_y + (down_gaze / 5) - (up_gaze / 7) + (blink_bias / 3) + (lateral_focus / 12),
                                                        -48, 96);
    refined_pose.center_lift_y = test_mesh_emote_clamp_i16((int32_t)pose->center_lift_y - (test_mesh_emote_abs_i32(vertical_gaze) / 8),
                                                         -28, 28);

    return refined_pose;
}

static void test_mesh_emote_update_eyelid_matrix(test_mesh_emote_eye_t *eye, gfx_obj_t *lid_mesh, int32_t y_drop, int32_t angle_permille, int32_t size_pad, bool is_top)
{
    int32_t half_w = 120 + size_pad;

    int32_t sin_a = angle_permille;
    int32_t cos_a = 1000;
    int32_t dir = is_top ? 1 : -1;

    int32_t edge_y = (is_top ? -80 : 80) + y_drop;
    int32_t back_y = edge_y - (140 * dir);

    for (int i = 0; i < 4; i++) {
        int32_t base_x = (i % 2 == 0) ? -half_w : half_w;
        int32_t base_y = (i < 2) ? back_y : edge_y;

        if (!is_top) {
            base_y = (i < 2) ? edge_y : back_y;
        }

        int32_t rot_x = (base_x * cos_a - base_y * sin_a) / 1000;
        int32_t rot_y = (base_x * sin_a + base_y * cos_a) / 1000;

        rot_x += eye->align_x;
        rot_y += eye->align_y;

        gfx_mesh_img_set_point(lid_mesh, i, (gfx_coord_t)rot_x, (gfx_coord_t)rot_y);
    }
}

static esp_err_t test_mesh_emote_apply_pose_to_eye(test_mesh_emote_eye_t *eye, const test_mesh_emote_pose_t *pose)
{
    int32_t center_x;
    int32_t center_y;
    int32_t max_abs_x = 1;
    int32_t max_abs_y = 1;
    test_mesh_emote_pose_t refined_pose;

    ESP_RETURN_ON_FALSE(eye != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: eye is NULL");
    ESP_RETURN_ON_FALSE(eye->mesh_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: object is NULL");
    ESP_RETURN_ON_FALSE(pose != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: pose is NULL");

    refined_pose = test_mesh_emote_refine_pose_for_eye(eye, pose);

    center_x = eye->base_points[TEST_MESH_EMOTE_EMOTE_CENTER_POINT_IDX].x;
    center_y = eye->base_points[TEST_MESH_EMOTE_EMOTE_CENTER_POINT_IDX].y;

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_POINT_COUNT; i++) {
        int32_t rel_x = eye->base_points[i].x - center_x;
        int32_t rel_y = eye->base_points[i].y - center_y;

        max_abs_x = test_mesh_emote_max_i32(max_abs_x, test_mesh_emote_abs_i32(rel_x));
        max_abs_y = test_mesh_emote_max_i32(max_abs_y, test_mesh_emote_abs_i32(rel_y));
    }

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_POINT_COUNT; i++) {
        int32_t rel_x = eye->base_points[i].x - center_x;
        int32_t rel_y = eye->base_points[i].y - center_y;
        int32_t nx = (rel_x * 1000) / max_abs_x;
        int32_t ny = (rel_y * 1000) / max_abs_y;
        int32_t abs_nx = test_mesh_emote_abs_i32(nx);
        int32_t abs_ny = test_mesh_emote_abs_i32(ny);
        int32_t center_x_weight = 1000 - abs_nx;
        int32_t center_y_weight = 1000 - abs_ny;
        int32_t center_weight = (center_x_weight * center_y_weight) / 1000;
        int32_t top_weight = (ny < 0) ? -ny : 0;
        int32_t bottom_weight = (ny > 0) ? ny : 0;
        int32_t left_weight = (nx < 0) ? -nx : 0;
        int32_t right_weight = (nx > 0) ? nx : 0;
        int32_t dx = refined_pose.offset_x;
        int32_t dy = refined_pose.offset_y;

        dx += (refined_pose.gaze_x * center_y_weight) / 1000;
        dy += (refined_pose.gaze_y * center_x_weight) / 1000;

        dx += (rel_x * (int32_t)refined_pose.stretch_x_permille * center_y_weight) / 1000000;
        dy += (rel_y * (int32_t)refined_pose.stretch_y_permille * center_x_weight) / 1000000;

        /* Radically decouple actual eyelid movements from base mesh geometry:
           We allow a tiny 8% fleshy squash to sell the organic feel of the eye 
           reacting to lid pressure, but the vector overlay handles 92% of the occlusion! */
        dy += (refined_pose.top_lid_y * top_weight * center_x_weight) / 12500000;
        dy -= (refined_pose.bottom_lid_y * bottom_weight * center_x_weight) / 12500000;

        dx += (refined_pose.side_inset_x * left_weight * center_y_weight) / 1000000;
        dx -= (refined_pose.side_inset_x * right_weight * center_y_weight) / 1000000;

        dy += (rel_x * (int32_t)refined_pose.tilt_y_permille * center_y_weight) / 1000000;
        dy += (refined_pose.center_lift_y * center_weight) / 1000;

        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_point(eye->mesh_obj, i,
                                                   (gfx_coord_t)(eye->base_points[i].x + dx),
                                                   (gfx_coord_t)(eye->base_points[i].y + dy)),
                            TAG, "apply pose: set point failed");
    }

    if (eye->top_lid_obj != NULL) {
        int32_t drop = refined_pose.top_lid_y + (refined_pose.gaze_y / 3);
        int32_t rot = refined_pose.tilt_y_permille / 2;
        test_mesh_emote_update_eyelid_matrix(eye, eye->top_lid_obj, drop, rot, 20, true);
    }
    if (eye->bottom_lid_obj != NULL) {
        int32_t rise = refined_pose.bottom_lid_y - (refined_pose.gaze_y / 5);
        int32_t rot = refined_pose.tilt_y_permille / 2;
        test_mesh_emote_update_eyelid_matrix(eye, eye->bottom_lid_obj, -rise, rot, 20, false);
    }

    test_mesh_emote_align_eye(eye);
    return ESP_OK;
}

static int16_t test_mesh_emote_triangle_wave_i16(uint32_t tick, uint16_t period, uint16_t phase, int16_t amplitude)
{
    int32_t local_tick;
    int32_t half_period;
    int32_t wave;

    if (period == 0U || amplitude == 0) {
        return 0;
    }

    local_tick = (int32_t)((tick + phase) % period);
    half_period = (int32_t)period / 2;
    if (half_period == 0) {
        return 0;
    }

    if (local_tick < half_period) {
        wave = -1000 + ((local_tick * 2000) / half_period);
    } else {
        wave = 1000 - (((local_tick - half_period) * 2000) / ((int32_t)period - half_period));
    }

    return (int16_t)((wave * amplitude) / 1000);
}

static test_mesh_emote_expr_t test_mesh_emote_apply_hold_motion(const test_mesh_emote_scene_t *scene,
                                                            const test_mesh_emote_clip_t *clip)
{
    test_mesh_emote_expr_t expr;
    int16_t primary_x;
    int16_t primary_y;
    int16_t secondary;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(clip);

    expr = clip->expr;
    primary_x = test_mesh_emote_triangle_wave_i16(scene->anim_tick, 42U,
                                                (uint16_t)((scene->action_idx * 17U) + (scene->clip_idx * 5U)), 4);
    primary_y = test_mesh_emote_triangle_wave_i16(scene->anim_tick, 56U,
                                                (uint16_t)((scene->action_idx * 23U) + (scene->clip_idx * 7U)), 3);
    secondary = test_mesh_emote_triangle_wave_i16(scene->anim_tick, 84U,
                                                (uint16_t)((scene->action_idx * 31U) + (scene->clip_idx * 11U)), 12);

    switch (clip->hold_motion_mode) {
    case TEST_MESH_EMOTE_EMOTE_HOLD_IDLE:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + primary_x, -180, 180);
        expr.gaze_y = test_mesh_emote_clamp_i16((int32_t)expr.gaze_y + (primary_y / 2), -180, 180);
        expr.shift_y = test_mesh_emote_clamp_i16((int32_t)expr.shift_y + (primary_y / 2), -24, 24);
        expr.widen = test_mesh_emote_clamp_i16((int32_t)expr.widen + (secondary / 2), -80, 120);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_STARE:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + (primary_x / 4), -180, 180);
        expr.gaze_y = test_mesh_emote_clamp_i16((int32_t)expr.gaze_y + (primary_y / 5), -180, 180);
        expr.shift_y = test_mesh_emote_clamp_i16((int32_t)expr.shift_y + (primary_y / 4), -24, 24);
        expr.widen = test_mesh_emote_clamp_i16((int32_t)expr.widen + 4 + (test_mesh_emote_abs_i32(secondary) / 4), -80, 140);
        expr.surprise = test_mesh_emote_clamp_i16((int32_t)expr.surprise + (test_mesh_emote_abs_i32(primary_y) / 4), 0, 120);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_FOCUS:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + (primary_x / 2), -180, 180);
        expr.squint = test_mesh_emote_clamp_i16((int32_t)expr.squint + test_mesh_emote_abs_i32(primary_x), -80, 240);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_THINK:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + (primary_x / 2), -180, 180);
        expr.gaze_y = test_mesh_emote_clamp_i16((int32_t)expr.gaze_y + primary_y, -180, 180);
        expr.widen = test_mesh_emote_clamp_i16((int32_t)expr.widen + 4 + (test_mesh_emote_abs_i32(primary_y) / 2), -80, 160);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_SLEEPY:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + (primary_x / 3), -180, 180);
        expr.gaze_y = test_mesh_emote_clamp_i16((int32_t)expr.gaze_y + test_mesh_emote_abs_i32(primary_y), -180, 180);
        expr.sleepy = test_mesh_emote_clamp_i16((int32_t)expr.sleepy + 4 + test_mesh_emote_abs_i32(primary_y), 0, 160);
        expr.shift_y = test_mesh_emote_clamp_i16((int32_t)expr.shift_y + 1 + (primary_y / 3), -24, 24);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_HAPPY:
        expr.gaze_x = test_mesh_emote_clamp_i16((int32_t)expr.gaze_x + (primary_x / 2), -180, 180);
        expr.smile = test_mesh_emote_clamp_i16((int32_t)expr.smile + 4 + test_mesh_emote_abs_i32(primary_x), 0, 180);
        expr.squint = test_mesh_emote_clamp_i16((int32_t)expr.squint + 2 + (test_mesh_emote_abs_i32(primary_y) / 2), -80, 180);
        break;
    case TEST_MESH_EMOTE_EMOTE_HOLD_NONE:
    default:
        break;
    }

    return expr;
}

static esp_err_t test_mesh_emote_apply_expr(test_mesh_emote_scene_t *scene, const test_mesh_emote_expr_t *expr)
{
    test_mesh_emote_pose_t pose;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply expr: scene is NULL");
    ESP_RETURN_ON_FALSE(expr != NULL, ESP_ERR_INVALID_ARG, TAG, "apply expr: expr is NULL");

    pose = test_mesh_emote_expr_to_pose(expr);
    ESP_RETURN_ON_ERROR(test_mesh_emote_apply_pose(scene, &pose), TAG, "apply expr: pose failed");
    scene->current_expr = *expr;
    scene->current_pose = pose;
    return ESP_OK;
}

static esp_err_t test_mesh_emote_apply_pose(test_mesh_emote_scene_t *scene, const test_mesh_emote_pose_t *pose)
{
    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: scene is NULL");
    ESP_RETURN_ON_FALSE(pose != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: pose is NULL");

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_EYE_COUNT; i++) {
        ESP_RETURN_ON_ERROR(test_mesh_emote_apply_pose_to_eye(&scene->eyes[i], pose),
                            TAG, "apply pose: eye %u failed", (unsigned)i);
    }

    scene->current_pose = *pose;
    return ESP_OK;
}

static void test_mesh_emote_update_next_btn_text(test_mesh_emote_scene_t *scene)
{
    char text[16];

    if (scene == NULL || scene->next_btn == NULL) {
        return;
    }

    snprintf(text, sizeof(text), "%u/%u",
             (unsigned)test_mesh_emote_get_manual_action_pos(scene->action_idx),
             (unsigned)test_mesh_emote_get_manual_action_count());
    gfx_button_set_text(scene->next_btn, text);
}

static void test_mesh_emote_next_btn_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    test_mesh_emote_scene_t *scene = (test_mesh_emote_scene_t *)user_data;

    (void)obj;

    if (scene == NULL || event == NULL) {
        return;
    }

    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        scene->pending_manual_next = true;
    }
}

static void test_mesh_emote_start_transition_to(test_mesh_emote_scene_t *scene, size_t next_action_idx, size_t next_clip_idx)
{
    const test_mesh_emote_action_t *action;
    const test_mesh_emote_action_t *next_action;

    if (scene == NULL) {
        return;
    }

    action = test_mesh_emote_get_action(scene->action_idx);
    scene->next_action_idx = next_action_idx;
    scene->next_clip_idx = next_clip_idx;
    next_action = test_mesh_emote_get_action(scene->next_action_idx);

    if (scene->next_action_idx != scene->action_idx) {
        ESP_LOGI(TAG, "Action transition: %s -> %s (%s)",
                 action->name, next_action->name,
                 (next_action->desc != NULL) ? next_action->desc : next_action->name);
    }

    scene->transition_from_expr = scene->current_expr;
    scene->transition_tick = 0U;
    scene->wait_for_manual_next = false;
    scene->is_transitioning = true;
    test_mesh_emote_update_next_btn_text(scene);
}

static void test_mesh_emote_anim_cb(void *user_data)
{
    test_mesh_emote_scene_t *scene = (test_mesh_emote_scene_t *)user_data;
    const test_mesh_emote_action_t *action;
    const test_mesh_emote_clip_t *target_clip;
    const test_mesh_emote_clip_t *current_clip;
    test_mesh_emote_expr_t expr;
    test_mesh_emote_expr_t hold_expr;
    uint16_t duration;
    uint16_t t_permille;
    uint16_t eased_permille;

    if (scene == NULL) {
        return;
    }

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_EYE_COUNT; i++) {
        if (scene->eyes[i].mesh_obj == NULL) {
            return;
        }
    }

    scene->anim_tick++;

    if (!scene->is_transitioning) {
        action = test_mesh_emote_get_action(scene->action_idx);
        current_clip = test_mesh_emote_get_clip(scene->action_idx, scene->clip_idx);
        hold_expr = test_mesh_emote_apply_hold_motion(scene, current_clip);

        if (test_mesh_emote_apply_expr(scene, &hold_expr) != ESP_OK) {
            scene->hold_tick = 0U;
            return;
        }

        scene->hold_tick++;
        if (scene->hold_tick < current_clip->hold_ticks) {
            return;
        }

        scene->hold_tick = 0U;

        if ((scene->clip_idx + 1U) < action->clip_count) {
            test_mesh_emote_start_transition_to(scene, scene->action_idx, scene->clip_idx + 1U);
            return;
        }

        if (!scene->wait_for_manual_next) {
            scene->wait_for_manual_next = true;
            test_mesh_emote_update_next_btn_text(scene);
        }

        if (!scene->pending_manual_next) {
            return;
        }

        scene->pending_manual_next = false;
        test_mesh_emote_start_transition_to(scene, test_mesh_emote_get_next_manual_action_idx(scene->action_idx), 0U);
    }

    target_clip = test_mesh_emote_get_clip(scene->next_action_idx, scene->next_clip_idx);
    duration = target_clip->transition_ticks;
    if (duration == 0U) {
        duration = 1U;
    }

    scene->transition_tick++;
    if (scene->transition_tick > duration) {
        scene->transition_tick = duration;
    }

    t_permille = (uint16_t)(((uint32_t)scene->transition_tick * 1000U) / duration);
    eased_permille = test_mesh_emote_apply_easing(target_clip->easing_mode, t_permille);
    expr = test_mesh_emote_lerp_expr(&scene->transition_from_expr, &target_clip->expr, eased_permille);

    if (test_mesh_emote_apply_expr(scene, &expr) != ESP_OK) {
        scene->transition_tick = 0U;
        scene->hold_tick = 0U;
        scene->is_transitioning = false;
        return;
    }

    if (scene->transition_tick >= duration) {
        scene->action_idx = scene->next_action_idx;
        scene->clip_idx = scene->next_clip_idx;
        scene->transition_tick = 0U;
        scene->hold_tick = 0U;
        scene->is_transitioning = false;
        test_mesh_emote_update_next_btn_text(scene);
    }
}

static void test_mesh_emote_scene_cleanup(test_mesh_emote_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }

    if (scene->next_btn != NULL) {
        gfx_obj_delete(scene->next_btn);
        scene->next_btn = NULL;
    }

    for (size_t i = 0; i < TEST_MESH_EMOTE_EMOTE_EYE_COUNT; i++) {
        if (scene->eyes[i].top_lid_obj != NULL) {
            gfx_obj_delete(scene->eyes[i].top_lid_obj);
            scene->eyes[i].top_lid_obj = NULL;
        }
        if (scene->eyes[i].bottom_lid_obj != NULL) {
            gfx_obj_delete(scene->eyes[i].bottom_lid_obj);
            scene->eyes[i].bottom_lid_obj = NULL;
        }
        if (scene->eyes[i].mesh_obj != NULL) {
            gfx_obj_delete(scene->eyes[i].mesh_obj);
            scene->eyes[i].mesh_obj = NULL;
        }
    }
}

static const uint16_t s_black_pixel = 0x0000;
static const gfx_image_dsc_t s_black_img = {
    .header = {
        .magic = 0x19, // C_ARRAY_HEADER_MAGIC
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2
    },
    .data_size = 2,
    .data = (const uint8_t *) &s_black_pixel
};

static void test_mesh_emote_init_eye(test_mesh_emote_eye_t *eye, int16_t align_x, int16_t align_y, int8_t side_sign)
{
    TEST_ASSERT_NOT_NULL(eye);

    eye->align_x = align_x;
    eye->align_y = align_y;
    eye->side_sign = side_sign;

    // Base eyeball layer
    eye->mesh_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(eye->mesh_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(eye->mesh_obj, (void *)&orb_ball_center));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(eye->mesh_obj, TEST_MESH_EMOTE_EMOTE_GRID_COLS, TEST_MESH_EMOTE_EMOTE_GRID_ROWS));
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->mesh_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->mesh_obj, true));

    test_mesh_emote_capture_base_points(eye);

    // Top eyelid layer
    eye->top_lid_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(eye->top_lid_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(eye->top_lid_obj, (void *)&s_black_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(eye->top_lid_obj, 1, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->top_lid_obj, false));
    // TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->top_lid_obj, true));

    // Bottom eyelid layer
    eye->bottom_lid_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(eye->bottom_lid_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(eye->bottom_lid_obj, (void *)&s_black_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(eye->bottom_lid_obj, 1, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->bottom_lid_obj, false));

    // Perform initial alignment off-screen so they don't block
    test_mesh_emote_update_eyelid_matrix(eye, eye->top_lid_obj, -200, 0, 0, true);
    test_mesh_emote_update_eyelid_matrix(eye, eye->bottom_lid_obj, -200, 0, 0, false);
}

static void test_mesh_emote_run(void)
{
    test_mesh_emote_scene_t scene = {0};

    test_app_log_case(TAG, "Mesh image local deform show case");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    // gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x0f172a));
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    test_mesh_emote_init_eye(&scene.eyes[0], TEST_MESH_EMOTE_EMOTE_LEFT_EYE_X_OFS, TEST_MESH_EMOTE_EMOTE_EYE_Y_OFS, TEST_MESH_EMOTE_EMOTE_LEFT_EYE_SIGN);
    test_mesh_emote_init_eye(&scene.eyes[1], TEST_MESH_EMOTE_EMOTE_RIGHT_EYE_X_OFS, TEST_MESH_EMOTE_EMOTE_EYE_Y_OFS, TEST_MESH_EMOTE_EMOTE_RIGHT_EYE_SIGN);

    scene.next_btn = gfx_button_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.next_btn);

    gfx_obj_set_size(scene.next_btn, 72, 40);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(scene.next_btn, "1/1"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_font(scene.next_btn, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(scene.next_btn, GFX_COLOR_HEX(0x2A6DF4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(scene.next_btn, GFX_COLOR_HEX(0x163D87)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(scene.next_btn, GFX_COLOR_HEX(0xDCE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_width(scene.next_btn, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.next_btn, test_mesh_emote_next_btn_cb, &scene));
    gfx_obj_align(scene.next_btn, GFX_ALIGN_TOP_MID, 0, 0);

    scene.action_idx = test_mesh_emote_get_first_manual_action_idx();
    scene.clip_idx = 0U;
    scene.next_action_idx = scene.action_idx;
    scene.next_clip_idx = 0U;
    scene.pending_manual_next = false;
    scene.wait_for_manual_next = false;
    scene.current_expr = test_mesh_emote_get_clip(scene.action_idx, scene.clip_idx)->expr;
    TEST_ASSERT_EQUAL(ESP_OK, test_mesh_emote_apply_expr(&scene, &scene.current_expr));
    test_mesh_emote_update_next_btn_text(&scene);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mesh_emote_anim_cb, TEST_MESH_EMOTE_EMOTE_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Observe local mesh point deformation");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mesh_emote_scene_cleanup(&scene);
    test_app_unlock();
}

// TEST_CASE("widget mesh image deform", "[widget][image][mesh]")
void test_mesh_emote_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mesh_emote_run();
    test_app_runtime_close(&runtime);
}
