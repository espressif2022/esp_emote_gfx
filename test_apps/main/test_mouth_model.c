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

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 0 — Display geometry and absolute screen anchor positions
 *
 * Every widget's initial position is an absolute screen coordinate.
 * Moving a widget is a one-constant change; no solver code needs editing.
 * ═══════════════════════════════════════════════════════════════════════════ */
#define FACE_DISPLAY_W          360
#define FACE_DISPLAY_H          360
#define FACE_DISPLAY_CX         (FACE_DISPLAY_W / 2)          /* 180 */
#define FACE_DISPLAY_CY         (FACE_DISPLAY_H / 2)          /* 180 */

/* Mouth anchor object (invisible bone mesh) */
#define FACE_MOUTH_CX           FACE_DISPLAY_CX               /* 160 */
#define FACE_MOUTH_CY           (FACE_DISPLAY_CY + 40)        /* 220 */
#define FACE_MOUTH_OBJ_W        148
#define FACE_MOUTH_OBJ_H        80

/* Eyes and eyebrows — offsets from mouth centre, in pixels.
 * Actual screen positions are derived at runtime from the mouth object's
 * real position, so the layout works on any display size.               */
#define FACE_EYE_X_HALF_GAP     50   /* half-distance between left and right eye centres */
#define FACE_EYE_X_BIAS         (-20) /* eye/brow global X bias: negative shifts left      */
#define FACE_EYE_Y_OFS         (-92) /* eye centre above mouth centre                    */
#define FACE_BROW_Y_OFS_EXTRA  (-34) /* brow centre above eye centre                     */

/* Convenience: absolute constants for documentation only (assumes 320×240).
 * Solvers must NOT use these directly — use the runtime-derived values instead. */
#define FACE_LEFT_EYE_CX        (FACE_MOUTH_CX - FACE_EYE_X_HALF_GAP + FACE_EYE_X_BIAS)  /* 108 */
#define FACE_LEFT_EYE_CY        (FACE_MOUTH_CY + FACE_EYE_Y_OFS)       /* 112 */
#define FACE_RIGHT_EYE_CX       (FACE_MOUTH_CX + FACE_EYE_X_HALF_GAP + FACE_EYE_X_BIAS)  /* 212 */
#define FACE_RIGHT_EYE_CY       FACE_LEFT_EYE_CY
#define FACE_LEFT_BROW_CX       FACE_LEFT_EYE_CX
#define FACE_LEFT_BROW_CY       (FACE_LEFT_EYE_CY + FACE_BROW_Y_OFS_EXTRA)
#define FACE_RIGHT_BROW_CX      FACE_RIGHT_EYE_CX
#define FACE_RIGHT_BROW_CY      FACE_LEFT_BROW_CY

/* ═══════════════════════════════════════════════════════════════════════════
 * Mesh / solver constants (kept here for cross-reference with anchors above)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define TEST_MOUTH_MODEL_GRID_COLS        14U
#define TEST_MOUTH_MODEL_GRID_ROWS        6U
#define TEST_MOUTH_MODEL_POINT_COUNT      ((TEST_MOUTH_MODEL_GRID_COLS + 1U) * (TEST_MOUTH_MODEL_GRID_ROWS + 1U))
#define TEST_MOUTH_MODEL_TIMER_PERIOD_MS  33U
#define TEST_MOUTH_MODEL_SMILE_LIMIT      42
#define TEST_MOUTH_MODEL_OPEN_LIMIT       40
#define TEST_MOUTH_MODEL_WIDTH_LIMIT      12
#define TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT 24
#define TEST_MOUTH_MODEL_CENTER_Y_OFS     40   /* = FACE_MOUTH_CY - FACE_DISPLAY_CY */
#define TEST_MOUTH_MODEL_MODE_THRESH      12
#define TEST_MOUTH_MODEL_DENSE_SEGMENTS   16U
#define TEST_MOUTH_MODEL_DENSE_POINT_COUNT (TEST_MOUTH_MODEL_DENSE_SEGMENTS + 1U)
#define TEST_MOUTH_MODEL_SAD_THRESH       10
#define TEST_MOUTH_MODEL_SMILE_THRESH     10
#define TEST_MOUTH_MODEL_SURPRISE_OPEN    24
#define TEST_MOUTH_MODEL_EYE_SEGS         12U
#define TEST_MOUTH_MODEL_EYE_PT_COUNT     (TEST_MOUTH_MODEL_EYE_SEGS + 1U)
#define TEST_MOUTH_MODEL_EYE_RX           28      /* x-radius per eye */
#define TEST_MOUTH_MODEL_EYE_RY           28      /* y-radius per eye */
#define TEST_MOUTH_MODEL_LEFT_EYE_X_OFS   (-50)
#define TEST_MOUTH_MODEL_RIGHT_EYE_X_OFS  50
#define TEST_MOUTH_MODEL_EYE_Y_OFS        (-92)   /* px above mouth centre */
#define TEST_MOUTH_MODEL_FRIGHT_OPEN      36      /* open threshold for frightened vs surprised  */
#define TEST_MOUTH_MODEL_BROW_SEGS        10U
#define TEST_MOUTH_MODEL_BROW_PT_COUNT    (TEST_MOUTH_MODEL_BROW_SEGS + 1U)
#define TEST_MOUTH_MODEL_BROW_HALF_W      30                             /* short rounded brow arc  */
#define TEST_MOUTH_MODEL_BROW_THICKNESS   4                              /* half-thickness in px    */
#define TEST_MOUTH_MODEL_BROW_Y_OFS       (-34)                          /* px above eye centre     */
#define TEST_MOUTH_MODEL_CORNER_ROWS      6U                             /* bezier rows per corner  */
#define TEST_MOUTH_MODEL_CORNER_PT_COUNT  ((TEST_MOUTH_MODEL_CORNER_ROWS + 1U) * 2U)

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

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 1 — Widget keyframe types
 *
 * Each struct is the complete description of one widget's target pose.
 * The vertex solvers (Layer 5) consume these directly — no formula
 * indirection, no weight reconstruction.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Eye keyframe.
 *   ry_top    : upper eyelid arc height [px].  Smaller = squints down.
 *   ry_bottom : lower eyelid arc height [px].  Smaller = squints up.
 *   tilt      : left eye: +ve = outer-left corner droops.
 *               Right eye solver always receives -tilt for symmetric mirroring.
 */
typedef struct {
    int16_t ry_top;
    int16_t ry_bottom;
    int16_t tilt;
} face_eye_kf_t;

/*
 * Eyebrow keyframe.
 *   arch   : centre arch height [px]; +ve = convex upward, -ve = V-shape.
 *   raise  : whole-brow vertical offset [px]; +ve = moves up.
 *   tilt   : same sign convention as eye.tilt.
 */
typedef struct {
    int16_t arch;
    int16_t raise;
    int16_t tilt;
} face_brow_kf_t;

/*
 * Mouth keyframe.
 *   smile : [-42..+42]  corner height; +ve = smile, -ve = frown.
 *   open  : [0..40]     jaw opening depth.
 *   width : [-24..+24]  horizontal expansion (+ wider, - narrower)
 *   upper : [-24..+24]  upper contour centre bias (+ pressed down)
 *   lower : [-24..+24]  lower contour centre bias (+ fuller down)
 *   pinch : [-24..+24]  corner aperture taper (+ sharper corners)
 *   corner: [-24..+24]  corner vertical bias (+ corners drop)
 */
typedef struct {
    int16_t smile;
    int16_t open;
    int16_t width;
    int16_t upper;
    int16_t lower;
    int16_t pinch;
    int16_t corner;
} face_mouth_kf_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 2 — User-facing emotion controller  (face_emotion_t)
 *
 * Users think in terms of emotions, not mesh vertices.
 * face_blend_emotion() converts these five weights into per-widget keyframes
 * automatically, so the user never needs to touch eye/brow/mouth KF values.
 *
 * All weights are [0..100] and additive (they can sum beyond 100 for
 * over-driven combos).  Zero on all weights = neutral face.
 *
 *   w_smile   : gentle corner lift, mild upper-lid squint
 *   w_happy   : big grin, strong squint (crescent eyes)
 *   w_sad     : frown, outer-corner eye/brow droop
 *   w_surprise: mouth wide open, eyelids & brows raised
 *   w_angry   : V-shape brows, frown + open (snarl)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char  *name;
    int16_t      w_smile;
    int16_t      w_happy;
    int16_t      w_sad;
    int16_t      w_surprise;
    int16_t      w_angry;
    uint16_t     hold_ticks;
} face_emotion_t;

/* ─── Pure expression keyframes + sequence tables ────────────────────────
 * Generated from scripts/face_keyframes*.json by:
 *   python3 scripts/generate_face_keyframes_c.py
 * ─────────────────────────────────────────────────────────────────────── */
#include "test_mouth_model_keyframes.inc"

/*
 * face_blend_emotion — convert user-facing emotion weights into per-widget KFs.
 *
 * Formula (additive delta from neutral):
 *   out.field = neutral.field
 *             + (smile.field   - neutral.field) * w_smile   / 100
 *             + (happy.field   - neutral.field) * w_happy   / 100
 *             + (sad.field     - neutral.field) * w_sad     / 100
 *             + (surprise.field - neutral.field) * w_surprise / 100
 *             + (angry.field   - neutral.field) * w_angry   / 100
 */
static void face_blend_emotion(const face_emotion_t *em,
                                face_eye_kf_t        *eye_out,
                                face_brow_kf_t       *brow_out,
                                face_mouth_kf_t      *mouth_out)
{
#define BLEND_F(tbl, field) \
    ((int32_t)(tbl)[0].field \
   + ((int32_t)(tbl)[1].field - (tbl)[0].field) * (em->w_smile)   / 100 \
   + ((int32_t)(tbl)[2].field - (tbl)[0].field) * (em->w_happy)   / 100 \
   + ((int32_t)(tbl)[3].field - (tbl)[0].field) * (em->w_sad)     / 100 \
   + ((int32_t)(tbl)[4].field - (tbl)[0].field) * (em->w_surprise) / 100 \
   + ((int32_t)(tbl)[5].field - (tbl)[0].field) * (em->w_angry)   / 100)

    eye_out->ry_top    = (int16_t)BLEND_F(s_ref_eye,   ry_top);
    eye_out->ry_bottom = (int16_t)BLEND_F(s_ref_eye,   ry_bottom);
    eye_out->tilt      = (int16_t)BLEND_F(s_ref_eye,   tilt);
    brow_out->arch     = (int16_t)BLEND_F(s_ref_brow,  arch);
    brow_out->raise    = (int16_t)BLEND_F(s_ref_brow,  raise);
    brow_out->tilt     = (int16_t)BLEND_F(s_ref_brow,  tilt);
    {
        int32_t sm = BLEND_F(s_ref_mouth, smile);
        int32_t op = BLEND_F(s_ref_mouth, open);
        int32_t wd = BLEND_F(s_ref_mouth, width);
        int32_t up = BLEND_F(s_ref_mouth, upper);
        int32_t lo = BLEND_F(s_ref_mouth, lower);
        int32_t pi = BLEND_F(s_ref_mouth, pinch);
        int32_t co = BLEND_F(s_ref_mouth, corner);
        if (sm < -TEST_MOUTH_MODEL_SMILE_LIMIT) { sm = -TEST_MOUTH_MODEL_SMILE_LIMIT; }
        if (sm >  TEST_MOUTH_MODEL_SMILE_LIMIT) { sm =  TEST_MOUTH_MODEL_SMILE_LIMIT; }
        if (op < 0)                              { op = 0; }
        if (op > TEST_MOUTH_MODEL_OPEN_LIMIT)    { op = TEST_MOUTH_MODEL_OPEN_LIMIT; }
        if (wd < -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { wd = -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (wd >  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { wd =  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (up < -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { up = -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (up >  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { up =  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (lo < -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { lo = -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (lo >  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { lo =  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (pi < -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { pi = -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (pi >  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { pi =  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (co < -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { co = -TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        if (co >  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT) { co =  TEST_MOUTH_MODEL_SHAPE_CTRL_LIMIT; }
        mouth_out->smile = (int16_t)sm;
        mouth_out->open  = (int16_t)op;
        mouth_out->width = (int16_t)wd;
        mouth_out->upper = (int16_t)up;
        mouth_out->lower = (int16_t)lo;
        mouth_out->pinch = (int16_t)pi;
        mouth_out->corner = (int16_t)co;
    }

#undef BLEND_F
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 4 — Easing / transition function
 *
 * face_ease_spring: exponential spring, step = diff/4, minimum ±1.
 * A 42-unit transition (full smile → full frown) completes in ≈5 ticks.
 * Swap for a different function to change the feel of all transitions.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef int16_t (*face_ease_fn_t)(int16_t cur, int16_t tgt);

static int16_t face_ease_spring(int16_t cur, int16_t tgt)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    step = diff / 4;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)((int32_t)cur + step);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 3 — Scene (per-widget animation state)
 *
 * For every widget there is a `cur` (current) and `tgt` (target) keyframe.
 * The easing function advances `cur` toward `tgt` each tick.
 * When an expression advances, only the `*_tgt` fields are updated.
 *
 * Eye/brow anchor positions are compile-time constants (FACE_*_CX/CY);
 * they are NOT stored in the scene struct, eliminating the runtime
 * derivation from the mouth mesh centre point.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    /* GFX objects */
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

    /* Mouth bone mesh bookkeeping (still needed for procedural lip solver) */
    gfx_mesh_img_point_t base_points[TEST_MOUTH_MODEL_POINT_COUNT];
    gfx_coord_t mouth_origin_x;
    gfx_coord_t mouth_origin_y;

    /* Per-widget keyframe state: cur is interpolated toward tgt each tick */
    face_eye_kf_t    eye_cur;
    face_eye_kf_t    eye_tgt;
    face_brow_kf_t   brow_cur;
    face_brow_kf_t   brow_tgt;
    face_mouth_kf_t  mouth_cur;
    face_mouth_kf_t  mouth_tgt;

    /* Sequencer state */
    uint16_t hold_tick;
    uint32_t anim_tick;
    size_t   expr_idx;

    /* Mouth mesh mode (flat / smile / open) */
    test_mouth_model_mode_t mouth_mode;
} test_mouth_model_scene_t;

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

/*
 * smooth_tent(i, N)  —  cubic-smoothstep tent, range [0..1000].
 *
 * Unlike a linear tent, this function has zero slope at both endpoints
 * (i=0 and i=N) and at the peak (i=N/2), so curves lift off from their
 * anchors without any sudden kink.
 *
 * Derivation: reflect the interval so the input is always in [0, N/2],
 * normalise to x ∈ [0, 1000], then apply the cubic smoothstep
 *   f(x) = x²·(3000 − 2x) / 10⁶
 * which satisfies f(0)=0, f'(0)=0, f(1000)=1000, f'(1000)=0.
 *
 * Requires N to be even for an exact 1000 peak; for odd N the peak is ≈990.
 */
static int32_t test_mouth_model_smooth_tent(size_t i, size_t N)
{
    int32_t half_i;
    int32_t x;

    if (N == 0U) {
        return 0;
    }
    half_i = (int32_t)(2U * ((i <= N / 2U) ? i : (N - i)));  /* 0 .. N   */
    x      = half_i * 1000 / (int32_t)N;                      /* 0 .. 1000 */
    return x * x * (3000 - 2 * x) / 1000000;                  /* 0 .. 1000 */
}

/* Integer sqrt for non-negative 32-bit values. */
static int32_t test_mouth_model_isqrt_i32(int32_t value)
{
    uint32_t op;
    uint32_t res;
    uint32_t one;

    if (value <= 0) {
        return 0;
    }

    op = (uint32_t)value;
    res = 0U;
    one = 1UL << 30;  /* second-to-top bit */

    while (one > op) {
        one >>= 2;
    }
    while (one != 0U) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (int32_t)res;
}

/*
 * Circular arc weight in [0..1000], matching script preview:
 *   w = sqrt(1 - u^2),  u in [-1, +1]
 * represented with u scaled by 1000.
 */
static int32_t test_mouth_model_circle_weight(size_t i, size_t segs)
{
    int32_t u_q;
    int32_t inside;

    if (segs == 0U) {
        return 0;
    }
    u_q = (int32_t)(2U * i * 1000U / segs) - 1000;
    inside = 1000000 - u_q * u_q;  /* 1000^2 * (1-u^2) */
    if (inside <= 0) {
        return 0;
    }
    return test_mouth_model_isqrt_i32(inside);  /* 0..1000 */
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

/*
 * Corner arc rendered as a quadratic Bézier strip with CORNER_ROWS subdivisions.
 *
 * Control points P0→P1→P2 define the curve:
 *   P0 = top attachment on the lip edge
 *   P1 = corner apex (protruding outward)
 *   P2 = bottom attachment on the lip edge
 *
 * Each row i samples the Bézier at  t = i / CORNER_ROWS:
 *   B(t) = (1-t)² · P0  +  2t(1-t) · P1  +  t² · P2
 *
 * The strip has 2 columns (left-edge, right-edge = ±half_thickness from centre),
 * producing CORNER_ROWS quads = 2×CORNER_ROWS triangles — far smoother than the
 * previous 4-triangle approximation.
 */
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
    gfx_mesh_img_point_t points[TEST_MOUTH_MODEL_CORNER_PT_COUNT];
    int32_t den;
    int32_t min_scr_x;
    int32_t min_scr_y;
    gfx_coord_t min_x;
    gfx_coord_t min_y;
    size_t i;

    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_INVALID_ARG, TAG, "apply vertical arc mesh: object is NULL");

    den = (int32_t)TEST_MOUTH_MODEL_CORNER_ROWS;

    /* First pass: compute all Bézier sample positions and find bounding box. */
    min_scr_x = (int32_t)origin_x + point0_x - half_thickness;
    min_scr_y = (int32_t)origin_y + point0_y;
    for (i = 0; i <= TEST_MOUTH_MODEL_CORNER_ROWS; i++) {
        int32_t t   = (int32_t)i;
        int32_t w0  = (den - t) * (den - t);
        int32_t w1  = 2 * t * (den - t);
        int32_t w2  = t * t;
        int32_t den2 = den * den;
        int32_t bx  = (w0 * point0_x + w1 * point1_x + w2 * point2_x) / den2;
        int32_t by  = (w0 * point0_y + w1 * point1_y + w2 * point2_y) / den2;
        int32_t sx  = (int32_t)origin_x + bx;
        int32_t sy  = (int32_t)origin_y + by;
        if (sx - half_thickness < min_scr_x) {
            min_scr_x = sx - half_thickness;
        }
        if (sy < min_scr_y) {
            min_scr_y = sy;
        }
    }
    min_x = (gfx_coord_t)min_scr_x;
    min_y = (gfx_coord_t)min_scr_y;

    /* Second pass: fill mesh points as local offsets from (min_x, min_y). */
    for (i = 0; i <= TEST_MOUTH_MODEL_CORNER_ROWS; i++) {
        int32_t t   = (int32_t)i;
        int32_t w0  = (den - t) * (den - t);
        int32_t w1  = 2 * t * (den - t);
        int32_t w2  = t * t;
        int32_t den2 = den * den;
        int32_t bx  = (w0 * point0_x + w1 * point1_x + w2 * point2_x) / den2;
        int32_t by  = (w0 * point0_y + w1 * point1_y + w2 * point2_y) / den2;
        int32_t sx  = (int32_t)origin_x + bx;
        int32_t sy  = (int32_t)origin_y + by;

        points[2U * i].x      = (gfx_coord_t)(sx - half_thickness - (int32_t)min_x);
        points[2U * i].y      = (gfx_coord_t)(sy - (int32_t)min_y);
        points[2U * i + 1U].x = (gfx_coord_t)(sx + half_thickness - (int32_t)min_x);
        points[2U * i + 1U].y = (gfx_coord_t)(sy - (int32_t)min_y);
    }

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
    /* smile widens corners only half as much as before (open still narrows them). */
    int32_t width_bias = smile / 2 - (open / 2);
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
        int32_t edge_w = test_mouth_model_smooth_tent(i, TEST_MOUTH_MODEL_BROW_SEGS);
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
        int32_t edge_w = test_mouth_model_smooth_tent(i, TEST_MOUTH_MODEL_BROW_SEGS);
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
        int32_t tilt   = tilt_at_left - (tilt_at_left * 2 * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_EYE_SEGS;
        int32_t edge_w = test_mouth_model_circle_weight(i, TEST_MOUTH_MODEL_EYE_SEGS);
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
        int32_t px     = cx - rx + ((2 * rx * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_EYE_SEGS);
        int32_t edge_w = test_mouth_model_circle_weight(i, TEST_MOUTH_MODEL_EYE_SEGS);
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

static esp_err_t test_mouth_model_apply_pose(test_mouth_model_scene_t *scene, const face_mouth_kf_t *mouth)
{
    test_mouth_model_bone_pose_t bones[TEST_MOUTH_MODEL_BONE_COUNT];
    test_mouth_model_shape_t shape;
    int16_t mode_smile;
    int16_t mode_open;
    int32_t left_x;
    int32_t right_x;
    int32_t center_y;
    int32_t x_points[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t upper_mid_y[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t lower_mid_y[TEST_MOUTH_MODEL_DENSE_POINT_COUNT];
    int32_t mid_y;
    int32_t aperture_upper;
    int32_t aperture_lower;
    int32_t left_corner_mid_x;
    int32_t right_corner_mid_x;
    int32_t left_corner_mid_y;
    int32_t right_corner_mid_y;
    int32_t line_half_thickness;
    int32_t corner_half_thickness;
    int32_t width_trim;
    int32_t min_aperture;
    int32_t mouth_width;
    int32_t mouth_upper;
    int32_t mouth_lower;
    int32_t mouth_pinch;
    int32_t mouth_corner;

    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: scene is NULL");
    ESP_RETURN_ON_FALSE(mouth != NULL, ESP_ERR_INVALID_ARG, TAG, "apply pose: mouth kf is NULL");
    ESP_RETURN_ON_FALSE(scene->mouth_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: anchor object is NULL");
    ESP_RETURN_ON_FALSE(scene->upper_lip_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: upper lip object is NULL");
    ESP_RETURN_ON_FALSE(scene->lower_lip_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: lower lip object is NULL");
    ESP_RETURN_ON_FALSE(scene->left_corner_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: left corner object is NULL");
    ESP_RETURN_ON_FALSE(scene->right_corner_obj != NULL, ESP_ERR_INVALID_STATE, TAG, "apply pose: right corner object is NULL");

    mode_smile = mouth->smile;
    mode_open = mouth->open;
    mouth_width = mouth->width;
    mouth_upper = mouth->upper;
    mouth_lower = mouth->lower;
    mouth_pinch = mouth->pinch;
    mouth_corner = mouth->corner;

    if (scene->mouth_mode == TEST_MOUTH_MODEL_MODE_SMILE) {
        mode_smile = mouth->smile - TEST_MOUTH_MODEL_MODE_THRESH;
    } else if (scene->mouth_mode == TEST_MOUTH_MODEL_MODE_OPEN) {
        mode_open = mouth->open - TEST_MOUTH_MODEL_MODE_THRESH;
    }

    test_mouth_model_build_bone_pose(scene, mode_smile, mode_open, bones);
    shape = test_mouth_model_select_shape(mouth->smile, mouth->open);
    left_x = bones[TEST_MOUTH_MODEL_BONE_LEFT_CORNER].x + bones[TEST_MOUTH_MODEL_BONE_LEFT_CORNER].dx;
    right_x = bones[TEST_MOUTH_MODEL_BONE_RIGHT_CORNER].x + bones[TEST_MOUTH_MODEL_BONE_RIGHT_CORNER].dx;
    center_y = bones[TEST_MOUTH_MODEL_BONE_CENTER].y + bones[TEST_MOUTH_MODEL_BONE_CENTER].dy;
    left_x -= mouth_width;
    right_x += mouth_width;

    width_trim = test_mouth_model_max_i32(0, mode_open / 6);
    left_x += width_trim;
    right_x -= width_trim;
    line_half_thickness = 2 + (mode_open / 24);
    min_aperture = line_half_thickness + 1;
    for (size_t i = 0; i < TEST_MOUTH_MODEL_DENSE_POINT_COUNT; i++) {
        /*
         * cw  —  smooth centre-weight in [0..1000].
         *
         * Uses cubic smoothstep (smooth_tent) instead of a linear tent so
         * that the curve meets its endpoints with zero slope.  This removes
         * the visible "kink" at the mouth corners and makes every shape look
         * like a single continuous arc rather than two joined straight lines.
         *
         * Comparison at i=1 (SEGS=16, near the corner):
         *   linear tent     : weight = 875 / 1000  → curve starts rising hard
         *   smooth_tent     : weight ~  43 / 1000  → curve barely lifts off
         * The smooth variant gives a much flatter shoulder near the corners,
         * creating the natural "corners stay put while centre arcs" effect.
         */
        int32_t cw;
        int32_t edge_w;
        int32_t sad_mag;
        int32_t smile_mag;
        int32_t corner_shift;
        int32_t upper_shape_shift;
        int32_t lower_shape_shift;
        int32_t pinch;
        int32_t upper_y;
        int32_t lower_y;

        x_points[i] = left_x + (((right_x - left_x) * (int32_t)i) / (int32_t)TEST_MOUTH_MODEL_DENSE_SEGMENTS);
        cw = test_mouth_model_smooth_tent(i, TEST_MOUTH_MODEL_DENSE_SEGMENTS);
        edge_w = 1000 - cw;

        /* Defaults — overridden by each shape case below. */
        mid_y         = center_y;
        aperture_upper = min_aperture;
        aperture_lower = min_aperture;

        switch (shape) {
        case TEST_MOUTH_MODEL_SHAPE_SAD_N:
            /*
             * ∩ frown.  Mouth corners pull DOWN (positive y), midline arcs UP.
             *
             * mid_y at corners : center_y + sad_mag/4   (corners drop)
             * mid_y at centre  : center_y − sad_mag/2   (centre lifts)
             *
             * Upper lip: tense and thin (frowning upper lip).
             * Lower lip: slightly more open at centre — a "trembling chin".
             */
            sad_mag = test_mouth_model_abs_i32(
                test_mouth_model_clamp_i16(mode_smile, -TEST_MOUTH_MODEL_SMILE_LIMIT, 0));
            mid_y = center_y + (sad_mag / 4)
                  - (cw * (sad_mag / 4 + sad_mag / 2) / 1000);
            aperture_upper = line_half_thickness + 1 + (mode_open / 14)
                           + (cw * (1 + mode_open / 12) / 1000);
            aperture_lower = line_half_thickness + 2 + (mode_open / 10)
                           + (cw * (2 + mode_open /  8) / 1000);
            break;

        case TEST_MOUTH_MODEL_SHAPE_SMILE_U:
            /*
             * ∪ smile.  Mouth corners lift UP (negative y), midline arcs DOWN.
             *
             * mid_y at corners : center_y − smile_mag/4   (corners rise)
             * mid_y at centre  : center_y + smile_mag/2   (centre dips)
             *
             * Upper lip: thin, relatively flat — sits above the smile line.
             * Lower lip: full and rounded — forms the visible "U" bottom arc.
             */
            smile_mag = test_mouth_model_max_i32(0, mode_smile);
            mid_y = center_y - (smile_mag / 4)
                  + (cw * (smile_mag / 4 + smile_mag / 2) / 1000);
            aperture_upper = line_half_thickness + 1 + (mode_open / 14)
                           + (cw * (1 + mode_open / 12) / 1000);
            aperture_lower = line_half_thickness + 2 + (mode_open / 10)
                           + (cw * (3 + mode_open /  8) / 1000);
            break;

        case TEST_MOUTH_MODEL_SHAPE_SURPRISE_O:
            /*
             * O surprise.
             *
             * Midline bows gently downward at the centre so the opening
             * looks naturally rounded rather than a flat slit.
             *
             * Upper arc  (D-shape top)  : 90 % of half_gap  — slightly flatter.
             * Lower arc  (∪-shape base) : 110 % of half_gap — slightly fuller.
             *
             * This mimics the real anatomy where the lower lip hangs lower
             * than the upper lip rises during an open-mouthed surprise.
             */
            {
                int32_t half_gap;
                mid_y    = center_y + (cw * (mode_open / 10) / 1000);
                half_gap = 4 + mode_open / 3
                         + (cw * (3 + mode_open / 4) / 1000);
                aperture_upper = (half_gap * 9) / 10;
                aperture_lower = (half_gap * 11) / 10;
            }
            break;

        case TEST_MOUTH_MODEL_SHAPE_IDLE:
        default:
            /*
             * Closed resting mouth.
             * A 1 px upward bow at the centre gives a subtle "content" look
             * rather than a completely dead-straight line.
             */
            mid_y          = center_y - (cw / 1000);   /* ≤ 1 px lift at centre */
            aperture_upper = line_half_thickness + 1;
            aperture_lower = line_half_thickness + 1;
            break;
        }

        /* Guarantee minimum lip separation so they never visually merge. */
        if (aperture_upper < min_aperture) {
            aperture_upper = min_aperture;
        }
        if (aperture_lower < min_aperture) {
            aperture_lower = min_aperture;
        }
        corner_shift = edge_w * mouth_corner / 1000;
        upper_shape_shift = cw * mouth_upper / 1000;
        lower_shape_shift = cw * mouth_lower / 1000;
        pinch = edge_w * mouth_pinch / 1000;

        upper_y = (mid_y - aperture_upper) + corner_shift + upper_shape_shift + pinch;
        lower_y = (mid_y + aperture_lower) + corner_shift + lower_shape_shift - pinch;
        if (lower_y <= upper_y) {
            lower_y = upper_y + 1;
        }
        upper_mid_y[i] = upper_y;
        lower_mid_y[i] = lower_y;
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

    /* Eyes and eyebrows are driven by per-widget keyframes in
     * test_mouth_model_apply_scene_pose; they are NOT solved here. */
    return ESP_OK;
}

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * apply_scene_pose — Layer 5 vertex solvers called each tick
 *
 * Per-tick evaluation order:
 *   1. face_ease_spring advances each keyframe field (cur → tgt)
 *   2. face_solve_mouth  — bone pose → lip-strip + corner-arc meshes
 *   3. face_solve_eye    — left  eye (+tilt)
 *   4. face_solve_eye    — right eye (−tilt, mirrored)
 *   5. face_solve_brow   — left  brow (+tilt, raise applied to anchor CY)
 *   6. face_solve_brow   — right brow (−tilt, mirrored)
 *   7. hint label updated with current expression name and mouth shape
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void test_mouth_model_apply_scene_pose(test_mouth_model_scene_t *scene)
{
    test_mouth_model_mode_t  mode;
    test_mouth_model_shape_t shape;

    TEST_ASSERT_NOT_NULL(scene);

    /* ── Layer 4: ease each widget keyframe field toward its target ─────────── */
    scene->eye_cur.ry_top    = face_ease_spring(scene->eye_cur.ry_top,    scene->eye_tgt.ry_top);
    scene->eye_cur.ry_bottom = face_ease_spring(scene->eye_cur.ry_bottom, scene->eye_tgt.ry_bottom);
    scene->eye_cur.tilt      = face_ease_spring(scene->eye_cur.tilt,      scene->eye_tgt.tilt);
    scene->brow_cur.arch     = face_ease_spring(scene->brow_cur.arch,     scene->brow_tgt.arch);
    scene->brow_cur.raise    = face_ease_spring(scene->brow_cur.raise,    scene->brow_tgt.raise);
    scene->brow_cur.tilt     = face_ease_spring(scene->brow_cur.tilt,     scene->brow_tgt.tilt);
    scene->mouth_cur.smile   = face_ease_spring(scene->mouth_cur.smile,   scene->mouth_tgt.smile);
    scene->mouth_cur.open    = face_ease_spring(scene->mouth_cur.open,    scene->mouth_tgt.open);
    scene->mouth_cur.width   = face_ease_spring(scene->mouth_cur.width,   scene->mouth_tgt.width);
    scene->mouth_cur.upper   = face_ease_spring(scene->mouth_cur.upper,   scene->mouth_tgt.upper);
    scene->mouth_cur.lower   = face_ease_spring(scene->mouth_cur.lower,   scene->mouth_tgt.lower);
    scene->mouth_cur.pinch   = face_ease_spring(scene->mouth_cur.pinch,   scene->mouth_tgt.pinch);
    scene->mouth_cur.corner  = face_ease_spring(scene->mouth_cur.corner,  scene->mouth_tgt.corner);

    /* ── Layer 5a: face_solve_mouth — procedural bone-driven mesh ───────────── */
    mode  = test_mouth_model_select_mode(scene->mouth_cur.smile, scene->mouth_cur.open);
    shape = test_mouth_model_select_shape(scene->mouth_cur.smile, scene->mouth_cur.open);
    (void)shape;   /* used for label only */
    test_mouth_model_set_mode(scene, mode);
    TEST_ASSERT_EQUAL(ESP_OK,
        test_mouth_model_apply_pose(scene, &scene->mouth_cur));

    /* ── Derive eye/brow anchor centres from the mouth object's actual screen
     *    position.  This is display-size-agnostic: only the offset constants
     *    (FACE_EYE_X_HALF_GAP, FACE_EYE_Y_OFS, FACE_BROW_Y_OFS_EXTRA) are
     *    hard-coded; the absolute coordinates adapt to any panel size.       */
    {
        int32_t mcx = (int32_t)scene->mouth_origin_x + FACE_MOUTH_OBJ_W / 2;
        int32_t mcy = (int32_t)scene->mouth_origin_y + FACE_MOUTH_OBJ_H / 2;
        int32_t eye_l_cx  = mcx - FACE_EYE_X_HALF_GAP + FACE_EYE_X_BIAS;
        int32_t eye_r_cx  = mcx + FACE_EYE_X_HALF_GAP + FACE_EYE_X_BIAS;
        int32_t eye_cy    = mcy + FACE_EYE_Y_OFS;
        int32_t brow_cy   = eye_cy + FACE_BROW_Y_OFS_EXTRA - (int32_t)scene->brow_cur.raise;

        /* ── Layer 5b: face_solve_eye ─────────────────────────────────────── */
        if (scene->left_eye_obj != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK,
                test_mouth_model_apply_eye_mesh(scene->left_eye_obj,
                                                eye_l_cx, eye_cy,
                                                TEST_MOUTH_MODEL_EYE_RX,
                                                (int32_t)scene->eye_cur.ry_top,
                                                (int32_t)scene->eye_cur.ry_bottom,
                                                (int32_t)scene->eye_cur.tilt));
        }
        if (scene->right_eye_obj != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK,
                test_mouth_model_apply_eye_mesh(scene->right_eye_obj,
                                                eye_r_cx, eye_cy,
                                                TEST_MOUTH_MODEL_EYE_RX,
                                                (int32_t)scene->eye_cur.ry_top,
                                                (int32_t)scene->eye_cur.ry_bottom,
                                                -(int32_t)scene->eye_cur.tilt));
        }

        /* ── Layer 5c: face_solve_brow ─────────────────────────────────────── */
        if (scene->left_brow_obj != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK,
                test_mouth_model_apply_brow_mesh(scene->left_brow_obj,
                                                 eye_l_cx, brow_cy,
                                                 TEST_MOUTH_MODEL_BROW_HALF_W,
                                                 (int32_t)scene->brow_cur.arch,
                                                 (int32_t)scene->brow_cur.tilt,
                                                 TEST_MOUTH_MODEL_BROW_THICKNESS));
        }
        if (scene->right_brow_obj != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK,
                test_mouth_model_apply_brow_mesh(scene->right_brow_obj,
                                                 eye_r_cx, brow_cy,
                                                 TEST_MOUTH_MODEL_BROW_HALF_W,
                                                 (int32_t)scene->brow_cur.arch,
                                                 -(int32_t)scene->brow_cur.tilt,
                                                 TEST_MOUTH_MODEL_BROW_THICKNESS));
        }
    }

    /* ── Labels ──────────────────────────────────────────────────────────────── */
    {
        const face_emotion_t *em = &s_face_sequence[scene->expr_idx];
        if (scene->hint_label != NULL) {
            gfx_label_set_text_fmt(scene->hint_label,
                                   "Sm:%d Hp:%d Sd:%d Su:%d An:%d",
                                   (int)em->w_smile,   (int)em->w_happy,
                                   (int)em->w_sad,     (int)em->w_surprise,
                                   (int)em->w_angry);
        }
        if (scene->title_label != NULL) {
            gfx_label_set_text_fmt(scene->title_label, "  %s", em->name);
        }
    }
}

static void test_mouth_model_anim_cb(void *user_data)
{
    test_mouth_model_scene_t   *scene = (test_mouth_model_scene_t *)user_data;
    const face_emotion_t       *em;
    bool                        all_settled;

    if (scene == NULL) {
        return;
    }

    scene->anim_tick++;

    /* Solve all widgets (includes easing cur toward tgt). */
    test_mouth_model_apply_scene_pose(scene);

    /* Settle check: all keyframe fields must have reached their targets. */
    all_settled = (scene->eye_cur.ry_top    == scene->eye_tgt.ry_top    &&
                   scene->eye_cur.ry_bottom == scene->eye_tgt.ry_bottom &&
                   scene->eye_cur.tilt      == scene->eye_tgt.tilt      &&
                   scene->brow_cur.arch     == scene->brow_tgt.arch     &&
                   scene->brow_cur.raise    == scene->brow_tgt.raise    &&
                   scene->brow_cur.tilt     == scene->brow_tgt.tilt     &&
                   scene->mouth_cur.smile   == scene->mouth_tgt.smile   &&
                   scene->mouth_cur.open    == scene->mouth_tgt.open    &&
                   scene->mouth_cur.width   == scene->mouth_tgt.width   &&
                   scene->mouth_cur.upper   == scene->mouth_tgt.upper   &&
                   scene->mouth_cur.lower   == scene->mouth_tgt.lower   &&
                   scene->mouth_cur.pinch   == scene->mouth_tgt.pinch   &&
                   scene->mouth_cur.corner  == scene->mouth_tgt.corner);

    if (all_settled) {
        scene->hold_tick++;
        if (scene->hold_tick >= s_face_sequence[scene->expr_idx].hold_ticks) {
            scene->hold_tick = 0U;
            scene->expr_idx  = (scene->expr_idx + 1U) %
                                TEST_APP_ARRAY_SIZE(s_face_sequence);
            /* Convert emotion weights → per-widget KF targets via blend function. */
            em = &s_face_sequence[scene->expr_idx];
            face_blend_emotion(em, &scene->eye_tgt, &scene->brow_tgt, &scene->mouth_tgt);
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

    /* ── Mouth anchor mesh (invisible bone-driver) ───────────────────────────
     * Centred on display then offset; FACE_MOUTH_CY - FACE_DISPLAY_CY = 40.
     * Vertex solvers subsequently place each lip component via GFX_ALIGN_TOP_LEFT. */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mouth_obj, (void *)&face_parts_mouth_flat));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mouth_obj, TEST_MOUTH_MODEL_GRID_COLS, TEST_MOUTH_MODEL_GRID_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.mouth_obj, FACE_MOUTH_OBJ_W, FACE_MOUTH_OBJ_H));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_visible(scene.mouth_obj, false));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mouth_obj, GFX_ALIGN_CENTER,
                                             0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    /* ── Lip strips and corner arcs ─────────────────────────────────────────── */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.upper_lip_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.upper_lip_obj, TEST_MOUTH_MODEL_DENSE_SEGMENTS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.upper_lip_obj, GFX_ALIGN_CENTER,
                                             0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.lower_lip_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.lower_lip_obj, TEST_MOUTH_MODEL_DENSE_SEGMENTS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.lower_lip_obj, GFX_ALIGN_CENTER,
                                             0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_corner_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_corner_obj, 1, TEST_MOUTH_MODEL_CORNER_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_corner_obj, GFX_ALIGN_CENTER,
                                             0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_corner_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_corner_obj, 1, TEST_MOUTH_MODEL_CORNER_ROWS));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_corner_obj, GFX_ALIGN_CENTER,
                                             0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    /* ── Eyes: centred then offset to FACE_*_EYE_CX/CY ──────────────────────── */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_eye_obj, GFX_ALIGN_CENTER,
                                             FACE_LEFT_EYE_CX  - FACE_DISPLAY_CX,
                                             FACE_LEFT_EYE_CY  - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_eye_obj, GFX_ALIGN_CENTER,
                                             FACE_RIGHT_EYE_CX - FACE_DISPLAY_CX,
                                             FACE_RIGHT_EYE_CY - FACE_DISPLAY_CY));

    /* ── Eyebrows: centred then offset to FACE_*_BROW_CX/CY ─────────────────── */
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_brow_obj, GFX_ALIGN_CENTER,
                                             FACE_LEFT_BROW_CX  - FACE_DISPLAY_CX,
                                             FACE_LEFT_BROW_CY  - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_brow_obj, GFX_ALIGN_CENTER,
                                             FACE_RIGHT_BROW_CX - FACE_DISPLAY_CX,
                                             FACE_RIGHT_BROW_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Face Rig"));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.hint_label, 310, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.hint_label, GFX_ALIGN_BOTTOM_MID, 0, -50));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.hint_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.hint_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.hint_label, GFX_COLOR_HEX(0x101A30)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.hint_label, GFX_COLOR_HEX(0xC9D8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.hint_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.hint_label, " neutral (-)  Sm:0 Hp:0 Sd:0 Su:0 "));

    test_mouth_model_capture_base_points(&scene);

    /* Derive eye screen-space centres from the mouth mesh centre point.
     * The mouth centre index is the middle of the grid:
     *   row = GRID_ROWS/2, col = GRID_COLS/2
     * Screen centre = mouth_origin + local base_point offset.
     */
    /* Eye/brow anchor positions are compile-time constants (FACE_*_CX/CY).
     * No runtime derivation from the mouth mesh centre is needed.         */

    /* Convert first emotion entry to KFs; snap cur = tgt for frame 0. */
    {
        scene.expr_idx = 0U;
        face_blend_emotion(&s_face_sequence[0],
                           &scene.eye_cur,   &scene.brow_cur,  &scene.mouth_cur);
        scene.eye_tgt   = scene.eye_cur;
        scene.brow_tgt  = scene.brow_cur;
        scene.mouth_tgt = scene.mouth_cur;
    }
    test_mouth_model_apply_scene_pose(&scene);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mouth_model_anim_cb,
                                        TEST_MOUTH_MODEL_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Keyframe demo: per-widget eye/brow/mouth KFs cycle through "
                           "neutral→smile→happy→sad→surprised→shocked→angry→…");
    test_app_wait_for_observe(1000 * 10000);

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
