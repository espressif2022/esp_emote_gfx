/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * face_emote_scene.inc — facial expression scene in the unified gfx_sm_scene format.
 *
 * Joints are BÉZIER CONTROL POINTS in design space (viewbox 200×200, centred at 0,0).
 * Segments encode the rendering primitive for each joint group:
 *
 *   Segment          Joints        Kind
 *   ───────────────  ────────────  ────────────────
 *   mouth            0-6 (7 pts)   BEZIER_LOOP
 *   eye_l            7-13 (7 pts)  BEZIER_LOOP
 *   eye_r           14-20 (7 pts)  BEZIER_LOOP
 *   brow_l          21-24 (4 pts)  BEZIER_STRIP
 *   brow_r          25-28 (4 pts)  BEZIER_STRIP
 *
 * Pose data is pre-computed by blending the reference shapes from
 * face_emote_expr_assets.inc with the expression weights — no runtime
 * blend logic needed.  The stickman fireman scene uses the SAME format.
 *
 * Include after gfx_sm_scene.h.
 */

#pragma once

#include "widget/gfx_sm_scene.h"

static const gfx_sm_meta_t s_fe_meta = {
    .version   = GFX_SM_SCENE_SCHEMA_VERSION,
    .viewbox_x = -100,
    .viewbox_y = -80,
    .viewbox_w =  200,
    .viewbox_h =  200,
};

static const gfx_sm_layout_t s_fe_layout = {
    .stroke_width    =  6,
    .mirror_x        =  0,
    .ground_y        =  80,
    .timer_period_ms =  33,
    .damping_div     =   4,  /* ease control-points for smooth transitions */
};

static const char * const s_fe_joint_names[] = {
    "mouth_0",
    "mouth_1",
    "mouth_2",
    "mouth_3",
    "mouth_4",
    "mouth_5",
    "mouth_6",
    "eye_l_0",
    "eye_l_1",
    "eye_l_2",
    "eye_l_3",
    "eye_l_4",
    "eye_l_5",
    "eye_l_6",
    "eye_r_0",
    "eye_r_1",
    "eye_r_2",
    "eye_r_3",
    "eye_r_4",
    "eye_r_5",
    "eye_r_6",
    "brow_l_0",
    "brow_l_1",
    "brow_l_2",
    "brow_l_3",
    "brow_r_0",
    "brow_r_1",
    "brow_r_2",
    "brow_r_3",
};

/* ── Pose data: 29 joints × (x,y) = 58 int16_t values per pose ── */
static const int16_t s_fe_p00[] = { -28, 25, -14, 24, 14, 24, 28, 25, 14, 26, -14, 26, -28, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 8, -43, 8, -51, -8, 51, -8, 43, 8, 27, 8, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* neutral */
static const int16_t s_fe_p01[] = { -27, 25, -14, 22, 14, 22, 27, 25, 14, 28, -14, 28, -27, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 8, -43, 8, -51, -8, 51, -8, 43, 8, 27, 8, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* look up */
static const int16_t s_fe_p02[] = { -28, 25, -14, 24, 14, 24, 28, 25, 14, 26, -14, 26, -28, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 8, -43, 8, -51, -8, 51, -8, 43, 8, 27, 8, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* look down */
static const int16_t s_fe_p03[] = { -28, 25, -14, 25, 14, 25, 28, 25, 15, 27, -15, 27, -28, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 7, -43, 7, -51, -8, 51, -8, 43, 7, 27, 7, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* look left */
static const int16_t s_fe_p04[] = { -28, 25, -14, 25, 14, 25, 28, 25, 15, 27, -15, 27, -28, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 7, -43, 7, -51, -8, 51, -8, 43, 7, 27, 7, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* look right */
static const int16_t s_fe_p05[] = { -28, 28, -14, 20, 14, 20, 28, 28, 14, 23, -14, 23, -28, 28, -51, -8, -43, -22, -27, -24, -19, -8, -27, 7, -43, 7, -51, -8, 51, -8, 43, 7, 27, 7, 19, -8, 27, -24, 43, -22, 51, -8, -51, -32, -40, -38, -30, -37, -19, -32, 19, -32, 30, -37, 40, -38, 51, -32 };  /* side-eye */
static const int16_t s_fe_p06[] = { -30, 24, -14, 29, 14, 29, 30, 24, 17, 33, -17, 33, -30, 24, -51, -7, -43, -22, -27, -22, -19, -7, -27, 3, -43, 3, -51, -7, 51, -7, 43, 3, 27, 3, 19, -7, 27, -22, 43, -22, 51, -7, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* daydream */
static const int16_t s_fe_p07[] = { -31, 23, -13, 33, 13, 33, 31, 23, 20, 41, -20, 41, -31, 23, -51, -6, -43, -21, -27, -21, -19, -6, -27, -2, -43, -2, -51, -6, 51, -6, 43, -2, 27, -2, 19, -6, 27, -21, 43, -21, 51, -6, -51, -31, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -31 };  /* soft smile */
static const int16_t s_fe_p08[] = { -34, 20, -12, 43, 12, 43, 34, 20, 26, 55, -26, 55, -34, 20, -51, -4, -43, -18, -27, -18, -19, -4, -27, -12, -43, -12, -51, -4, 51, -4, 43, -12, 27, -12, 19, -4, 27, -18, 43, -18, 51, -4, -51, -30, -40, -37, -30, -37, -19, -33, 19, -33, 30, -37, 40, -37, 51, -30 };  /* smile */
static const int16_t s_fe_p09[] = { -34, 10, -12, 13, 12, 13, 34, 10, 34, 65, -34, 65, -34, 10, -51, -4, -43, -16, -27, -16, -19, -4, -27, -12, -43, -12, -51, -4, 51, -4, 43, -12, 27, -12, 19, -4, 27, -16, 43, -16, 51, -4, -51, -33, -40, -40, -30, -40, -19, -33, 19, -33, 30, -40, 40, -40, 51, -33 };  /* happy */
static const int16_t s_fe_p10[] = { -37, 7, -11, 23, 11, 23, 37, 7, 40, 79, -40, 79, -37, 7, -51, -2, -43, -13, -27, -13, -19, -2, -27, -22, -43, -22, -51, -2, 51, -2, 43, -22, 27, -22, 19, -2, 27, -13, 43, -13, 51, -2, -51, -32, -40, -40, -30, -40, -19, -34, 19, -34, 30, -40, 40, -40, 51, -32 };  /* grinning */
static const int16_t s_fe_p11[] = { -36, 6, -11, 21, 11, 21, 36, 6, 44, 95, -44, 95, -36, 6, -51, -1, -43, -12, -27, -12, -19, -1, -27, -27, -43, -27, -51, -1, 51, -1, 43, -27, 27, -27, 19, -1, 27, -12, 43, -12, 51, -1, -51, -33, -40, -41, -30, -41, -19, -35, 19, -35, 30, -41, 40, -41, 51, -33 };  /* big grin */
static const int16_t s_fe_p12[] = { -34, 18, -12, 37, 12, 37, 34, 18, 28, 57, -28, 57, -34, 18, -51, -4, -43, -18, -27, -18, -19, -4, -27, -12, -43, -12, -51, -4, 51, -4, 43, -12, 27, -12, 19, -4, 27, -18, 43, -18, 51, -4, -51, -31, -40, -38, -30, -38, -19, -33, 19, -33, 30, -38, 40, -38, 51, -31 };  /* loving */
static const int16_t s_fe_p13[] = { -31, 7, -11, 7, 11, 7, 31, 7, 42, 99, -42, 99, -31, 7, -50, -2, -43, -15, -27, -15, -20, -2, -27, -21, -43, -21, -50, -2, 50, -2, 43, -21, 27, -21, 20, -2, 27, -15, 43, -15, 50, -2, -50, -36, -40, -44, -30, -44, -20, -38, 20, -38, 30, -44, 40, -44, 50, -36 };  /* excited */
static const int16_t s_fe_p14[] = { -30, 9, -12, 3, 12, 3, 30, 9, 37, 84, -37, 84, -30, 9, -50, -3, -43, -17, -27, -17, -20, -3, -27, -14, -43, -14, -50, -3, 50, -3, 43, -14, 27, -14, 20, -3, 27, -17, 43, -17, 50, -3, -50, -36, -40, -43, -30, -43, -20, -36, 20, -36, 30, -43, 40, -43, 50, -36 };  /* laughing */
static const int16_t s_fe_p15[] = { -28, 22, -13, 25, 13, 25, 28, 22, 22, 54, -22, 54, -28, 22, -50, -6, -43, -22, -27, -22, -20, -6, -27, -2, -43, -2, -50, -6, 50, -6, 43, -2, 27, -2, 20, -6, 27, -22, 43, -22, 50, -6, -50, -33, -40, -40, -30, -40, -20, -35, 20, -35, 30, -40, 40, -40, 50, -33 };  /* amused */
static const int16_t s_fe_p16[] = { -30, 23, -13, 32, 13, 32, 30, 23, 19, 38, -19, 38, -30, 23, -51, -6, -43, -22, -27, -22, -19, -6, -27, 0, -43, 0, -51, -6, 51, -6, 43, 0, 27, 0, 19, -6, 27, -22, 43, -22, 51, -6, -51, -31, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -31 };  /* relaxed */
static const int16_t s_fe_p17[] = { -35, 15, -12, 33, 12, 33, 35, 15, 30, 62, -30, 62, -35, 15, -51, -4, -43, -17, -27, -17, -19, -4, -27, -14, -43, -14, -51, -4, 51, -4, 43, -14, 27, -14, 19, -4, 27, -17, 43, -17, 51, -4, -51, -31, -40, -38, -30, -38, -19, -33, 19, -33, 30, -38, 40, -38, 51, -31 };  /* blissful */
static const int16_t s_fe_p18[] = { -33, 21, -12, 29, 12, 29, 33, 21, 24, 45, -24, 45, -33, 21, -51, -6, -43, -18, -27, -19, -19, -4, -27, -7, -43, -7, -51, -6, 51, -6, 43, -7, 27, -7, 19, -4, 27, -19, 43, -18, 51, -6, -51, -32, -40, -38, -30, -36, -19, -31, 19, -31, 30, -36, 40, -38, 51, -32 };  /* proud */
static const int16_t s_fe_p19[] = { -32, 25, -12, 33, 12, 33, 32, 25, 21, 39, -21, 39, -32, 25, -51, -6, -43, -18, -27, -20, -19, -4, -27, -5, -43, -5, -51, -6, 51, -6, 43, -5, 27, -5, 19, -4, 27, -20, 43, -18, 51, -6, -51, -32, -40, -37, -30, -35, -19, -31, 19, -31, 30, -35, 40, -37, 51, -32 };  /* smug */
static const int16_t s_fe_p20[] = { -30, 37, -15, 0, 15, 0, 30, 37, 15, 15, -15, 15, -30, 37, -51, 0, -43, -20, -27, -24, -19, -12, -27, 6, -43, 8, -51, 0, 51, 0, 43, 8, 27, 6, 19, -12, 27, -24, 43, -20, 51, 0, -51, -28, -40, -37, -30, -41, -19, -40, 19, -40, 30, -41, 40, -37, 51, -28 };  /* sad */
static const int16_t s_fe_p21[] = { -30, 38, -15, -2, 15, -2, 30, 38, 15, 14, -15, 14, -30, 38, -51, 1, -43, -20, -27, -24, -19, -12, -27, 6, -43, 8, -51, 1, 51, 1, 43, 8, 27, 6, 19, -12, 27, -24, 43, -20, 51, 1, -51, -28, -40, -37, -30, -41, -19, -41, 19, -41, 30, -41, 40, -37, 51, -28 };  /* sorrowful */
static const int16_t s_fe_p22[] = { -30, 37, -15, 0, 15, 0, 30, 37, 15, 15, -15, 15, -30, 37, -51, 0, -43, -20, -27, -24, -19, -12, -27, 6, -43, 8, -51, 0, 51, 0, 43, 8, 27, 6, 19, -12, 27, -24, 43, -20, 51, 0, -51, -28, -40, -37, -30, -41, -19, -40, 19, -40, 30, -41, 40, -37, 51, -28 };  /* crying */
static const int16_t s_fe_p23[] = { -30, 39, -15, -5, 15, -5, 30, 39, 15, 13, -15, 13, -30, 39, -51, 2, -43, -19, -27, -24, -19, -13, -27, 6, -43, 8, -51, 2, 51, 2, 43, 8, 27, 6, 19, -13, 27, -24, 43, -19, 51, 2, -51, -27, -40, -37, -30, -42, -19, -42, 19, -42, 30, -42, 40, -37, 51, -27 };  /* sobbing */
static const int16_t s_fe_p24[] = { -30, 35, -15, 5, 15, 5, 30, 35, 15, 17, -15, 17, -30, 35, -51, -2, -43, -21, -27, -24, -19, -11, -27, 6, -43, 8, -51, -2, 51, -2, 43, 8, 27, 6, 19, -11, 27, -24, 43, -21, 51, -2, -51, -29, -40, -37, -30, -40, -19, -38, 19, -38, 30, -40, 40, -37, 51, -29 };  /* dejected */
static const int16_t s_fe_p25[] = { -31, 29, -14, 18, 14, 18, 31, 29, 18, 29, -18, 29, -31, 29, -51, -3, -43, -20, -27, -22, -19, -9, -27, 1, -43, 2, -51, -3, 51, -3, 43, 2, 27, 1, 19, -9, 27, -22, 43, -20, 51, -3, -51, -29, -40, -37, -30, -39, -19, -36, 19, -36, 30, -39, 40, -37, 51, -29 };  /* sympathetic */
static const int16_t s_fe_p26[] = { -30, 29, -14, 18, 14, 18, 30, 29, 17, 27, -17, 27, -30, 29, -51, -4, -43, -21, -27, -23, -19, -9, -27, 3, -43, 4, -51, -4, 51, -4, 43, 4, 27, 3, 19, -9, 27, -23, 43, -21, 51, -4, -51, -30, -40, -37, -30, -39, -19, -35, 19, -35, 30, -39, 40, -37, 51, -30 };  /* longing */
static const int16_t s_fe_p27[] = { -26, 36, -15, -8, 15, -8, 26, 36, 15, 26, -15, 26, -26, 36, -50, -1, -43, -22, -27, -26, -20, -12, -27, 8, -43, 10, -50, -1, 50, -1, 43, 10, 27, 8, 20, -12, 27, -26, 43, -22, 50, -1, -50, -31, -40, -40, -30, -43, -20, -42, 20, -42, 30, -43, 40, -40, 50, -31 };  /* tearful plea */
static const int16_t s_fe_p28[] = { -15, 25, -15, -10, 15, -10, 15, 25, 15, 60, -15, 60, -15, 25, -49, -8, -42, -30, -28, -30, -21, -8, -28, 14, -42, 14, -49, -8, 49, -8, 42, 14, 28, 14, 21, -8, 28, -30, 42, -30, 49, -8, -49, -40, -40, -47, -30, -47, -21, -40, 21, -40, 30, -47, 40, -47, 49, -40 };  /* wow */
static const int16_t s_fe_p29[] = { -12, 25, -15, -17, 15, -17, 12, 25, 15, 67, -15, 67, -12, 25, -49, -8, -42, -31, -28, -31, -21, -8, -28, 15, -42, 15, -49, -8, 49, -8, 42, 15, 28, 15, 21, -8, 28, -31, 42, -31, 49, -8, -49, -42, -40, -49, -30, -49, -21, -42, 21, -42, 30, -49, 40, -49, 49, -42 };  /* shocked */
static const int16_t s_fe_p30[] = { -16, 32, -16, -24, 16, -24, 16, 32, 16, 53, -16, 53, -16, 32, -49, -3, -42, -28, -28, -30, -21, -10, -28, 13, -42, 14, -49, -3, 49, -3, 42, 14, 28, 13, 21, -10, 28, -30, 42, -28, 49, -3, -49, -38, -40, -46, -30, -49, -21, -45, 21, -45, 30, -49, 40, -46, 49, -38 };  /* horrified */
static const int16_t s_fe_p31[] = { -16, 29, -15, -17, 15, -17, 16, 29, 15, 57, -15, 57, -16, 29, -49, -6, -42, -29, -28, -30, -21, -9, -28, 13, -42, 14, -49, -6, 49, -6, 42, 14, 28, 13, 21, -9, 28, -30, 42, -29, 49, -6, -49, -39, -40, -47, -30, -48, -21, -42, 21, -42, 30, -48, 40, -47, 49, -39 };  /* spooked */
static const int16_t s_fe_p32[] = { -17, 25, -15, -5, 15, -5, 17, 25, 16, 59, -16, 59, -17, 25, -49, -8, -42, -29, -28, -29, -21, -8, -28, 11, -42, 11, -49, -8, 49, -8, 42, 11, 28, 11, 21, -8, 28, -29, 42, -29, 49, -8, -49, -39, -40, -46, -30, -46, -21, -39, 21, -39, 30, -46, 40, -46, 49, -39 };  /* in awe */
static const int16_t s_fe_p33[] = { -18, 29, -15, -10, 15, -10, 18, 29, 15, 48, -15, 48, -18, 29, -49, -7, -42, -26, -28, -29, -21, -8, -28, 12, -42, 12, -49, -7, 49, -7, 42, 12, 28, 12, 21, -8, 28, -29, 42, -26, 49, -7, -49, -39, -40, -45, -30, -44, -21, -39, 21, -39, 30, -44, 40, -45, 49, -39 };  /* alarmed */
static const int16_t s_fe_p34[] = { -30, 35, -12, 15, 12, 15, 30, 35, 12, 10, -12, 10, -30, 35, -51, -10, -43, -16, -27, -24, -19, -2, -27, 4, -43, 4, -51, -10, 51, -10, 43, 4, 27, 4, 19, -2, 27, -24, 43, -16, 51, -10, -51, -37, -40, -37, -30, -29, -19, -26, 19, -26, 30, -29, 40, -37, 51, -37 };  /* angry */
static const int16_t s_fe_p35[] = { -30, 37, -12, 13, 12, 13, 30, 37, 12, 7, -12, 7, -30, 37, -51, -10, -43, -14, -27, -24, -19, -1, -27, 3, -43, 3, -51, -10, 51, -10, 43, 3, 27, 3, 19, -1, 27, -24, 43, -14, 51, -10, -51, -38, -40, -37, -30, -27, -19, -25, 19, -25, 30, -27, 40, -37, 51, -38 };  /* furious */
static const int16_t s_fe_p36[] = { -30, 33, -13, 14, 13, 14, 30, 33, 13, 14, -13, 14, -30, 33, -51, -8, -43, -18, -27, -24, -19, -5, -27, 5, -43, 6, -51, -8, 51, -8, 43, 6, 27, 5, 19, -5, 27, -24, 43, -18, 51, -8, -51, -34, -40, -37, -30, -33, -19, -30, 19, -30, 30, -33, 40, -37, 51, -34 };  /* displeased */
static const int16_t s_fe_p37[] = { -31, 29, -12, 24, 12, 24, 31, 29, 16, 25, -16, 25, -31, 29, -51, -8, -43, -17, -27, -22, -19, -3, -27, 0, -43, 0, -51, -8, 51, -8, 43, 0, 27, 0, 19, -3, 27, -22, 43, -17, 51, -8, -51, -34, -40, -37, -30, -32, -19, -29, 19, -29, 30, -32, 40, -37, 51, -34 };  /* contempt */
static const int16_t s_fe_p38[] = { -29, 31, -13, 17, 13, 17, 29, 31, 13, 18, -13, 18, -29, 31, -51, -8, -43, -20, -27, -24, -19, -6, -27, 6, -43, 6, -51, -8, 51, -8, 43, 6, 27, 6, 19, -6, 27, -24, 43, -20, 51, -8, -51, -33, -40, -37, -30, -35, -19, -31, 19, -31, 30, -35, 40, -37, 51, -33 };  /* suspicious */
static const int16_t s_fe_p39[] = { -29, 30, -14, 17, 14, 17, 29, 30, 14, 19, -14, 19, -29, 30, -51, -7, -43, -21, -27, -24, -19, -7, -27, 6, -43, 7, -51, -7, 51, -7, 43, 7, 27, 6, 19, -7, 27, -24, 43, -21, 51, -7, -51, -33, -40, -38, -30, -36, -19, -32, 19, -32, 30, -36, 40, -38, 51, -33 };  /* thinking */
static const int16_t s_fe_p40[] = { -28, 26, -14, 20, 14, 20, 28, 26, 15, 31, -15, 31, -28, 26, -51, -7, -43, -24, -27, -24, -19, -8, -27, 6, -43, 7, -51, -7, 51, -7, 43, 7, 27, 6, 19, -8, 27, -24, 43, -24, 51, -7, -51, -32, -40, -39, -30, -39, -19, -34, 19, -34, 30, -39, 40, -39, 51, -32 };  /* pondering */
static const int16_t s_fe_p41[] = { -26, 31, -14, 8, 14, 8, 26, 31, 14, 26, -14, 26, -26, 31, -51, -6, -43, -22, -27, -25, -19, -8, -27, 8, -43, 8, -51, -6, 51, -6, 43, 8, 27, 8, 19, -8, 27, -25, 43, -22, 51, -6, -51, -33, -40, -39, -30, -39, -19, -35, 19, -35, 30, -39, 40, -39, 51, -33 };  /* confused */
static const int16_t s_fe_p42[] = { -23, 24, -14, 11, 14, 11, 23, 24, 17, 49, -17, 49, -23, 24, -50, -7, -43, -26, -27, -26, -20, -7, -27, 7, -43, 7, -50, -7, 50, -7, 43, 7, 27, 7, 20, -7, 27, -26, 43, -26, 50, -7, -50, -36, -40, -42, -30, -42, -20, -36, 20, -36, 30, -42, 40, -42, 50, -36 };  /* curious */
static const int16_t s_fe_p43[] = { -32, 26, -13, 26, 13, 26, 32, 26, 20, 37, -20, 37, -32, 26, -51, -4, -43, -20, -27, -21, -19, -7, -27, -3, -43, -2, -51, -4, 51, -4, 43, -2, 27, -3, 19, -7, 27, -21, 43, -20, 51, -4, -51, -30, -40, -37, -30, -38, -19, -35, 19, -35, 30, -38, 40, -37, 51, -30 };  /* sheepish */
static const int16_t s_fe_p44[] = { -32, 26, -12, 31, 12, 31, 32, 26, 20, 36, -20, 36, -32, 26, -51, -6, -43, -18, -27, -21, -19, -4, -27, -4, -43, -4, -51, -6, 51, -6, 43, -4, 27, -4, 19, -4, 27, -21, 43, -18, 51, -6, -51, -33, -40, -37, -30, -34, -19, -30, 19, -30, 30, -34, 40, -37, 51, -33 };  /* mischievous */
static const int16_t s_fe_p45[] = { -30, 31, -13, 21, 13, 21, 30, 31, 14, 19, -14, 19, -30, 31, -51, -9, -43, -19, -27, -23, -19, -4, -27, 4, -43, 4, -51, -9, 51, -9, 43, 4, 27, 4, 19, -4, 27, -23, 43, -19, 51, -9, -51, -35, -40, -37, -30, -33, -19, -29, 19, -29, 30, -33, 40, -37, 51, -35 };  /* determined */
static const int16_t s_fe_p46[] = { -28, 27, -14, 16, 14, 16, 28, 27, 18, 38, -18, 38, -28, 27, -51, -4, -43, -22, -27, -23, -19, -8, -27, 3, -43, 3, -51, -4, 51, -4, 43, 3, 27, 3, 19, -8, 27, -23, 43, -22, 51, -4, -51, -32, -40, -39, -30, -40, -19, -36, 19, -36, 30, -40, 40, -39, 51, -32 };  /* nervous */
static const int16_t s_fe_p47[] = { -28, 25, -14, 20, 14, 20, 28, 25, 19, 42, -19, 42, -28, 25, -51, -5, -43, -22, -27, -23, -19, -7, -27, 1, -43, 1, -51, -5, 51, -5, 43, 1, 27, 1, 19, -7, 27, -23, 43, -22, 51, -5, -51, -32, -40, -39, -30, -40, -19, -36, 19, -36, 30, -40, 40, -39, 51, -32 };  /* embarrassed */
static const int16_t s_fe_p48[] = { -31, 20, -13, 26, 13, 26, 31, 20, 26, 58, -26, 58, -31, 20, -51, -4, -43, -19, -27, -20, -19, -5, -27, -8, -43, -8, -51, -4, 51, -4, 43, -8, 27, -8, 19, -5, 27, -20, 43, -19, 51, -4, -51, -32, -40, -39, -30, -40, -19, -35, 19, -35, 30, -40, 40, -39, 51, -32 };  /* flattered */

static const gfx_sm_pose_t s_fe_poses[] = {
    { s_fe_p00 },  /* neutral */
    { s_fe_p01 },  /* look up */
    { s_fe_p02 },  /* look down */
    { s_fe_p03 },  /* look left */
    { s_fe_p04 },  /* look right */
    { s_fe_p05 },  /* side-eye */
    { s_fe_p06 },  /* daydream */
    { s_fe_p07 },  /* soft smile */
    { s_fe_p08 },  /* smile */
    { s_fe_p09 },  /* happy */
    { s_fe_p10 },  /* grinning */
    { s_fe_p11 },  /* big grin */
    { s_fe_p12 },  /* loving */
    { s_fe_p13 },  /* excited */
    { s_fe_p14 },  /* laughing */
    { s_fe_p15 },  /* amused */
    { s_fe_p16 },  /* relaxed */
    { s_fe_p17 },  /* blissful */
    { s_fe_p18 },  /* proud */
    { s_fe_p19 },  /* smug */
    { s_fe_p20 },  /* sad */
    { s_fe_p21 },  /* sorrowful */
    { s_fe_p22 },  /* crying */
    { s_fe_p23 },  /* sobbing */
    { s_fe_p24 },  /* dejected */
    { s_fe_p25 },  /* sympathetic */
    { s_fe_p26 },  /* longing */
    { s_fe_p27 },  /* tearful plea */
    { s_fe_p28 },  /* wow */
    { s_fe_p29 },  /* shocked */
    { s_fe_p30 },  /* horrified */
    { s_fe_p31 },  /* spooked */
    { s_fe_p32 },  /* in awe */
    { s_fe_p33 },  /* alarmed */
    { s_fe_p34 },  /* angry */
    { s_fe_p35 },  /* furious */
    { s_fe_p36 },  /* displeased */
    { s_fe_p37 },  /* contempt */
    { s_fe_p38 },  /* suspicious */
    { s_fe_p39 },  /* thinking */
    { s_fe_p40 },  /* pondering */
    { s_fe_p41 },  /* confused */
    { s_fe_p42 },  /* curious */
    { s_fe_p43 },  /* sheepish */
    { s_fe_p44 },  /* mischievous */
    { s_fe_p45 },  /* determined */
    { s_fe_p46 },  /* nervous */
    { s_fe_p47 },  /* embarrassed */
    { s_fe_p48 },  /* flattered */
};

static const gfx_sm_segment_t s_fe_segments[] = {
    /* mouth  */ { GFX_SM_SEG_BEZIER_LOOP,  .joint_a =  0, .joint_count = 7, .stroke_width = 10 },
    /* eye_l  */ { GFX_SM_SEG_BEZIER_FILL,  .joint_a =  7, .joint_count = 7 },
    /* eye_r  */ { GFX_SM_SEG_BEZIER_FILL,  .joint_a = 14, .joint_count = 7 },
    /* brow_l */ { GFX_SM_SEG_BEZIER_STRIP, .joint_a = 21, .joint_count = 4, .stroke_width =  6 },
    /* brow_r */ { GFX_SM_SEG_BEZIER_STRIP, .joint_a = 25, .joint_count = 4, .stroke_width =  6 },
};

static const gfx_sm_clip_step_t s_fe_step00[] = { { .pose_index= 0, .hold_ticks= 60, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* neutral */
static const gfx_sm_clip_step_t s_fe_step01[] = { { .pose_index= 1, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* look up */
static const gfx_sm_clip_step_t s_fe_step02[] = { { .pose_index= 2, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* look down */
static const gfx_sm_clip_step_t s_fe_step03[] = { { .pose_index= 3, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* look left */
static const gfx_sm_clip_step_t s_fe_step04[] = { { .pose_index= 4, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* look right */
static const gfx_sm_clip_step_t s_fe_step05[] = { { .pose_index= 5, .hold_ticks= 40, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* side-eye */
static const gfx_sm_clip_step_t s_fe_step06[] = { { .pose_index= 6, .hold_ticks= 50, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* daydream */
static const gfx_sm_clip_step_t s_fe_step07[] = { { .pose_index= 7, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* soft smile */
static const gfx_sm_clip_step_t s_fe_step08[] = { { .pose_index= 8, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* smile */
static const gfx_sm_clip_step_t s_fe_step09[] = { { .pose_index= 9, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* happy */
static const gfx_sm_clip_step_t s_fe_step10[] = { { .pose_index=10, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* grinning */
static const gfx_sm_clip_step_t s_fe_step11[] = { { .pose_index=11, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* big grin */
static const gfx_sm_clip_step_t s_fe_step12[] = { { .pose_index=12, .hold_ticks= 40, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* loving */
static const gfx_sm_clip_step_t s_fe_step13[] = { { .pose_index=13, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* excited */
static const gfx_sm_clip_step_t s_fe_step14[] = { { .pose_index=14, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* laughing */
static const gfx_sm_clip_step_t s_fe_step15[] = { { .pose_index=15, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* amused */
static const gfx_sm_clip_step_t s_fe_step16[] = { { .pose_index=16, .hold_ticks= 45, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* relaxed */
static const gfx_sm_clip_step_t s_fe_step17[] = { { .pose_index=17, .hold_ticks= 40, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* blissful */
static const gfx_sm_clip_step_t s_fe_step18[] = { { .pose_index=18, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* proud */
static const gfx_sm_clip_step_t s_fe_step19[] = { { .pose_index=19, .hold_ticks= 40, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* smug */
static const gfx_sm_clip_step_t s_fe_step20[] = { { .pose_index=20, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* sad */
static const gfx_sm_clip_step_t s_fe_step21[] = { { .pose_index=21, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* sorrowful */
static const gfx_sm_clip_step_t s_fe_step22[] = { { .pose_index=22, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* crying */
static const gfx_sm_clip_step_t s_fe_step23[] = { { .pose_index=23, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* sobbing */
static const gfx_sm_clip_step_t s_fe_step24[] = { { .pose_index=24, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* dejected */
static const gfx_sm_clip_step_t s_fe_step25[] = { { .pose_index=25, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* sympathetic */
static const gfx_sm_clip_step_t s_fe_step26[] = { { .pose_index=26, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* longing */
static const gfx_sm_clip_step_t s_fe_step27[] = { { .pose_index=27, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* tearful plea */
static const gfx_sm_clip_step_t s_fe_step28[] = { { .pose_index=28, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* wow */
static const gfx_sm_clip_step_t s_fe_step29[] = { { .pose_index=29, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* shocked */
static const gfx_sm_clip_step_t s_fe_step30[] = { { .pose_index=30, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* horrified */
static const gfx_sm_clip_step_t s_fe_step31[] = { { .pose_index=31, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* spooked */
static const gfx_sm_clip_step_t s_fe_step32[] = { { .pose_index=32, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* in awe */
static const gfx_sm_clip_step_t s_fe_step33[] = { { .pose_index=33, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* alarmed */
static const gfx_sm_clip_step_t s_fe_step34[] = { { .pose_index=34, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* angry */
static const gfx_sm_clip_step_t s_fe_step35[] = { { .pose_index=35, .hold_ticks= 25, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* furious */
static const gfx_sm_clip_step_t s_fe_step36[] = { { .pose_index=36, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* displeased */
static const gfx_sm_clip_step_t s_fe_step37[] = { { .pose_index=37, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* contempt */
static const gfx_sm_clip_step_t s_fe_step38[] = { { .pose_index=38, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* suspicious */
static const gfx_sm_clip_step_t s_fe_step39[] = { { .pose_index=39, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* thinking */
static const gfx_sm_clip_step_t s_fe_step40[] = { { .pose_index=40, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* pondering */
static const gfx_sm_clip_step_t s_fe_step41[] = { { .pose_index=41, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* confused */
static const gfx_sm_clip_step_t s_fe_step42[] = { { .pose_index=42, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* curious */
static const gfx_sm_clip_step_t s_fe_step43[] = { { .pose_index=43, .hold_ticks= 40, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* sheepish */
static const gfx_sm_clip_step_t s_fe_step44[] = { { .pose_index=44, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* mischievous */
static const gfx_sm_clip_step_t s_fe_step45[] = { { .pose_index=45, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* determined */
static const gfx_sm_clip_step_t s_fe_step46[] = { { .pose_index=46, .hold_ticks= 30, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* nervous */
static const gfx_sm_clip_step_t s_fe_step47[] = { { .pose_index=47, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* embarrassed */
static const gfx_sm_clip_step_t s_fe_step48[] = { { .pose_index=48, .hold_ticks= 35, .interp=GFX_SM_INTERP_DAMPED, .facing=1 } };  /* flattered */

static const gfx_sm_clip_t s_fe_clips[] = {
    { "neutral", "平静", s_fe_step00, 1, true },
    { "look up", "抬头看", s_fe_step01, 1, true },
    { "look down", "低头看", s_fe_step02, 1, true },
    { "look left", "向左看", s_fe_step03, 1, true },
    { "look right", "向右看", s_fe_step04, 1, true },
    { "side-eye", "斜视", s_fe_step05, 1, true },
    { "daydream", "发呆", s_fe_step06, 1, true },
    { "soft smile", "微笑", s_fe_step07, 1, true },
    { "smile", "灿烂微笑", s_fe_step08, 1, true },
    { "happy", "开心", s_fe_step09, 1, true },
    { "grinning", "咧嘴笑", s_fe_step10, 1, true },
    { "big grin", "大嘴笑", s_fe_step11, 1, true },
    { "loving", "爱慕", s_fe_step12, 1, true },
    { "excited", "激动", s_fe_step13, 1, true },
    { "laughing", "大笑", s_fe_step14, 1, true },
    { "amused", "忍俊不禁", s_fe_step15, 1, true },
    { "relaxed", "放松", s_fe_step16, 1, true },
    { "blissful", "陶醉", s_fe_step17, 1, true },
    { "proud", "骄傲", s_fe_step18, 1, true },
    { "smug", "自鸣得意", s_fe_step19, 1, true },
    { "sad", "难过", s_fe_step20, 1, true },
    { "sorrowful", "哀愁", s_fe_step21, 1, true },
    { "crying", "哭泣", s_fe_step22, 1, true },
    { "sobbing", "抽泣", s_fe_step23, 1, true },
    { "dejected", "沮丧", s_fe_step24, 1, true },
    { "sympathetic", "同情", s_fe_step25, 1, true },
    { "longing", "思念", s_fe_step26, 1, true },
    { "tearful plea", "泪眼求饶", s_fe_step27, 1, true },
    { "wow", "哇", s_fe_step28, 1, true },
    { "shocked", "震惊", s_fe_step29, 1, true },
    { "horrified", "恐惧", s_fe_step30, 1, true },
    { "spooked", "受惊", s_fe_step31, 1, true },
    { "in awe", "敬畏", s_fe_step32, 1, true },
    { "alarmed", "警觉", s_fe_step33, 1, true },
    { "angry", "生气", s_fe_step34, 1, true },
    { "furious", "暴怒", s_fe_step35, 1, true },
    { "displeased", "不悦", s_fe_step36, 1, true },
    { "contempt", "鄙视", s_fe_step37, 1, true },
    { "suspicious", "起疑", s_fe_step38, 1, true },
    { "thinking", "思考", s_fe_step39, 1, true },
    { "pondering", "沉思", s_fe_step40, 1, true },
    { "confused", "困惑", s_fe_step41, 1, true },
    { "curious", "好奇", s_fe_step42, 1, true },
    { "sheepish", "装乖", s_fe_step43, 1, true },
    { "mischievous", "坏笑", s_fe_step44, 1, true },
    { "determined", "坚定", s_fe_step45, 1, true },
    { "nervous", "紧张", s_fe_step46, 1, true },
    { "embarrassed", "尴尬", s_fe_step47, 1, true },
    { "flattered", "受宠若惊", s_fe_step48, 1, true },
};

static const uint16_t s_fe_sequence[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 };

static const gfx_sm_asset_t s_fe_scene_asset = {
    .meta           = &s_fe_meta,
    .joint_names    = s_fe_joint_names,
    .joint_count    = 29,
    .segments       = s_fe_segments,
    .segment_count  = 5,
    .poses          = s_fe_poses,
    .pose_count     = 49,
    .clips          = s_fe_clips,
    .clip_count     = 49,
    .sequence       = s_fe_sequence,
    .sequence_count = 49,
    .layout         = &s_fe_layout,
};
