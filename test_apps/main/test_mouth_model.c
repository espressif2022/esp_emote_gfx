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
 * ═══════════════════════════════════════════════════════════════════════════ */
#define FACE_DISPLAY_W          360
#define FACE_DISPLAY_H          360
#define FACE_DISPLAY_CX         (FACE_DISPLAY_W / 2)          /* 180 */
#define FACE_DISPLAY_CY         (FACE_DISPLAY_H / 2)          /* 180 */

#define FACE_MOUTH_CX           FACE_DISPLAY_CX
#define FACE_MOUTH_CY           (FACE_DISPLAY_CY + 40)
#define FACE_EYE_X_HALF_GAP     50
#define FACE_EYE_Y_OFS         (-92)
#define FACE_BROW_Y_OFS_EXTRA  (-34)

#define FACE_LEFT_EYE_CX        (FACE_MOUTH_CX - FACE_EYE_X_HALF_GAP)
#define FACE_LEFT_EYE_CY        (FACE_MOUTH_CY + FACE_EYE_Y_OFS)
#define FACE_RIGHT_EYE_CX       (FACE_MOUTH_CX + FACE_EYE_X_HALF_GAP)
#define FACE_RIGHT_EYE_CY       FACE_LEFT_EYE_CY
#define FACE_LEFT_BROW_CX       FACE_LEFT_EYE_CX
#define FACE_LEFT_BROW_CY       (FACE_LEFT_EYE_CY + FACE_BROW_Y_OFS_EXTRA)
#define FACE_RIGHT_BROW_CX      FACE_RIGHT_EYE_CX
#define FACE_RIGHT_BROW_CY      FACE_LEFT_BROW_CY

/* ═══════════════════════════════════════════════════════════════════════════
 * Mesh / solver constants
 * ═══════════════════════════════════════════════════════════════════════════ */
#define TEST_MOUTH_MODEL_TIMER_PERIOD_MS  33U
#define TEST_MOUTH_MODEL_EYE_SEGS         24U
#define TEST_MOUTH_MODEL_BROW_SEGS        24U
#define TEST_MOUTH_MODEL_MOUTH_SEGS       32U

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 1 — Widget keyframe types (14-coordinate Beziers)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    int16_t pts[14];
} face_eye_kf_t;

typedef struct {
    int16_t pts[8];
} face_brow_kf_t;

typedef struct {
    int16_t pts[14];
} face_mouth_kf_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 2 — User-facing emotion controller
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    const char  *name;
    int16_t      w_smile;
    int16_t      w_happy;
    int16_t      w_sad;
    int16_t      w_surprise;
    int16_t      w_angry;
    int16_t      w_look_x;
    int16_t      w_look_y;
    uint16_t     hold_ticks;
} face_emotion_t;

#include "test_mouth_model_keyframes.inc"

static void face_blend_emotion(const face_emotion_t *em,
                               face_eye_kf_t        *eye_out,
                               face_brow_kf_t       *brow_out,
                               face_mouth_kf_t      *mouth_out,
                               int16_t              *look_x_out,
                               int16_t              *look_y_out)
{
    if (look_x_out) *look_x_out = em->w_look_x;
    if (look_y_out) *look_y_out = em->w_look_y;
    int i;
    for (i = 0; i < 14; i++) {
        eye_out->pts[i] = s_ref_eye[0].pts[i]
            + (s_ref_eye[1].pts[i] - s_ref_eye[0].pts[i]) * em->w_smile   / 100
            + (s_ref_eye[2].pts[i] - s_ref_eye[0].pts[i]) * em->w_happy   / 100
            + (s_ref_eye[3].pts[i] - s_ref_eye[0].pts[i]) * em->w_sad     / 100
            + (s_ref_eye[4].pts[i] - s_ref_eye[0].pts[i]) * em->w_surprise/ 100
            + (s_ref_eye[5].pts[i] - s_ref_eye[0].pts[i]) * em->w_angry   / 100;

        mouth_out->pts[i] = s_ref_mouth[0].pts[i]
            + (s_ref_mouth[1].pts[i] - s_ref_mouth[0].pts[i]) * em->w_smile   / 100
            + (s_ref_mouth[2].pts[i] - s_ref_mouth[0].pts[i]) * em->w_happy   / 100
            + (s_ref_mouth[3].pts[i] - s_ref_mouth[0].pts[i]) * em->w_sad     / 100
            + (s_ref_mouth[4].pts[i] - s_ref_mouth[0].pts[i]) * em->w_surprise/ 100
            + (s_ref_mouth[5].pts[i] - s_ref_mouth[0].pts[i]) * em->w_angry   / 100;
    }
    for (i = 0; i < 8; i++) {
        brow_out->pts[i] = s_ref_brow[0].pts[i]
            + (s_ref_brow[1].pts[i] - s_ref_brow[0].pts[i]) * em->w_smile   / 100
            + (s_ref_brow[2].pts[i] - s_ref_brow[0].pts[i]) * em->w_happy   / 100
            + (s_ref_brow[3].pts[i] - s_ref_brow[0].pts[i]) * em->w_sad     / 100
            + (s_ref_brow[4].pts[i] - s_ref_brow[0].pts[i]) * em->w_surprise/ 100
            + (s_ref_brow[5].pts[i] - s_ref_brow[0].pts[i]) * em->w_angry   / 100;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 4 — Easing / transition function
 * ═══════════════════════════════════════════════════════════════════════════ */
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
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    gfx_obj_t *mouth_obj;
    gfx_obj_t *left_eye_obj;
    gfx_obj_t *right_eye_obj;
    gfx_obj_t *left_brow_obj;
    gfx_obj_t *right_brow_obj;
    gfx_obj_t *title_label;
    gfx_timer_handle_t anim_timer;

    face_eye_kf_t    eye_cur;
    face_eye_kf_t    eye_tgt;
    face_brow_kf_t   brow_cur;
    face_brow_kf_t   brow_tgt;
    face_mouth_kf_t  mouth_cur;
    face_mouth_kf_t  mouth_tgt;
    int16_t          look_x_cur;
    int16_t          look_x_tgt;
    int16_t          look_y_cur;
    int16_t          look_y_tgt;

    uint16_t hold_tick;
    uint32_t anim_tick;
    size_t   expr_idx;
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

static int32_t test_mouth_model_isqrt_i64(uint64_t value)
{
    uint64_t op, res, one;
    if (value <= 0) return 0;
    op = value;
    res = 0U;
    one = 1ULL << 62;
    while (one > op) one >>= 2;
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer 5 — Universal Bezier Ribbon Solvers (Q8 Sub-pixel precision)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Evaluates a completely continuous, hollow 2D stroked curve with defined thickness.
 * Automatically mirrors the X coordinates if flip_x is true.
 * Evaluates everything at 256x scale (Q8 math) to completely eliminate normal vector jitters.
 */
static esp_err_t test_mouth_model_apply_bezier_stroke(gfx_obj_t *obj,
                                                      const int16_t *pts,
                                                      bool closed,
                                                      int32_t cx,
                                                      int32_t cy,
                                                      int32_t scale_percent,
                                                      int32_t thickness,
                                                      bool flip_x,
                                                      int32_t segs)
{
    int32_t total_segs = closed ? (segs * 2) : segs;
    if (total_segs > 128) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t px_q8[129];
    int32_t py_q8[129];
    int c, i;

    /* Evaluate 1 or 2 cubic Beziers */
    for (c = 0; c < (closed ? 2 : 1); c++) {
        int32_t p0x = cx + ((flip_x ? -1 : 1) * pts[c * 6 + 0] * scale_percent) / 100;
        int32_t p0y = cy + (pts[c * 6 + 1] * scale_percent) / 100;
        int32_t p1x = cx + ((flip_x ? -1 : 1) * pts[c * 6 + 2] * scale_percent) / 100;
        int32_t p1y = cy + (pts[c * 6 + 3] * scale_percent) / 100;
        int32_t p2x = cx + ((flip_x ? -1 : 1) * pts[c * 6 + 4] * scale_percent) / 100;
        int32_t p2y = cy + (pts[c * 6 + 5] * scale_percent) / 100;
        int32_t p3x, p3y;
        
        if (c == 1) { // Close back to start
            p3x = cx + ((flip_x ? -1 : 1) * pts[0] * scale_percent) / 100;
            p3y = cy + (pts[1] * scale_percent) / 100;
        } else {
            p3x = cx + ((flip_x ? -1 : 1) * (closed ? pts[6] : pts[6]) * scale_percent) / 100;
            p3y = cy + (closed ? pts[7] : pts[7]) * scale_percent / 100;
        }

        for (i = 0; i <= segs; i++) {
            if (c == 1 && i == 0) {
                continue; // Skip overlapping center point
            }
            int idx = c * segs + i;
            int32_t t = (i * 1000) / segs;
            int32_t mt = 1000 - t;
            int32_t w0 = (mt * mt / 1000 * mt) / 1000;
            int32_t w1 = 3 * t * mt / 1000 * mt / 1000;
            int32_t w2 = 3 * t * t / 1000 * mt / 1000;
            int32_t w3 = t * t / 1000 * t / 1000;
            
            px_q8[idx] = (p0x * w0 + p1x * w1 + p2x * w2 + p3x * w3) * 256 / 1000;
            py_q8[idx] = (p0y * w0 + p1y * w1 + p2y * w2 + p3y * w3) * 256 / 1000;
        }
    }

    gfx_mesh_img_point_t mesh_pts[258];
    int32_t min_x = 999999, min_y = 999999;
    int32_t max_x = -999999;
    int32_t thick_q8 = thickness * 256;

    for (i = 0; i <= total_segs; i++) {
        int32_t nx_q8 = 0;
        int32_t ny_q8 = thick_q8;

        int prev = (i == 0) ? (closed ? total_segs - 1 : 0) : i - 1;
        int next = (i == total_segs) ? (closed ? 1 : total_segs) : i + 1;
        int32_t dx = px_q8[next] - px_q8[prev];
        int32_t dy = py_q8[next] - py_q8[prev];
        
        uint64_t sq_len = (uint64_t)((int64_t)dx * dx + (int64_t)dy * dy);
        int32_t len = test_mouth_model_isqrt_i64(sq_len);
        
        if (len > 0) {
            nx_q8 = (int32_t)(-(int64_t)dy * thick_q8) / len;
            ny_q8 = (int32_t)( (int64_t)dx * thick_q8) / len;
        }

        int32_t out_x = (px_q8[i] + nx_q8) / 256;
        int32_t out_y = (py_q8[i] + ny_q8) / 256;
        int32_t in_x  = (px_q8[i] - nx_q8) / 256;
        int32_t in_y  = (py_q8[i] - ny_q8) / 256;

        mesh_pts[i].x = out_x;
        mesh_pts[i].y = out_y;
        mesh_pts[total_segs + 1 + i].x = in_x;
        mesh_pts[total_segs + 1 + i].y = in_y;

        if (out_x < min_x) { min_x = out_x; }
        if (out_x > max_x) { max_x = out_x; }
        if (out_y < min_y) { min_y = out_y; }
        if (in_x < min_x) { min_x = in_x; }
        if (in_x > max_x) { max_x = in_x; }
        if (in_y < min_y) { min_y = in_y; }
    }

    for (i = 0; i < (total_segs + 1) * 2; i++) {
        mesh_pts[i].x = (gfx_coord_t)((int32_t)mesh_pts[i].x - min_x);
        mesh_pts[i].y = (gfx_coord_t)((int32_t)mesh_pts[i].y - min_y);
    }

    {
        int32_t width = max_x - min_x + 1;
        int32_t align_x = cx - (width / 2);
        ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, align_x, min_y), TAG, "stroke align failed");
    }
    return gfx_mesh_img_set_points(obj, mesh_pts, (total_segs + 1) * 2);
}

/* Evaluates a closed Bezier curve as a solid filled polygon structure using Q8 sub-pixel precision. */
static esp_err_t test_mouth_model_apply_bezier_fill(gfx_obj_t *obj,
                                                    const int16_t *pts,
                                                    int32_t cx,
                                                    int32_t cy,
                                                    int32_t scale_percent,
                                                    bool flip_x,
                                                    int32_t segs)
{
    if (segs > 64) return ESP_ERR_INVALID_ARG;
    int32_t px0[65], py0[65]; // upper loop
    int32_t px1[65], py1[65]; // lower loop
    int i;

    // Upper curve
    int32_t p0x = cx + ((flip_x ? -1 : 1) * pts[0] * scale_percent) / 100;
    int32_t p0y = cy + (pts[1] * scale_percent) / 100;
    int32_t p1x = cx + ((flip_x ? -1 : 1) * pts[2] * scale_percent) / 100;
    int32_t p1y = cy + (pts[3] * scale_percent) / 100;
    int32_t p2x = cx + ((flip_x ? -1 : 1) * pts[4] * scale_percent) / 100;
    int32_t p2y = cy + (pts[5] * scale_percent) / 100;
    int32_t p3x = cx + ((flip_x ? -1 : 1) * pts[6] * scale_percent) / 100;
    int32_t p3y = cy + (pts[7] * scale_percent) / 100;
    for (i = 0; i <= segs; i++) {
        int32_t t = (i * 1000) / segs;
        int32_t mt = 1000 - t;
        int32_t w0 = (mt * mt / 1000 * mt) / 1000;
        int32_t w1 = 3 * t * mt / 1000 * mt / 1000;
        int32_t w2 = 3 * t * t / 1000 * mt / 1000;
        int32_t w3 = t * t / 1000 * t / 1000;
        px0[i] = ((p0x * w0 + p1x * w1 + p2x * w2 + p3x * w3) * 256 / 1000) / 256;
        py0[i] = ((p0y * w0 + p1y * w1 + p2y * w2 + p3y * w3) * 256 / 1000) / 256;
    }

    // Lower curve (reverse evaluate)
    p0x = cx + ((flip_x ? -1 : 1) * pts[6] * scale_percent) / 100;
    p0y = cy + (pts[7] * scale_percent) / 100;
    p1x = cx + ((flip_x ? -1 : 1) * pts[8] * scale_percent) / 100;
    p1y = cy + (pts[9] * scale_percent) / 100;
    p2x = cx + ((flip_x ? -1 : 1) * pts[10] * scale_percent) / 100;
    p2y = cy + (pts[11] * scale_percent) / 100;
    p3x = cx + ((flip_x ? -1 : 1) * pts[12] * scale_percent) / 100;
    p3y = cy + (pts[13] * scale_percent) / 100;
    for (i = 0; i <= segs; i++) {
        int32_t t = ((segs - i) * 1000) / segs; // backwards evaluation
        int32_t mt = 1000 - t;
        int32_t w0 = (mt * mt / 1000 * mt) / 1000;
        int32_t w1 = 3 * t * mt / 1000 * mt / 1000;
        int32_t w2 = 3 * t * t / 1000 * mt / 1000;
        int32_t w3 = t * t / 1000 * t / 1000;
        px1[i] = ((p0x * w0 + p1x * w1 + p2x * w2 + p3x * w3) * 256 / 1000) / 256;
        py1[i] = ((p0y * w0 + p1y * w1 + p2y * w2 + p3y * w3) * 256 / 1000) / 256;
    }

    gfx_mesh_img_point_t mesh_pts[130];
    int32_t min_x = px0[0], min_y = py0[0];
    int32_t max_x = px0[0];
    for (i = 0; i <= segs; i++) {
        if (px0[i] < min_x) min_x = px0[i];
        if (px0[i] > max_x) max_x = px0[i];
        if (py0[i] < min_y) min_y = py0[i];
        if (px1[i] < min_x) min_x = px1[i];
        if (px1[i] > max_x) max_x = px1[i];
        if (py1[i] < min_y) min_y = py1[i];
        mesh_pts[i].x = px0[i];
        mesh_pts[i].y = py0[i];
        mesh_pts[segs + 1 + i].x = px1[i];
        mesh_pts[segs + 1 + i].y = py1[i];
    }
    for (i = 0; i < (segs + 1) * 2; i++) {
        mesh_pts[i].x = (gfx_coord_t)((int32_t)mesh_pts[i].x - min_x);
        mesh_pts[i].y = (gfx_coord_t)((int32_t)mesh_pts[i].y - min_y);
    }
    {
        int32_t width = max_x - min_x + 1;
        int32_t align_x = cx - (width / 2);
        ESP_RETURN_ON_ERROR(gfx_obj_align(obj, GFX_ALIGN_TOP_LEFT, align_x, min_y), TAG, "fill align failed");
    }
    return gfx_mesh_img_set_points(obj, mesh_pts, (segs + 1) * 2);
}

static void test_mouth_model_apply_scene_pose(test_mouth_model_scene_t *scene)
{
    int i;
    for (i = 0; i < 14; i++) {
        scene->eye_cur.pts[i] = face_ease_spring(scene->eye_cur.pts[i], scene->eye_tgt.pts[i]);
        scene->mouth_cur.pts[i] = face_ease_spring(scene->mouth_cur.pts[i], scene->mouth_tgt.pts[i]);
    }
    for (i = 0; i < 8; i++) {
        scene->brow_cur.pts[i] = face_ease_spring(scene->brow_cur.pts[i], scene->brow_tgt.pts[i]);
    }
    
    scene->look_x_cur = face_ease_spring(scene->look_x_cur, scene->look_x_tgt);
    scene->look_y_cur = face_ease_spring(scene->look_y_cur, scene->look_y_tgt);
    
    int32_t lx = scene->look_x_cur;
    int32_t ly = scene->look_y_cur;

    test_mouth_model_apply_bezier_fill(scene->left_eye_obj, scene->eye_cur.pts, FACE_LEFT_EYE_CX + lx, FACE_LEFT_EYE_CY + ly, 200, false, TEST_MOUTH_MODEL_EYE_SEGS);
    test_mouth_model_apply_bezier_fill(scene->right_eye_obj, scene->eye_cur.pts, FACE_RIGHT_EYE_CX + lx, FACE_RIGHT_EYE_CY + ly, 200, true, TEST_MOUTH_MODEL_EYE_SEGS);

    test_mouth_model_apply_bezier_stroke(scene->left_brow_obj, scene->brow_cur.pts, false, FACE_LEFT_BROW_CX + lx, FACE_LEFT_BROW_CY + ly, 200, 4, false, TEST_MOUTH_MODEL_BROW_SEGS);
    test_mouth_model_apply_bezier_stroke(scene->right_brow_obj, scene->brow_cur.pts, false, FACE_RIGHT_BROW_CX + lx, FACE_RIGHT_BROW_CY + ly, 200, 4, true, TEST_MOUTH_MODEL_BROW_SEGS);

    test_mouth_model_apply_bezier_stroke(scene->mouth_obj, scene->mouth_cur.pts, true, FACE_MOUTH_CX, FACE_MOUTH_CY, 200, 5, false, TEST_MOUTH_MODEL_MOUTH_SEGS);

    if (scene->title_label != NULL) {
        gfx_label_set_text_fmt(scene->title_label, "  %s", s_face_sequence[scene->expr_idx].name);
    }
}

static void test_mouth_model_anim_cb(void *user_data)
{
    test_mouth_model_scene_t   *scene = (test_mouth_model_scene_t *)user_data;
    const face_emotion_t       *em;
    bool                        all_settled = true;
    int i;

    if (scene == NULL) return;

    scene->anim_tick++;
    test_mouth_model_apply_scene_pose(scene);

    for (i = 0; i < 14; i++) {
        if (scene->eye_cur.pts[i] != scene->eye_tgt.pts[i] ||
            scene->mouth_cur.pts[i] != scene->mouth_tgt.pts[i]) {
            all_settled = false; break;
        }
    }
    for (i = 0; i < 8; i++) {
        if (scene->brow_cur.pts[i] != scene->brow_tgt.pts[i]) {
            all_settled = false; break;
        }
    }
    if (scene->look_x_cur != scene->look_x_tgt || scene->look_y_cur != scene->look_y_tgt) {
        all_settled = false;
    }

    if (all_settled) {
        scene->hold_tick++;
        if (scene->hold_tick >= s_face_sequence[scene->expr_idx].hold_ticks) {
            scene->hold_tick = 0U;
            scene->expr_idx  = (scene->expr_idx + 1U) % TEST_APP_ARRAY_SIZE(s_face_sequence);
            em = &s_face_sequence[scene->expr_idx];
            face_blend_emotion(em, &scene->eye_tgt, &scene->brow_tgt, &scene->mouth_tgt, &scene->look_x_tgt, &scene->look_y_tgt);
        }
    } else {
        scene->hold_tick = 0U;
    }
}

static void test_mouth_model_scene_cleanup(test_mouth_model_scene_t *scene)
{
    if (scene == NULL) return;
    if (scene->anim_timer != NULL) gfx_timer_delete(emote_handle, scene->anim_timer);
    if (scene->right_brow_obj != NULL) gfx_obj_delete(scene->right_brow_obj);
    if (scene->left_brow_obj != NULL) gfx_obj_delete(scene->left_brow_obj);
    if (scene->right_eye_obj != NULL) gfx_obj_delete(scene->right_eye_obj);
    if (scene->left_eye_obj != NULL) gfx_obj_delete(scene->left_eye_obj);
    if (scene->mouth_obj != NULL) gfx_obj_delete(scene->mouth_obj);
    if (scene->title_label != NULL) gfx_obj_delete(scene->title_label);
}

static void test_mouth_model_run(void)
{
    test_mouth_model_scene_t scene = {0};

    test_app_log_case(TAG, "Standalone mouth model");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));

    scene.mouth_obj = gfx_mesh_img_create(disp_default);
    scene.left_eye_obj = gfx_mesh_img_create(disp_default);
    scene.right_eye_obj = gfx_mesh_img_create(disp_default);
    scene.left_brow_obj = gfx_mesh_img_create(disp_default);
    scene.right_brow_obj = gfx_mesh_img_create(disp_default);
    scene.title_label = gfx_label_create(disp_default);
    
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.mouth_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.mouth_obj, TEST_MOUTH_MODEL_MOUTH_SEGS * 2, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.mouth_obj, GFX_ALIGN_CENTER, 0, FACE_MOUTH_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_eye_obj, GFX_ALIGN_CENTER, FACE_LEFT_EYE_CX - FACE_DISPLAY_CX, FACE_LEFT_EYE_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_eye_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_eye_obj, TEST_MOUTH_MODEL_EYE_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_eye_obj, GFX_ALIGN_CENTER, FACE_RIGHT_EYE_CX - FACE_DISPLAY_CX, FACE_RIGHT_EYE_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.left_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.left_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.left_brow_obj, GFX_ALIGN_CENTER, FACE_LEFT_BROW_CX - FACE_DISPLAY_CX, FACE_LEFT_BROW_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(scene.right_brow_obj, (void *)&s_white_img));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(scene.right_brow_obj, TEST_MOUTH_MODEL_BROW_SEGS, 1));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.right_brow_obj, GFX_ALIGN_CENTER, FACE_RIGHT_BROW_CX - FACE_DISPLAY_CX, FACE_RIGHT_BROW_CY - FACE_DISPLAY_CY));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(scene.title_label, 306, 24));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(scene.title_label, GFX_ALIGN_TOP_MID, 0, 8));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(scene.title_label, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(scene.title_label, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(scene.title_label, GFX_COLOR_HEX(0x122544)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(scene.title_label, GFX_COLOR_HEX(0xEAF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text_align(scene.title_label, GFX_TEXT_ALIGN_CENTER));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(scene.title_label, " Face Rig "));

    {
        scene.expr_idx = 0U;
        face_blend_emotion(&s_face_sequence[0], &scene.eye_cur, &scene.brow_cur, &scene.mouth_cur, &scene.look_x_cur, &scene.look_y_cur);
        scene.eye_tgt    = scene.eye_cur;
        scene.brow_tgt   = scene.brow_cur;
        scene.mouth_tgt  = scene.mouth_cur;
        scene.look_x_tgt = scene.look_x_cur;
        scene.look_y_tgt = scene.look_y_cur;
    }
    test_mouth_model_apply_scene_pose(&scene);

    scene.anim_timer = gfx_timer_create(emote_handle, test_mouth_model_anim_cb,
                                        TEST_MOUTH_MODEL_TIMER_PERIOD_MS, &scene);
    TEST_ASSERT_NOT_NULL(scene.anim_timer);
    test_app_unlock();

    test_app_log_step(TAG, "Keyframe demo: pure Bezier face interpolator running.");
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
