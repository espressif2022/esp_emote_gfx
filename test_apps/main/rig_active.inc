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
    .stroke_width    = 20,
    .mirror_x        = 0,
    .ground_y        = 900,
    .timer_period_ms = 33,
    .damping_div     = 4,
};

static const char *const s_rig_joint_names[] = {
    "right_wing_0", /* 0 */
    "right_wing_1", /* 1 */
    "right_wing_2", /* 2 */
    "right_wing_3", /* 3 */
    "right_wing_4", /* 4 */
    "right_wing_5", /* 5 */
    "right_wing_6", /* 6 */
    "right_wing_7", /* 7 */
    "right_wing_8", /* 8 */
    "right_wing_9", /* 9 */
    "right_wing_10", /* 10 */
    "right_wing_11", /* 11 */
    "right_wing_12", /* 12 */
    "right_wing_13", /* 13 */
    "right_wing_14", /* 14 */
    "right_wing_15", /* 15 */
    "right_wing_16", /* 16 */
    "right_wing_17", /* 17 */
    "right_wing_18", /* 18 */
    "right_wing_19", /* 19 */
    "right_wing_20", /* 20 */
    "right_wing_21", /* 21 */
    "right_wing_22", /* 22 */
    "right_wing_23", /* 23 */
    "right_wing_24", /* 24 */
    "right_wing_25", /* 25 */
    "right_wing_26", /* 26 */
    "right_wing_27", /* 27 */
    "right_wing_28", /* 28 */
    "right_wing_29", /* 29 */
    "right_wing_30", /* 30 */
    "right_wing_31", /* 31 */
    "right_wing_32", /* 32 */
    "right_wing_33", /* 33 */
    "right_arm_0", /* 34 */
    "right_arm_1", /* 35 */
    "right_arm_2", /* 36 */
    "right_arm_3", /* 37 */
    "right_arm_4", /* 38 */
    "right_arm_5", /* 39 */
    "right_arm_6", /* 40 */
    "right_arm_7", /* 41 */
    "right_arm_8", /* 42 */
    "right_arm_9", /* 43 */
    "right_arm_10", /* 44 */
    "right_arm_11", /* 45 */
    "right_arm_12", /* 46 */
    "right_arm_13", /* 47 */
    "right_arm_14", /* 48 */
    "right_arm_15", /* 49 */
    "right_arm_16", /* 50 */
    "right_arm_17", /* 51 */
    "right_arm_18", /* 52 */
    "right_arm_19", /* 53 */
    "right_arm_20", /* 54 */
    "right_arm_21", /* 55 */
    "right_arm_22", /* 56 */
    "right_arm_23", /* 57 */
    "right_arm_24", /* 58 */
    "right_arm_25", /* 59 */
    "right_arm_26", /* 60 */
    "right_arm_27", /* 61 */
    "right_arm_28", /* 62 */
    "right_arm_29", /* 63 */
    "right_arm_30", /* 64 */
    "right_arm_31", /* 65 */
    "right_arm_32", /* 66 */
    "right_arm_33", /* 67 */
    "right_shoulder_0", /* 68 */
    "right_shoulder_1", /* 69 */
    "right_shoulder_2", /* 70 */
    "right_shoulder_3", /* 71 */
    "right_shoulder_4", /* 72 */
    "right_shoulder_5", /* 73 */
    "right_shoulder_6", /* 74 */
    "right_shoulder_7", /* 75 */
    "right_shoulder_8", /* 76 */
    "right_shoulder_9", /* 77 */
    "right_shoulder_10", /* 78 */
    "right_shoulder_11", /* 79 */
    "right_shoulder_12", /* 80 */
    "right_shoulder_13", /* 81 */
    "right_shoulder_14", /* 82 */
    "right_shoulder_15", /* 83 */
    "right_shoulder_16", /* 84 */
    "right_shoulder_17", /* 85 */
    "right_shoulder_18", /* 86 */
    "right_shoulder_19", /* 87 */
    "right_shoulder_20", /* 88 */
    "right_shoulder_21", /* 89 */
    "right_shoulder_22", /* 90 */
    "right_shoulder_23", /* 91 */
    "right_shoulder_24", /* 92 */
    "right_shoulder_25", /* 93 */
    "right_shoulder_26", /* 94 */
    "right_shoulder_27", /* 95 */
    "right_shoulder_28", /* 96 */
    "right_shoulder_29", /* 97 */
    "right_shoulder_30", /* 98 */
    "main_body_0", /* 99 */
    "main_body_1", /* 100 */
    "main_body_2", /* 101 */
    "main_body_3", /* 102 */
    "main_body_4", /* 103 */
    "main_body_5", /* 104 */
    "main_body_6", /* 105 */
    "main_body_7", /* 106 */
    "main_body_8", /* 107 */
    "main_body_9", /* 108 */
    "main_body_10", /* 109 */
    "main_body_11", /* 110 */
    "main_body_12", /* 111 */
    "main_body_13", /* 112 */
    "main_body_14", /* 113 */
    "main_body_15", /* 114 */
    "main_body_16", /* 115 */
    "main_body_17", /* 116 */
    "main_body_18", /* 117 */
    "main_body_19", /* 118 */
    "main_body_20", /* 119 */
    "main_body_21", /* 120 */
    "main_body_22", /* 121 */
    "main_body_23", /* 122 */
    "main_body_24", /* 123 */
    "main_body_25", /* 124 */
    "main_body_26", /* 125 */
    "main_body_27", /* 126 */
    "main_body_28", /* 127 */
    "main_body_29", /* 128 */
    "main_body_30", /* 129 */
    "main_body_31", /* 130 */
    "main_body_32", /* 131 */
    "main_body_33", /* 132 */
    "right_claw_0", /* 133 */
    "right_claw_1", /* 134 */
    "right_claw_2", /* 135 */
    "right_claw_3", /* 136 */
    "right_claw_4", /* 137 */
    "right_claw_5", /* 138 */
    "right_claw_6", /* 139 */
    "right_claw_7", /* 140 */
    "right_claw_8", /* 141 */
    "right_claw_9", /* 142 */
    "right_claw_10", /* 143 */
    "right_claw_11", /* 144 */
    "right_claw_12", /* 145 */
    "left_claw_0", /* 146 */
    "left_claw_1", /* 147 */
    "left_claw_2", /* 148 */
    "left_claw_3", /* 149 */
    "left_claw_4", /* 150 */
    "left_claw_5", /* 151 */
    "left_claw_6", /* 152 */
    "left_claw_7", /* 153 */
    "left_claw_8", /* 154 */
    "left_claw_9", /* 155 */
    "left_claw_10", /* 156 */
    "left_claw_11", /* 157 */
    "left_claw_12", /* 158 */
    "right_antenna_0", /* 159 */
    "right_antenna_1", /* 160 */
    "right_antenna_2", /* 161 */
    "right_antenna_3", /* 162 */
    "left_antenna_0", /* 163 */
    "left_antenna_1", /* 164 */
    "left_antenna_2", /* 165 */
    "left_antenna_3", /* 166 */
    "left_body_0", /* 167 */
    "left_body_1", /* 168 */
    "left_body_2", /* 169 */
    "left_body_3", /* 170 */
    "left_body_4", /* 171 */
    "left_body_5", /* 172 */
    "left_body_6", /* 173 */
    "left_body_7", /* 174 */
    "left_body_8", /* 175 */
    "left_body_9", /* 176 */
    "left_body_10", /* 177 */
    "left_body_11", /* 178 */
    "left_body_12", /* 179 */
    "left_body_13", /* 180 */
    "left_body_14", /* 181 */
    "left_body_15", /* 182 */
    "left_body_16", /* 183 */
    "left_body_17", /* 184 */
    "left_body_18", /* 185 */
    "left_body_19", /* 186 */
    "left_body_20", /* 187 */
    "left_body_21", /* 188 */
    "left_body_22", /* 189 */
    "left_body_23", /* 190 */
    "left_body_24", /* 191 */
    "left_body_25", /* 192 */
    "left_body_26", /* 193 */
    "left_body_27", /* 194 */
    "left_body_28", /* 195 */
    "left_body_29", /* 196 */
    "left_body_30", /* 197 */
    "left_body_31", /* 198 */
    "left_body_32", /* 199 */
    "left_body_33", /* 200 */
    "head_circle_0", /* 201 */
    "head_circle_1", /* 202 */
    "head_circle_2", /* 203 */
    "head_circle_3", /* 204 */
    "head_circle_4", /* 205 */
    "head_circle_5", /* 206 */
    "head_circle_6", /* 207 */
    "head_circle_7", /* 208 */
    "head_circle_8", /* 209 */
    "head_circle_9", /* 210 */
    "head_circle_10", /* 211 */
    "head_circle_11", /* 212 */
    "head_circle_12", /* 213 */
    "eye_r_sclera_0", /* 214 */
    "eye_r_sclera_1", /* 215 */
    "eye_r_sclera_2", /* 216 */
    "eye_r_sclera_3", /* 217 */
    "eye_r_sclera_4", /* 218 */
    "eye_r_sclera_5", /* 219 */
    "eye_r_sclera_6", /* 220 */
    "eye_r_sclera_7", /* 221 */
    "eye_r_sclera_8", /* 222 */
    "eye_r_sclera_9", /* 223 */
    "eye_r_sclera_10", /* 224 */
    "eye_r_sclera_11", /* 225 */
    "eye_r_sclera_12", /* 226 */
    "eye_l_sclera_0", /* 227 */
    "eye_l_sclera_1", /* 228 */
    "eye_l_sclera_2", /* 229 */
    "eye_l_sclera_3", /* 230 */
    "eye_l_sclera_4", /* 231 */
    "eye_l_sclera_5", /* 232 */
    "eye_l_sclera_6", /* 233 */
    "eye_l_sclera_7", /* 234 */
    "eye_l_sclera_8", /* 235 */
    "eye_l_sclera_9", /* 236 */
    "eye_l_sclera_10", /* 237 */
    "eye_l_sclera_11", /* 238 */
    "eye_l_sclera_12", /* 239 */
    "eye_r_pupil_0", /* 240 */
    "eye_r_pupil_1", /* 241 */
    "eye_r_pupil_2", /* 242 */
    "eye_r_pupil_3", /* 243 */
    "eye_r_pupil_4", /* 244 */
    "eye_r_pupil_5", /* 245 */
    "eye_r_pupil_6", /* 246 */
    "eye_r_pupil_7", /* 247 */
    "eye_r_pupil_8", /* 248 */
    "eye_r_pupil_9", /* 249 */
    "eye_r_pupil_10", /* 250 */
    "eye_r_pupil_11", /* 251 */
    "eye_r_pupil_12", /* 252 */
    "eye_l_pupil_0", /* 253 */
    "eye_l_pupil_1", /* 254 */
    "eye_l_pupil_2", /* 255 */
    "eye_l_pupil_3", /* 256 */
    "eye_l_pupil_4", /* 257 */
    "eye_l_pupil_5", /* 258 */
    "eye_l_pupil_6", /* 259 */
    "eye_l_pupil_7", /* 260 */
    "eye_l_pupil_8", /* 261 */
    "eye_l_pupil_9", /* 262 */
    "eye_l_pupil_10", /* 263 */
    "eye_l_pupil_11", /* 264 */
    "eye_l_pupil_12", /* 265 */
    "foot1_0", /* 266 */
    "foot1_1", /* 267 */
    "foot1_2", /* 268 */
    "foot1_3", /* 269 */
    "foot1_4", /* 270 */
    "foot1_5", /* 271 */
    "foot1_6", /* 272 */
    "foot1_7", /* 273 */
    "foot1_8", /* 274 */
    "foot1_9", /* 275 */
    "foot1_10", /* 276 */
    "foot1_11", /* 277 */
    "foot1_12", /* 278 */
    "foot2_0", /* 279 */
    "foot2_1", /* 280 */
    "foot2_2", /* 281 */
    "foot2_3", /* 282 */
    "foot2_4", /* 283 */
    "foot2_5", /* 284 */
    "foot2_6", /* 285 */
    "foot2_7", /* 286 */
    "foot2_8", /* 287 */
    "foot2_9", /* 288 */
    "foot2_10", /* 289 */
    "foot2_11", /* 290 */
    "foot2_12", /* 291 */
    "foot3_0", /* 292 */
    "foot3_1", /* 293 */
    "foot3_2", /* 294 */
    "foot3_3", /* 295 */
    "foot3_4", /* 296 */
    "foot3_5", /* 297 */
    "foot3_6", /* 298 */
    "foot3_7", /* 299 */
    "foot3_8", /* 300 */
    "foot3_9", /* 301 */
    "foot3_10", /* 302 */
    "foot3_11", /* 303 */
    "foot3_12", /* 304 */
};

static const int16_t s_rig_p00[] = { 778, 252, 777, 249, 776, 245, 778, 242, 780, 237, 801, 203, 806, 197, 810, 190, 819, 185, 829, 191, 839, 197, 881, 251, 892, 267, 903, 283, 925, 319, 926, 336, 928, 353, 916, 361, 907, 362, 899, 364, 867, 364, 862, 363, 856, 363, 851, 365, 849, 358, 847, 350, 841, 333, 820, 304, 802, 278, 779, 259, 778, 252, 778, 252, 778, 252, 778, 252, 666, 318, 677, 312, 700, 300, 705, 298, 710, 295, 720, 294, 725, 299, 729, 304, 745, 319, 759, 339, 773, 358, 790, 388, 790, 396, 790, 405, 785, 408, 781, 412, 776, 416, 750, 444, 748, 445, 746, 446, 745, 447, 745, 439, 745, 431, 741, 407, 722, 373, 703, 339, 667, 325, 665, 322, 664, 320, 666, 318, 666, 318, 666, 318, 666, 318, 666, 318, 726, 288, 726, 286, 726, 285, 729, 283, 731, 282, 754, 271, 763, 268, 773, 266, 774, 264, 793, 286, 813, 307, 829, 336, 835, 347, 840, 358, 837, 365, 834, 368, 831, 371, 810, 388, 807, 391, 804, 393, 799, 396, 797, 390, 794, 385, 787, 365, 766, 335, 744, 304, 726, 292, 726, 288, 726, 288, 726, 288, 726, 288, 564, 609, 554, 612, 528, 619, 510, 625, 488, 633, 480, 635, 475, 630, 469, 626, 467, 615, 473, 584, 479, 553, 502, 529, 541, 517, 580, 505, 627, 526, 647, 561, 666, 596, 661, 635, 641, 665, 620, 695, 585, 708, 551, 703, 517, 697, 499, 681, 495, 674, 491, 667, 491, 658, 498, 654, 504, 650, 545, 623, 564, 609, 564, 609, 564, 609, 564, 609, 559, 328, 562, 324, 569, 318, 577, 318, 586, 317, 596, 321, 601, 328, 601, 328, 559, 328, 559, 328, 559, 328, 559, 328, 559, 328, 471, 328, 474, 324, 481, 318, 490, 318, 499, 317, 509, 321, 514, 328, 514, 328, 471, 328, 471, 328, 471, 328, 471, 328, 471, 328, 620, 234, 610, 244, 603, 243, 590, 264, 530, 234, 520, 244, 512, 243, 500, 264, 386, 479, 375, 481, 349, 485, 330, 489, 307, 494, 299, 495, 294, 490, 289, 485, 288, 474, 298, 444, 307, 414, 333, 392, 372, 385, 412, 377, 457, 402, 473, 439, 489, 476, 480, 515, 456, 542, 432, 570, 396, 580, 362, 570, 329, 561, 314, 543, 310, 536, 306, 529, 308, 520, 315, 516, 321, 513, 365, 490, 386, 479, 386, 479, 386, 479, 386, 479, 589, 307, 671, 307, 738, 374, 738, 456, 738, 538, 671, 605, 589, 605, 507, 605, 440, 538, 440, 456, 440, 374, 507, 307, 589, 307, 579, 319, 598, 319, 613, 341, 613, 367, 613, 394, 598, 416, 579, 416, 561, 416, 546, 394, 546, 367, 546, 341, 561, 319, 579, 319, 492, 319, 511, 319, 526, 341, 526, 367, 526, 394, 511, 416, 492, 416, 474, 416, 459, 394, 459, 367, 459, 341, 474, 319, 492, 319, 562, 370, 567, 370, 571, 374, 571, 379, 571, 384, 567, 388, 562, 388, 557, 388, 553, 384, 553, 379, 553, 374, 557, 370, 562, 370, 471, 366, 476, 366, 480, 370, 480, 375, 480, 380, 476, 384, 471, 384, 466, 384, 462, 380, 462, 375, 462, 370, 466, 366, 471, 366, 759, 483, 768, 483, 776, 491, 776, 500, 776, 510, 768, 517, 759, 517, 750, 517, 743, 510, 743, 500, 743, 491, 750, 483, 759, 483, 746, 523, 755, 523, 763, 530, 763, 540, 763, 549, 755, 557, 746, 557, 737, 557, 730, 549, 730, 540, 730, 530, 737, 523, 746, 523, 719, 553, 728, 553, 735, 561, 735, 570, 735, 580, 728, 587, 719, 587, 710, 587, 702, 580, 702, 570, 702, 561, 710, 553, 719, 553 }; /* pose_f1 */
static const int16_t s_rig_p01[] = { 816, 292, 815, 288, 816, 285, 818, 282, 821, 278, 849, 248, 855, 243, 861, 238, 870, 235, 879, 243, 887, 250, 917, 313, 924, 331, 932, 348, 945, 388, 943, 405, 941, 422, 928, 427, 919, 427, 910, 426, 880, 419, 874, 418, 869, 417, 864, 417, 863, 410, 862, 402, 861, 384, 847, 351, 834, 322, 816, 299, 816, 292, 816, 292, 816, 292, 816, 292, 693, 332, 705, 329, 730, 322, 735, 320, 741, 319, 750, 320, 754, 326, 758, 332, 770, 350, 780, 372, 789, 394, 799, 427, 797, 435, 795, 443, 790, 445, 785, 448, 779, 452, 748, 473, 746, 474, 744, 475, 742, 475, 744, 467, 746, 459, 747, 436, 736, 398, 725, 361, 692, 339, 691, 336, 690, 334, 692, 333, 693, 332, 693, 332, 693, 332, 693, 332, 757, 316, 758, 314, 759, 313, 761, 312, 763, 311, 788, 305, 798, 304, 808, 304, 809, 303, 824, 328, 839, 353, 849, 385, 851, 396, 854, 408, 850, 414, 846, 416, 843, 419, 819, 432, 815, 433, 812, 435, 806, 437, 805, 431, 804, 425, 801, 404, 786, 370, 772, 335, 757, 320, 757, 316, 757, 316, 757, 316, 757, 316, 533, 641, 523, 645, 498, 656, 482, 664, 461, 675, 453, 678, 447, 675, 441, 671, 437, 660, 439, 629, 440, 598, 460, 570, 496, 553, 533, 536, 583, 549, 607, 581, 632, 612, 633, 652, 616, 685, 600, 717, 567, 736, 533, 735, 498, 734, 479, 721, 474, 715, 468, 709, 468, 700, 474, 695, 479, 690, 515, 657, 533, 641, 533, 641, 533, 641, 533, 641, 559, 328, 562, 324, 569, 318, 577, 318, 586, 317, 596, 321, 601, 328, 601, 328, 559, 328, 559, 328, 559, 328, 559, 328, 559, 328, 471, 328, 474, 324, 481, 318, 490, 318, 499, 317, 509, 321, 514, 328, 514, 328, 471, 328, 471, 328, 471, 328, 471, 328, 471, 328, 620, 234, 610, 244, 603, 243, 590, 264, 530, 234, 520, 244, 512, 243, 500, 264, 388, 453, 377, 452, 351, 451, 332, 451, 309, 451, 301, 450, 297, 444, 293, 438, 295, 427, 310, 400, 326, 373, 356, 357, 396, 358, 436, 359, 475, 394, 482, 433, 490, 472, 472, 508, 443, 530, 414, 552, 376, 553, 346, 537, 315, 520, 304, 500, 302, 492, 300, 484, 304, 476, 312, 474, 318, 472, 365, 459, 388, 453, 388, 453, 388, 453, 388, 453, 589, 307, 671, 307, 738, 374, 738, 456, 738, 538, 671, 605, 589, 605, 507, 605, 440, 538, 440, 456, 440, 374, 507, 307, 589, 307, 579, 319, 598, 319, 613, 341, 613, 367, 613, 394, 598, 416, 579, 416, 561, 416, 546, 394, 546, 367, 546, 341, 561, 319, 579, 319, 492, 319, 511, 319, 526, 341, 526, 367, 526, 394, 511, 416, 492, 416, 474, 416, 459, 394, 459, 367, 459, 341, 474, 319, 492, 319, 562, 370, 567, 370, 571, 374, 571, 379, 571, 384, 567, 388, 562, 388, 557, 388, 553, 384, 553, 379, 553, 374, 557, 370, 562, 370, 471, 366, 476, 366, 480, 370, 480, 375, 480, 380, 476, 384, 471, 384, 466, 384, 462, 380, 462, 375, 462, 370, 466, 366, 471, 366, 745, 515, 754, 515, 761, 523, 761, 532, 761, 541, 754, 549, 745, 549, 736, 549, 728, 541, 728, 532, 728, 523, 736, 515, 745, 515, 726, 552, 735, 552, 742, 559, 742, 569, 742, 578, 735, 586, 726, 586, 717, 586, 709, 578, 709, 569, 709, 559, 717, 552, 726, 552, 694, 578, 703, 578, 710, 585, 710, 595, 710, 604, 703, 612, 694, 612, 685, 612, 677, 604, 677, 595, 677, 585, 685, 578, 694, 578 }; /* pose_f2 */

#define S_RIG_POSE(arr) { .coords = (arr) }

static const gfx_sm_pose_t s_rig_poses[] = {
    S_RIG_POSE(s_rig_p00),  S_RIG_POSE(s_rig_p01), S_RIG_POSE(s_rig_p00),
};

static const gfx_sm_segment_t s_rig_segments[] = {
    /* right_wing */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 0, .joint_count = 34, .stroke_width = 10 },
    /* right_arm */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 34, .joint_count = 34, .stroke_width = 10 },
    /* right_shoulder */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 68, .joint_count = 31, .stroke_width = 10 },
    /* main_body */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 99, .joint_count = 34, .stroke_width = 10 },
    /* right_claw */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 133, .joint_count = 13 },
    /* left_claw */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 146, .joint_count = 13 },
    /* right_antenna */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 159, .joint_count = 4, .stroke_width = 20 },
    /* left_antenna */ { .kind = GFX_SM_SEG_BEZIER_STRIP, .joint_a = 163, .joint_count = 4, .stroke_width = 20 },
    /* left_body */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 167, .joint_count = 34, .stroke_width = 10 },
    /* head_circle */ { .kind = GFX_SM_SEG_BEZIER_LOOP, .joint_a = 201, .joint_count = 13, .stroke_width = 15 },
    /* eye_r_sclera */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 214, .joint_count = 13 },
    /* eye_l_sclera */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 227, .joint_count = 13 },
    /* eye_r_pupil */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 240, .joint_count = 13 },
    /* eye_l_pupil */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 253, .joint_count = 13 },
    /* foot1 */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 266, .joint_count = 13 },
    /* foot2 */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 279, .joint_count = 13 },
    /* foot3 */ { .kind = GFX_SM_SEG_BEZIER_FILL, .joint_a = 292, .joint_count = 13 },
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
    .joint_count    = (uint16_t)(sizeof(s_rig_joint_names) / sizeof(s_rig_joint_names[0])),
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