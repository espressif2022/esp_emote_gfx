/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_lobster_emote.h"

#define LTR(X, Y, R, S) { .x = (X), .y = (Y), .r = (R), .s = (S) }
#define LHOME           { .x = 0, .y = 0, .r = 0, .s = 100 }

static const gfx_mesh_img_point_t s_lobster_body_pts[] = {
    {   0, -170 }, {  86, -148 }, { 150, -92 }, { 182,   0 }, { 150,  92 }, {  86, 148 }, {   0, 170 },
    { -86,  148 }, { -150,  92 }, { -182,   0 }, { -150, -92 }, { -86, -148 }, {   0, -170 }, {   0, -170 }
};

static const gfx_mesh_img_point_t s_lobster_tail_pts[] = {
    {   0, -108 }, {  68,  -84 }, { 122,  -20 }, { 110,   46 }, {  60, 116 }, {   0, 162 }, { -60, 116 },
    { -110,  46 }, { -122, -20 }, { -68, -84 }, {   0, -108 }, {   0, -108 }
};

static const gfx_mesh_img_point_t s_lobster_claw_l_pts[] = {
    {  34,   8 }, {  12, -20 }, { -24, -64 }, { -86, -108 }, { -140, -88 }, { -126, -28 }, { -154,  24 },
    { -120,  82 }, { -56, 108 }, {  -2,  82 }, {  28,  36 }, {  34,   8 }
};

static const gfx_mesh_img_point_t s_lobster_claw_r_pts[] = {
    { -34,   8 }, { -12, -20 }, {  24, -64 }, {  86, -108 }, { 140, -88 }, { 126, -28 }, { 154,  24 },
    { 120,  82 }, {  56, 108 }, {   2,  82 }, { -28,  36 }, { -34,   8 }
};

static const gfx_mesh_img_point_t s_lobster_eye_white_pts[] = {
    {   0, -42 }, {  18, -36 }, {  32, -20 }, {  38,   0 }, {  32,  20 }, {  18,  36 }, {   0,  42 },
    { -18,  36 }, { -32,  20 }, { -38,   0 }, { -32, -20 }, { -18, -36 }, {   0, -42 }, {   0, -42 }
};

static const gfx_mesh_img_point_t s_lobster_eye_pupil_pts[] = {
    {   0, -16 }, {   8, -14 }, {  14,  -8 }, {  16,   0 }, {  14,   8 }, {   8,  14 }, {   0,  16 },
    {  -8,  14 }, { -14,   8 }, { -16,   0 }, { -14,  -8 }, {  -8, -14 }, {   0, -16 }, {   0, -16 }
};

static const gfx_lobster_emote_state_t s_lobster_sequence[] = {
    { "idle",       "待机漂浮", LTR(0, 0, -2, 100), LTR(0, 0, 10, 100), LTR(0, 0, -10, 100), LTR(0, 0, 10, 100), 100,   0,  0, 60 },
    { "look_left",  "左侧打量", LTR(0, -4, -4, 101), LTR(0, 6, 18, 100), LTR(-10, -4, -18, 100), LTR(-6, 4, 8, 100), 108, -16, -4, 40 },
    { "look_right", "右侧打量", LTR(0, -4, 4, 101), LTR(0, 6, -18, 100), LTR(6, 4, -8, 100), LTR(10, -4, 18, 100), 108,  16, -4, 40 },
    { "wave_left",  "左钳招呼", LTR(0, -8, 4, 102), LTR(0, 4, 12, 100), LTR(-8, -24, -34, 112), LTR(8, 0, 8, 100), 110,  -4, -2, 36 },
    { "wave_right", "右钳招呼", LTR(0, -8, -4, 102), LTR(0, 4, -12, 100), LTR(-8, 0, -8, 100), LTR(8, -24, 34, 112), 110,   4, -2, 36 },
    { "excited",    "兴奋亮相", LTR(0, -14, 0, 104), LTR(0, 8, 24, 108), LTR(-16, -18, -28, 114), LTR(16, -18, 28, 114), 124, 0, -6, 32 },
    { "shy",        "害羞缩头", LTR(0, 12, 8, 98), LTR(0, -4, 4, 96), LTR(4, 8, -4, 96), LTR(-4, 8, 4, 96), 62, -10, 10, 42 },
};

static const gfx_lobster_emote_assets_t s_lobster_assets = {
    .pts_body = s_lobster_body_pts,
    .pts_tail = s_lobster_tail_pts,
    .pts_claw_l = s_lobster_claw_l_pts,
    .pts_claw_r = s_lobster_claw_r_pts,
    .pts_eye_white = s_lobster_eye_white_pts,
    .pts_eye_pupil = s_lobster_eye_pupil_pts,
    .count_body = sizeof(s_lobster_body_pts) / sizeof(s_lobster_body_pts[0]),
    .count_tail = sizeof(s_lobster_tail_pts) / sizeof(s_lobster_tail_pts[0]),
    .count_claw_l = sizeof(s_lobster_claw_l_pts) / sizeof(s_lobster_claw_l_pts[0]),
    .count_claw_r = sizeof(s_lobster_claw_r_pts) / sizeof(s_lobster_claw_r_pts[0]),
    .count_eye_white = sizeof(s_lobster_eye_white_pts) / sizeof(s_lobster_eye_white_pts[0]),
    .count_eye_pupil = sizeof(s_lobster_eye_pupil_pts) / sizeof(s_lobster_eye_pupil_pts[0]),
    .sequence = s_lobster_sequence,
    .sequence_count = sizeof(s_lobster_sequence) / sizeof(s_lobster_sequence[0]),
};
