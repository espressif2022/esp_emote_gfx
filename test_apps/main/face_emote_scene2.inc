/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * face_emote_scene.inc — auto-generated from Lottie JSON by lottie2inc.py
 *
 * Format version: GFX_SM_SCENE_SCHEMA_VERSION (2)
 *
 * Segments:
 *   Segment      Joints           Kind             Notes
 *   ────────────  ──────────────── ──────────────── ────────────
 *   mouth        0-6 (7 pts)      BEZIER_LOOP      sw=10
 *   eye_l        7-13 (7 pts)     BEZIER_FILL      solid colour
 *   eye_r        14-20 (7 pts)    BEZIER_FILL      solid colour
 *   brow_l       21-24 (4 pts)    BEZIER_STRIP     sw=6
 *   brow_r       25-28 (4 pts)    BEZIER_STRIP     sw=6
 */

#pragma once

#include "widget/gfx_sm_scene.h"

/* ────────────────────────────────────────────────────────────────── */
/*  Metadata                                                         */
/* ────────────────────────────────────────────────────────────────── */

static const gfx_sm_meta_t s_fe_meta = {
    .version   = GFX_SM_SCENE_SCHEMA_VERSION,
    .viewbox_x = -100,
    .viewbox_y = -80,
    .viewbox_w =  200,
    .viewbox_h =  200,
};

/* ────────────────────────────────────────────────────────────────── */
/*  Layout / rendering hints                                         */
/* ────────────────────────────────────────────────────────────────── */

static const gfx_sm_layout_t s_fe_layout = {
    .stroke_width    =   6,
    .mirror_x        =   0,
    .ground_y        =  80,
    .timer_period_ms =  33,
    .damping_div     =   4,
};

/* ────────────────────────────────────────────────────────────────── */
/*  Joint names                                                      */
/* ────────────────────────────────────────────────────────────────── */

static const char *const s_fe_joint_names[] = {
    "mouth_0",             /* 0 */
    "mouth_1",             /* 1 */
    "mouth_2",             /* 2 */
    "mouth_3",             /* 3 */
    "mouth_4",             /* 4 */
    "mouth_5",             /* 5 */
    "mouth_6",             /* 6 */
    "eye_l_0",             /* 7 */
    "eye_l_1",             /* 8 */
    "eye_l_2",             /* 9 */
    "eye_l_3",             /* 10 */
    "eye_l_4",             /* 11 */
    "eye_l_5",             /* 12 */
    "eye_l_6",             /* 13 */
    "eye_r_0",             /* 14 */
    "eye_r_1",             /* 15 */
    "eye_r_2",             /* 16 */
    "eye_r_3",             /* 17 */
    "eye_r_4",             /* 18 */
    "eye_r_5",             /* 19 */
    "eye_r_6",             /* 20 */
    "brow_l_0",            /* 21 */
    "brow_l_1",            /* 22 */
    "brow_l_2",            /* 23 */
    "brow_l_3",            /* 24 */
    "brow_r_0",            /* 25 */
    "brow_r_1",            /* 26 */
    "brow_r_2",            /* 27 */
    "brow_r_3",            /* 28 */
};

/* ────────────────────────────────────────────────────────────────── */
/*  Poses — flat coord arrays [x0,y0, x1,y1, …] for 29 joints        */
/* ────────────────────────────────────────────────────────────────── */

static const int16_t s_fe_p00[] = { -28, 25, -14, 24, 14, 24, 28, 25, 14, 26, -14, 26, -28, 25, -51, -8, -43, -24, -27, -24, -19, -8, -27, 8, -43, 8, -51, -8, 51, -8, 43, 8, 27, 8, 19, -8, 27, -24, 43, -24, 51, -8, -51, -32, -40, -38, -30, -38, -19, -32, 19, -32, 30, -38, 40, -38, 51, -32 };  /* neutral */
static const int16_t s_fe_p01[] = { -34, 20, -12, 43, 12, 43, 34, 20, 26, 55, -26, 55, -34, 20, -51, -4, -43, -18, -27, -18, -19, -4, -27, -12, -43, -12, -51, -4, 51, -4, 43, -12, 27, -12, 19, -4, 27, -18, 43, -18, 51, -4, -51, -30, -40, -37, -30, -37, -19, -33, 19, -33, 30, -37, 40, -37, 51, -30 };  /* smile */
static const int16_t s_fe_p02[] = { -34, 10, -12, 13, 12, 13, 34, 10, 34, 65, -34, 65, -34, 10, -51, -4, -43, -16, -27, -16, -19, -4, -27, -12, -43, -12, -51, -4, 51, -4, 43, -12, 27, -12, 19, -4, 27, -16, 43, -16, 51, -4, -51, -33, -40, -40, -30, -40, -19, -33, 19, -33, 30, -40, 40, -40, 51, -33 };  /* happy */
static const int16_t s_fe_p03[] = { -30, 37, -15, 0, 15, 0, 30, 37, 15, 15, -15, 15, -30, 37, -51, 0, -43, -20, -27, -24, -19, -12, -27, 6, -43, 8, -51, 0, 51, 0, 43, 8, 27, 6, 19, -12, 27, -24, 43, -20, 51, 0, -51, -28, -40, -37, -30, -41, -19, -40, 19, -40, 30, -41, 40, -37, 51, -28 };  /* sad */
static const int16_t s_fe_p04[] = { -30, 35, -12, 15, 12, 15, 30, 35, 12, 10, -12, 10, -30, 35, -51, -10, -43, -16, -27, -24, -19, -2, -27, 4, -43, 4, -51, -10, 51, -10, 43, 4, 27, 4, 19, -2, 27, -24, 43, -16, 51, -10, -51, -37, -40, -37, -30, -29, -19, -26, 19, -26, 30, -29, 40, -37, 51, -37 };  /* angry */

#define S_FE_POSE(arr) { .coords = (arr) }

static const gfx_sm_pose_t s_fe_poses[] = {
    S_FE_POSE(s_fe_p00),  S_FE_POSE(s_fe_p01),  S_FE_POSE(s_fe_p02),
    S_FE_POSE(s_fe_p03),  S_FE_POSE(s_fe_p04),
};

/* ────────────────────────────────────────────────────────────────── */
/*  Segment wiring                                                   */
/* ────────────────────────────────────────────────────────────────── */

static const gfx_sm_segment_t s_fe_segments[] = {
    /* mouth    */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a =  0, .joint_count = 7, .stroke_width = 6 },
    /* eye_l    */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a =  7, .joint_count = 7 },
    /* eye_r    */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 14, .joint_count = 7 },
    /* brow_l   */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 21, .joint_count = 4, .stroke_width =  6 },
    /* brow_r   */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 25, .joint_count = 4, .stroke_width =  6 },
};

/* ────────────────────────────────────────────────────────────────── */
/*  Clip step arrays                                                 */
/* ────────────────────────────────────────────────────────────────── */

#define STEP(p, h, f) { .pose_index = (p), .hold_ticks = (h), .interp = GFX_SM_INTERP_DAMPED, .facing = (f) }

static const gfx_sm_clip_step_t s_fe_steps[] = {
    STEP( 0, 60,  1),  /*  0: neutral */
    STEP( 1, 30,  1),  /*  1: smile */
    STEP( 2, 30,  1),  /*  2: happy */
    STEP( 3, 30,  1),  /*  3: sad */
    STEP( 4, 30,  1),  /*  4: angry */
};

/* ────────────────────────────────────────────────────────────────── */
/*  Clips                                                            */
/* ────────────────────────────────────────────────────────────────── */

#define CLIP(n, cn, idx, lp) \
    { .name = (n), .name_cn = (cn), .steps = &s_fe_steps[(idx)], \
      .step_count = 1, .loop = (lp) }

static const gfx_sm_clip_t s_fe_clips[] = {
    CLIP("neutral",         "平静",         0, true),
    CLIP("smile",           "灿烂微笑",   1, true),
    CLIP("happy",           "开心",         2, true),
    CLIP("sad",             "难过",         3, true),
    CLIP("angry",           "生气",         4, true),
};

/* ────────────────────────────────────────────────────────────────── */
/*  Default playback sequence — one entry per clip                   */
/* ────────────────────────────────────────────────────────────────── */

static const uint16_t s_fe_sequence[] = {
     0,  1,  2,  3,  4,
};

/* ────────────────────────────────────────────────────────────────── */
/*  Top-level asset                                                  */
/* ────────────────────────────────────────────────────────────────── */

static const gfx_sm_asset_t s_fe_scene_asset = {
    .meta           = &s_fe_meta,
    .joint_names    = s_fe_joint_names,
    .joint_count    = (uint8_t)(sizeof(s_fe_joint_names) / sizeof(s_fe_joint_names[0])),
    .segments       = s_fe_segments,
    .segment_count  = (uint8_t)(sizeof(s_fe_segments) / sizeof(s_fe_segments[0])),
    .poses          = s_fe_poses,
    .pose_count     = (uint16_t)(sizeof(s_fe_poses) / sizeof(s_fe_poses[0])),
    .clips          = s_fe_clips,
    .clip_count     = (uint16_t)(sizeof(s_fe_clips) / sizeof(s_fe_clips[0])),
    .sequence       = s_fe_sequence,
    .sequence_count = (uint16_t)(sizeof(s_fe_sequence) / sizeof(s_fe_sequence[0])),
    .layout         = &s_fe_layout,
};
