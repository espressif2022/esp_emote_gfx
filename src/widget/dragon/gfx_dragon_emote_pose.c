/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "core/object/gfx_obj_priv.h"
#include "widget/dragon/gfx_dragon_emote_priv.h"

static const char *TAG = "dragon_pose";

#define PI 3.14159265f
#define DEG_TO_RAD(d) ((d) * PI / 180.0f)
#define DRAGON_VIEWBOX_X   120.0f
#define DRAGON_VIEWBOX_Y   150.0f
#define DRAGON_VIEWBOX_W  1000.0f
#define DRAGON_VIEWBOX_H   980.0f
#define DRAGON_VIEWBOX_CX (DRAGON_VIEWBOX_X + DRAGON_VIEWBOX_W * 0.5f)
#define DRAGON_VIEWBOX_CY (DRAGON_VIEWBOX_Y + DRAGON_VIEWBOX_H * 0.5f)

static const gfx_mesh_img_point_t s_dragon_tail_stripe1_pts[] = {
    { 60, -9 }, { 62, -8 }, { 63, -7 }, { 64, -5 }, { 64, 5 }, { 63, 7 }, { 62, 8 }, { 60, 9 },
    { -60, 9 }, { -62, 8 }, { -63, 7 }, { -64, 5 }, { -64, -5 }, { -63, -7 }, { -62, -8 }, { -60, -9 },
    { 60, -9 }, { 60, -9 }
};
static const gfx_mesh_img_point_t s_dragon_tail_stripe2_pts[] = {
    { 62, -9 }, { 64, -8 }, { 65, -7 }, { 66, -5 }, { 66, 5 }, { 65, 7 }, { 64, 8 }, { 62, 9 },
    { -62, 9 }, { -64, 8 }, { -65, 7 }, { -66, 5 }, { -66, -5 }, { -65, -7 }, { -64, -8 }, { -62, -9 },
    { 62, -9 }, { 62, -9 }
};
static const gfx_mesh_img_point_t s_dragon_pupil_o_pts[] = {
    { 0, -18 }, { 9, -16 }, { 16, -9 }, { 18, 0 }, { 16, 9 }, { 9, 16 }, { 0, 18 }, { -9, 16 },
    { -16, 9 }, { -18, 0 }, { -16, -9 }, { -9, -16 }, { 0, -18 }, { 9, -16 }, { 16, -9 }, { 18, 0 },
    { 16, 9 }, { 9, 16 }, { 0, 18 }, { -9, 16 }, { -16, 9 }, { 0, -18 }
};
static const gfx_mesh_img_point_t s_dragon_pupil_u_pts[] = {
    { -26, -2 }, { -22, 5 }, { -18, 11 }, { -12, 16 }, { -6, 19 }, { 0, 20 }, { 6, 19 }, { 12, 16 },
    { 18, 11 }, { 22, 5 }, { 26, -2 }, { 18, -2 }, { 14, 4 }, { 9, 8 }, { 4, 10 }, { 0, 11 },
    { -4, 10 }, { -9, 8 }, { -14, 4 }, { -18, -2 }, { -26, -2 }, { -26, -2 }
};
static const gfx_mesh_img_point_t s_dragon_pupil_n_pts[] = {
    { -26, 2 }, { -22, -5 }, { -18, -11 }, { -12, -16 }, { -6, -19 }, { 0, -20 }, { 6, -19 }, { 12, -16 },
    { 18, -11 }, { 22, -5 }, { 26, 2 }, { 18, 2 }, { 14, -4 }, { 9, -8 }, { 4, -10 }, { 0, -11 },
    { -4, -10 }, { -9, -8 }, { -14, -4 }, { -18, 2 }, { -26, 2 }, { -26, 2 }
};
static const gfx_mesh_img_point_t s_dragon_pupil_line_pts[] = {
    { -28, 4 }, { -22, 2 }, { -14, 1 }, { -5, 0 }, { 5, 0 }, { 14, 1 }, { 22, 2 }, { 28, 4 },
    { 28, -4 }, { 22, -2 }, { 14, -1 }, { 5, 0 }, { -5, 0 }, { -14, -1 }, { -22, -2 }, { -28, -4 },
    { -28, 4 }, { -22, 2 }, { -14, 1 }, { -5, 0 }, { 5, 0 }, { -28, 4 }
};
static const gfx_mesh_img_point_t s_dragon_antenna_l_pts[] = {
    { 8, 97 }, { -3, 75 }, { -14, 34 }, { -13, 1 }, { -5, -27 }, { 7, -50 }, { 22, -67 }, { 35, -79 },
    { 43, -85 }, { 44, -85 }, { 27, -110 }, { 25, -109 }, { 16, -102 }, { 0, -88 }, { -17, -66 }, { -33, -38 },
    { -43, -3 }, { -44, 39 }, { -31, 85 }, { -18, 110 }, { 8, 97 }, { 8, 97 }
};
static const gfx_mesh_img_point_t s_dragon_antenna_r_pts[] = {
    { 42, -111 }, { 29, -111 }, { 18, -105 }, { 3, -92 }, { -15, -72 }, { -31, -44 }, { -41, -8 }, { -42, 35 },
    { -30, 83 }, { -18, 111 }, { 9, 99 }, { -1, 75 }, { -12, 31 }, { -11, -4 }, { -3, -33 }, { 10, -55 },
    { 24, -71 }, { 36, -80 }, { 41, -83 }, { 30, -83 }, { 42, -111 }, { 42, -111 }
};
static const gfx_mesh_img_point_t s_dragon_dot_circle_pts[] = {
    { 27, 0 }, { 25, 10 }, { 19, 19 }, { 10, 25 }, { 0, 27 }, { -10, 25 }, { -19, 19 }, { -25, 10 },
    { -27, 0 }, { -25, -10 }, { -19, -19 }, { -10, -25 }, { 0, -27 }, { 10, -25 }, { 19, -19 }, { 25, -10 },
    { 27, 0 }, { 27, 0 }
};

typedef struct {
    float eye_open;
    float eye_focus;
    float eye_soft;
    float pupil_x;
    float pupil_y;
    float pupil_scale;
    float droop;
    float alert;
    float antenna_lift;
    float antenna_open;
    float antenna_curl;
    float look_bias_x;
    float look_bias_y;
} gfx_dragon_emotion_axes_t;

typedef struct {
    gfx_dragon_transform_t eye_white;
    gfx_dragon_transform_t pupil;
    gfx_dragon_transform_t antenna;
} gfx_dragon_derived_pose_t;

static const gfx_dragon_emotion_axes_t s_dragon_axes_neutral = { 0 };
static const gfx_dragon_emotion_axes_t s_dragon_axes_smile = {
    .eye_open = -0.10f, .eye_focus = 0.10f, .eye_soft = 0.80f,
    .pupil_x = 0.0f, .pupil_y = -2.0f, .pupil_scale = 0.92f,
    .droop = -0.10f, .alert = 0.10f, .antenna_lift = 0.12f,
    .antenna_open = 0.22f, .antenna_curl = 0.18f,
    .look_bias_x = 0.0f, .look_bias_y = -0.04f
};
static const gfx_dragon_emotion_axes_t s_dragon_axes_happy = {
    .eye_open = 0.06f, .eye_focus = 0.22f, .eye_soft = 1.00f,
    .pupil_x = 0.0f, .pupil_y = -6.0f, .pupil_scale = 0.84f,
    .droop = -0.14f, .alert = 0.32f, .antenna_lift = 0.30f,
    .antenna_open = 0.46f, .antenna_curl = 0.30f,
    .look_bias_x = 0.0f, .look_bias_y = -0.12f
};
static const gfx_dragon_emotion_axes_t s_dragon_axes_sad = {
    .eye_open = -0.24f, .eye_focus = -0.12f, .eye_soft = -0.10f,
    .pupil_x = -8.0f, .pupil_y = 8.0f, .pupil_scale = 1.05f,
    .droop = 1.00f, .alert = -0.20f, .antenna_lift = -0.12f,
    .antenna_open = -0.24f, .antenna_curl = -0.18f,
    .look_bias_x = -0.18f, .look_bias_y = 0.22f
};
static const gfx_dragon_emotion_axes_t s_dragon_axes_surprise = {
    .eye_open = 0.95f, .eye_focus = 0.45f, .eye_soft = 0.10f,
    .pupil_x = 0.0f, .pupil_y = -10.0f, .pupil_scale = 0.72f,
    .droop = -0.20f, .alert = 1.00f, .antenna_lift = 0.62f,
    .antenna_open = 0.80f, .antenna_curl = 0.34f,
    .look_bias_x = 0.0f, .look_bias_y = -0.28f
};
static const gfx_dragon_emotion_axes_t s_dragon_axes_angry = {
    .eye_open = -0.18f, .eye_focus = 1.00f, .eye_soft = -0.50f,
    .pupil_x = 0.0f, .pupil_y = -2.0f, .pupil_scale = 0.88f,
    .droop = 0.10f, .alert = 0.55f, .antenna_lift = 0.18f,
    .antenna_open = 0.10f, .antenna_curl = 0.70f,
    .look_bias_x = 0.0f, .look_bias_y = 0.02f
};

static inline int16_t gfx_dragon_emote_ease(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    if (diff == 0) return cur;
    int32_t step = diff / div;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    return (int16_t)((int32_t)cur + step);
}

const gfx_mesh_img_point_t *gfx_dragon_emote_get_pupil_shape_points(gfx_dragon_pupil_shape_t shape, size_t *count_out)
{
    const gfx_mesh_img_point_t *pts = s_dragon_pupil_line_pts;
    size_t count = sizeof(s_dragon_pupil_line_pts) / sizeof(s_dragon_pupil_line_pts[0]);

    switch (shape) {
    case GFX_DRAGON_PUPIL_SHAPE_O:
        pts = s_dragon_pupil_o_pts;
        count = sizeof(s_dragon_pupil_o_pts) / sizeof(s_dragon_pupil_o_pts[0]);
        break;
    case GFX_DRAGON_PUPIL_SHAPE_U:
        pts = s_dragon_pupil_u_pts;
        count = sizeof(s_dragon_pupil_u_pts) / sizeof(s_dragon_pupil_u_pts[0]);
        break;
    case GFX_DRAGON_PUPIL_SHAPE_N:
        pts = s_dragon_pupil_n_pts;
        count = sizeof(s_dragon_pupil_n_pts) / sizeof(s_dragon_pupil_n_pts[0]);
        break;
    case GFX_DRAGON_PUPIL_SHAPE_LINE:
    case GFX_DRAGON_PUPIL_SHAPE_AUTO:
    default:
        break;
    }

    if (count_out != NULL) {
        *count_out = count;
    }
    return pts;
}

static gfx_dragon_pupil_shape_t gfx_dragon_resolve_pupil_shape(const gfx_dragon_emote_mix_t *mix)
{
    if (mix == NULL) {
        return GFX_DRAGON_PUPIL_SHAPE_O;
    }
    if (mix->pupil_shape != GFX_DRAGON_PUPIL_SHAPE_AUTO) {
        return mix->pupil_shape;
    }

    int16_t best_weight = 0;
    gfx_dragon_pupil_shape_t best_shape = GFX_DRAGON_PUPIL_SHAPE_O;
    if (mix->w_smile > best_weight) {
        best_weight = mix->w_smile;
        best_shape = GFX_DRAGON_PUPIL_SHAPE_U;
    }
    if (mix->w_happy > best_weight) {
        best_weight = mix->w_happy;
        best_shape = GFX_DRAGON_PUPIL_SHAPE_U;
    }
    if (mix->w_sad > best_weight) {
        best_weight = mix->w_sad;
        best_shape = GFX_DRAGON_PUPIL_SHAPE_N;
    }
    if (mix->w_surprise > best_weight) {
        best_weight = mix->w_surprise;
        best_shape = GFX_DRAGON_PUPIL_SHAPE_O;
    }
    if (mix->w_angry > best_weight) {
        best_weight = mix->w_angry;
        best_shape = GFX_DRAGON_PUPIL_SHAPE_LINE;
    }
    return best_shape;
}

static void gfx_dragon_blend_axes(const gfx_dragon_emote_mix_t *mix, gfx_dragon_emotion_axes_t *out)
{
    float sm;
    float hp;
    float sd;
    float su;
    float an;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (mix == NULL) {
        return;
    }

    sm = (float)mix->w_smile / 100.0f;
    hp = (float)mix->w_happy / 100.0f;
    sd = (float)mix->w_sad / 100.0f;
    su = (float)mix->w_surprise / 100.0f;
    an = (float)mix->w_angry / 100.0f;

    out->eye_open = s_dragon_axes_smile.eye_open * sm + s_dragon_axes_happy.eye_open * hp +
                    s_dragon_axes_sad.eye_open * sd + s_dragon_axes_surprise.eye_open * su +
                    s_dragon_axes_angry.eye_open * an;
    out->eye_focus = s_dragon_axes_smile.eye_focus * sm + s_dragon_axes_happy.eye_focus * hp +
                     s_dragon_axes_sad.eye_focus * sd + s_dragon_axes_surprise.eye_focus * su +
                     s_dragon_axes_angry.eye_focus * an;
    out->eye_soft = s_dragon_axes_smile.eye_soft * sm + s_dragon_axes_happy.eye_soft * hp +
                    s_dragon_axes_sad.eye_soft * sd + s_dragon_axes_surprise.eye_soft * su +
                    s_dragon_axes_angry.eye_soft * an;
    out->pupil_x = s_dragon_axes_smile.pupil_x * sm + s_dragon_axes_happy.pupil_x * hp +
                   s_dragon_axes_sad.pupil_x * sd + s_dragon_axes_surprise.pupil_x * su +
                   s_dragon_axes_angry.pupil_x * an;
    out->pupil_y = s_dragon_axes_smile.pupil_y * sm + s_dragon_axes_happy.pupil_y * hp +
                   s_dragon_axes_sad.pupil_y * sd + s_dragon_axes_surprise.pupil_y * su +
                   s_dragon_axes_angry.pupil_y * an;
    out->pupil_scale = s_dragon_axes_neutral.pupil_scale +
                       (s_dragon_axes_smile.pupil_scale - s_dragon_axes_neutral.pupil_scale) * sm +
                       (s_dragon_axes_happy.pupil_scale - s_dragon_axes_neutral.pupil_scale) * hp +
                       (s_dragon_axes_sad.pupil_scale - s_dragon_axes_neutral.pupil_scale) * sd +
                       (s_dragon_axes_surprise.pupil_scale - s_dragon_axes_neutral.pupil_scale) * su +
                       (s_dragon_axes_angry.pupil_scale - s_dragon_axes_neutral.pupil_scale) * an;
    out->droop = s_dragon_axes_smile.droop * sm + s_dragon_axes_happy.droop * hp +
                 s_dragon_axes_sad.droop * sd + s_dragon_axes_surprise.droop * su +
                 s_dragon_axes_angry.droop * an;
    out->alert = s_dragon_axes_smile.alert * sm + s_dragon_axes_happy.alert * hp +
                 s_dragon_axes_sad.alert * sd + s_dragon_axes_surprise.alert * su +
                 s_dragon_axes_angry.alert * an;
    out->antenna_lift = s_dragon_axes_smile.antenna_lift * sm + s_dragon_axes_happy.antenna_lift * hp +
                        s_dragon_axes_sad.antenna_lift * sd + s_dragon_axes_surprise.antenna_lift * su +
                        s_dragon_axes_angry.antenna_lift * an;
    out->antenna_open = s_dragon_axes_smile.antenna_open * sm + s_dragon_axes_happy.antenna_open * hp +
                        s_dragon_axes_sad.antenna_open * sd + s_dragon_axes_surprise.antenna_open * su +
                        s_dragon_axes_angry.antenna_open * an;
    out->antenna_curl = s_dragon_axes_smile.antenna_curl * sm + s_dragon_axes_happy.antenna_curl * hp +
                        s_dragon_axes_sad.antenna_curl * sd + s_dragon_axes_surprise.antenna_curl * su +
                        s_dragon_axes_angry.antenna_curl * an;
    out->look_bias_x = s_dragon_axes_smile.look_bias_x * sm + s_dragon_axes_happy.look_bias_x * hp +
                       s_dragon_axes_sad.look_bias_x * sd + s_dragon_axes_surprise.look_bias_x * su +
                       s_dragon_axes_angry.look_bias_x * an;
    out->look_bias_y = s_dragon_axes_smile.look_bias_y * sm + s_dragon_axes_happy.look_bias_y * hp +
                       s_dragon_axes_sad.look_bias_y * sd + s_dragon_axes_surprise.look_bias_y * su +
                       s_dragon_axes_angry.look_bias_y * an;
}

static int16_t gfx_dragon_round_to_i16(float value)
{
    return (int16_t)lroundf(value);
}

static void gfx_dragon_derive_pose(const gfx_dragon_emote_mix_t *mix, gfx_dragon_derived_pose_t *out)
{
    gfx_dragon_emotion_axes_t axes;
    float look_bias_x;
    float look_bias_y;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->eye_white.s = 100;
    out->pupil.s = 100;
    out->antenna.s = 100;

    gfx_dragon_blend_axes(mix, &axes);
    look_bias_x = axes.look_bias_x * 46.0f + (mix != NULL ? (float)mix->look_x : 0.0f);
    look_bias_y = axes.look_bias_y * 34.0f + (mix != NULL ? (float)mix->look_y : 0.0f);

    out->eye_white.x = gfx_dragon_round_to_i16(look_bias_x * 0.20f + look_bias_x);
    out->eye_white.y = gfx_dragon_round_to_i16((-axes.alert * 2.0f) + (axes.droop * 3.0f) + look_bias_y);
    out->eye_white.r = gfx_dragon_round_to_i16(axes.eye_focus * 2.0f - axes.eye_soft);
    out->eye_white.s = gfx_dragon_round_to_i16((1.0f + axes.eye_open * 0.16f - axes.droop * 0.05f) * 100.0f);

    out->pupil.x = gfx_dragon_round_to_i16(axes.pupil_x + look_bias_x * 0.35f);
    out->pupil.y = gfx_dragon_round_to_i16(axes.pupil_y + look_bias_y * 0.35f);
    out->pupil.s = gfx_dragon_round_to_i16(axes.pupil_scale * 100.0f);

    out->antenna.x = gfx_dragon_round_to_i16(look_bias_x * 0.10f);
    out->antenna.y = gfx_dragon_round_to_i16(-axes.antenna_lift * 8.0f + axes.droop * 2.0f);
    out->antenna.r = gfx_dragon_round_to_i16(axes.antenna_open * 12.0f + axes.antenna_curl * 18.0f - axes.droop * 6.0f);
    out->antenna.s = gfx_dragon_round_to_i16((1.0f + axes.alert * 0.05f + axes.antenna_lift * 0.04f) * 100.0f);
}

void gfx_dragon_emote_expr_to_mix(const gfx_dragon_emote_expr_t *expr, gfx_dragon_emote_mix_t *mix)
{
    if (mix == NULL) {
        return;
    }

    memset(mix, 0, sizeof(*mix));
    mix->pupil_shape = GFX_DRAGON_PUPIL_SHAPE_AUTO;
    if (expr == NULL) {
        return;
    }

    mix->w_smile = expr->w_smile;
    mix->w_happy = expr->w_happy;
    mix->w_sad = expr->w_sad;
    mix->w_surprise = expr->w_surprise;
    mix->w_angry = expr->w_angry;
    mix->look_x = expr->w_look_x;
    mix->look_y = expr->w_look_y;
    mix->pupil_shape = expr->pupil_shape;
}

esp_err_t gfx_dragon_emote_find_expr_index(const gfx_dragon_emote_assets_t *assets,
                                           const char *name,
                                           size_t *index_out)
{
    size_t index;

    ESP_RETURN_ON_FALSE(assets != NULL && assets->expr_sequence != NULL, ESP_ERR_INVALID_STATE, TAG, "dragon expr assets are not ready");
    ESP_RETURN_ON_FALSE(name != NULL, ESP_ERR_INVALID_ARG, TAG, "name is NULL");
    ESP_RETURN_ON_FALSE(index_out != NULL, ESP_ERR_INVALID_ARG, TAG, "index out is NULL");

    for (index = 0; index < assets->expr_sequence_count; index++) {
        if (assets->expr_sequence[index].name != NULL &&
                strcmp(assets->expr_sequence[index].name, name) == 0) {
            *index_out = index;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t gfx_dragon_emote_eval_mix(const gfx_dragon_emote_assets_t *assets,
                                    const gfx_dragon_emote_mix_t *mix,
                                    gfx_dragon_emote_pose_t *pose_out,
                                    gfx_dragon_transform_t *pupil_out,
                                    gfx_dragon_pupil_shape_t *pupil_shape_out)
{
    gfx_dragon_derived_pose_t derived;

    ESP_RETURN_ON_FALSE(assets != NULL, ESP_ERR_INVALID_STATE, TAG, "dragon assets are not ready");
    ESP_RETURN_ON_FALSE(mix != NULL, ESP_ERR_INVALID_ARG, TAG, "mix is NULL");
    ESP_RETURN_ON_FALSE(pose_out != NULL, ESP_ERR_INVALID_ARG, TAG, "pose out is NULL");

    memset(pose_out, 0, sizeof(*pose_out));
    pose_out->head.s = 100;
    pose_out->body.s = 100;
    pose_out->tail.s = 100;
    pose_out->clawL.s = 100;
    pose_out->clawR.s = 100;
    pose_out->antenna.s = 100;
    pose_out->eyeL.s = 100;
    pose_out->eyeR.s = 100;
    pose_out->dots.s = 100;

    gfx_dragon_derive_pose(mix, &derived);
    pose_out->antenna = derived.antenna;
    pose_out->eyeL = derived.eye_white;
    pose_out->eyeR = derived.eye_white;
    if (pupil_out != NULL) {
        *pupil_out = derived.pupil;
    }

    if (pupil_shape_out != NULL) {
        *pupil_shape_out = gfx_dragon_resolve_pupil_shape(mix);
    }
    return ESP_OK;
}

static esp_err_t apply_scaled_transform(gfx_obj_t *part_obj,
                                        gfx_obj_t *root_obj,
                                        const gfx_mesh_img_point_t *base_pts,
                                        size_t pt_count,
                                        int32_t ax, int32_t ay,
                                        float sc, int32_t ox, int32_t oy,
                                        const gfx_dragon_transform_t *t)
{
    if (part_obj == NULL || root_obj == NULL || base_pts == NULL || pt_count == 0 || t == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gfx_mesh_img_point_t transformed[64];
    float abs_x[64];
    float abs_y[64];
    float min_x;
    float min_y;
    if (pt_count > 64) pt_count = 64;

    float angle = DEG_TO_RAD((float)t->r);
    float s = ((float)t->s / 100.0f) * sc;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    // Convert from dragon_animator.html viewBox coordinates into widget-local coordinates.
    float sax = ((float)ax - DRAGON_VIEWBOX_CX) * sc + (float)ox;
    float say = ((float)ay - DRAGON_VIEWBOX_CY) * sc + (float)oy;

    for (size_t i = 0; i < pt_count; i++) {
        float px = (float)base_pts[i].x;
        float py = (float)base_pts[i].y;

        // Rotate
        float rx = px * cos_a - py * sin_a;
        float ry = px * sin_a + py * cos_a;

        abs_x[i] = rx * s + (float)t->x * sc + sax;
        abs_y[i] = ry * s + (float)t->y * sc + say;
    }

    min_x = abs_x[0];
    min_y = abs_y[0];
    for (size_t i = 1; i < pt_count; i++) {
        if (abs_x[i] < min_x) {
            min_x = abs_x[i];
        }
        if (abs_y[i] < min_y) {
            min_y = abs_y[i];
        }
    }

    ESP_RETURN_ON_ERROR(gfx_obj_align_to(part_obj, root_obj, GFX_ALIGN_TOP_LEFT,
                                         (gfx_coord_t)min_x, (gfx_coord_t)min_y),
                        TAG, "align part failed");

    for (size_t i = 0; i < pt_count; i++) {
        transformed[i].x = (int16_t)(abs_x[i] - min_x);
        transformed[i].y = (int16_t)(abs_y[i] - min_y);
    }

    return gfx_mesh_img_set_points(part_obj, transformed, pt_count);
}

static void gfx_dragon_emote_update_part(gfx_dragon_part_state_t *ps, int16_t div)
{
    ps->cur.x = gfx_dragon_emote_ease(ps->cur.x, ps->tgt.x, div);
    ps->cur.y = gfx_dragon_emote_ease(ps->cur.y, ps->tgt.y, div);
    ps->cur.r = gfx_dragon_emote_ease(ps->cur.r, ps->tgt.r, div);
    ps->cur.s = gfx_dragon_emote_ease(ps->cur.s, ps->tgt.s, div);
}

static void gfx_dragon_emote_update_shape(gfx_dragon_shape_state_t *shape, int16_t div)
{
    size_t i;

    if (shape == NULL) {
        return;
    }
    for (i = 0; i < shape->count; i++) {
        shape->cur[i].x = gfx_dragon_emote_ease(shape->cur[i].x, shape->tgt[i].x, div);
        shape->cur[i].y = gfx_dragon_emote_ease(shape->cur[i].y, shape->tgt[i].y, div);
    }
}

esp_err_t gfx_dragon_emote_update_pose(gfx_obj_t *obj, gfx_dragon_emote_t *dragon)
{
    if (dragon == NULL || dragon->assets == NULL) return ESP_OK;

    int16_t div = dragon->cfg.damping_div;
    if (div < 1) div = 1;

    gfx_dragon_emote_update_part(&dragon->head, div);
    gfx_dragon_emote_update_part(&dragon->body, div);
    gfx_dragon_emote_update_part(&dragon->tail, div);
    gfx_dragon_emote_update_part(&dragon->clawL, div);
    gfx_dragon_emote_update_part(&dragon->clawR, div);
    gfx_dragon_emote_update_part(&dragon->antenna, div);
    gfx_dragon_emote_update_part(&dragon->eyeL, div);
    gfx_dragon_emote_update_part(&dragon->eyeR, div);
    gfx_dragon_emote_update_part(&dragon->pupilL, div);
    gfx_dragon_emote_update_part(&dragon->pupilR, div);
    gfx_dragon_emote_update_part(&dragon->dots, div);
    gfx_dragon_emote_update_shape(&dragon->pupil_shape, div);

    // Get widget-local resolution
    gfx_obj_calc_pos_in_parent(obj);
    int32_t ww = dragon->cfg.display_w;
    int32_t wh = dragon->cfg.display_h;
    const gfx_dragon_emote_assets_t *ass = dragon->assets;

    // Match dragon_animator.html viewBox: 1000x980 content inside 120,150 origin.
    float sc = (float)ww / DRAGON_VIEWBOX_W;
    if ((float)wh / DRAGON_VIEWBOX_H < sc) sc = (float)wh / DRAGON_VIEWBOX_H;

    // Widget-local center, then each part aligns to the dragon root object.
    int32_t ox = (int32_t)(ww / 2);
    int32_t oy = (int32_t)(wh / 2);

    apply_scaled_transform(dragon->clawL_obj, obj,        ass->pts_clawL,          ass->count_clawL,   283, 731, sc, ox, oy, &dragon->clawL.cur);
    apply_scaled_transform(dragon->body_obj, obj,         ass->pts_body,           ass->count_body,    860, 424, sc, ox, oy, &dragon->body.cur);
    apply_scaled_transform(dragon->tail_obj, obj,         ass->pts_tail,           ass->count_tail,    980, 312, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->tail_stripe1_obj, obj, s_dragon_tail_stripe1_pts, sizeof(s_dragon_tail_stripe1_pts) / sizeof(s_dragon_tail_stripe1_pts[0]),
                           955, 230, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->tail_stripe2_obj, obj, s_dragon_tail_stripe2_pts, sizeof(s_dragon_tail_stripe2_pts) / sizeof(s_dragon_tail_stripe2_pts[0]),
                           1024, 296, sc, ox, oy, &dragon->tail.cur);
    apply_scaled_transform(dragon->antennaL_obj, obj,     s_dragon_antenna_l_pts,  sizeof(s_dragon_antenna_l_pts) / sizeof(s_dragon_antenna_l_pts[0]),
                           526, 353, sc, ox, oy, &dragon->antenna.cur);
    apply_scaled_transform(dragon->antennaR_obj, obj,     s_dragon_antenna_r_pts,  sizeof(s_dragon_antenna_r_pts) / sizeof(s_dragon_antenna_r_pts[0]),
                           673, 355, sc, ox, oy, &dragon->antenna.cur);
    apply_scaled_transform(dragon->head_obj, obj,         ass->pts_head,           ass->count_head,    633, 633, sc, ox, oy, &dragon->head.cur);
    apply_scaled_transform(dragon->eyeL_obj, obj,         ass->pts_eyeL,           ass->count_eyeL,    478, 652, sc, ox, oy, &dragon->eyeL.cur);
    apply_scaled_transform(dragon->eyeR_obj, obj,         ass->pts_eyeR,           ass->count_eyeR,    633, 652, sc, ox, oy, &dragon->eyeR.cur);
    apply_scaled_transform(dragon->eyeLineL_obj, obj,     dragon->pupil_shape.cur, dragon->pupil_shape.count,
                           478, 665, sc, ox, oy, &dragon->pupilL.cur);
    apply_scaled_transform(dragon->eyeLineR_obj, obj,     dragon->pupil_shape.cur, dragon->pupil_shape.count,
                           633, 665, sc, ox, oy, &dragon->pupilR.cur);
    apply_scaled_transform(dragon->clawR_obj, obj,        ass->pts_clawR,          ass->count_clawR,   703, 974, sc, ox, oy, &dragon->clawR.cur);
    apply_scaled_transform(dragon->dot1_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           872, 846, sc, ox, oy, &dragon->dots.cur);
    apply_scaled_transform(dragon->dot2_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           915, 784, sc, ox, oy, &dragon->dots.cur);
    apply_scaled_transform(dragon->dot3_obj, obj,         s_dragon_dot_circle_pts, sizeof(s_dragon_dot_circle_pts) / sizeof(s_dragon_dot_circle_pts[0]),
                           940, 714, sc, ox, oy, &dragon->dots.cur);

    return ESP_OK;
}
