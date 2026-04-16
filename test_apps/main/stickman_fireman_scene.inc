/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * stickman_fireman_scene.inc — stickman scene in the unified gfx_sm_scene format.
 *
 * Format version: GFX_SM_SCENE_SCHEMA_VERSION (2)
 *
 * Segments encode the rendering primitive for each joint pair:
 *
 *   Segment    Joints   Kind      Notes
 *   ─────────  ───────  ────────  ──────────────
 *   head       0        RING      radius_hint=20
 *   neck       1→2      CAPSULE   solid colour
 *   spine      2→3      CAPSULE   solid colour
 *   arm_l_u    2→4      CAPSULE   solid colour
 *   arm_l_l    4→5      CAPSULE   solid colour
 *   arm_r_u    2→6      CAPSULE   solid colour
 *   arm_r_l    6→7      CAPSULE   solid colour
 *   leg_l_u    3→8      CAPSULE   solid colour
 *   leg_l_l    8→9      CAPSULE   solid colour
 *   leg_r_u    3→10     CAPSULE   solid colour
 *   leg_r_l    10→11    CAPSULE   solid colour
 *
 * Joint index table (0-based, 12 joints):
 *   0  head_center
 *   1  neck
 *   2  shoulder
 *   3  hip
 *   4  elbow_l
 *   5  hand_l
 *   6  elbow_r
 *   7  hand_r
 *   8  knee_l
 *   9  foot_l
 *  10  knee_r
 *  11  foot_r
 */

#pragma once
#include "widget/gfx_sm_scene.h"

/* ------------------------------------------------------------------ */
/*  Metadata                                                           */
/* ------------------------------------------------------------------ */

static const gfx_sm_meta_t s_fm_meta = {
    .version   = GFX_SM_SCENE_SCHEMA_VERSION,
    .viewbox_x = 0,
    .viewbox_y = 0,
    .viewbox_w = 200,
    .viewbox_h = 300,
};

/* ------------------------------------------------------------------ */
/*  Layout / rendering hints                                           */
/* ------------------------------------------------------------------ */

static const gfx_sm_layout_t s_fm_layout = {
    .stroke_width    = 4,
    .mirror_x        = 100,
    .ground_y        = 238,
    .timer_period_ms = 33,
    .damping_div     = 4,
};

/* ------------------------------------------------------------------ */
/*  Joint names                                                        */
/* ------------------------------------------------------------------ */

static const char *const s_fm_joint_names[] = {
    "head_center",  /* 0 */
    "neck",         /* 1 */
    "shoulder",     /* 2 */
    "hip",          /* 3 */
    "elbow_l",      /* 4 */
    "hand_l",       /* 5 */
    "elbow_r",      /* 6 */
    "hand_r",       /* 7 */
    "knee_l",       /* 8 */
    "foot_l",       /* 9 */
    "knee_r",       /* 10 */
    "foot_r",       /* 11 */
};

/* ------------------------------------------------------------------ */
/*  Segment wiring                                                     */
/*  (previously buried in s_segment_point_pairs[] inside .c file)     */
/* ------------------------------------------------------------------ */

static const gfx_sm_segment_t s_fm_segments[] = {
    /* head   */ { .kind = GFX_SM_SEG_RING,    .joint_a = 0,  .joint_b = 0,  .radius_hint = 20 },
    /* neck   */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 1,  .joint_b = 2  },
    /* spine  */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 2,  .joint_b = 3  },
    /* arm_lu */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 2,  .joint_b = 4  },
    /* arm_ll */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 4,  .joint_b = 5  },
    /* arm_ru */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 2,  .joint_b = 6  },
    /* arm_rl */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 6,  .joint_b = 7  },
    /* leg_lu */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 3,  .joint_b = 8  },
    /* leg_ll */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 8,  .joint_b = 9  },
    /* leg_ru */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 3,  .joint_b = 10 },
    /* leg_rl */ { .kind = GFX_SM_SEG_CAPSULE, .joint_a = 10, .joint_b = 11 },
};

/* ------------------------------------------------------------------ */
/*  Poses — flat coord arrays [x0,y0, x1,y1, …] for 12 joints         */
/* ------------------------------------------------------------------ */

/* pose 0 — calm */
static const int16_t s_fm_pose0[]  = { 100,50, 100,70, 100,100, 100,160,  82,118,  66,138, 118,118, 134,138,  86,186,  72,214, 114,186, 128,214 };
/* pose 1 — walk s1 */
static const int16_t s_fm_pose1[]  = { 102,50, 101,70, 100,100,  98,160,  90,108,  82,90,  114,120, 126,142,  84,184,  70,214, 112,182, 132,198 };
/* pose 2 — walk s2 */
static const int16_t s_fm_pose2[]  = {  98,50,  99,70, 100,100, 102,160,  86,120,  74,142, 110,108, 118,90,   88,182,  68,198, 116,184, 130,214 };
/* pose 3 — run s1 */
static const int16_t s_fm_pose3[]  = { 112,48, 106,68, 102,94,   88,154,  86,96,   78,72,  114,112, 130,136,  72,180,  54,210, 116,172, 142,190 };
/* pose 4 — run s2 */
static const int16_t s_fm_pose4[]  = { 114,50, 108,68, 104,94,   90,154, 110,110, 126,136,  82,96,   74,72,  106,172, 138,190,  74,180,  54,210 };
/* pose 5 — jump */
static const int16_t s_fm_pose5[]  = { 100,44, 100,64, 100,94,  100,146,  78,70,   60,46,  122,70,  140,46,   84,172,  68,194, 116,172, 132,194 };
/* pose 6 — wave */
static const int16_t s_fm_pose6[]  = {  98,50,  98,70,  98,100,  98,160,  84,118,  72,140, 116,82,  112,48,   88,186,  76,214, 110,186, 122,214 };
/* pose 7 — deep_squat_hold_head */
static const int16_t s_fm_pose7[]  = { 100,64, 100,84, 100,120, 100,176,  78,106,  90,78,  122,106, 110,78,   74,206,  92,224, 126,206, 108,224 };
/* pose 8 — half_squat */
static const int16_t s_fm_pose8[]  = { 100,60, 100,80, 100,108, 100,166,  86,124,  74,146, 114,124, 126,146,  78,194,  92,216, 122,194, 108,216 };
/* pose 9 — hands_on_head */
static const int16_t s_fm_pose9[]  = { 100,52, 100,72, 100,102, 100,162,  74,88,   90,58,  126,88,  110,58,   86,188,  72,214, 114,188, 128,214 };
/* pose 10 — cheer */
static const int16_t s_fm_pose10[] = { 100,48, 100,68, 100,98,  100,158,  82,78,   64,52,  118,78,  136,52,   84,184,  70,212, 116,184, 130,212 };
/* pose 11 — point_r */
static const int16_t s_fm_pose11[] = {  96,50,  97,70,  98,100,  94,160,  84,116,  74,138, 126,100, 154,96,   84,186,  72,214, 110,184, 126,212 };
/* pose 12 — akimbo */
static const int16_t s_fm_pose12[] = { 100,50, 100,70, 100,100, 100,160,  84,110,  92,136, 116,110, 108,136,  82,186,  66,214, 118,186, 134,214 };
/* pose 13 — salute */
static const int16_t s_fm_pose13[] = { 100,50, 100,70, 100,100, 100,160,  84,118,  72,140, 118,88,  106,60,   86,186,  72,214, 114,186, 128,214 };
/* pose 14 — guard */
static const int16_t s_fm_pose14[] = {  98,54,  99,74, 100,104,  96,164,  84,108,  92,92,  116,110, 126,94,   84,190,  72,218, 112,186, 126,210 };
/* pose 15 — kick_r */
static const int16_t s_fm_pose15[] = { 100,52, 100,72, 100,102,  96,160,  84,106,  70,118, 118,110, 132,126,  88,188,  80,216, 120,166, 150,166 };
/* pose 16 — stretch */
static const int16_t s_fm_pose16[] = { 100,46, 100,66, 100,96,  100,158,  78,96,   56,96,  122,96,  144,96,   86,184,  72,212, 114,184, 128,212 };
/* pose 17 — thinking */
static const int16_t s_fm_pose17[] = { 102,50, 102,70, 100,100, 100,160,  84,112,  92,136, 116,92,  104,74,   86,186,  72,214, 114,186, 128,214 };
/* pose 18 — slouch */
static const int16_t s_fm_pose18[] = { 108,54, 106,74, 102,104,  94,164,  90,124,  86,146, 104,120, 100,144,  92,188,  88,214, 112,190, 114,220 };
/* pose 19 — reach_up */
static const int16_t s_fm_pose19[] = { 100,48, 100,68, 100,98,   98,160,  84,86,   78,54,  116,76,  118,38,   86,186,  72,214, 114,186, 128,214 };
/* pose 20 — strut */
static const int16_t s_fm_pose20[] = { 104,50, 104,70, 102,100, 100,160,  86,108,  68,108, 118,118, 132,134,  90,188,  76,214, 118,178, 134,198 };
/* pose 21 — balance */
static const int16_t s_fm_pose21[] = { 100,48, 100,68, 100,96,   98,158,  80,96,   56,96,  118,88,  136,72,   88,184,  76,212, 122,166, 140,190 };
/* pose 22 — lunge */
static const int16_t s_fm_pose22[] = {  98,50,  98,70,  98,100,  92,160,  80,116,  62,120, 122,94,  146,80,   80,186,  62,214, 118,174, 146,190 };
/* pose 23 — shrug */
static const int16_t s_fm_pose23[] = { 100,52, 100,72, 100,104, 100,162,  86,100,  78,84,  114,100, 122,84,   84,188,  70,214, 116,188, 130,214 };

#define FM_POSE(arr) { .coords = (arr) }

static const gfx_sm_pose_t s_fm_poses[] = {
    FM_POSE(s_fm_pose0),  FM_POSE(s_fm_pose1),  FM_POSE(s_fm_pose2),
    FM_POSE(s_fm_pose3),  FM_POSE(s_fm_pose4),  FM_POSE(s_fm_pose5),
    FM_POSE(s_fm_pose6),  FM_POSE(s_fm_pose7),  FM_POSE(s_fm_pose8),
    FM_POSE(s_fm_pose9),  FM_POSE(s_fm_pose10), FM_POSE(s_fm_pose11),
    FM_POSE(s_fm_pose12), FM_POSE(s_fm_pose13), FM_POSE(s_fm_pose14),
    FM_POSE(s_fm_pose15), FM_POSE(s_fm_pose16), FM_POSE(s_fm_pose17),
    FM_POSE(s_fm_pose18), FM_POSE(s_fm_pose19), FM_POSE(s_fm_pose20),
    FM_POSE(s_fm_pose21), FM_POSE(s_fm_pose22), FM_POSE(s_fm_pose23),
};

/* ------------------------------------------------------------------ */
/*  Clip step arrays                                                   */
/*  (multi-step clips collapse what were separate actions in old fmt)  */
/* ------------------------------------------------------------------ */

#define STEP(p, h, f) { .pose_index = (p), .hold_ticks = (h), .interp = GFX_SM_INTERP_DAMPED, .facing = (f) }

static const gfx_sm_clip_step_t s_steps_calm[]               = { STEP( 0, 6,  1) };
static const gfx_sm_clip_step_t s_steps_walk_r[]             = { STEP( 1, 7,  1), STEP( 2, 7,  1) };
static const gfx_sm_clip_step_t s_steps_walk_l[]             = { STEP( 1, 7, -1), STEP( 2, 7, -1) };
static const gfx_sm_clip_step_t s_steps_run_r[]              = { STEP( 3, 7,  1), STEP( 4, 7,  1) };
static const gfx_sm_clip_step_t s_steps_run_l[]              = { STEP( 3, 7, -1), STEP( 4, 7, -1) };
static const gfx_sm_clip_step_t s_steps_jump[]               = { STEP( 5, 8,  1) };
static const gfx_sm_clip_step_t s_steps_wave[]               = { STEP( 6, 7,  1) };
static const gfx_sm_clip_step_t s_steps_deep_squat_hh[]      = { STEP( 7, 9,  1) };
static const gfx_sm_clip_step_t s_steps_half_squat[]         = { STEP( 8, 7,  1) };
static const gfx_sm_clip_step_t s_steps_hands_on_head[]      = { STEP( 9, 7,  1) };
static const gfx_sm_clip_step_t s_steps_cheer[]              = { STEP(10, 7,  1) };
static const gfx_sm_clip_step_t s_steps_point_r[]            = { STEP(11, 6,  1) };
static const gfx_sm_clip_step_t s_steps_akimbo[]             = { STEP(12, 6,  1) };
static const gfx_sm_clip_step_t s_steps_salute[]             = { STEP(13, 6,  1) };
static const gfx_sm_clip_step_t s_steps_guard[]              = { STEP(14, 7,  1) };
static const gfx_sm_clip_step_t s_steps_kick_r[]             = { STEP(15, 7,  1) };
static const gfx_sm_clip_step_t s_steps_stretch[]            = { STEP(16, 6,  1) };
static const gfx_sm_clip_step_t s_steps_thinking[]           = { STEP(17, 6,  1) };
static const gfx_sm_clip_step_t s_steps_slouch[]             = { STEP(18, 7,  1) };
static const gfx_sm_clip_step_t s_steps_reach_up[]           = { STEP(19, 6,  1) };
static const gfx_sm_clip_step_t s_steps_strut[]              = { STEP(20, 7,  1) };
static const gfx_sm_clip_step_t s_steps_balance[]            = { STEP(21, 7,  1) };
static const gfx_sm_clip_step_t s_steps_lunge[]              = { STEP(22, 7,  1) };
static const gfx_sm_clip_step_t s_steps_shrug[]              = { STEP(23, 6,  1) };

#define CLIP(n, cn, arr, lp) \
    { .name = (n), .name_cn = (cn), .steps = (arr), \
      .step_count = (uint8_t)(sizeof(arr) / sizeof((arr)[0])), .loop = (lp) }

static const gfx_sm_clip_t s_fm_clips[] = {
    CLIP("calm",               "平静",     s_steps_calm,          true),
    CLIP("walk_r",             "走右",     s_steps_walk_r,        true),
    CLIP("walk_l",             "走左",     s_steps_walk_l,        true),
    CLIP("run_r",              "跑右",     s_steps_run_r,         true),
    CLIP("run_l",              "跑左",     s_steps_run_l,         true),
    CLIP("jump",               "跳跃",     s_steps_jump,          true),
    CLIP("wave",               "挥手",     s_steps_wave,          true),
    CLIP("deep_squat_hh",      "深蹲抱头", s_steps_deep_squat_hh, true),
    CLIP("half_squat",         "半蹲",     s_steps_half_squat,    true),
    CLIP("hands_on_head",      "抱头",     s_steps_hands_on_head, true),
    CLIP("cheer",              "欢呼",     s_steps_cheer,         true),
    CLIP("point_r",            "指向右",   s_steps_point_r,       true),
    CLIP("akimbo",             "叉腰",     s_steps_akimbo,        true),
    CLIP("salute",             "敬礼",     s_steps_salute,        true),
    CLIP("guard",              "警戒",     s_steps_guard,         true),
    CLIP("kick_r",             "右踢",     s_steps_kick_r,        true),
    CLIP("stretch",            "展臂",     s_steps_stretch,       true),
    CLIP("thinking",           "思考",     s_steps_thinking,      true),
    CLIP("slouch",             "沮丧",     s_steps_slouch,        true),
    CLIP("reach_up",           "上探",     s_steps_reach_up,      true),
    CLIP("strut",              "神气走",   s_steps_strut,         true),
    CLIP("balance",            "单脚平衡", s_steps_balance,       true),
    CLIP("lunge",              "弓步冲",   s_steps_lunge,         true),
    CLIP("shrug",              "摊手",     s_steps_shrug,         true),
};

/* ------------------------------------------------------------------ */
/*  Default playback sequence — one entry per clip                     */
/* ------------------------------------------------------------------ */

static const uint16_t s_fm_sequence[] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23,
};

/* ------------------------------------------------------------------ */
/*  Top-level asset                                                    */
/* ------------------------------------------------------------------ */

static const gfx_sm_asset_t s_fm_scene_asset = {
    .meta           = &s_fm_meta,
    .joint_names    = s_fm_joint_names,
    .joint_count    = (uint8_t)(sizeof(s_fm_joint_names) / sizeof(s_fm_joint_names[0])),
    .segments       = s_fm_segments,
    .segment_count  = (uint8_t)(sizeof(s_fm_segments) / sizeof(s_fm_segments[0])),
    .poses          = s_fm_poses,
    .pose_count     = (uint16_t)(sizeof(s_fm_poses) / sizeof(s_fm_poses[0])),
    .clips          = s_fm_clips,
    .clip_count     = (uint16_t)(sizeof(s_fm_clips) / sizeof(s_fm_clips[0])),
    .sequence       = s_fm_sequence,
    .sequence_count = (uint16_t)(sizeof(s_fm_sequence) / sizeof(s_fm_sequence[0])),
    .layout         = &s_fm_layout,
};
