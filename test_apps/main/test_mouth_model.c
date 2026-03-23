/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_mouth_model";

#define TEST_MOUTH_MODEL_GRID_COLS        14U
#define TEST_MOUTH_MODEL_GRID_ROWS        6U
#define TEST_MOUTH_MODEL_POINT_COUNT      ((TEST_MOUTH_MODEL_GRID_COLS + 1U) * (TEST_MOUTH_MODEL_GRID_ROWS + 1U))
#define TEST_MOUTH_MODEL_TIMER_PERIOD_MS  33U
#define TEST_MOUTH_MODEL_SMILE_LIMIT      42
#define TEST_MOUTH_MODEL_OPEN_LIMIT       40
#define TEST_MOUTH_MODEL_WIDTH_LIMIT      26
#define TEST_MOUTH_MODEL_CENTER_Y_OFS     50
#define TEST_MOUTH_MODEL_MODE_THRESH      12
#define TEST_MOUTH_MODEL_DENSE_SEGMENTS   10U
#define TEST_MOUTH_MODEL_DENSE_POINT_COUNT (TEST_MOUTH_MODEL_DENSE_SEGMENTS + 1U)
#define TEST_MOUTH_MODEL_SAD_THRESH       10
#define TEST_MOUTH_MODEL_SMILE_THRESH     10
#define TEST_MOUTH_MODEL_SURPRISE_OPEN    24
#define TEST_MOUTH_MODEL_EYE_SEGS         8U
#define TEST_MOUTH_MODEL_EYE_PT_COUNT     (TEST_MOUTH_MODEL_EYE_SEGS + 1U)
#define TEST_MOUTH_MODEL_EYE_RX           26      /* x-radius → 52 px wide per eye */
#define TEST_MOUTH_MODEL_EYE_RY           22      /* y-radius → up to 44 px tall   */
#define TEST_MOUTH_MODEL_LEFT_EYE_X_OFS   (-52)
#define TEST_MOUTH_MODEL_RIGHT_EYE_X_OFS  52
#define TEST_MOUTH_MODEL_EYE_Y_OFS        (-58)   /* px above mouth centre → more breathing room */
#define TEST_MOUTH_MODEL_FRIGHT_OPEN      36      /* open threshold for frightened vs surprised  */
#define TEST_MOUTH_MODEL_BROW_SEGS        6U
#define TEST_MOUTH_MODEL_BROW_PT_COUNT    (TEST_MOUTH_MODEL_BROW_SEGS + 1U)
#define TEST_MOUTH_MODEL_BROW_HALF_W      (TEST_MOUTH_MODEL_EYE_RX + 2)  /* slightly wider than eye */
#define TEST_MOUTH_MODEL_BROW_THICKNESS   4                              /* half-thickness in px    */
#define TEST_MOUTH_MODEL_BROW_Y_OFS       (-32)                          /* px above eye centre     */

typedef enum {
    TEST_MOUTH_MODEL_MODE_FLAT = 0,
    TEST_MOUTH_MODEL_MODE_SMILE,
    TEST_MOUTH_MODEL_MODE_OPEN,
} test_mouth_model_mode_t;

typedef enum {
    TEST_MOUTH_MODEL_SHAPE_IDLE = 0,
    TEST_MOUTH_MODEL_SHAPE_SAD_N,
    TEST_MOUTH_MODEL_SHAPE_SMILE_U,
    TEST_MOUTH_MODEL_SHAPE_SURPRISE_O,
} test_mouth_model_shape_t;

typedef enum {
    TEST_MOUTH_MODEL_BONE_LEFT_CORNER = 0,
    TEST_MOUTH_MODEL_BONE_RIGHT_CORNER,
    TEST_MOUTH_MODEL_BONE_UPPER_LIP,
    TEST_MOUTH_MODEL_BONE_LOWER_LIP,
    TEST_MOUTH_MODEL_BONE_CENTER,
    TEST_MOUTH_MODEL_BONE_COUNT,
} test_mouth_model_bone_id_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
} test_mouth_model_bone_pose_t;

typedef struct {
    const char *name;
    int16_t smile;
    int16_t open;
    uint16_t hold_ticks;
} test_mouth_model_clip_t;

typedef struct {
    gfx_obj_t *mouth_obj;
    gfx_obj_t *upper_lip_obj;
    gfx_obj_t *lower_lip_obj;
    gfx_obj_t *left_corner_obj;
    gfx_obj_t *right_corner_obj;
    gfx_obj_t *left_eye_obj;
    gfx_obj_t *right_eye_obj;
    gfx_obj_t *left_brow_obj;
    gfx_obj_t *right_brow_obj;
    gfx_obj_t *title_label;
    gfx_obj_t *hint_label;
    gfx_timer_handle_t anim_timer;
    gfx_mesh_img_point_t base_points[TEST_MOUTH_MODEL_POINT_COUNT];
    gfx_coord_t mouth_origin_x;
    gfx_coord_t mouth_origin_y;
    gfx_coord_t left_eye_cx;
    gfx_coord_t left_eye_cy;
    gfx_coord_t right_eye_cx;
    gfx_coord_t right_eye_cy;
    int16_t current_smile;
    int16_t current_open;
    int16_t target_smile;
    int16_t target_open;
    uint16_t hold_tick;
    uint32_t anim_tick;
    size_t clip_idx;
    test_mouth_model_mode_t mouth_mode;
} test_mouth_model_scene_t;

/*
 * Mouth shape reference (7 upper + 7 lower control points shown as '·'):
 *
 *  Idle  '—'                      Sad  'n'                 Smile  'U'                Surprise  'O'
 *  smile≈0, open<24               smile≤-10               smile≥+10                 open≥24
 *
 *  · · · · · · ·                  · ·         · ·          · ·           · ·         · · · · · · ·
 *  · · · · · · ·                      · · · ·                  · · · · ·            ·             ·
 *                                                                                    · · · · · · ·
 *
 * Clip parameters:
 *   smile : [-42 .. +42]   negative = frown/sad, positive = happy/smile
 *   open  : [  0 .. +40]   mouth opening depth
 *   hold  : frames to hold after reaching the target pose (1 frame ≈ 33 ms)
 */
/*
 * Every emotion follows the pattern:
 *   {"emote", 0, 2, 5U}          -- enter  : brief rest at neutral before the expression
 *   {"emote", smile, open, holdU} -- peak   : the actual expression (one or more clips)
 *   {"emote", 0, 2, 5U}          -- exit   : rest at neutral before cycling on
 *
 * Naming the enter/exit clips with the emotion name keeps the bottom label
 * displaying the emotion context even during transitions.
 *
 * smile : [-42..+42]   open : [0..40]   hold : frames (1 frame ≈ 33 ms)
 */
static const test_mouth_model_clip_t s_mouth_model_clips[] = {

    /* ══ initial idle ════════════════════════════════════════════════════════ */
    {"idle",          0,   2, 22U},

    /* ══ happy ══════════════════════════════════════════════════════════════ */
    {"happy",         0,   2,  5U},
    {"happy",        28,   6, 26U},   /* warm beam                             */
    {"happy",         0,   2,  5U},

    /* ══ laughing ════════════════════════════════════════════════════════════ */
    {"laughing",      0,   2,  5U},
    {"laughing",     42,  14, 10U},   /* burst 1: peak grin + open             */
    {"laughing",     38,   6,  8U},   /* settle between bursts                 */
    {"laughing",     42,  18, 10U},   /* burst 2: a little wider               */
    {"laughing",      0,   2,  5U},

    /* ══ funny ═══════════════════════════════════════════════════════════════ */
    {"funny",         0,   2,  5U},
    {"funny",        36,   8, 22U},   /* amused side-grin                      */
    {"funny",         0,   2,  5U},

    /* ══ loving ══════════════════════════════════════════════════════════════ */
    {"loving",        0,   2,  5U},
    {"loving",       22,   4, 30U},   /* soft, warm, content smile             */
    {"loving",        0,   2,  5U},

    /* ══ embarrassed ════════════════════════════════════════════════════════ */
    {"embarrassed",   0,   2,  5U},
    {"embarrassed",  14,   4, 24U},   /* shy half-smile                        */
    {"embarrassed",   0,   2,  5U},

    /* ══ confident ══════════════════════════════════════════════════════════ */
    {"confident",     0,   2,  5U},
    {"confident",    26,   6, 22U},   /* assured, poised smile                 */
    {"confident",     0,   2,  5U},

    /* ══ delicious ══════════════════════════════════════════════════════════ */
    {"delicious",     0,   2,  5U},
    {"delicious",    30,  16, 18U},   /* mmm – smile + open                    */
    {"delicious",    32,   6, 14U},   /* savoring, mouth closes slightly       */
    {"delicious",     0,   2,  5U},

    /* ══ sad ═════════════════════════════════════════════════════════════════ */
    {"sad",           0,   2,  5U},
    {"sad",         -24,   4, 28U},   /* downturned mouth, sad eyes droop      */
    {"sad",           0,   2,  5U},

    /* ══ crying ══════════════════════════════════════════════════════════════ */
    {"crying",        0,   2,  5U},
    {"crying",      -38,   8, 12U},   /* heavy sob                             */
    {"crying",      -32,   4,  8U},   /* catch breath                          */
    {"crying",      -38,  10, 12U},   /* sob again                             */
    {"crying",        0,   2,  5U},

    /* ══ sleepy ══════════════════════════════════════════════════════════════ */
    {"sleepy",        0,   2,  5U},
    {"sleepy",        0,   2, 32U},   /* drowsy hold (open<=4 → eyes droop)    */
    {"sleepy",        4,   2, 16U},   /* gentle sigh                           */
    {"sleepy",        0,   2,  5U},

    /* ══ silly ═══════════════════════════════════════════════════════════════ */
    {"silly",         0,   2,  5U},
    {"silly",        38,  22, 16U},   /* goofy wide grin                       */
    {"silly",        36,  12, 12U},   /* bounce / wobble                       */
    {"silly",         0,   2,  5U},

    /* ══ angry ═══════════════════════════════════════════════════════════════ */
    {"angry",         0,   2,  5U},
    {"angry",       -32,  10, 22U},   /* snarl: frown + open                   */
    {"angry",       -28,   6, 14U},   /* simmer                                */
    {"angry",         0,   2,  5U},

    /* ══ surprised ══════════════════════════════════════════════════════════ */
    {"surprised",     0,   2,  5U},
    {"surprised",     0,  30, 20U},   /* wide open 'O' (surprise eyes)         */
    {"surprised",     4,  22, 12U},   /* settling                              */
    {"surprised",     0,   2,  5U},

    /* ══ shocked ════════════════════════════════════════════════════════════ */
    {"shocked",       0,   2,  5U},
    {"shocked",       0,  38, 18U},   /* open>=36 → frightened-wide eyes       */
    {"shocked",       0,  32, 12U},   /* realising                             */
    {"shocked",       0,   2,  5U},

    /* ══ thinking ═══════════════════════════════════════════════════════════ */
    {"thinking",      0,   2,  5U},
    {"thinking",     -6,   6, 26U},   /* pondering, slight purse               */
    {"thinking",     -4,   4, 18U},   /* mulling it over                       */
    {"thinking",      0,   2,  5U},

    /* ══ winking ════════════════════════════════════════════════════════════ */
    {"winking",       0,   2,  5U},
    {"winking",      20,   2, 24U},   /* coy one-sided smile                   */
    {"winking",       0,   2,  5U},

    /* ══ relaxed ════════════════════════════════════════════════════════════ */
    {"relaxed",       0,   2,  5U},
    {"relaxed",      14,   3, 30U},   /* peaceful, soft smile                  */
    {"relaxed",       0,   2,  5U},

    /* ══ confused ═══════════════════════════════════════════════════════════ */
    {"confused",      0,   2,  5U},
    {"confused",     -8,   8, 24U},   /* unsure, mouth ajar                    */
    {"confused",     -4,   6, 16U},   /* still confused                        */
    {"confused",      0,   2,  5U},

    /* ══ neutral ════════════════════════════════════════════════════════════ */
    {"neutral",       0,   2,  5U},
    {"neutral",       0,   2, 22U},   /* blank resting face                    */
    {"neutral",       0,   2,  5U},
};

static const uint16_t s_white_pixel = 0xFFFF;
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

static int16_t test_mouth_model_clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }

    return (int16_t)value;
}

static int32_t test_mouth_model_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t test_mouth_model_max_i32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static int16_t test_mouth_model_follow_axis(int16_t current, int16_t target)
{
    int32_t diff = (int32_t)target - current;
    int32_t step;

    if (diff == 0) {
        return current;
    }

    step = diff / 4;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }

    return (int16_t)(current + step);
}

static int16_t test_mouth_model_triangle_wave_i16(uint32_t tick, uint16_t period, uint16_t phase, int16_t amplitude)
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

static const test_mouth_model_clip_t *test_mouth_model_get_clip(size_t clip_idx)
{
    return &s_mouth_model_clips[clip_idx % TEST_APP_ARRAY_SIZE(s_mouth_model_clips)];
}

static test_mouth_model_mode_t test_mouth_model_select_mode(int16_t smile, int16_t open)
{
    if (open >= TEST_MOUTH_MODEL_MODE_THRESH) {
        return TEST_MOUTH_MODEL_MODE_OPEN;
    }
    if (smile >= TEST_MOUTH_MODEL_MODE_THRESH) {
        return TEST_MOUTH_MODEL_MODE_SMILE;
    }
    return TEST_MOUTH_MODEL_MODE_FLAT;
}

static test_mouth_model_shape_t test_mouth_model_select_shape(int16_t smile, int16_t open)
{
    if (open >= TEST_MOUTH_MODEL_SURPRISE_OPEN) {
        return TEST_MOUTH_MODEL_SHAPE_SURPRISE_O;
    }
    if (smile >= TEST_MOUTH_MODEL_SMILE_THRESH) {
        return TEST_MOUTH_MODEL_SHAPE_SMILE_U;
    }
    if (smile <= -TEST_MOUTH_MODEL_SAD_THRESH) {
        return TEST_MOUTH_MODEL_SHAPE_SAD_N;
    }
    return TEST_MOUTH_MODEL_SHAPE_IDLE;
}

static const gfx_image_dsc_t *test_mouth_model_get_src(test_mouth_model_mode_t mode)
{
    switch (mode) {
    case TEST_MOUTH_MODEL_MODE_SMILE:
        return &face_parts_mouth;
    case TEST_MOUTH_MODEL_MODE_OPEN:
        return &face_parts_mouth_o;
    case TEST_MOUTH_MODEL_MODE_FLAT:
    default:
        return &face_parts_mouth_flat;
    }
}

static void test_mouth_model_capture_base_points(test_mouth_model_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mouth_obj);
    TEST_ASSERT_EQUAL_UINT32(TEST_MOUTH_MODEL_POINT_COUNT, gfx_mesh_img_get_point_count(scene->mouth_obj));

    /* Point 0 is at local (0,0), so screen coords give us the draw origin directly. */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point_screen(scene->mouth_obj, 0,
                                                            &scene->mouth_origin_x,
                                                            &scene->mouth_origin_y));

    for (size_t i = 0; i < TEST_MOUTH_MODEL_POINT_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(scene->mouth_obj, i, &scene->base_points[i]));
    }
}

static void test_mouth_model_set_mode(test_mouth_model_scene_t *scene, test_mouth_model_mode_t mode)
{
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(scene->mouth_obj);

    if (scene->mouth_mode == mode) {
        return;
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene->mouth_obj, (void *)test_mouth_model_get_src(mode)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene->mouth_obj, TEST_MOUTH_MODEL_GRID_COLS, TEST_MOUTH_MODEL_GRID_ROWS));
    test_mouth_model_capture_base_points(scene);
    scene->mouth_mode = mode;
}

static esp_err_t test_mouth_model_apply_line_dense_mesh(gfx_obj_t *obj,
                                                         gfx_coord_t origin_x,
                                                         gfx_coord_t origin_y,
                                                         const int32_t x[TEST_MOUTH_MODEL_DENSE_POINT_COUNT],
                                                         const int32_t y[TEST_MOUTH_MODEL_DENSE_POINT_COUNT],
                                                         int32_t half_thickness)
{
    gfx_mesh_img_point_t points[TEST_MOUTH_MODEL_DENSE_POINT_COUNT * 2U];
    gfx_coord_t min_x;
    gfx_coord_t min_y;
    size_t i;

    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_INVALID_ARG, TAG, "apply dense mesh: object is NULL");
    ESP_RETURN_ON_FALSE(x != NULL, ESP_ERR_INVALID_ARG, TAG, "apply dense mesh: x is NULL");
    ESP_RETURN_ON_FALSE(y != NULL, ESP_ERR_INVALID_ARG, TAG, "apply dense mesh: y is NULL");

    /*
     * Mesh points are in mouth-local image space.  Convert to screen space by
     * adding origin, then anchor this object at the bounding-box top-left so
     * that get_draw_origin() == (min_x, min_y) and local coords = screen coords.
     *
     * With GFX_ALIGN_TOP_LEFT, geometry.x == x_ofs regardless of object width,
     * so update_layout() won't re-center and shift the object every frame.
     */
    min_x = (gfx_coord_t)(origin_x + x[0]);
    min_y = (gfx_coord_t)(origin_y + y[0] - half_thickness);
    for (i = 1; i < TEST_MOUTH_MODEL_DENSE_POINT_COUNT; i++) {
        gfx_coord_t sx = (gfx_coord_t)(origin_x + x[i]);
        gfx_coord_t sy_top = (gfx_coord_t)(origin_y + y[i] - half_thickness);
        if (sx < min_x) {
            min_x = sx;
        }
        if (sy_top < min_y) {
            min_y = sy_top;
        }
    }

    for (i = 0; i < TEST_MOUTH_MODEL_DENSE_POINT_COUNT; i++) {
        points[i].x = (gfx_coord_t)(origin_x + x[i] - min_x);
        points[i].y = (gfx_coord_t)(origin_y + y[i] - half_thickness - min_y);
        points[TEST_MOUTH_MODEL_DENSE_POINT_COUNT + i].x = (gfx_coord_t)(origin_x + x[i] - min_x);
        points[TEST_MOUTH_MODEL_DENSE_POINT_COUNT + i].y = (gfx_coord_t)(origin_y + y[i] + half_thickness - min_y);
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, min_x, min_y),
                        TAG, "apply dense mesh: align failed");
    return gfx_mesh_img_set_points(obj, points, TEST_APP_ARRAY_SIZE(points));
}

static esp_err_t test_mouth_model_apply_vertical_arc_mesh(gfx_obj_t *obj,
                                                          gfx_coord_t origin_x,
                                                          gfx_coord_t origin_y,
                                                          int32_t point0_x,
                                                          int32_t point1_x,
                                                          int32_t point2_x,
                                                          int32_t point0_y,
                                                          int32_t point1_y,
                                                          int32_t point2_y,
                                                          int32_t half_thickness)
{
    gfx_mesh_img_point_t points[6];
    int32_t min_px;
    int32_t min_py;
    gfx_coord_t min_x;
    gfx_coord_t min_y;

    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_INVALID_ARG, TAG, "apply vertical arc mesh: object is NULL");

    /* Convert from mouth-local to screen space, then anchor at bounding-box top-left. */
    min_px = point0_x;
    if (point1_x < min_px) {
        min_px = point1_x;
    }
    if (point2_x < min_px) {
        min_px = point2_x;
    }
    min_py = point0_y;
    if (point1_y < min_py) {
        min_py = point1_y;
    }
    if (point2_y < min_py) {
        min_py = point2_y;
    }
    min_x = (gfx_coord_t)(origin_x + min_px - half_thickness);
    min_y = (gfx_coord_t)(origin_y + min_py);

    points[0].x = (gfx_coord_t)(origin_x + point0_x - half_thickness - min_x);
    points[0].y = (gfx_coord_t)(origin_y + point0_y - min_y);
    points[1].x = (gfx_coord_t)(origin_x + point0_x + half_thickness - min_x);
    points[1].y = (gfx_coord_t)(origin_y + point0_y - min_y);

    points[2].x = (gfx_coord_t)(origin_x + point1_x - half_thickness - min_x);
    points[2].y = (gfx_coord_t)(origin_y + point1_y - min_y);
    points[3].x = (gfx_coord_t)(origin_x + point1_x + half_thickness - min_x);
    points[3].y = (gfx_coord_t)(origin_y + point1_y - min_y);

    points[4].x = (gfx_coord_t)(origin_x + point2_x - half_thickness - min_x);
    points[4].y = (gfx_coord_t)(origin_y + point2_y - min_y);
    points[5].x = (gfx_coord_t)(origin_x + point2_x + half_thickness - min_x);
    points[5].y = (gfx_coord_t)(origin_y + point2_y - min_y);

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, min_x, min_y),
                        TAG, "apply vertical arc mesh: align failed");
    return gfx_mesh_img_set_points(obj, points, TEST_APP_ARRAY_SIZE(points));
}

static void test_mouth_model_build_bone_pose(const test_mouth_model_scene_t *scene,
                                             int16_t smile,
                                             int16_t open,
                                             test_mouth_model_bone_pose_t bones[TEST_MOUTH_MODEL_BONE_COUNT])
{
    int32_t center_x;
    int32_t center_y;
    int32_t left_x;
    int32_t right_x;
    int32_t upper_y;
    int32_t lower_y;
    int32_t width_bias = smile - (open / 2);
    int32_t smile_corner_lift;
    int32_t open_corner_drop;
    int32_t upper_pull;
    int32_t lower_drop;
    int32_t center_drop;

    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(bones);

    center_x = scene->base_points[(TEST_MOUTH_MODEL_GRID_ROWS / 2U) * (TEST_MOUTH_MODEL_GRID_COLS + 1U) +
                                  (TEST_MOUTH_MODEL_GRID_COLS / 2U)].x;
    center_y = scene->base_points[(TEST_MOUTH_MODEL_GRID_ROWS / 2U) * (TEST_MOUTH_MODEL_GRID_COLS + 1U) +
                                  (TEST_MOUTH_MODEL_GRID_COLS / 2U)].y;
    left_x = scene->base_points[(TEST_MOUTH_MODEL_GRID_ROWS / 2U) * (TEST_MOUTH_MODEL_GRID_COLS + 1U)].x;
    right_x = scene->base_points[(TEST_MOUTH_MODEL_GRID_ROWS / 2U) * (TEST_MOUTH_MODEL_GRID_COLS + 1U) +
                                 TEST_MOUTH_MODEL_GRID_COLS].x;
    upper_y = scene->base_points[(TEST_MOUTH_MODEL_GRID_COLS / 2U)].y;
    lower_y = scene->base_points[(TEST_MOUTH_MODEL_GRID_ROWS * (TEST_MOUTH_MODEL_GRID_COLS + 1U)) +
                                 (TEST_MOUTH_MODEL_GRID_COLS / 2U)].y;

    width_bias = test_mouth_model_clamp_i16(width_bias,
                                            -TEST_MOUTH_MODEL_WIDTH_LIMIT,
                                            TEST_MOUTH_MODEL_WIDTH_LIMIT);
    smile_corner_lift = (smile * 3) / 4;
    open_corner_drop = open / 6;
    upper_pull = -(open / 3) - (smile / 10);
    lower_drop = open + (smile / 8);
    center_drop = (open * 4) / 5 - (smile / 8);

    bones[TEST_MOUTH_MODEL_BONE_LEFT_CORNER] = (test_mouth_model_bone_pose_t) {
        .x = left_x,
        .y = center_y,
        .dx = -width_bias,
        .dy = -smile_corner_lift + open_corner_drop,
    };
    bones[TEST_MOUTH_MODEL_BONE_RIGHT_CORNER] = (test_mouth_model_bone_pose_t) {
        .x = right_x,
        .y = center_y,
        .dx = width_bias,
        .dy = -smile_corner_lift + open_corner_drop,
    };
    bones[TEST_MOUTH_MODEL_BONE_UPPER_LIP] = (test_mouth_model_bone_pose_t) {
        .x = center_x,
        .y = upper_y,
        .dx = 0,
        .dy = upper_pull,
    };
    bones[TEST_MOUTH_MODEL_BONE_LOWER_LIP] = (test_mouth_model_bone_pose_t) {
        .x = center_x,
        .y = lower_y,
        .dx = 0,
        .dy = lower_drop,
    };
    bones[TEST_MOUTH_MODEL_BONE_CENTER] = (test_mouth_model_bone_pose_t) {
        .x = center_x,
        .y = center_y,
        .dx = 0,
        .dy = center_drop,
    };
}

/*
 * Eyebrow mesh — a thin curved band above the eye.
 *
 *   half_w     : half the brow width (px)
 *   arch       : upward arch at the centre; positive = raises centre, negative = V-shape
 *   tilt_at_left: linear tilt across the brow.
 *                 Positive → left (outer) corner goes DOWN (same convention as eye).
 *                 For sad left brow:  +value  (outer-left down, inner-right up)
 *                 For angry left brow: -value (inner-right down = furrowed)
 *   thickness  : half-thickness of the brow strip
 *
 * Expressions → brow mapping:
 *   Smile    : arch flattens slightly, brow raises a little
 *   Sad      : outer corner down, inner up (tilt > 0 for left brow)
 *   Angry    : inner corner down (tilt < 0 for left brow), arch → V, brow presses lower
 *   Surprise : whole brow raised, more arched
 */
static esp_err_t test_mouth_model_apply_brow_mesh(gfx_obj_t *obj,
                                                   int32_t cx,
                                                   int32_t cy,
                                                   int32_t half_w,
                                                   int32_t arch,
                                                   int32_t tilt_at_left,
                                                   int32_t thickness)
{
    gfx_mesh_img_point_t points[TEST_MOUTH_MODEL_BROW_PT_COUNT * 2U];
    gfx_coord_t min_x;
    gfx_coord_t min_y;
    int32_t y_tmp;
    size_t i;

    /* Clamp thickness. */
    if (thickness < 1) {
        thickness = 1;
    }

    /* First pass: find vertical bounding box (tilt and arch both affect it). */
    min_x = (gfx_coord_t)(cx - half_w);
    {
        int32_t tilt_abs = (tilt_at_left < 0) ? -tilt_at_left : tilt_at_left;
        int32_t arch_abs = (arch < 0) ? -arch : arch;
        min_y = (gfx_coord_t)(cy - arch_abs - thickness - tilt_abs);
    }
    for (i = 0; i <= TEST_MOUTH_MODEL_BROW_SEGS; i++) {
        int32_t edge_w = (4 * (int32_t)i * ((int32_t)TEST_MOUTH_MODEL_BROW_SEGS - (int32_t)i) * 1000) /
                         ((int32_t)TEST_MOUTH_MODEL_BROW_SEGS * (int32_t)TEST_MOUTH_MODEL_BROW_SEGS);
        int32_t tilt  = tilt_at_left - (tilt_at_left * 2 * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_BROW_SEGS;
        int32_t mid_y = cy - (arch * edge_w / 1000) + tilt;
        y_tmp = mid_y - thickness;
        if ((gfx_coord_t)y_tmp < min_y) {
            min_y = (gfx_coord_t)y_tmp;
        }
    }

    /* Second pass: fill mesh points as local offsets from (min_x, min_y). */
    for (i = 0; i <= TEST_MOUTH_MODEL_BROW_SEGS; i++) {
        int32_t px    = cx - half_w + ((2 * half_w * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_BROW_SEGS);
        int32_t edge_w = (4 * (int32_t)i * ((int32_t)TEST_MOUTH_MODEL_BROW_SEGS - (int32_t)i) * 1000) /
                         ((int32_t)TEST_MOUTH_MODEL_BROW_SEGS * (int32_t)TEST_MOUTH_MODEL_BROW_SEGS);
        int32_t tilt  = tilt_at_left - (tilt_at_left * 2 * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_BROW_SEGS;
        int32_t mid_y = cy - (arch * edge_w / 1000) + tilt;

        /* Row 0 = top edge of brow, row 1 = bottom edge. */
        points[i].x = (gfx_coord_t)(px - (int32_t)min_x);
        points[i].y = (gfx_coord_t)(mid_y - thickness - (int32_t)min_y);
        points[TEST_MOUTH_MODEL_BROW_PT_COUNT + i].x = (gfx_coord_t)(px - (int32_t)min_x);
        points[TEST_MOUTH_MODEL_BROW_PT_COUNT + i].y = (gfx_coord_t)(mid_y + thickness - (int32_t)min_y);
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, min_x, min_y),
                        TAG, "apply brow mesh: align failed");
    return gfx_mesh_img_set_points(obj, points, TEST_APP_ARRAY_SIZE(points));
}

/*
 * Eye mesh — lens (almond) shape driven by three parameters:
 *
 *   ry_top      : max upward radius from centre to top of upper eyelid (px)
 *   ry_bottom   : max downward radius from centre to bottom of lower eyelid (px)
 *   tilt_at_left: linear tilt applied left→right.
 *                 Positive → left corner moves DOWN, right corner moves UP.
 *                 Use +tilt for left eye (outer-left droops), −tilt for right eye.
 *
 * The upper and lower arcs meet at both corners → closed lens with no extra pieces.
 *
 *   Happy squint : ry_top ↓  (upper eyelid descends)
 *   Sad droop    : tilt_at_left ↑ for left eye, ↓ for right eye
 *   Surprise     : ry_top ↑, ry_bottom ↑  (eyes widen)
 */
static esp_err_t test_mouth_model_apply_eye_mesh(gfx_obj_t *obj,
                                                  int32_t cx,
                                                  int32_t cy,
                                                  int32_t rx,
                                                  int32_t ry_top,
                                                  int32_t ry_bottom,
                                                  int32_t tilt_at_left)
{
    gfx_mesh_img_point_t points[TEST_MOUTH_MODEL_EYE_PT_COUNT * 2U];
    gfx_coord_t min_x;
    gfx_coord_t min_y;
    int32_t y_tmp;
    size_t i;

    /* Clamp radii to avoid degenerate shapes. */
    if (ry_top < 2) {
        ry_top = 2;
    }
    if (ry_bottom < 2) {
        ry_bottom = 2;
    }

    /* First pass: find vertical extent (tilt shifts corners). */
    min_x = (gfx_coord_t)(cx - rx);
    min_y = (gfx_coord_t)(cy - ry_top);
    for (i = 0; i <= TEST_MOUTH_MODEL_EYE_SEGS; i++) {
        int32_t tilt = tilt_at_left - (tilt_at_left * 2 * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_EYE_SEGS;
        int32_t edge_w = (4 * (int32_t)i * ((int32_t)TEST_MOUTH_MODEL_EYE_SEGS - (int32_t)i) * 1000) /
                         ((int32_t)TEST_MOUTH_MODEL_EYE_SEGS * (int32_t)TEST_MOUTH_MODEL_EYE_SEGS);
        y_tmp = cy - (ry_top * edge_w / 1000) + tilt;
        if ((gfx_coord_t)y_tmp < min_y) {
            min_y = (gfx_coord_t)y_tmp;
        }
        y_tmp = cy + (ry_bottom * edge_w / 1000) + tilt;
        if ((gfx_coord_t)y_tmp < min_y) {
            min_y = (gfx_coord_t)y_tmp;
        }
    }

    /* Second pass: fill mesh points as local offsets from (min_x, min_y). */
    for (i = 0; i <= TEST_MOUTH_MODEL_EYE_SEGS; i++) {
        int32_t px = cx - rx + ((2 * rx * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_EYE_SEGS);
        int32_t edge_w = (4 * (int32_t)i * ((int32_t)TEST_MOUTH_MODEL_EYE_SEGS - (int32_t)i) * 1000) /
                         ((int32_t)TEST_MOUTH_MODEL_EYE_SEGS * (int32_t)TEST_MOUTH_MODEL_EYE_SEGS);
        int32_t tilt = tilt_at_left - (tilt_at_left * 2 * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_EYE_SEGS;

        int32_t upper_y = cy - (ry_top * edge_w / 1000) + tilt;
        int32_t lower_y = cy + (ry_bottom * edge_w / 1000) + tilt;

        /* Row 0 = upper eyelid arc, row 1 = lower eyelid arc. */
        points[i].x = (gfx_coord_t)(px - (int32_t)min_x);
        points[i].y = (gfx_coord_t)(upper_y - (int32_t)min_y);
        points[TEST_MOUTH_MODEL_EYE_PT_COUNT + i].x = (gfx_coord_t)(px - (int32_t)min_x);
        points[TEST_MOUTH_MODEL_EYE_PT_COUNT + i].y = (gfx_coord_t)(lower_y - (int32_t)min_y);
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, min_x, min_y),
                        TAG, "apply eye mesh: align failed");
    return gfx_mesh_img_set_points(obj, points, TEST_APP_ARRAY_SIZE(points));
}

static esp_err_t test_mouth_model_apply_pose(test_mouth_model_scene_t *scene, int16_t smile, int16_t open)
{
    test_mouth_model_bone_pose_t bones[TEST_MOUTH_MODEL_BONE_COUNT];
    test_mouth_model_shape_t shape;
    int16_t mode_smile = smile;
    int16_t mode_open = open;
    int32_t left_x;
    int32_t right_x;
    int32_t center_y;
    int32_t x_points[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t upper_mid_y[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t lower_mid_y[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t mid_y;
    int32_t aperture;
    int32_t left_corner_mid_x;
    int32_t right_corner_mid_x;
    int32_t left_corner_mid_y;
    int32_t right_corner_mid_y;
    int32_t line_half_thickness;
    int32_t corner_half_thickness;
    int32_t width_trim;
    int32_t min_aperture;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: scene is NULL");
    ESP_RETURN_ON_FALSE(scene->mouth_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: anchor object is NULL");
    ESP_RETURN_ON_FALSE(scene->upper_lip_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: upper lip object is NULL");
    ESP_RETURN_ON_FALSE(scene->lower_lip_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: lower lip object is NULL");
    ESP_RETURN_ON_FALSE(scene->left_corner_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: left corner object is NULL");
    ESP_RETURN_ON_FALSE(scene->right_corner_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: right corner object is NULL");

    if (scene->mouth_mode == TEST_MOUTH_MODEL_MODE_SMILE) {
        mode_smile = smile - TEST_MOUTH_MODEL_MODE_THRESH;
    } else if (scene->mouth_mode == TEST_MOUTH_MODEL_MODE_OPEN) {
        mode_open = open - TEST_MOUTH_MODEL_MODE_THRESH;
    }

    test_mouth_model_build_bone_pose(scene, mode_smile, mode_open, bones);
    shape = test_mouth_model_select_shape(smile, open);
    left_x = bones[TEST_MOUTH_MODEL_BONE_LEFT_CORNER].x + bones[TEST_MOUTH_MODEL_BONE_LEFT_CORNER].dx;
    right_x = bones[TEST_MOUTH_MODEL_BONE_RIGHT_CORNER].x + bones[TEST_MOUTH_MODEL_BONE_RIGHT_CORNER].dx;
    center_y = bones[TEST_MOUTH_MODEL_BONE_CENTER].y + bones[TEST_MOUTH_MODEL_BONE_CENTER].dy;

    width_trim = test_mouth_model_max_i32(0, mode_open / 6);
    left_x += width_trim;
    right_x -= width_trim;
    line_half_thickness = 2 + (mode_open / 24);
    min_aperture = line_half_thickness + 1;
    for (size_t i = 0; i < TEST_MOUTH_MODEL_DENSE_POINT_COUNT; i++) {
        int32_t center_weight;
        int32_t sad_mag;
        int32_t smile_mag;

        x_points[i] = left_x + (((right_x - left_x) * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_DENSE_SEGMENTS);
        center_weight = 1000 - ((test_mouth_model_abs_i32((int32_t)i - (int32_t)(TEST_MOUTH_MODEL_DENSE_SEGMENTS / 2U)) * 1000) /
                                (int32_t)(TEST_MOUTH_MODEL_DENSE_SEGMENTS / 2U));
        if (center_weight < 0) {
            center_weight = 0;
        }

        mid_y = center_y;
        aperture = 1 + (mode_open / 14);
        switch (shape) {
        case TEST_MOUTH_MODEL_SHAPE_SAD_N:
            sad_mag = test_mouth_model_abs_i32(test_mouth_model_clamp_i16(mode_smile, -TEST_MOUTH_MODEL_SMILE_LIMIT, 0));
            mid_y = center_y + (sad_mag / 5) - ((center_weight * ((sad_mag / 5) + (sad_mag / 4))) / 1000);
            aperture = 2 + (mode_open / 10) + ((center_weight * (2 + (mode_open / 8))) / 1000);
            break;
        case TEST_MOUTH_MODEL_SHAPE_SMILE_U:
            smile_mag = test_mouth_model_max_i32(0, mode_smile);
            mid_y = center_y - (smile_mag / 5) + ((center_weight * ((smile_mag / 5) + (smile_mag / 4))) / 1000);
            aperture = 2 + (mode_open / 12) + ((center_weight * (2 + (mode_open / 10))) / 1000);
            break;
        case TEST_MOUTH_MODEL_SHAPE_SURPRISE_O:
            aperture = 4 + (mode_open / 3) + ((center_weight * (3 + (mode_open / 4))) / 1000);
            break;
        case TEST_MOUTH_MODEL_SHAPE_IDLE:
        default:
            aperture = 1 + ((center_weight * 1) / 1000);
            break;
        }

        /* Keep upper/lower lips visually separated even in idle '-'. */
        if (aperture < min_aperture) {
            aperture = min_aperture;
        }
        upper_mid_y[i] = mid_y - aperture;
        lower_mid_y[i] = mid_y + aperture;
    }

    corner_half_thickness = line_half_thickness;
    left_corner_mid_x = left_x - (3 + (mode_open / 12));
    right_corner_mid_x = right_x + (3 + (mode_open / 12));
    left_corner_mid_y = (upper_mid_y[0] + lower_mid_y[0]) / 2;
    right_corner_mid_y = (upper_mid_y[TEST_MOUTH_MODEL_DENSE_SEGMENTS] +
                          lower_mid_y[TEST_MOUTH_MODEL_DENSE_SEGMENTS]) / 2;

    ESP_RETURN_ON_ERROR(test_mouth_model_apply_line_dense_mesh(scene->upper_lip_obj,
                                                               scene->mouth_origin_x,
                                                               scene->mouth_origin_y,
                                                               x_points,
                                                               upper_mid_y,
                                                               line_half_thickness),
                        TAG, "apply pose: upper lip failed");
    ESP_RETURN_ON_ERROR(test_mouth_model_apply_line_dense_mesh(scene->lower_lip_obj,
                                                               scene->mouth_origin_x,
                                                               scene->mouth_origin_y,
                                                               x_points,
                                                               lower_mid_y,
                                                               line_half_thickness),
                        TAG, "apply pose: lower lip failed");
    /*
     * Connect corner arcs to the outer edges of the lip strips, not their midlines.
     * The lip strip spans midline ± line_half_thickness.  If the corner starts at the
     * midline, the outer half of the lip's end-cap is left uncovered, producing a
     * visible triangular notch.  Starting at the outer edges eliminates the gap.
     */
    ESP_RETURN_ON_ERROR(test_mouth_model_apply_vertical_arc_mesh(scene->left_corner_obj,
                                                                 scene->mouth_origin_x,
                                                                 scene->mouth_origin_y,
                                                                 left_x, left_corner_mid_x, left_x,
                                                                 upper_mid_y[0] - line_half_thickness,
                                                                 left_corner_mid_y,
                                                                 lower_mid_y[0] + line_half_thickness,
                                                                 corner_half_thickness),
                        TAG, "apply pose: left corner failed");
    ESP_RETURN_ON_ERROR(test_mouth_model_apply_vertical_arc_mesh(scene->right_corner_obj,
                                                                 scene->mouth_origin_x,
                                                                 scene->mouth_origin_y,
                                                                 right_x, right_corner_mid_x, right_x,
                                                                 upper_mid_y[TEST_MOUTH_MODEL_DENSE_SEGMENTS] - line_half_thickness,
                                                                 right_corner_mid_y,
                                                                 lower_mid_y[TEST_MOUTH_MODEL_DENSE_SEGMENTS] + line_half_thickness,
                                                                 corner_half_thickness),
                        TAG, "apply pose: right corner failed");

    /* ── Eyes & Eyebrows — Additive Blend System ────────────────────────────
     *
     * Inspired by Rive's additive expression blending: each expression
     * (smile, sad, surprise, fright, angry) contributes delta offsets that
     * are summed onto a neutral base, so expressions can combine naturally.
     *
     * Blend weights  w_*  are in the range [0..100].
     *
     * is_angry: frown (smile ≤ -15) combined with an open mouth (open ≥ 10)
     *           and not extreme sob (smile > -36). Angry and sad are mutually
     *           exclusive so brow direction is unambiguous.
     *
     * Eye params:  ry_top, ry_bottom, eye_tilt_l
     * Brow params: brow_arch (centre height), brow_raise (whole-brow lift),
     *              brow_tilt_l (tilt_at_left — same sign convention as eye)
     *
     * tilt > 0 for LEFT eye/brow → outer-left corner moves DOWN.
     * Right eye/brow always receives -tilt for symmetric mirroring.
     */
    {
        /* ── Expression weights ─────────────────────────────────────────── */
        int32_t w_smile   = ((int32_t)smile > 0) ?
                            ((int32_t)smile * 100 / TEST_MOUTH_MODEL_SMILE_LIMIT) : 0;
        int32_t w_sad     = ((int32_t)smile < 0) ?
                            (-(int32_t)smile * 100 / TEST_MOUTH_MODEL_SMILE_LIMIT) : 0;
        int32_t w_surprise = ((int32_t)open >= TEST_MOUTH_MODEL_SURPRISE_OPEN &&
                              (int32_t)open < TEST_MOUTH_MODEL_FRIGHT_OPEN) ?
                             (((int32_t)open - TEST_MOUTH_MODEL_SURPRISE_OPEN) * 100 /
                              (TEST_MOUTH_MODEL_FRIGHT_OPEN - TEST_MOUTH_MODEL_SURPRISE_OPEN)) : 0;
        int32_t w_fright  = ((int32_t)open >= TEST_MOUTH_MODEL_FRIGHT_OPEN) ?
                            (((int32_t)open - TEST_MOUTH_MODEL_FRIGHT_OPEN) * 100 /
                             (TEST_MOUTH_MODEL_OPEN_LIMIT - TEST_MOUTH_MODEL_FRIGHT_OPEN)) : 0;

        /* Angry: snarling frown (not extreme sob). Replaces sad for brow direction. */
        int32_t is_angry = ((int32_t)smile <= -15 && (int32_t)smile >= -35 &&
                            (int32_t)open >= 10) ? 1 : 0;
        int32_t w_angry  = is_angry ? w_sad : 0;
        int32_t w_sad_pure = is_angry ? 0 : w_sad;  /* pure sad (non-angry) */

        /* ── Eye parameters — neutral base + additive contributions ─────── */
        int32_t ry_top    = TEST_MOUTH_MODEL_EYE_RY;
        int32_t ry_bottom = (TEST_MOUTH_MODEL_EYE_RY * 85) / 100;
        int32_t eye_tilt_l = 0;

        /* Smile: upper lid squints down (~80 %), lower lid rises (~30 %) */
        ry_top    -= w_smile * TEST_MOUTH_MODEL_EYE_RY * 80 / (100 * 100);
        ry_bottom -= w_smile * (TEST_MOUTH_MODEL_EYE_RY * 85 / 100) * 30 / (100 * 100);

        /* Sad: outer corners droop, upper lid narrows slightly */
        eye_tilt_l += w_sad_pure * 7 / 100;
        ry_top     -= w_sad_pure * 4 / 100;

        /* Surprise: eyelids widen proportionally */
        ry_top    += w_surprise * (TEST_MOUTH_MODEL_EYE_RY * 50 / 100) / 100;
        ry_bottom += w_surprise * (TEST_MOUTH_MODEL_EYE_RY * 25 / 100) / 100;

        /* Fright: eyelids stretch very wide */
        ry_top    += w_fright * (TEST_MOUTH_MODEL_EYE_RY * 55 / 100) / 100;
        ry_bottom += w_fright * (TEST_MOUTH_MODEL_EYE_RY * 35 / 100) / 100;

        /* Drowsy: low open + no smile/sad → upper lid droops ~22 % */
        if ((int32_t)open <= 4 && w_smile < 10 && w_sad < 10) {
            ry_top = ry_top * 78 / 100;
        }

        /* ── Eyebrow parameters — neutral base + additive contributions ─── */
        int32_t brow_arch   = 4;   /* natural slight arch (px, centre raised) */
        int32_t brow_raise  = 0;   /* vertical lift of whole brow (+ve = up)  */
        int32_t brow_tilt_l = 0;   /* same sign convention as eye_tilt_l      */

        /* Smile: brow relaxes and lifts slightly */
        brow_arch  -= w_smile * 3 / 100;
        brow_raise += w_smile * 2 / 100;

        /* Sad: outer corner down / inner corner up */
        brow_tilt_l += w_sad_pure * 7 / 100;
        brow_raise  -= w_sad_pure * 1 / 100;

        /* Angry: inner corner DOWN (opposite of sad), brow presses lower, V-arch */
        brow_tilt_l -= w_angry * 7 / 100;   /* negative = inner corner down */
        brow_raise  -= w_angry * 3 / 100;   /* presses toward eyes           */
        brow_arch   -= w_angry * 8 / 100;   /* flatten → V-shape when angry  */

        /* Surprise: brows shoot up, more arched */
        brow_raise += w_surprise * 8 / 100;
        brow_arch  += w_surprise * 4 / 100;

        /* Fright: even higher */
        brow_raise += w_fright * 6 / 100;
        brow_arch  += w_fright * 2 / 100;

        /* ── Apply eyes ─────────────────────────────────────────────────── */
        if (scene->left_eye_obj != NULL) {
            ESP_RETURN_ON_ERROR(
                test_mouth_model_apply_eye_mesh(scene->left_eye_obj,
                                                (int32_t)scene->left_eye_cx,
                                                (int32_t)scene->left_eye_cy,
                                                TEST_MOUTH_MODEL_EYE_RX,
                                                ry_top, ry_bottom, eye_tilt_l),
                TAG, "apply pose: left eye failed");
        }
        if (scene->right_eye_obj != NULL) {
            ESP_RETURN_ON_ERROR(
                test_mouth_model_apply_eye_mesh(scene->right_eye_obj,
                                                (int32_t)scene->right_eye_cx,
                                                (int32_t)scene->right_eye_cy,
                                                TEST_MOUTH_MODEL_EYE_RX,
                                                ry_top, ry_bottom, -eye_tilt_l),
                TAG, "apply pose: right eye failed");
        }

        /* ── Apply eyebrows ─────────────────────────────────────────────── */
        if (scene->left_brow_obj != NULL) {
            int32_t bcy = (int32_t)scene->left_eye_cy + TEST_MOUTH_MODEL_BROW_Y_OFS - brow_raise;
            ESP_RETURN_ON_ERROR(
                test_mouth_model_apply_brow_mesh(scene->left_brow_obj,
                                                 (int32_t)scene->left_eye_cx,
                                                 bcy,
                                                 TEST_MOUTH_MODEL_BROW_HALF_W,
                                                 brow_arch, brow_tilt_l,
                                                 TEST_MOUTH_MODEL_BROW_THICKNESS),
                TAG, "apply pose: left brow failed");
        }
        if (scene->right_brow_obj != NULL) {
            int32_t bcy = (int32_t)scene->right_eye_cy + TEST_MOUTH_MODEL_BROW_Y_OFS - brow_raise;
            ESP_RETURN_ON_ERROR(
                test_mouth_model_apply_brow_mesh(scene->right_brow_obj,
                                                 (int32_t)scene->right_eye_cx,
                                                 bcy,
                                                 TEST_MOUTH_MODEL_BROW_HALF_W,
                                                 brow_arch, -brow_tilt_l,
                                                 TEST_MOUTH_MODEL_BROW_THICKNESS),
                TAG, "apply pose: right brow failed");
        }
    }

    return ESP_OK;
}

static void test_mouth_model_apply_scene_pose(test_mouth_model_scene_t *scene)
{
    test_mouth_model_mode_t mode;
    test_mouth_model_shape_t shape;
    const test_mouth_model_clip_t *clip;

    TEST_ASSERT_NOT_NULL(scene);
    clip = test_mouth_model_get_clip(scene->clip_idx);
    mode = test_mouth_model_select_mode(scene->current_smile, scene->current_open);
    shape = test_mouth_model_select_shape(scene->current_smile, scene->current_open);
    test_mouth_model_set_mode(scene, mode);
    TEST_ASSERT_EQUAL(ESP_OK, test_mouth_model_apply_pose(scene, scene->current_smile, scene->current_open));

    if (scene->hint_label != NULL) {
        const char *shape_name = (shape == TEST_MOUTH_MODEL_SHAPE_SAD_N)      ? "n" :
                                 (shape == TEST_MOUTH_MODEL_SHAPE_SMILE_U)     ? "U" :
                                 (shape == TEST_MOUTH_MODEL_SHAPE_SURPRISE_O)  ? "O" : "-";

        gfx_label_set_text_fmt(scene->hint_label, "%s:%s", clip->name, shape_name);
    }
}

static void test_mouth_model_anim_cb(void *user_data)
{
    test_mouth_model_scene_t *scene = (test_mouth_model_scene_t *)user_data;
    const test_mouth_model_clip_t *clip;
    int16_t next_smile;
    int16_t next_open;
    int16_t idle_smile_noise;
    int16_t idle_open_noise;

    if (scene == NULL) {
        return;
    }

    scene->anim_tick++;
    clip = test_mouth_model_get_clip(scene->clip_idx);
    idle_smile_noise = test_mouth_model_triangle_wave_i16(scene->anim_tick, 84U,
                                                          (uint16_t)(scene->clip_idx * 13U), 3);
    idle_open_noise = test_mouth_model_triangle_wave_i16(scene->anim_tick, 66U,
                                                         (uint16_t)(scene->clip_idx * 7U), 2);
    scene->target_smile = test_mouth_model_clamp_i16((int32_t)clip->smile + idle_smile_noise,
                                                     -TEST_MOUTH_MODEL_SMILE_LIMIT,
                                                     TEST_MOUTH_MODEL_SMILE_LIMIT);
    scene->target_open = test_mouth_model_clamp_i16((int32_t)clip->open + 2 + idle_open_noise,
                                                    0,
                                                    TEST_MOUTH_MODEL_OPEN_LIMIT);

    next_smile = test_mouth_model_follow_axis(scene->current_smile, scene->target_smile);
    next_open = test_mouth_model_follow_axis(scene->current_open, scene->target_open);
    scene->current_smile = next_smile;
    scene->current_open = next_open;
    test_mouth_model_apply_scene_pose(scene);

    if (scene->current_smile == scene->target_smile && scene->current_open == scene->target_open) {
        scene->hold_tick++;
        if (scene->hold_tick >= clip->hold_ticks) {
            scene->hold_tick = 0U;
            scene->clip_idx = (scene->clip_idx + 1U) % TEST_APP_ARRAY_SIZE(s_mouth_model_clips);
        }
    } else {
        scene->hold_tick = 0U;
    }
}

static void test_mouth_model_scene_cleanup(test_mouth_model_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->anim_timer != NULL) {
        gfx_timer_delete(emote_handle, scene->anim_timer);
        scene->anim_timer = NULL;
    }
    if (scene->right_brow_obj != NULL) {
        gfx_obj_delete(scene->right_brow_obj);
        scene->right_brow_obj = NULL;
    }
    if (scene->left_brow_obj != NULL) {
        gfx_obj_delete(scene->left_brow_obj);
        scene->left_brow_obj = NULL;
    }
    if (scene->right_eye_obj != NULL) {
        gfx_obj_delete(scene->right_eye_obj);
        scene->right_eye_obj = NULL;
    }
    if (scene->left_eye_obj != NULL) {
        gfx_obj_delete(scene->left_eye_obj);
        scene->left_eye_obj = NULL;
    }
    if (scene->right_corner_obj != NULL) {
        gfx_obj_delete(scene->right_corner_obj);
        scene->right_corner_obj = NULL;
    }
    if (scene->left_corner_obj != NULL) {
        gfx_obj_delete(scene->left_corner_obj);
        scene->left_corner_obj = NULL;
    }
    if (scene->lower_lip_obj != NULL) {
        gfx_obj_delete(scene->lower_lip_obj);
        scene->lower_lip_obj = NULL;
    }
    if (scene->upper_lip_obj != NULL) {
        gfx_obj_delete(scene->upper_lip_obj);
        scene->upper_lip_obj = NULL;
    }
    if (scene->mouth_obj != NULL) {
        gfx_obj_delete(scene->mouth_obj);
        scene->mouth_obj = NULL;
    }
    if (scene->title_label != NULL) {
        gfx_obj_delete(scene->title_label);
        scene->title_label = NULL;
    }
    if (scene->hint_label != NULL) {
        gfx_obj_delete(scene->hint_label);
        scene->hint_label = NULL;
    }
}

static void test_mouth_model_run(void)
{
    test_mouth_model_scene_t scene = {0};

    test_app_log_case(TAG, "Standalone mouth model");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    // gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x060B14));
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.mouth_obj = gfx_mesh_img_create(disp_default);
    scene.upper_lip_obj = gfx_mesh_img_create(disp_default);
    scene.lower_lip_obj = gfx_mesh_img_create(disp_default);
    scene.left_corner_obj = gfx_mesh_img_create(disp_default);
    scene.right_corner_obj = gfx_mesh_img_create(disp_default);
    scene.left_eye_obj = gfx_mesh_img_create(disp_default);
    scene.right_eye_obj = gfx_mesh_img_create(disp_default);
    scene.left_brow_obj = gfx_mesh_img_create(disp_default);
    scene.right_brow_obj = gfx_mesh_img_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    scene.hint_label = gfx_label_create(disp_default);
    scene.mouth_mode = TEST_MOUTH_MODEL_MODE_FLAT;
    TEST_ASSERT_NOT_NULL(scene.mouth_obj);
    TEST_ASSERT_NOT_NULL(scene.upper_lip_obj);
    TEST_ASSERT_NOT_NULL(scene.lower_lip_obj);
    TEST_ASSERT_NOT_NULL(scene.left_corner_obj);
    TEST_ASSERT_NOT_NULL(scene.right_corner_obj);
    TEST_ASSERT_NOT_NULL(scene.left_eye_obj);
    TEST_ASSERT_NOT_NULL(scene.right_eye_obj);
    TEST_ASSERT_NOT_NULL(scene.left_brow_obj);
    TEST_ASSERT_NOT_NULL(scene.right_brow_obj);
    TEST_ASSERT_NOT_NULL(scene.title_label);
    TEST_ASSERT_NOT_NULL(scene.hint_label);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mouth_obj, (void *)&face_parts_mouth_flat));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mouth_obj, TEST_MOUTH_MODEL_GRID_COLS, TEST_MOUTH_MODEL_GRID_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_visible(scene.mouth_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mouth_obj, GFX_ALIGN_CENTER, 0, TEST_MOUTH_MODEL_CENTER_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.upper_lip_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.upper_lip_obj, TEST_MOUTH_MODEL_DENSE_SEGMENTS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.upper_lip_obj, GFX_ALIGN_CENTER, 0, TEST_MOUTH_MODEL_CENTER_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.lower_lip_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.lower_lip_obj, TEST_MOUTH_MODEL_DENSE_SEGMENTS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.lower_lip_obj, GFX_ALIGN_CENTER, 0, TEST_MOUTH_MODEL_CENTER_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_corner_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_corner_obj, 1, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_corner_obj, GFX_ALIGN_CENTER, 0, TEST_MOUTH_MODEL_CENTER_Y_OFS));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_corner_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_corner_obj, 1, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_corner_obj, GFX_ALIGN_CENTER, 0, TEST_MOUTH_MODEL_CENTER_Y_OFS));

    /* Eyes: white-filled lens mesh (EYE_SEGS×1 grid → upper + lower arc rows). */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_eye_obj, GFX_ALIGN_CENTER, 0, 0));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_eye_obj, GFX_ALIGN_CENTER, 0, 0));

    /* Eyebrows: white-filled curved band (BROW_SEGS×1 grid). */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_brow_obj, GFX_ALIGN_CENTER, 0, 0));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_brow_obj, GFX_ALIGN_CENTER, 0, 0));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Face Rig: Eyes + Mouth"));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 310, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -10));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.hint_label, " idle  s:0  o:0  m:flat  shp:- "));

    test_mouth_model_capture_base_points(&scene);

    /* Derive eye screen-space centres from the mouth mesh centre point.
     * The mouth centre index is the middle of the grid:
     *   row = GRID_ROWS/2, col = GRID_COLS/2
     * Screen centre = mouth_origin + local base_point offset.
     */
    {
        size_t centre_idx = (TEST_MOUTH_MODEL_GRID_ROWS / 2U) * (TEST_MOUTH_MODEL_GRID_COLS + 1U) +
                            (TEST_MOUTH_MODEL_GRID_COLS / 2U);
        gfx_coord_t mcx = scene.mouth_origin_x + scene.base_points[centre_idx].x;
        gfx_coord_t mcy = scene.mouth_origin_y + scene.base_points[centre_idx].y;

        scene.left_eye_cx  = (gfx_coord_t)(mcx + TEST_MOUTH_MODEL_LEFT_EYE_X_OFS);
        scene.left_eye_cy  = (gfx_coord_t)(mcy + TEST_MOUTH_MODEL_EYE_Y_OFS);
        scene.right_eye_cx = (gfx_coord_t)(mcx + TEST_MOUTH_MODEL_RIGHT_EYE_X_OFS);
        scene.right_eye_cy = scene.left_eye_cy;
    }

    scene.clip_idx = 0U;
    scene.target_smile = test_mouth_model_get_clip(scene.clip_idx)->smile;
    scene.target_open = test_mouth_model_get_clip(scene.clip_idx)->open;
    test_mouth_model_apply_scene_pose(&scene);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mouth_model_anim_cb, TEST_MOUTH_MODEL_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Face cycles: idle→happy→laughing→funny→loving→embarrassed→confident→delicious→sad→crying→sleepy→silly→angry→surprised→shocked→thinking→winking→relaxed→confused→neutral→idle");
    test_app_wait_for_observe(120000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_mouth_model_scene_cleanup(&scene);
    test_app_unlock();
}

void test_mouth_model_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_mouth_model_run();
    test_app_runtime_close(&runtime);
}
