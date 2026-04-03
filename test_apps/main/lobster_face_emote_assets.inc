/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "widget/gfx_lobster_face_emote.h"

static const gfx_lobster_face_emote_eye_shape_t s_lobster_ref_eye[] = {
    { { -16,  0,  -8,-16,   8,-16,  16,  0,   8, 16,  -8, 16, -16,  0 } },
    { { -16,  4,  -8,-10,   8,-10,  16,  4,   8, -4,  -8, -4, -16,  4 } },
    { { -16,  2,  -9,-14,   9,-14,  16,  2,   9,  8,  -9,  8, -16,  2 } },
    { { -16,  8,  -8,-10,   8,-14,  16, -2,   8, 16,  -8, 16, -16,  8 } },
    { { -14,  0,  -7,-24,   7,-24,  14,  0,   7, 24,  -7, 24, -14,  0 } },
    { { -16, -2,  -8, -8,   8,-18,  16,  6,   8, 10,  -8, 10, -16, -2 } },
};

static const gfx_lobster_face_emote_claw_shape_t s_lobster_ref_claw[] = {
    { {  16,  0,   6,-12, -20,-22, -48,-10, -38,  6, -18, 20,  16,  0 } },
    { {  18, -2,   8,-16, -24,-26, -54,-16, -44, 10, -18, 24,  18, -2 } },
    { {  20, -6,   6,-20, -30,-30, -60,-18, -48, 14, -18, 28,  20, -6 } },
    { {  14,  8,   4, -2, -18,-10, -40, -4, -30, 16, -12, 26,  14,  8 } },
    { {  22, -8,   6,-26, -34,-38, -68,-22, -54, 18, -18, 34,  22, -8 } },
    { {  18,  6,   4,-10, -18,-22, -52,-24, -40, -2, -10, 14,  18,  6 } },
};

static const gfx_lobster_face_emote_expr_t s_lobster_face_sequence[] = {
    { "neutral",  "平静",   0,   0,   0,   0,   0,   0,   0, 40U },
    { "smile",    "友好", 100,   0,   0,   0,   0,   0,   0, 30U },
    { "happy",    "开心",  20, 100,   0,   0,   0,   0,  -6, 30U },
    { "sad",      "委屈",   0,   0, 100,   0,   0,  -6,   8, 35U },
    { "surprise", "惊讶",   0,   0,   0, 100,   0,   0, -12, 28U },
    { "angry",    "生气",   0,   0,   0,   0, 100,   0,   0, 30U },
    { "look left","左看",  20,   0,   0,   0,   0, -18,  -2, 30U },
    { "look right","右看", 20,   0,   0,   0,   0,  18,  -2, 30U },
};

static const gfx_lobster_face_emote_assets_t s_lobster_face_assets = {
    .ref_eye = s_lobster_ref_eye,
    .ref_eye_count = sizeof(s_lobster_ref_eye) / sizeof(s_lobster_ref_eye[0]),
    .ref_claw = s_lobster_ref_claw,
    .ref_claw_count = sizeof(s_lobster_ref_claw) / sizeof(s_lobster_ref_claw[0]),
    .sequence = s_lobster_face_sequence,
    .sequence_count = sizeof(s_lobster_face_sequence) / sizeof(s_lobster_face_sequence[0]),
};
