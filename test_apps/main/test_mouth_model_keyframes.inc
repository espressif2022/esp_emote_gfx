/* Auto-generated file. Do not edit manually. */
/* Source: /home/xuxin/esp_work/esp_emote_gfx/scripts/face_keyframes_cute.json */

static const face_eye_kf_t s_ref_eye[] = {
    /* [0] neutral */ { 28,  24,   0 },
    /* [1] smile   */ { 18,  21,  -2 },
    /* [2] happy   */ {  9,  17,  -2 },
    /* [3] sad     */ { 26,  22,   7 },
    /* [4] surprise */ { 40,  32,   0 },
    /* [5] angry   */ { 26,  22,  -6 },
};

static const face_brow_kf_t s_ref_brow[] = {
    /* [0] neutral */ {  4,   0,   0 },
    /* [1] smile   */ {  2,   1,   0 },
    /* [2] happy   */ {  1,   2,  -1 },
    /* [3] sad     */ {  4,  -1,   7 },
    /* [4] surprise */ {  7,   7,   0 },
    /* [5] angry   */ { -4,  -3,  -6 },
};

static const face_mouth_kf_t s_ref_mouth[] = {
    /* [0] neutral */ {   0,  2,   0,   0,   0,   4,   0 },
    /* [1] smile   */ {  22,  2,   6,   4,  10,  10,  -4 },
    /* [2] happy   */ {  42, 10,  10,   8,  20,   6,  -3 },
    /* [3] sad     */ { -36,  4,   4, -10,  -4,  -8,   8 },
    /* [4] surprise */ {   0, 30, -14,  -2,   8,  14,   0 },
    /* [5] angry   */ { -28, 10,   0,  -4,   2,   2,   6 },
};

static const face_emotion_t s_face_sequence[] = {
/*   name            Sm    Hp    Sd    Su    An   hold */
    { "neutral",    0,    0,    0,    0,    0,  28U },
    { "smile",  100,    0,    0,    0,    0,  28U },
    { "happy",    0,  100,    0,    0,    0,  28U },
    { "grinning",   50,  100,    0,    0,    0,  24U },
    { "loving",   80,   20,    0,    0,    0,  28U },
    { "neutral",    0,    0,    0,    0,    0,  20U },
    { "sad",    0,    0,  100,    0,    0,  28U },
    { "crying",    0,    0,  100,    0,    0,  20U },
    { "sobbing",    0,    0,  120,    0,    0,  18U },
    { "horrified",    0,    0,   60,  100,    0,  24U },
    { "neutral",    0,    0,    0,    0,    0,  20U },
    { "surprise",    0,    0,    0,  100,    0,  24U },
    { "shocked",    0,    0,    0,  120,    0,  24U },
    { "excited",   60,  100,    0,   50,    0,  24U },
    { "laughing",   20,  100,    0,   40,    0,  20U },
    { "neutral",    0,    0,    0,    0,    0,  20U },
    { "angry",    0,    0,    0,    0,  100,  24U },
    { "thinking",    0,    0,   20,    0,   30,  28U },
    { "confused",    0,    0,   30,   20,   20,  28U },
    { "sympathetic",   30,    0,   50,    0,    0,  28U },
    { "sheepish",   50,    0,   30,    0,    0,  24U },
    { "amused",   60,    0,    0,   30,    0,  24U },
    { "relaxed",   40,    0,    0,    0,    0,  28U },
    { "neutral",    0,    0,    0,    0,    0,  28U },
};
