/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_log.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_eye_layered";

#define TEST_EYE_TIMER_PERIOD_MS       33U
#define TEST_EYE_WHITE_SIZE            120
#define TEST_EYE_WHITE_Y_OFS           (-6)
#define TEST_EYE_LID_SIDE_PAD          24
#define TEST_EYE_LID_OVERDRAW          36
#define TEST_EYE_SOCKET_FOLLOW_X_DIV   5
#define TEST_EYE_SOCKET_FOLLOW_Y_DIV   6
#define TEST_EYE_LOOK_X_PIXELS         26
#define TEST_EYE_LOOK_Y_PIXELS         18

#define TEST_FACE_EYE_GAP              24
#define TEST_FACE_Y_OFS                (-16)
#define TEST_FACE_MOUTH_WIDTH          100
#define TEST_FACE_MOUTH_BASE_H         6
#define TEST_FACE_MOUTH_COLS           4
#define TEST_FACE_MOUTH_BELOW_EYE      28
#define TEST_FACE_MOUTH_SMILE_PX       32
#define TEST_FACE_MOUTH_OPEN_PX        40

typedef struct {
    int16_t eye_x_ofs;
    int16_t eye_y_ofs;
    int16_t upper_lid;
    int16_t lower_lid;
    int16_t lid_tilt_permille;
    int16_t mouth_curve;
    int16_t mouth_open_px;
} test_eye_layered_pose_t;

/* Semantic controls: this is the "rig" layer that animation clips keyframe. */
typedef struct {
    int16_t look_x;
    int16_t look_y;
    int16_t blink;
    int16_t squint;
    int16_t widen;
    int16_t softness;
    int16_t eye_x_ofs;
    int16_t eye_y_ofs;
    int16_t mouth_smile;
    int16_t mouth_open;
} test_eye_layered_ctrl_t;

typedef enum {
    TEST_EYE_EASING_LINEAR = 0,
    TEST_EYE_EASING_SMOOTHSTEP,
    TEST_EYE_EASING_EASE_OUT_CUBIC,
    TEST_EYE_EASING_EASE_IN_OUT_CUBIC,
} test_eye_layered_easing_t;

/* One keyframe in a clip: semantic controls + timing + easing. */
typedef struct {
    test_eye_layered_ctrl_t ctrl;
    uint16_t transition_ticks;
    uint16_t hold_ticks;
    test_eye_layered_easing_t easing;
} test_eye_layered_key_t;

typedef struct {
    const char *name;
    const test_eye_layered_key_t *keys;
    size_t key_count;
} test_eye_layered_clip_t;

typedef struct {
    gfx_obj_t *white_obj;
    gfx_obj_t *top_lid_obj;
    gfx_obj_t *bottom_lid_obj;
} test_eye_layered_eye_t;

typedef struct {
    test_eye_layered_eye_t left_eye;
    test_eye_layered_eye_t right_eye;
    gfx_obj_t *mouth_obj;
    gfx_timer_handle_t anim_timer;
    test_eye_layered_pose_t current_pose;
    test_eye_layered_ctrl_t current_ctrl;
    test_eye_layered_ctrl_t transition_from_ctrl;
    size_t clip_idx;
    size_t state_idx;
    size_t next_state_idx;
    uint16_t hold_tick;
    uint16_t transition_tick;
    bool is_transitioning;
    /* Organic micro-motion overlay */
    uint16_t rng_state;
    uint32_t global_tick;
    int16_t saccade_x;
    int16_t saccade_y;
    int16_t saccade_target_x;
    int16_t saccade_target_y;
    uint16_t saccade_countdown;
    uint16_t saccade_settle_ticks;
} test_eye_layered_scene_t;

static const test_eye_layered_ctrl_t s_ctrl_idle = {0};

/* ctrl fields: {look_x, look_y, blink, squint, widen, softness, eye_x_ofs, eye_y_ofs, mouth_smile, mouth_open} */

static const test_eye_layered_key_t s_clip_idle_blink[] = {
    {{0, 0,   0,  0,  0,  0,  0,  0,  30,  0},  1U, 22U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0, 0,   0,  0, 10,  6,  0, -1,  35,  0}, 10U, 12U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 0, 100,  0,  0, 10,  0,  0,  25,  0},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0, 0,  12,  0,  8,  8,  0, -1,  32,  0},  7U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 0,   0,  0,  0,  0,  0,  0,  30,  0}, 10U, 14U, TEST_EYE_EASING_SMOOTHSTEP},
};

static const test_eye_layered_key_t s_clip_look_left_blink[] = {
    {{0,   0,    0,  0,  0,  0,  0,  0,  25,  0},  1U, 16U, TEST_EYE_EASING_SMOOTHSTEP},
    {{-35, -6,   0,  6,  0, 10, -2, -1,  15,  0}, 10U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-78, -14,  6, 10,  0, 14, -4, -2,   5,  0}, 14U, 14U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-70, -12, 100, 8,  0, 12, -3, -1,  10,  0},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{-54, -8,  10,  6,  4, 14, -2, -1,  18,  0},  7U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,   0,    0,  0,  0,  0,  0,  0,  25,  0}, 12U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

static const test_eye_layered_key_t s_clip_look_right_soft[] = {
    {{0,  0,  0,  0, 0,  0, 0, 0,  30,  0},  1U, 16U, TEST_EYE_EASING_SMOOTHSTEP},
    {{28, 4,  0, 10, 0, 12, 2, 1,  50,  0}, 10U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{70, 10, 0, 16, 4, 18, 4, 2,  65,  0}, 14U, 16U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{62, 8, 80, 12, 0, 16, 3, 1,  55,  0},  5U,  3U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{44, 6, 10, 12, 8, 18, 2, 1,  60,  0},  8U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,  0,  0,  0, 0,  0, 0, 0,  30,  0}, 12U, 14U, TEST_EYE_EASING_SMOOTHSTEP},
};

static const test_eye_layered_key_t s_clip_surprised_settle[] = {
    {{0,  0,  0,  0,  0,  0, 0,  0,  20,   0},  1U, 18U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0, -8,  0,  0, 50,  0, 0, -4, -20,  80},  8U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, -6,  0,  0, 68,  0, 0, -3, -30, 100},  8U, 12U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, -4, 18,  0, 30,  4, 0, -2,   0,  50},  6U,  8U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0,  0,  0,  0,  0,  0, 0,  0,  20,   0}, 12U, 16U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Sleepy: heavy lids, slow double-blink, droopy gaze, frown */
static const test_eye_layered_key_t s_clip_sleepy_blink[] = {
    {{0,  6,   0, 20,  0, 16,  0,  1, -25,  0},  1U, 20U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0,  8,  30, 30,  0, 20,  0,  2, -35,  0}, 14U, 18U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 10, 100, 24,  0, 22,  0,  2, -20,  0},  6U,  4U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0,  8,  40, 28,  0, 20,  0,  2, -30,  0},  8U, 22U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 10, 100, 26,  0, 24,  0,  2, -20,  0},  5U,  3U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0,  6,  20, 22,  0, 18,  0,  1, -22,  0}, 10U, 16U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,  0,   0,  0,  0,  0,  0,  0,  20,  0}, 12U, 10U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Angry: tight squint, slight downward gaze, deep frown */
static const test_eye_layered_key_t s_clip_angry_focus[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  20,  0},  1U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0,   -4,   0, 40,  0,  0,  0, -1, -40,  0},  8U,  6U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-20, -8,   0, 60,  0,  0, -2, -2, -70,  0}, 10U, 18U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-16, -6, 100, 50,  0,  0, -1, -1, -60,  0},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{-20, -8,   8, 58,  0,  0, -2, -2, -65,  0},  6U, 14U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  20,  0}, 14U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Happy: soft squint with upward curve, full smile */
static const test_eye_layered_key_t s_clip_happy_squint[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,  0},  1U, 14U, TEST_EYE_EASING_SMOOTHSTEP},
    {{8,  -10,   0, 30,  0, 20,  1, -2,  70,  0}, 10U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{12, -14,   0, 48,  0, 28,  1, -3, 100,  0}, 12U, 20U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{10, -12, 100, 40,  0, 24,  1, -2,  85,  0},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{14, -14,   8, 44,  0, 26,  1, -3,  95,  0},  7U, 12U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,  0}, 12U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Double-blink: two fast blinks with a short gap */
static const test_eye_layered_key_t s_clip_double_blink[] = {
    {{0, 0,   0,  0,  0,  0,  0,  0,  30,  0},  1U, 18U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0, 0, 100,  0,  0,  8,  0,  0,  20,  0},  3U,  1U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0, 0,   0,  0,  6,  4,  0, -1,  30,  0},  4U,  4U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 0, 100,  0,  0, 10,  0,  0,  20,  0},  3U,  1U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0, 0,  10,  0,  8,  6,  0, -1,  35,  0},  5U,  8U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0, 0,   0,  0,  0,  0,  0,  0,  30,  0}, 10U, 14U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Curious: wide-eyed look upward, O mouth */
static const test_eye_layered_key_t s_clip_curious_up[] = {
    {{0,   0,    0,  0,  0,  0,  0,  0,  25,   0},  1U, 14U, TEST_EYE_EASING_SMOOTHSTEP},
    {{20, -30,   0,  0, 30,  4,  2, -3,   5,  40}, 10U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{30, -50,   0,  0, 56,  0,  3, -5, -10,  65}, 12U, 16U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{26, -44, 100,  0, 40,  0,  2, -4,   0,  50},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{28, -48,  10,  0, 50,  2,  3, -4,  -5,  55},  6U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,   0,    0,  0,  0,  0,  0,  0,  25,   0}, 14U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Scan horizon: smooth left-to-right sweep */
static const test_eye_layered_key_t s_clip_scan_horizon[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  25,  0},  1U, 10U, TEST_EYE_EASING_SMOOTHSTEP},
    {{-60, -8,   0,  4,  0, 10, -3, -1,  15,  0}, 14U,  8U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{-80,-12,   0,  6,  0, 14, -4, -2,  10,  0},  8U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  4,  6,  0,  0,  25,  0}, 16U,  4U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{60,   8,   0,  4,  0, 10,  3,  1,  15,  0}, 14U,  8U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{80,  12,   0,  6,  0, 14,  4,  2,  10,  0},  8U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  25,  0}, 16U, 10U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Excited: wide eyes with quick bounce, big smile + open */
static const test_eye_layered_key_t s_clip_excited_bounce[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,   0},  1U,  8U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0,  -12,   0,  0, 60,  0,  0, -5,  80,  50},  6U,  4U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-30, -8,   0,  0, 50,  0, -2, -3,  90,  40},  5U,  3U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{30, -10,   0,  0, 55,  0,  2, -4,  95,  45},  5U,  3U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,  -14,   0,  0, 64,  0,  0, -5, 100,  55},  5U,  6U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,  -10, 100,  0, 44,  4,  0, -3,  75,  25},  3U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{0,   -8,  10,  0, 48,  2,  0, -3,  85,  35},  5U,  8U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,   0}, 12U, 10U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Sad: droopy downward gaze, deep frown */
static const test_eye_layered_key_t s_clip_sad_down[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  20,  0},  1U, 16U, TEST_EYE_EASING_SMOOTHSTEP},
    {{0,   20,   0, 10,  0, 18,  0,  2, -40,  0}, 12U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-10, 36,   0, 16,  0, 24, -1,  3, -80,  0}, 14U, 20U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{-8,  30, 100, 14,  0, 22, -1,  2, -70,  0},  5U,  3U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{-10, 34,  14, 16,  0, 24, -1,  3, -75,  0},  8U, 14U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  20,  0}, 14U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Playful wink: quick blink with a sideways glance, big playful smile */
static const test_eye_layered_key_t s_clip_playful_wink[] = {
    {{0,   0,   0,  0,  0,  0,  0,  0,  30,  0},  1U, 16U, TEST_EYE_EASING_SMOOTHSTEP},
    {{16, -6,   0,  0, 12,  8,  1, -1,  60,  0},  8U,  6U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{20, -8, 100,  0, 10, 10,  1, -1,  75, 20},  3U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{18, -6,   0,  0, 14,  8,  1, -1,  80,  0},  4U,  6U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{24,-10,   0, 14,  8, 14,  2, -2,  85,  0},  8U, 12U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,   0,   0,  0,  0,  0,  0,  0,  30,  0}, 12U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
};

/* Gentle drift: slow meandering gaze, mouth wanders between smile/neutral */
static const test_eye_layered_key_t s_clip_gentle_drift[] = {
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,  0},  1U, 12U, TEST_EYE_EASING_SMOOTHSTEP},
    {{-20, 10,   0,  6,  0, 12, -1,  1,  15,  0}, 16U, 14U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{15,  -8,   0,  4,  4, 10,  1, -1,  45,  0}, 18U, 14U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{-10,-14,   0,  2,  8,  8, -1, -1,  20,  0}, 16U, 12U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{10,   6, 100,  0,  0, 10,  1,  0,  25,  0},  4U,  2U, TEST_EYE_EASING_EASE_IN_OUT_CUBIC},
    {{8,    4,  12,  4,  6, 10,  1,  0,  40,  0},  6U, 10U, TEST_EYE_EASING_EASE_OUT_CUBIC},
    {{0,    0,   0,  0,  0,  0,  0,  0,  30,  0}, 14U, 10U, TEST_EYE_EASING_SMOOTHSTEP},
};

static const test_eye_layered_clip_t s_eye_clips[] = {
    {"idle_blink",       s_clip_idle_blink,       TEST_APP_ARRAY_SIZE(s_clip_idle_blink)},
    {"gentle_drift",     s_clip_gentle_drift,     TEST_APP_ARRAY_SIZE(s_clip_gentle_drift)},
    {"look_left_blink",  s_clip_look_left_blink,  TEST_APP_ARRAY_SIZE(s_clip_look_left_blink)},
    {"happy_squint",     s_clip_happy_squint,     TEST_APP_ARRAY_SIZE(s_clip_happy_squint)},
    {"double_blink",     s_clip_double_blink,     TEST_APP_ARRAY_SIZE(s_clip_double_blink)},
    {"look_right_soft",  s_clip_look_right_soft,  TEST_APP_ARRAY_SIZE(s_clip_look_right_soft)},
    {"curious_up",       s_clip_curious_up,       TEST_APP_ARRAY_SIZE(s_clip_curious_up)},
    {"surprised_settle", s_clip_surprised_settle,  TEST_APP_ARRAY_SIZE(s_clip_surprised_settle)},
    {"scan_horizon",     s_clip_scan_horizon,     TEST_APP_ARRAY_SIZE(s_clip_scan_horizon)},
    {"sleepy_blink",     s_clip_sleepy_blink,     TEST_APP_ARRAY_SIZE(s_clip_sleepy_blink)},
    {"playful_wink",     s_clip_playful_wink,     TEST_APP_ARRAY_SIZE(s_clip_playful_wink)},
    {"angry_focus",      s_clip_angry_focus,      TEST_APP_ARRAY_SIZE(s_clip_angry_focus)},
    {"excited_bounce",   s_clip_excited_bounce,   TEST_APP_ARRAY_SIZE(s_clip_excited_bounce)},
    {"sad_down",         s_clip_sad_down,         TEST_APP_ARRAY_SIZE(s_clip_sad_down)},
};

static const uint16_t s_black_pixel = 0x0000;
static const gfx_image_dsc_t s_black_img = {
    .header = {
        .magic = 0x19,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = 2,
    .data = (const uint8_t *)&s_black_pixel,
};

static const uint16_t s_white_pixel = 0xFFFF;
// static const uint16_t s_white_pixel = 0x8DD3;

static const gfx_image_dsc_t s_white_img = {
    .header = {
        .magic = 0x19,
        .cf = GFX_COLOR_FORMAT_RGB565,
        .w = 1,
        .h = 1,
        .stride = 2,
    },
    .data_size = 2,
    .data = (const uint8_t *)&s_white_pixel,
};

static int32_t test_eye_layered_lerp_i32(int32_t from, int32_t to, uint16_t t_permille)
{
    return from + (((to - from) * (int32_t)t_permille) / 1000);
}

static int32_t test_eye_layered_clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint16_t test_eye_layered_apply_easing(test_eye_layered_easing_t easing, uint16_t t_permille)
{
    int64_t t = t_permille;
    int64_t eased;

    switch (easing) {
    case TEST_EYE_EASING_LINEAR:
        return t_permille;
    case TEST_EYE_EASING_EASE_OUT_CUBIC:
        eased = 1000 - (((1000 - t) * (1000 - t) * (1000 - t) + 500000) / 1000000);
        break;
    case TEST_EYE_EASING_EASE_IN_OUT_CUBIC:
        if (t < 500) {
            eased = (4 * t * t * t + 500000) / 1000000;
        } else {
            int64_t inv = 1000 - t;
            eased = 1000 - ((4 * inv * inv * inv + 500000) / 1000000);
        }
        break;
    case TEST_EYE_EASING_SMOOTHSTEP:
    default:
        eased = (t * t * (3000 - 2 * t) + 500000) / 1000000;
        break;
    }

    if (eased < 0) {
        eased = 0;
    } else if (eased > 1000) {
        eased = 1000;
    }
    return (uint16_t)eased;
}

static test_eye_layered_ctrl_t test_eye_layered_lerp_ctrl(const test_eye_layered_ctrl_t *from,
                                                          const test_eye_layered_ctrl_t *to,
                                                          uint16_t t_permille)
{
    test_eye_layered_ctrl_t ctrl = {0};

    ctrl.look_x = (int16_t)test_eye_layered_lerp_i32(from->look_x, to->look_x, t_permille);
    ctrl.look_y = (int16_t)test_eye_layered_lerp_i32(from->look_y, to->look_y, t_permille);
    ctrl.blink = (int16_t)test_eye_layered_lerp_i32(from->blink, to->blink, t_permille);
    ctrl.squint = (int16_t)test_eye_layered_lerp_i32(from->squint, to->squint, t_permille);
    ctrl.widen = (int16_t)test_eye_layered_lerp_i32(from->widen, to->widen, t_permille);
    ctrl.softness = (int16_t)test_eye_layered_lerp_i32(from->softness, to->softness, t_permille);
    ctrl.eye_x_ofs = (int16_t)test_eye_layered_lerp_i32(from->eye_x_ofs, to->eye_x_ofs, t_permille);
    ctrl.eye_y_ofs = (int16_t)test_eye_layered_lerp_i32(from->eye_y_ofs, to->eye_y_ofs, t_permille);
    ctrl.mouth_smile = (int16_t)test_eye_layered_lerp_i32(from->mouth_smile, to->mouth_smile, t_permille);
    ctrl.mouth_open = (int16_t)test_eye_layered_lerp_i32(from->mouth_open, to->mouth_open, t_permille);
    return ctrl;
}

/* Convert semantic controls into the simple render pose used by this demo. */
static test_eye_layered_pose_t test_eye_layered_ctrl_to_pose(const test_eye_layered_ctrl_t *ctrl)
{
    test_eye_layered_pose_t pose = {0};
    int32_t look_x;
    int32_t look_y;
    int32_t blink;
    int32_t squint;
    int32_t widen;
    int32_t softness;
    int32_t upper_lid;
    int32_t lower_lid;

    TEST_ASSERT_NOT_NULL(ctrl);

    look_x = test_eye_layered_clamp_i32(ctrl->look_x, -100, 100);
    look_y = test_eye_layered_clamp_i32(ctrl->look_y, -100, 100);
    blink = test_eye_layered_clamp_i32(ctrl->blink, 0, 100);
    squint = test_eye_layered_clamp_i32(ctrl->squint, 0, 100);
    widen = test_eye_layered_clamp_i32(ctrl->widen, 0, 100);
    softness = test_eye_layered_clamp_i32(ctrl->softness, 0, 100);

    pose.eye_x_ofs = (int16_t)test_eye_layered_clamp_i32((ctrl->eye_x_ofs * 3 / 2) + ((look_x * TEST_EYE_LOOK_X_PIXELS) / 100), -30, 30);
    pose.eye_y_ofs = (int16_t)test_eye_layered_clamp_i32((ctrl->eye_y_ofs * 3 / 2) + ((look_y * TEST_EYE_LOOK_Y_PIXELS) / 100), -24, 24);

    upper_lid = ((blink * 74) / 100) + ((squint * 20) / 100) + ((softness * 10) / 100) - ((widen * 20) / 100);
    lower_lid = ((blink * 42) / 100) + ((squint * 22) / 100) + ((softness * 12) / 100) - ((widen * 14) / 100);

    if (look_y < 0) {
        upper_lid += (-look_y) / 8;
    } else {
        lower_lid += look_y / 6;
    }

    pose.upper_lid = (int16_t)test_eye_layered_clamp_i32(upper_lid, 0, 84);
    pose.lower_lid = (int16_t)test_eye_layered_clamp_i32(lower_lid, 0, 52);
    pose.lid_tilt_permille = (int16_t)test_eye_layered_clamp_i32((look_x * 20) / 10, -240, 240);

    pose.mouth_curve = (int16_t)test_eye_layered_clamp_i32(
        (test_eye_layered_clamp_i32(ctrl->mouth_smile, -100, 100) * TEST_FACE_MOUTH_SMILE_PX) / 100,
        -TEST_FACE_MOUTH_SMILE_PX, TEST_FACE_MOUTH_SMILE_PX);
    pose.mouth_open_px = (int16_t)test_eye_layered_clamp_i32(
        (test_eye_layered_clamp_i32(ctrl->mouth_open, 0, 100) * TEST_FACE_MOUTH_OPEN_PX) / 100,
        0, TEST_FACE_MOUTH_OPEN_PX);
    return pose;
}

static uint16_t test_eye_layered_xorshift16(uint16_t *state)
{
    uint16_t x = *state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    *state = x;
    return x;
}

static void test_eye_layered_update_saccade(test_eye_layered_scene_t *scene)
{
    uint16_t rng;

    scene->global_tick++;

    if (scene->saccade_settle_ticks > 0U) {
        scene->saccade_settle_ticks--;
        if (scene->saccade_settle_ticks == 0U) {
            scene->saccade_x = scene->saccade_target_x;
            scene->saccade_y = scene->saccade_target_y;
        } else {
            scene->saccade_x = (int16_t)(((int32_t)scene->saccade_x + (int32_t)scene->saccade_target_x) / 2);
            scene->saccade_y = (int16_t)(((int32_t)scene->saccade_y + (int32_t)scene->saccade_target_y) / 2);
        }
    }

    if (scene->saccade_countdown > 0U) {
        scene->saccade_countdown--;
        return;
    }

    rng = test_eye_layered_xorshift16(&scene->rng_state);
    scene->saccade_target_x = (int16_t)((int32_t)(rng % 7U) - 3);
    rng = test_eye_layered_xorshift16(&scene->rng_state);
    scene->saccade_target_y = (int16_t)((int32_t)(rng % 5U) - 2);
    rng = test_eye_layered_xorshift16(&scene->rng_state);
    scene->saccade_countdown = 8U + (uint16_t)(rng % 18U);
    scene->saccade_settle_ticks = 2U;
}

static void test_eye_layered_get_organic_offset(const test_eye_layered_scene_t *scene,
                                                int32_t *out_dx, int32_t *out_dy)
{
    int32_t breath_phase;
    int32_t breath_y;
    int32_t drift_phase;
    int32_t drift_x;

    breath_phase = (int32_t)(scene->global_tick % 180U);
    if (breath_phase < 90) {
        breath_y = breath_phase - 45;
    } else {
        breath_y = 135 - breath_phase;
    }

    drift_phase = (int32_t)(scene->global_tick % 240U);
    if (drift_phase < 120) {
        drift_x = drift_phase - 60;
    } else {
        drift_x = 180 - drift_phase;
    }

    if (scene->current_ctrl.blink > 50) {
        *out_dx = drift_x / 40;
        *out_dy = breath_y / 30;
    } else {
        *out_dx = (int32_t)scene->saccade_x + (drift_x / 40);
        *out_dy = (int32_t)scene->saccade_y + (breath_y / 30);
    }
}

static void test_eye_layered_set_quad(gfx_obj_t *obj,
                                      int32_t width,
                                      int32_t height,
                                      int32_t left_top_x,
                                      int32_t right_top_x,
                                      int32_t left_bottom_x,
                                      int32_t right_bottom_x)
{
    int32_t min_x;

    TEST_ASSERT_NOT_NULL(obj);
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    min_x = left_top_x;
    if (right_top_x < min_x) {
        min_x = right_top_x;
    }
    if (left_bottom_x < min_x) {
        min_x = left_bottom_x;
    }
    if (right_bottom_x < min_x) {
        min_x = right_bottom_x;
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(obj, 0, (gfx_coord_t)(left_top_x - min_x), 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(obj, 1, (gfx_coord_t)(right_top_x - min_x), 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(obj, 2, (gfx_coord_t)(left_bottom_x - min_x), (gfx_coord_t)height));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(obj, 3, (gfx_coord_t)(right_bottom_x - min_x), (gfx_coord_t)height));
}

static void test_eye_layered_apply_single_eye(test_eye_layered_scene_t *scene,
                                               test_eye_layered_eye_t *eye,
                                               const test_eye_layered_pose_t *pose,
                                               int32_t socket_left,
                                               int32_t socket_top,
                                               int32_t organic_dx,
                                               int32_t organic_dy)
{
    int32_t eye_left;
    int32_t eye_top;
    int32_t lid_x;
    int32_t lid_y_base;
    int32_t top_cover;
    int32_t bottom_cover;
    int32_t lid_width;
    int32_t top_height;
    int32_t bottom_height;
    int32_t top_skew;
    int32_t bottom_skew;
    int32_t top_origin_x;
    int32_t bottom_origin_x;

    eye_left = socket_left + pose->eye_x_ofs + organic_dx;
    eye_top = socket_top + pose->eye_y_ofs + organic_dy;

    lid_x = socket_left - TEST_EYE_LID_SIDE_PAD + (pose->eye_x_ofs / TEST_EYE_SOCKET_FOLLOW_X_DIV);
    lid_y_base = socket_top + (pose->eye_y_ofs / TEST_EYE_SOCKET_FOLLOW_Y_DIV);
    lid_width = TEST_EYE_WHITE_SIZE + (TEST_EYE_LID_SIDE_PAD * 2);

    top_cover = pose->upper_lid + 4;
    bottom_cover = pose->lower_lid + 4;
    if (top_cover < 1) {
        top_cover = 1;
    }
    if (bottom_cover < 1) {
        bottom_cover = 1;
    }

    top_height = top_cover + TEST_EYE_LID_OVERDRAW;
    bottom_height = bottom_cover + TEST_EYE_LID_OVERDRAW;
    top_skew = test_eye_layered_clamp_i32((pose->lid_tilt_permille * 22) / 1000, -22, 22);
    bottom_skew = test_eye_layered_clamp_i32((pose->lid_tilt_permille * 16) / 1000, -16, 16);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_pos(eye->white_obj, (gfx_coord_t)eye_left, (gfx_coord_t)eye_top));

    top_origin_x = lid_x + ((top_skew < 0) ? top_skew : 0);
    test_eye_layered_set_quad(eye->top_lid_obj,
                              lid_width,
                              top_height,
                              0,
                              lid_width,
                              test_eye_layered_clamp_i32(10 + top_skew, -16, 34),
                              lid_width - test_eye_layered_clamp_i32(10 - top_skew, -16, 34));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_pos(eye->top_lid_obj,
                                              (gfx_coord_t)top_origin_x,
                                              (gfx_coord_t)(lid_y_base - TEST_EYE_LID_OVERDRAW)));

    bottom_origin_x = lid_x + ((bottom_skew > 0) ? -bottom_skew : 0);
    test_eye_layered_set_quad(eye->bottom_lid_obj,
                              lid_width,
                              bottom_height,
                              test_eye_layered_clamp_i32(8 - bottom_skew, -12, 26),
                              lid_width - test_eye_layered_clamp_i32(8 + bottom_skew, -12, 26),
                              0,
                              lid_width);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_pos(eye->bottom_lid_obj,
                                              (gfx_coord_t)bottom_origin_x,
                                              (gfx_coord_t)(lid_y_base + TEST_EYE_WHITE_SIZE - bottom_cover)));
}

static void test_eye_layered_apply_mouth(test_eye_layered_scene_t *scene,
                                         const test_eye_layered_pose_t *pose,
                                         int32_t center_x, int32_t top_y)
{
    int32_t cols = TEST_FACE_MOUTH_COLS;
    int32_t w = TEST_FACE_MOUTH_WIDTH;
    int32_t seg = w / cols;
    int32_t base_h = TEST_FACE_MOUTH_BASE_H;
    int32_t curve = pose->mouth_curve;
    int32_t open = pose->mouth_open_px;
    int32_t half = cols / 2;
    int32_t i;
    int32_t min_y;
    int32_t top_y_pt;
    int32_t bot_y_pt;
    int32_t cw;
    int32_t dist;

    min_y = 0;
    for (i = 0; i <= cols; i++) {
        dist = (i <= half) ? i : (cols - i);
        cw = (dist * 1000) / half;
        top_y_pt = (curve * cw) / 1000;
        if (top_y_pt < min_y) {
            min_y = top_y_pt;
        }
        bot_y_pt = base_h + (curve * cw) / 1000 + (open * cw) / 1000;
        if (bot_y_pt < min_y) {
            min_y = bot_y_pt;
        }
    }

    for (i = 0; i <= cols; i++) {
        dist = (i <= half) ? i : (cols - i);
        cw = (dist * 1000) / half;
        top_y_pt = (curve * cw) / 1000 - min_y;
        bot_y_pt = base_h + (curve * cw) / 1000 + (open * cw) / 1000 - min_y;
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(
            scene->mouth_obj, i,
            (gfx_coord_t)(i * seg), (gfx_coord_t)top_y_pt));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(
            scene->mouth_obj, (cols + 1) + i,
            (gfx_coord_t)(i * seg), (gfx_coord_t)bot_y_pt));
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_pos(scene->mouth_obj,
                                              (gfx_coord_t)(center_x - w / 2),
                                              (gfx_coord_t)(top_y + min_y)));
}

static void test_eye_layered_apply_pose(test_eye_layered_scene_t *scene, const test_eye_layered_pose_t *pose)
{
    int32_t disp_w;
    int32_t disp_h;
    int32_t face_cx;
    int32_t eye_top;
    int32_t left_socket;
    int32_t right_socket;
    int32_t mouth_y;
    int32_t organic_dx;
    int32_t organic_dy;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(pose);

    disp_w = (int32_t)gfx_disp_get_hor_res(disp_default);
    disp_h = (int32_t)gfx_disp_get_ver_res(disp_default);
    face_cx = disp_w / 2;
    eye_top = ((disp_h - TEST_EYE_WHITE_SIZE) / 2) + TEST_EYE_WHITE_Y_OFS + TEST_FACE_Y_OFS;
    left_socket = face_cx - TEST_EYE_WHITE_SIZE - (TEST_FACE_EYE_GAP / 2);
    right_socket = face_cx + (TEST_FACE_EYE_GAP / 2);

    test_eye_layered_get_organic_offset(scene, &organic_dx, &organic_dy);

    test_eye_layered_apply_single_eye(scene, &scene->left_eye, pose,
                                      left_socket, eye_top, organic_dx, organic_dy);
    test_eye_layered_apply_single_eye(scene, &scene->right_eye, pose,
                                      right_socket, eye_top, organic_dx, organic_dy);

    mouth_y = eye_top + TEST_EYE_WHITE_SIZE + TEST_FACE_MOUTH_BELOW_EYE;
    {
        test_eye_layered_pose_t talk = *pose;
        int32_t t = (int32_t)scene->global_tick;
        int32_t w1 = (t % 7 < 4)  ? (t % 7)  : (7  - t % 7);
        int32_t w2 = (t % 11 < 6) ? (t % 11) : (11 - t % 11);
        int32_t w3 = (t % 23 < 12) ? (t % 23) : (23 - t % 23);
        talk.mouth_open_px = (int16_t)(5 + w1 * 4 + w2 * 2 + w3 / 2);
        talk.mouth_curve = (int16_t)(3 + w3 / 4);
        test_eye_layered_apply_mouth(scene, &talk, face_cx, mouth_y);
    }

    scene->current_pose = *pose;
}

static void test_eye_layered_apply_ctrl(test_eye_layered_scene_t *scene, const test_eye_layered_ctrl_t *ctrl)
{
    test_eye_layered_pose_t pose;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(ctrl);

    pose = test_eye_layered_ctrl_to_pose(ctrl);
    test_eye_layered_apply_pose(scene, &pose);
    scene->current_ctrl = *ctrl;
}

static void test_eye_layered_load_clip(test_eye_layered_scene_t *scene, size_t clip_idx)
{
    const test_eye_layered_clip_t *clip;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_TRUE(TEST_APP_ARRAY_SIZE(s_eye_clips) > 0U);

    clip = &s_eye_clips[clip_idx % TEST_APP_ARRAY_SIZE(s_eye_clips)];
    TEST_ASSERT_TRUE(clip->key_count > 0U);

    scene->clip_idx = clip_idx % TEST_APP_ARRAY_SIZE(s_eye_clips);
    scene->state_idx = 0U;
    scene->next_state_idx = 0U;
    scene->hold_tick = 0U;
    scene->transition_tick = 0U;
    scene->is_transitioning = false;
    scene->transition_from_ctrl = s_ctrl_idle;
    test_eye_layered_apply_ctrl(scene, &clip->keys[0].ctrl);
    ESP_LOGI(TAG, "clip -> %s", clip->name);
}

static void test_eye_layered_start_transition(test_eye_layered_scene_t *scene, size_t next_key_idx)
{
    TEST_ASSERT_NOT_NULL(scene);

    scene->next_state_idx = next_key_idx;
    scene->transition_from_ctrl = scene->current_ctrl;
    scene->transition_tick = 0U;
    scene->is_transitioning = true;
}

static void test_eye_layered_anim_cb(void *user_data)
{
    test_eye_layered_scene_t *scene = (test_eye_layered_scene_t *)user_data;
    const test_eye_layered_clip_t *clip;
    const test_eye_layered_key_t *current_key;
    const test_eye_layered_key_t *target_key;
    test_eye_layered_ctrl_t ctrl;
    uint16_t duration;
    uint16_t t_permille;
    uint16_t eased_permille;

    if (scene == NULL) {
        return;
    }

    test_eye_layered_update_saccade(scene);
    clip = &s_eye_clips[scene->clip_idx];

    if (!scene->is_transitioning) {
        current_key = &clip->keys[scene->state_idx];
        scene->hold_tick++;
        if (scene->hold_tick < current_key->hold_ticks) {
            test_eye_layered_apply_pose(scene, &scene->current_pose);
            return;
        }

        scene->hold_tick = 0U;
        if ((scene->state_idx + 1U) < clip->key_count) {
            test_eye_layered_start_transition(scene, scene->state_idx + 1U);
        } else {
            test_eye_layered_load_clip(scene, (scene->clip_idx + 1U) % TEST_APP_ARRAY_SIZE(s_eye_clips));
            return;
        }
    }

    target_key = &clip->keys[scene->next_state_idx];
    duration = target_key->transition_ticks;
    if (duration == 0U) {
        duration = 1U;
    }

    scene->transition_tick++;
    if (scene->transition_tick > duration) {
        scene->transition_tick = duration;
    }

    t_permille = (uint16_t)(((uint32_t)scene->transition_tick * 1000U) / duration);
    eased_permille = test_eye_layered_apply_easing(target_key->easing, t_permille);
    ctrl = test_eye_layered_lerp_ctrl(&scene->transition_from_ctrl, &target_key->ctrl, eased_permille);
    test_eye_layered_apply_ctrl(scene, &ctrl);

    if (scene->transition_tick >= duration) {
        scene->state_idx = scene->next_state_idx;
        scene->transition_tick = 0U;
        scene->hold_tick = 0U;
        scene->is_transitioning = false;
    }
}

static void test_eye_layered_cleanup_eye(test_eye_layered_eye_t *eye)
{
    if (eye->bottom_lid_obj != NULL) {
        gfx_obj_delete(eye->bottom_lid_obj);
        eye->bottom_lid_obj = NULL;
    }
    if (eye->top_lid_obj != NULL) {
        gfx_obj_delete(eye->top_lid_obj);
        eye->top_lid_obj = NULL;
    }
    if (eye->white_obj != NULL) {
        gfx_obj_delete(eye->white_obj);
        eye->white_obj = NULL;
    }
}

static void test_eye_layered_scene_cleanup(test_eye_layered_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }
    if (scene->mouth_obj != NULL) {
        gfx_obj_delete(scene->mouth_obj);
        scene->mouth_obj = NULL;
    }
    test_eye_layered_cleanup_eye(&scene->right_eye);
    test_eye_layered_cleanup_eye(&scene->left_eye);
}

static void test_eye_layered_init_eye(test_eye_layered_eye_t *eye)
{
    eye->white_obj = gfx_img_create(disp_default);
    eye->top_lid_obj = gfx_mesh_img_create(disp_default);
    eye->bottom_lid_obj = gfx_mesh_img_create(disp_default);

    TEST_ASSERT_NOT_NULL(eye->white_obj);
    TEST_ASSERT_NOT_NULL(eye->top_lid_obj);
    TEST_ASSERT_NOT_NULL(eye->bottom_lid_obj);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_img_set_src(eye->white_obj, (void *)&orb_ball_center));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(eye->top_lid_obj, (void *)&s_black_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(eye->top_lid_obj, 1, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->top_lid_obj, false));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(eye->bottom_lid_obj, (void *)&s_black_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(eye->bottom_lid_obj, 1, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(eye->bottom_lid_obj, false));
}

static void test_eye_layered_run(void)
{
    test_eye_layered_scene_t scene = {0};

    test_app_log_case(TAG, "Face animation demo: two eyes + mouth");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    test_eye_layered_init_eye(&scene.left_eye);
    test_eye_layered_init_eye(&scene.right_eye);

    scene.mouth_obj = gfx_mesh_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.mouth_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mouth_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mouth_obj, TEST_FACE_MOUTH_COLS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_ctrl_points_visible(scene.mouth_obj, false));

    scene.rng_state = 0xACE1U;
    scene.current_ctrl = s_ctrl_idle;
    test_eye_layered_load_clip(&scene, 0U);

    scene.anim_timer = gfx_timer_create(emote_handle, test_eye_layered_anim_cb, TEST_EYE_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Observe face animation: two eyes + deforming mouth");
    test_app_wait_for_observe(1000 * 1000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_eye_layered_scene_cleanup(&scene);
    test_app_unlock();
}

void test_eye_layered_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_eye_layered_run();
    test_app_runtime_close(&runtime);
}
