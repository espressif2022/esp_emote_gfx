/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * rig_active.inc - generated from scene.json v1 by gui_designer.html
 */

#pragma once

#include "widget/gfx_sm_scene.h"

static const gfx_sm_meta_t s_rig_meta = {
    .version   = GFX_SM_SCENE_SCHEMA_VERSION,
    .viewbox_x = 0,
    .viewbox_y = 0,
    .viewbox_w = 1200,
    .viewbox_h = 900,
};

static const gfx_sm_layout_t s_rig_layout = {
    .stroke_width    = 20,  /* 4 × strokeNorm(5) — default for segments without explicit sw */
    .mirror_x        = 0,
    .ground_y        = 900,
    .timer_period_ms = 33,
    .damping_div     = 4,
};

static const char *const s_rig_joint_names[] = {
    "part_ind4_0", /* 0 */
    "part_ind4_1", /* 1 */
    "part_ind4_2", /* 2 */
    "part_ind4_3", /* 3 */
    "part_ind4_4", /* 4 */
    "part_ind4_5", /* 5 */
    "part_ind4_6", /* 6 */
    "part_ind4_7", /* 7 */
    "part_ind4_8", /* 8 */
    "part_ind4_9", /* 9 */
    "part_ind4_10", /* 10 */
    "part_ind4_11", /* 11 */
    "part_ind4_12", /* 12 */
    "part_ind4_13", /* 13 */
    "part_ind4_14", /* 14 */
    "part_ind4_15", /* 15 */
    "part_ind4_16", /* 16 */
    "part_ind4_17", /* 17 */
    "part_ind4_18", /* 18 */
    "part_ind4_19", /* 19 */
    "part_ind4_20", /* 20 */
    "part_ind4_21", /* 21 */
    "part_ind4_22", /* 22 */
    "part_ind4_23", /* 23 */
    "part_ind4_24", /* 24 */
    "part_ind4_25", /* 25 */
    "part_ind4_26", /* 26 */
    "part_ind4_27", /* 27 */
    "part_ind4_28", /* 28 */
    "part_ind4_29", /* 29 */
    "part_ind4_30", /* 30 */
    "part_ind4_31", /* 31 */
    "part_ind4_32", /* 32 */
    "part_ind4_33", /* 33 */
    "part_ind5_0", /* 34 */
    "part_ind5_1", /* 35 */
    "part_ind5_2", /* 36 */
    "part_ind5_3", /* 37 */
    "part_ind5_4", /* 38 */
    "part_ind5_5", /* 39 */
    "part_ind5_6", /* 40 */
    "part_ind5_7", /* 41 */
    "part_ind5_8", /* 42 */
    "part_ind5_9", /* 43 */
    "part_ind5_10", /* 44 */
    "part_ind5_11", /* 45 */
    "part_ind5_12", /* 46 */
    "part_ind5_13", /* 47 */
    "part_ind5_14", /* 48 */
    "part_ind5_15", /* 49 */
    "part_ind5_16", /* 50 */
    "part_ind5_17", /* 51 */
    "part_ind5_18", /* 52 */
    "part_ind5_19", /* 53 */
    "part_ind5_20", /* 54 */
    "part_ind5_21", /* 55 */
    "part_ind5_22", /* 56 */
    "part_ind5_23", /* 57 */
    "part_ind5_24", /* 58 */
    "part_ind5_25", /* 59 */
    "part_ind5_26", /* 60 */
    "part_ind5_27", /* 61 */
    "part_ind5_28", /* 62 */
    "part_ind5_29", /* 63 */
    "part_ind5_30", /* 64 */
    "part_ind5_31", /* 65 */
    "part_ind5_32", /* 66 */
    "part_ind5_33", /* 67 */
    "part_ind6_0", /* 68 */
    "part_ind6_1", /* 69 */
    "part_ind6_2", /* 70 */
    "part_ind6_3", /* 71 */
    "part_ind6_4", /* 72 */
    "part_ind6_5", /* 73 */
    "part_ind6_6", /* 74 */
    "part_ind6_7", /* 75 */
    "part_ind6_8", /* 76 */
    "part_ind6_9", /* 77 */
    "part_ind6_10", /* 78 */
    "part_ind6_11", /* 79 */
    "part_ind6_12", /* 80 */
    "part_ind6_13", /* 81 */
    "part_ind6_14", /* 82 */
    "part_ind6_15", /* 83 */
    "part_ind6_16", /* 84 */
    "part_ind6_17", /* 85 */
    "part_ind6_18", /* 86 */
    "part_ind6_19", /* 87 */
    "part_ind6_20", /* 88 */
    "part_ind6_21", /* 89 */
    "part_ind6_22", /* 90 */
    "part_ind6_23", /* 91 */
    "part_ind6_24", /* 92 */
    "part_ind6_25", /* 93 */
    "part_ind6_26", /* 94 */
    "part_ind6_27", /* 95 */
    "part_ind6_28", /* 96 */
    "part_ind6_29", /* 97 */
    "part_ind6_30", /* 98 */
    "part_ind7_0", /* 99 */
    "part_ind7_1", /* 100 */
    "part_ind7_2", /* 101 */
    "part_ind7_3", /* 102 */
    "part_ind7_4", /* 103 */
    "part_ind7_5", /* 104 */
    "part_ind7_6", /* 105 */
    "part_ind7_7", /* 106 */
    "part_ind7_8", /* 107 */
    "part_ind7_9", /* 108 */
    "part_ind7_10", /* 109 */
    "part_ind7_11", /* 110 */
    "part_ind7_12", /* 111 */
    "part_ind7_13", /* 112 */
    "part_ind7_14", /* 113 */
    "part_ind7_15", /* 114 */
    "part_ind7_16", /* 115 */
    "part_ind7_17", /* 116 */
    "part_ind7_18", /* 117 */
    "part_ind7_19", /* 118 */
    "part_ind7_20", /* 119 */
    "part_ind7_21", /* 120 */
    "part_ind7_22", /* 121 */
    "part_ind7_23", /* 122 */
    "part_ind7_24", /* 123 */
    "part_ind7_25", /* 124 */
    "part_ind7_26", /* 125 */
    "part_ind7_27", /* 126 */
    "part_ind7_28", /* 127 */
    "part_ind7_29", /* 128 */
    "part_ind7_30", /* 129 */
    "part_ind7_31", /* 130 */
    "part_ind7_32", /* 131 */
    "part_ind7_33", /* 132 */
    "part_ind8_0", /* 133 */
    "part_ind8_1", /* 134 */
    "part_ind8_2", /* 135 */
    "part_ind8_3", /* 136 */
    "part_ind8_4", /* 137 */
    "part_ind8_5", /* 138 */
    "part_ind8_6", /* 139 */
    "part_ind8_7", /* 140 */
    "part_ind8_8", /* 141 */
    "part_ind8_9", /* 142 */
    "part_ind8_10", /* 143 */
    "part_ind8_11", /* 144 */
    "part_ind8_12", /* 145 */
    "part_ind12_0", /* 146 */
    "part_ind12_1", /* 147 */
    "part_ind12_2", /* 148 */
    "part_ind12_3", /* 149 */
    "part_ind12_4", /* 150 */
    "part_ind12_5", /* 151 */
    "part_ind12_6", /* 152 */
    "part_ind12_7", /* 153 */
    "part_ind12_8", /* 154 */
    "part_ind12_9", /* 155 */
    "part_ind12_10", /* 156 */
    "part_ind12_11", /* 157 */
    "part_ind12_12", /* 158 */
    "part_ind25_0", /* 159 */
    "part_ind25_1", /* 160 */
    "part_ind25_2", /* 161 */
    "part_ind25_3", /* 162 */
    "part_ind25_4", /* 163 */
    "part_ind25_5", /* 164 */
    "part_ind25_6", /* 165 */
    "part_ind29_0", /* 166 */
    "part_ind29_1", /* 167 */
    "part_ind29_2", /* 168 */
    "part_ind29_3", /* 169 */
    "part_ind29_4", /* 170 */
    "part_ind29_5", /* 171 */
    "part_ind29_6", /* 172 */
    "part_ind29_7", /* 173 */
    "part_ind29_8", /* 174 */
    "part_ind29_9", /* 175 */
    "part_ind29_10", /* 176 */
    "part_ind29_11", /* 177 */
    "part_ind29_12", /* 178 */
    "part_ind29_13", /* 179 */
    "part_ind29_14", /* 180 */
    "part_ind29_15", /* 181 */
    "part_ind29_16", /* 182 */
    "part_ind29_17", /* 183 */
    "part_ind29_18", /* 184 */
    "part_ind29_19", /* 185 */
    "part_ind29_20", /* 186 */
    "part_ind29_21", /* 187 */
    "part_ind29_22", /* 188 */
    "part_ind29_23", /* 189 */
    "part_ind29_24", /* 190 */
    "part_ind29_25", /* 191 */
    "part_ind29_26", /* 192 */
    "part_ind29_27", /* 193 */
    "part_ind29_28", /* 194 */
    "part_ind29_29", /* 195 */
    "part_ind29_30", /* 196 */
    "part_ind29_31", /* 197 */
    "part_ind29_32", /* 198 */
    "part_ind29_33", /* 199 */
    "part_ind30_0", /* 200 */
    "part_ind30_1", /* 201 */
    "part_ind30_2", /* 202 */
    "part_ind30_3", /* 203 */
    "part_ind30_4", /* 204 */
    "part_ind30_5", /* 205 */
    "part_ind30_6", /* 206 */
};

static const int16_t s_rig_p00[] = { 778, 252, 777, 249, 776, 245, 778, 242, 780, 237, 801, 203, 806, 197, 810, 190, 819, 185, 829, 191, 839, 197, 881, 251, 892, 267, 903, 283, 925, 319, 926, 336, 928, 353, 916, 361, 907, 362, 899, 364, 867, 364, 862, 363, 856, 363, 851, 365, 849, 358, 847, 350, 841, 333, 820, 304, 802, 278, 779, 259, 778, 252, 778, 252, 778, 252, 778, 252, 666, 318, 677, 312, 700, 300, 705, 298, 710, 295, 720, 294, 725, 299, 729, 304, 745, 319, 759, 339, 773, 358, 790, 388, 790, 396, 790, 405, 785, 408, 781, 412, 776, 416, 750, 444, 748, 445, 746, 446, 745, 447, 745, 439, 745, 431, 741, 407, 722, 373, 703, 339, 667, 325, 665, 322, 664, 320, 666, 318, 666, 318, 666, 318, 666, 318, 666, 318, 726, 288, 726, 286, 726, 285, 729, 283, 731, 282, 754, 271, 763, 268, 773, 266, 774, 264, 793, 286, 813, 307, 829, 336, 835, 347, 840, 358, 837, 365, 834, 368, 831, 371, 810, 388, 807, 391, 804, 393, 799, 396, 797, 390, 794, 385, 787, 365, 766, 335, 744, 304, 726, 292, 726, 288, 726, 288, 726, 288, 726, 288, 564, 609, 554, 612, 528, 619, 510, 625, 488, 633, 480, 635, 475, 630, 469, 626, 467, 615, 473, 584, 479, 553, 502, 529, 541, 517, 580, 505, 627, 526, 647, 561, 666, 596, 661, 635, 641, 665, 620, 695, 585, 708, 551, 703, 517, 697, 499, 681, 495, 674, 491, 667, 491, 658, 498, 654, 504, 650, 545, 623, 564, 609, 564, 609, 564, 609, 564, 609, 559, 328, 562, 324, 569, 318, 577, 318, 586, 317, 596, 321, 601, 328, 601, 328, 559, 328, 559, 328, 559, 328, 559, 328, 559, 328, 471, 328, 474, 324, 481, 318, 490, 318, 499, 317, 509, 321, 514, 328, 514, 328, 471, 328, 471, 328, 471, 328, 471, 328, 471, 328, 620, 234, 620, 234, 603, 243, 590, 264, 578, 285, 580, 328, 580, 328, 386, 479, 375, 481, 349, 485, 330, 489, 307, 494, 299, 495, 294, 490, 289, 485, 288, 474, 298, 444, 307, 414, 333, 392, 372, 385, 412, 377, 457, 402, 473, 439, 489, 476, 480, 515, 456, 542, 432, 570, 396, 580, 362, 570, 329, 561, 314, 543, 310, 536, 306, 529, 308, 520, 315, 516, 321, 513, 365, 490, 386, 479, 386, 479, 386, 479, 386, 479, 530, 234, 530, 234, 512, 243, 500, 264, 487, 285, 490, 328, 490, 328 }; /* pose_f1 */
static const int16_t s_rig_p01[] = { 816, 292, 815, 288, 816, 285, 818, 282, 821, 278, 849, 248, 855, 243, 861, 238, 870, 235, 879, 243, 887, 250, 917, 313, 924, 331, 932, 348, 945, 388, 943, 405, 941, 422, 928, 427, 919, 427, 910, 426, 880, 419, 874, 418, 869, 417, 864, 417, 863, 410, 862, 402, 861, 384, 847, 351, 834, 322, 816, 299, 816, 292, 816, 292, 816, 292, 816, 292, 693, 332, 705, 329, 730, 322, 735, 320, 741, 319, 750, 320, 754, 326, 758, 332, 770, 350, 780, 372, 789, 394, 799, 427, 797, 435, 795, 443, 790, 445, 785, 448, 779, 452, 748, 473, 746, 474, 744, 475, 742, 475, 744, 467, 746, 459, 747, 436, 736, 398, 725, 361, 692, 339, 691, 336, 690, 334, 692, 333, 693, 332, 693, 332, 693, 332, 693, 332, 757, 316, 758, 314, 759, 313, 761, 312, 763, 311, 788, 305, 798, 304, 808, 304, 809, 303, 824, 328, 839, 353, 849, 385, 851, 396, 854, 408, 850, 414, 846, 416, 843, 419, 819, 432, 815, 433, 812, 435, 806, 437, 805, 431, 804, 425, 801, 404, 786, 370, 772, 335, 757, 320, 757, 316, 757, 316, 757, 316, 757, 316, 533, 641, 523, 645, 498, 656, 482, 664, 461, 675, 453, 678, 447, 675, 441, 671, 437, 660, 439, 629, 440, 598, 460, 570, 496, 553, 533, 536, 583, 549, 607, 581, 632, 612, 633, 652, 616, 685, 600, 717, 567, 736, 533, 735, 498, 734, 479, 721, 474, 715, 468, 709, 468, 700, 474, 695, 479, 690, 515, 657, 533, 641, 533, 641, 533, 641, 533, 641, 559, 328, 562, 324, 569, 318, 577, 318, 586, 317, 596, 321, 601, 328, 601, 328, 559, 328, 559, 328, 559, 328, 559, 328, 559, 328, 471, 328, 474, 324, 481, 318, 490, 318, 499, 317, 509, 321, 514, 328, 514, 328, 471, 328, 471, 328, 471, 328, 471, 328, 471, 328, 620, 234, 620, 234, 603, 243, 590, 264, 578, 285, 580, 328, 580, 328, 388, 453, 377, 452, 351, 451, 332, 451, 309, 451, 301, 450, 297, 444, 293, 438, 295, 427, 310, 400, 326, 373, 356, 357, 396, 358, 436, 359, 475, 394, 482, 433, 490, 472, 472, 508, 443, 530, 414, 552, 376, 553, 346, 537, 315, 520, 304, 500, 302, 492, 300, 484, 304, 476, 312, 474, 318, 472, 365, 459, 388, 453, 388, 453, 388, 453, 388, 453, 530, 234, 530, 234, 512, 243, 500, 264, 487, 285, 490, 328, 490, 328 }; /* pose_f2 */

#define S_RIG_POSE(arr) { .coords = (arr) }

static const gfx_sm_pose_t s_rig_poses[] = {
    S_RIG_POSE(s_rig_p00),  S_RIG_POSE(s_rig_p01),
};

static const gfx_sm_segment_t s_rig_segments[] = {
    /* part_ind4  body-right-wing  3×5=15 → ~5px@390 */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 0, .joint_count = 34, .stroke_width = 15 },
    /* part_ind5  right-arm        3×5=15 → ~5px@390 */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 34, .joint_count = 34, .stroke_width = 15 },
    /* part_ind6  right-shoulder   3×5=15 → ~5px@390 */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 68, .joint_count = 31, .stroke_width = 15 },
    /* part_ind7  main-body        3×5=15 → ~5px@390 */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 99, .joint_count = 34, .stroke_width = 15 },
    /* part_ind8  right-mouth      filled             */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 133, .joint_count = 13 },
    /* part_ind12 left-mouth       filled             */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 146, .joint_count = 13 },
    /* part_ind25 right-antenna    30 → ~10px@390      */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 159, .joint_count = 7, .stroke_width = 30 },
    /* part_ind29 left-body        15 → ~5px@390       */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 166, .joint_count = 34, .stroke_width = 15 },
    /* part_ind30 left-antenna     30 → ~10px@390      */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 200, .joint_count = 7, .stroke_width = 30 },
};

#define STEP(p, h, f, i) { .pose_index = (p), .hold_ticks = (h), .interp = (i), .facing = (f) }

static const gfx_sm_clip_step_t s_rig_steps_active[] = {
    STEP(0, 30, 1, GFX_SM_INTERP_DAMPED),
    STEP(1, 30, 1, GFX_SM_INTERP_DAMPED),
};

#define CLIP(n, cn, arr, lp) { .name = (n), .name_cn = (cn), .steps = (arr), .step_count = (uint8_t)(sizeof(arr) / sizeof((arr)[0])), .loop = (lp) }

static const gfx_sm_clip_t s_rig_clips[] = {
    CLIP("active", "动作", s_rig_steps_active, true),
};

static const uint16_t s_rig_sequence[] = { 0 };

static const gfx_sm_asset_t s_rig_scene_asset = {
    .meta           = &s_rig_meta,
    .joint_names    = s_rig_joint_names,
    .joint_count    = (uint8_t)(sizeof(s_rig_joint_names) / sizeof(s_rig_joint_names[0])),
    .segments       = s_rig_segments,
    .segment_count  = (uint8_t)(sizeof(s_rig_segments) / sizeof(s_rig_segments[0])),
    .poses          = s_rig_poses,
    .pose_count     = (uint16_t)(sizeof(s_rig_poses) / sizeof(s_rig_poses[0])),
    .clips          = s_rig_clips,
    .clip_count     = (uint16_t)(sizeof(s_rig_clips) / sizeof(s_rig_clips[0])),
    .sequence       = s_rig_sequence,
    .sequence_count = (uint16_t)(sizeof(s_rig_sequence) / sizeof(s_rig_sequence[0])),
    .layout         = &s_rig_layout,
};