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
#include "widget/lobster/gfx_lobster_emote_priv.h"

#define GFX_LOBSTER_DEFAULT_LOOK_SCALE_X 46.0f
#define GFX_LOBSTER_DEFAULT_LOOK_SCALE_Y 34.0f
#define GFX_LOBSTER_DEFAULT_EYE_X_FROM_LOOK 0.20f
#define GFX_LOBSTER_DEFAULT_EYE_Y_FROM_ALERT -2.0f
#define GFX_LOBSTER_DEFAULT_EYE_Y_FROM_DROOP 3.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_BASE 1.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_OPEN 0.16f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_DROOP -0.05f
#define GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_FOCUS 2.0f
#define GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_SOFT -1.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_FROM_LOOK 0.35f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_FROM_LOOK 0.35f
#define GFX_LOBSTER_DEFAULT_MOUTH_X_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_MOUTH_Y_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_ANTENNA_X_FROM_LOOK 0.10f
#define GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_LIFT -8.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_DROOP 2.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_OPEN 12.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_CURL 18.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_DROOP -6.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_BASE 1.0f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_ALERT 0.05f
#define GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_LIFT 0.04f
#define GFX_LOBSTER_DEFAULT_LOOK_X_MIN -22.0f
#define GFX_LOBSTER_DEFAULT_LOOK_X_MAX 22.0f
#define GFX_LOBSTER_DEFAULT_LOOK_Y_MIN -16.0f
#define GFX_LOBSTER_DEFAULT_LOOK_Y_MAX 16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_MIN -16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_X_MAX 16.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_MIN -18.0f
#define GFX_LOBSTER_DEFAULT_PUPIL_Y_MAX 18.0f
#define GFX_LOBSTER_DEFAULT_EYE_SCALE_MULTIPLIER 1.12f
#define GFX_LOBSTER_DEFAULT_ANTENNA_THICKNESS_BASE 15.0f
#define GFX_LOBSTER_DEFAULT_TIMER_PERIOD_MS 33U
#define GFX_LOBSTER_DEFAULT_DAMPING_DIV 4
#define GFX_LOBSTER_DEFAULT_EYE_SEGS 18U
#define GFX_LOBSTER_DEFAULT_PUPIL_SEGS 14U
#define GFX_LOBSTER_DEFAULT_ANTENNA_SEGS 18U

static const char *TAG = "lobster_pose";

extern esp_err_t gfx_face_emote_apply_bezier_stroke(gfx_obj_t *obj, const int16_t *pts, bool closed,
                                                    int32_t cx, int32_t cy, int32_t scale_percent,
                                                    int32_t thickness, bool flip_x, int32_t segs);
extern esp_err_t gfx_face_emote_apply_bezier_fill(gfx_obj_t *obj, const int16_t *pts,
                                                  int32_t cx, int32_t cy, int32_t scale_percent,
                                                  bool flip_x, int32_t segs);

#define PI 3.14159265f
#define DEG_TO_RAD(d) ((d) * PI / 180.0f)
#define LOBSTER_VIEWBOX_X   120.0f
#define LOBSTER_VIEWBOX_Y   150.0f
#define LOBSTER_VIEWBOX_W  1000.0f
#define LOBSTER_VIEWBOX_H   980.0f
#define LOBSTER_VIEWBOX_CX (LOBSTER_VIEWBOX_X + LOBSTER_VIEWBOX_W * 0.5f)
#define LOBSTER_VIEWBOX_CY (LOBSTER_VIEWBOX_Y + LOBSTER_VIEWBOX_H * 0.5f)

static const gfx_mesh_img_point_t s_lobster_pupil_o_pts[] = {
    { 0, -18 }, { 9, -16 }, { 16, -9 }, { 18, 0 }, { 16, 9 }, { 9, 16 }, { 0, 18 }, { -9, 16 },
    { -16, 9 }, { -18, 0 }, { -16, -9 }, { -9, -16 }, { 0, -18 }, { 9, -16 }, { 16, -9 }, { 18, 0 },
    { 16, 9 }, { 9, 16 }, { 0, 18 }, { -9, 16 }, { -16, 9 }, { 0, -18 }
};
static const gfx_mesh_img_point_t s_lobster_pupil_u_pts[] = {
    { -26, -2 }, { -22, 5 }, { -18, 11 }, { -12, 16 }, { -6, 19 }, { 0, 20 }, { 6, 19 }, { 12, 16 },
    { 18, 11 }, { 22, 5 }, { 26, -2 }, { 18, -2 }, { 14, 4 }, { 9, 8 }, { 4, 10 }, { 0, 11 },
    { -4, 10 }, { -9, 8 }, { -14, 4 }, { -18, -2 }, { -26, -2 }, { -26, -2 }
};
static const gfx_mesh_img_point_t s_lobster_pupil_n_pts[] = {
    { -26, 2 }, { -22, -5 }, { -18, -11 }, { -12, -16 }, { -6, -19 }, { 0, -20 }, { 6, -19 }, { 12, -16 },
    { 18, -11 }, { 22, -5 }, { 26, 2 }, { 18, 2 }, { 14, -4 }, { 9, -8 }, { 4, -10 }, { 0, -11 },
    { -4, -10 }, { -9, -8 }, { -14, -4 }, { -18, 2 }, { -26, 2 }, { -26, 2 }
};
static const gfx_mesh_img_point_t s_lobster_pupil_line_pts[] = {
    { -28, 4 }, { -22, 2 }, { -14, 1 }, { -5, 0 }, { 5, 0 }, { 14, 1 }, { 22, 2 }, { 28, 4 },
    { 28, -4 }, { 22, -2 }, { 14, -1 }, { 5, 0 }, { -5, 0 }, { -14, -1 }, { -22, -2 }, { -28, -4 },
    { -28, 4 }, { -22, 2 }, { -14, 1 }, { -5, 0 }, { 5, 0 }, { -28, 4 }
};
static const int16_t s_eye_white_base[6][14] = {
    { -58, 0, -52, -62, 52, -62, 58, 0, 52, 62, -52, 62, -58, 0 },
    { -58, 8, -46, -46, 46, -46, 58, 8, 50, 42, -50, 42, -58, 8 },
    { -58, 10, -44, -40, 44, -40, 58, 10, 52, 36, -52, 36, -58, 10 },
    { -58, 12, -50, -50, 42, -62, 58, -6, 50, 52, -46, 60, -58, 12 },
    { -50, 0, -50, -82, 50, -82, 50, 0, 50, 82, -50, 82, -50, 0 },
    { -58, -4, -46, -34, 40, -56, 58, 6, 50, 44, -50, 48, -58, -4 }
};
static const int16_t s_antenna_base[6][8] = {
    { 0, 95, -38, 30, -25, -60, 32, -108 },
    { 0, 98, -34, 38, -18, -56, 38, -106 },
    { 0, 102, -40, 26, -10, -62, 44, -114 },
    { 0, 90, -28, 46, -26, -42, 22, -94 },
    { 0, 108, -46, 20, -6, -72, 50, -124 },
    { 0, 100, -18, 18, 8, -70, 40, -118 }
};
static const int16_t s_antenna_right_base[6][8] = {
    { 0, 95, 26, 28, 78, -40, 118, -104 },
    { 0, 98, 30, 36, 84, -34, 122, -100 },
    { 0, 102, 22, 20, 88, -46, 128, -110 },
    { 0, 90, 34, 40, 74, -18, 108, -86 },
    { 0, 108, 18, 16, 94, -54, 136, -118 },
    { 0, 100, 20, 10, 86, -56, 126, -112 }
};
static const int16_t s_pupil_curve_o[14] = { -18, 0, -18, -14, 18, -14, 18, 0, 18, 14, -18, 14, -18, 0 };
static const int16_t s_pupil_curve_u[14] = { -24, -2, -14, 22, 14, 22, 24, -2, 12, 10, -12, 10, -24, -2 };
static const int16_t s_pupil_curve_n[14] = { -24, 2, -14, -22, 14, -22, 24, 2, 12, -10, -12, -10, -24, 2 };
static const int16_t s_pupil_curve_line[14] = { -26, 0, -8, -2, 8, -2, 26, 0, 8, 2, -8, 2, -26, 0 };
static const int16_t s_mouth_base[6][14] = {
    { -20, 0, -10, 0, 10, 0, 20, 0, 10, 1, -10, 1, -20, 0 },
    { -25, -2, -12, 10, 12, 10, 25, -2, 15, 6, -15, 6, -25, -2 },
    { -25, -5, -12, 8, 12, 8, 25, -5, 15, -2, -15, -2, -25, -5 },
    { -25, 5, -12, -8, 12, -8, 25, 5, 15, 2, -15, 2, -25, 5 },
    { -12, -12, -12, 14, 12, 14, 12, -12, 12, -26, -12, -26, -12, -12 },
    { -25, 0, -12, -6, 12, -6, 25, 0, 15, -1, -15, -1, -25, 0 }
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
} gfx_lobster_emotion_axes_t;

static const gfx_lobster_emote_axis_t s_default_axis_smile = { -0.10f, 0.10f, 0.80f, 0.0f, -2.0f, 0.92f, -0.10f, 0.10f, 0.12f, 0.22f, 0.18f, 0.0f, -0.04f };
static const gfx_lobster_emote_axis_t s_default_axis_happy = { 0.06f, 0.22f, 1.00f, 0.0f, -6.0f, 0.84f, -0.14f, 0.32f, 0.30f, 0.46f, 0.30f, 0.0f, -0.12f };
static const gfx_lobster_emote_axis_t s_default_axis_sad = { -0.24f, -0.12f, -0.10f, -8.0f, 8.0f, 1.05f, 1.00f, -0.20f, -0.12f, -0.24f, -0.18f, -0.18f, 0.22f };
static const gfx_lobster_emote_axis_t s_default_axis_surprise = { 0.95f, 0.45f, 0.10f, 0.0f, -10.0f, 0.72f, -0.20f, 1.00f, 0.62f, 0.80f, 0.34f, 0.0f, -0.28f };
static const gfx_lobster_emote_axis_t s_default_axis_angry = { -0.18f, 1.00f, -0.50f, 0.0f, -2.0f, 0.88f, 0.10f, 0.55f, 0.18f, 0.10f, 0.70f, 0.0f, 0.02f };

static inline int16_t gfx_lobster_emote_ease(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;
    if (diff == 0) return cur;
    if (div < 1) div = 1;
    step = diff / div;
    if (step == 0) step = (diff > 0) ? 1 : -1;
    return (int16_t)((int32_t)cur + step);
}

static const gfx_lobster_emote_semantics_t s_default_semantics = {
    .look_scale_x = GFX_LOBSTER_DEFAULT_LOOK_SCALE_X,
    .look_scale_y = GFX_LOBSTER_DEFAULT_LOOK_SCALE_Y,
    .eye_x_from_look = GFX_LOBSTER_DEFAULT_EYE_X_FROM_LOOK,
    .eye_y_from_alert = GFX_LOBSTER_DEFAULT_EYE_Y_FROM_ALERT,
    .eye_y_from_droop = GFX_LOBSTER_DEFAULT_EYE_Y_FROM_DROOP,
    .eye_scale_base = GFX_LOBSTER_DEFAULT_EYE_SCALE_BASE,
    .eye_scale_from_eye_open = GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_OPEN,
    .eye_scale_from_droop = GFX_LOBSTER_DEFAULT_EYE_SCALE_FROM_DROOP,
    .eye_rot_from_focus = GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_FOCUS,
    .eye_rot_from_soft = GFX_LOBSTER_DEFAULT_EYE_ROT_FROM_SOFT,
    .pupil_x_from_look = GFX_LOBSTER_DEFAULT_PUPIL_X_FROM_LOOK,
    .pupil_y_from_look = GFX_LOBSTER_DEFAULT_PUPIL_Y_FROM_LOOK,
    .mouth_x_from_look = GFX_LOBSTER_DEFAULT_MOUTH_X_FROM_LOOK,
    .mouth_y_from_look = GFX_LOBSTER_DEFAULT_MOUTH_Y_FROM_LOOK,
    .antenna_x_from_look = GFX_LOBSTER_DEFAULT_ANTENNA_X_FROM_LOOK,
    .antenna_y_from_lift = GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_LIFT,
    .antenna_y_from_droop = GFX_LOBSTER_DEFAULT_ANTENNA_Y_FROM_DROOP,
    .antenna_rot_from_open = GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_OPEN,
    .antenna_rot_from_curl = GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_CURL,
    .antenna_rot_from_droop = GFX_LOBSTER_DEFAULT_ANTENNA_ROT_FROM_DROOP,
    .antenna_scale_base = GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_BASE,
    .antenna_scale_from_alert = GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_ALERT,
    .antenna_scale_from_lift = GFX_LOBSTER_DEFAULT_ANTENNA_SCALE_FROM_LIFT,
    .look_x_min = GFX_LOBSTER_DEFAULT_LOOK_X_MIN,
    .look_x_max = GFX_LOBSTER_DEFAULT_LOOK_X_MAX,
    .look_y_min = GFX_LOBSTER_DEFAULT_LOOK_Y_MIN,
    .look_y_max = GFX_LOBSTER_DEFAULT_LOOK_Y_MAX,
    .pupil_x_min = GFX_LOBSTER_DEFAULT_PUPIL_X_MIN,
    .pupil_x_max = GFX_LOBSTER_DEFAULT_PUPIL_X_MAX,
    .pupil_y_min = GFX_LOBSTER_DEFAULT_PUPIL_Y_MIN,
    .pupil_y_max = GFX_LOBSTER_DEFAULT_PUPIL_Y_MAX,
    .eye_scale_multiplier = GFX_LOBSTER_DEFAULT_EYE_SCALE_MULTIPLIER,
    .antenna_thickness_base = GFX_LOBSTER_DEFAULT_ANTENNA_THICKNESS_BASE,
    .timer_period_ms = GFX_LOBSTER_DEFAULT_TIMER_PERIOD_MS,
    .damping_div = GFX_LOBSTER_DEFAULT_DAMPING_DIV,
    .eye_segs = GFX_LOBSTER_DEFAULT_EYE_SEGS,
    .pupil_segs = GFX_LOBSTER_DEFAULT_PUPIL_SEGS,
    .antenna_segs = GFX_LOBSTER_DEFAULT_ANTENNA_SEGS,
    .reserved = 0,
    .smile = { -0.10f, 0.10f, 0.80f, 0.0f, -2.0f, 0.92f, -0.10f, 0.10f, 0.12f, 0.22f, 0.18f, 0.0f, -0.04f },
    .happy = { 0.06f, 0.22f, 1.00f, 0.0f, -6.0f, 0.84f, -0.14f, 0.32f, 0.30f, 0.46f, 0.30f, 0.0f, -0.12f },
    .sad = { -0.24f, -0.12f, -0.10f, -8.0f, 8.0f, 1.05f, 1.00f, -0.20f, -0.12f, -0.24f, -0.18f, -0.18f, 0.22f },
    .surprise = { 0.95f, 0.45f, 0.10f, 0.0f, -10.0f, 0.72f, -0.20f, 1.00f, 0.62f, 0.80f, 0.34f, 0.0f, -0.28f },
    .angry = { -0.18f, 1.00f, -0.50f, 0.0f, -2.0f, 0.88f, 0.10f, 0.55f, 0.18f, 0.10f, 0.70f, 0.0f, 0.02f },
};

static const gfx_lobster_emote_semantics_t *gfx_lobster_emote_get_semantics(const gfx_lobster_emote_assets_t *assets)
{
    if (assets != NULL && assets->semantics != NULL) {
        return assets->semantics;
    }
    return &s_default_semantics;
}

static const gfx_lobster_emote_axis_t *gfx_lobster_emote_get_axis(const gfx_lobster_emote_semantics_t *semantics,
                                                                  const char *name)
{
    if (semantics == NULL || name == NULL) {
        return NULL;
    }
    if (strcmp(name, "smile") == 0) {
        return &semantics->smile;
    }
    if (strcmp(name, "happy") == 0) {
        return &semantics->happy;
    }
    if (strcmp(name, "sad") == 0) {
        return &semantics->sad;
    }
    if (strcmp(name, "surprise") == 0) {
        return &semantics->surprise;
    }
    if (strcmp(name, "angry") == 0) {
        return &semantics->angry;
    }
    return NULL;
}

static void gfx_lobster_emote_accumulate_axis(gfx_lobster_emotion_axes_t *dst,
                                              const gfx_lobster_emote_axis_t *axis,
                                              float weight)
{
    if (dst == NULL || axis == NULL || weight == 0.0f) {
        return;
    }
    dst->eye_open += axis->eye_open * weight;
    dst->eye_focus += axis->eye_focus * weight;
    dst->eye_soft += axis->eye_soft * weight;
    dst->pupil_x += axis->pupil_x * weight;
    dst->pupil_y += axis->pupil_y * weight;
    dst->pupil_scale += (axis->pupil_scale - 1.0f) * weight;
    dst->droop += axis->droop * weight;
    dst->alert += axis->alert * weight;
    dst->antenna_lift += axis->antenna_lift * weight;
    dst->antenna_open += axis->antenna_open * weight;
    dst->antenna_curl += axis->antenna_curl * weight;
    dst->look_bias_x += axis->look_bias_x * weight;
    dst->look_bias_y += axis->look_bias_y * weight;
}

const gfx_mesh_img_point_t *gfx_lobster_emote_get_pupil_shape_points(gfx_lobster_pupil_shape_t shape, size_t *count_out)
{
    const gfx_mesh_img_point_t *pts = s_lobster_pupil_o_pts;
    size_t count = sizeof(s_lobster_pupil_o_pts) / sizeof(s_lobster_pupil_o_pts[0]);
    if (shape == GFX_LOBSTER_PUPIL_SHAPE_U) {
        pts = s_lobster_pupil_u_pts;
        count = sizeof(s_lobster_pupil_u_pts) / sizeof(s_lobster_pupil_u_pts[0]);
    } else if (shape == GFX_LOBSTER_PUPIL_SHAPE_N) {
        pts = s_lobster_pupil_n_pts;
        count = sizeof(s_lobster_pupil_n_pts) / sizeof(s_lobster_pupil_n_pts[0]);
    } else if (shape == GFX_LOBSTER_PUPIL_SHAPE_LINE) {
        pts = s_lobster_pupil_line_pts;
        count = sizeof(s_lobster_pupil_line_pts) / sizeof(s_lobster_pupil_line_pts[0]);
    }
    if (count_out != NULL) *count_out = count;
    return pts;
}

static gfx_lobster_pupil_shape_t resolve_pupil_shape(const gfx_lobster_emote_state_t *state)
{
    if (state == NULL) return GFX_LOBSTER_PUPIL_SHAPE_O;
    if (state->pupil_shape != GFX_LOBSTER_PUPIL_SHAPE_AUTO) return state->pupil_shape;
    if (state->w_angry >= state->w_surprise && state->w_angry >= state->w_sad &&
            state->w_angry >= state->w_happy && state->w_angry >= state->w_smile) {
        return GFX_LOBSTER_PUPIL_SHAPE_LINE;
    }
    if (state->w_sad >= state->w_surprise && state->w_sad >= state->w_happy && state->w_sad >= state->w_smile) {
        return GFX_LOBSTER_PUPIL_SHAPE_N;
    }
    if (state->w_happy >= state->w_surprise || state->w_smile >= state->w_surprise) {
        return GFX_LOBSTER_PUPIL_SHAPE_U;
    }
    return GFX_LOBSTER_PUPIL_SHAPE_O;
}

esp_err_t gfx_lobster_emote_find_state_index(const gfx_lobster_emote_assets_t *assets, const char *name, size_t *index_out)
{
    ESP_RETURN_ON_FALSE(assets != NULL && assets->sequence != NULL, ESP_ERR_INVALID_STATE, TAG, "sequence assets not ready");
    ESP_RETURN_ON_FALSE(name != NULL && index_out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    for (size_t i = 0; i < assets->sequence_count; i++) {
        if (assets->sequence[i].name != NULL && strcmp(assets->sequence[i].name, name) == 0) {
            *index_out = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void gfx_lobster_emote_state_to_mix(const gfx_lobster_emote_state_t *state, gfx_lobster_emote_state_t *mix_out)
{
    if (mix_out == NULL) return;
    memset(mix_out, 0, sizeof(*mix_out));
    if (state != NULL) *mix_out = *state;
}

static int16_t iroundf(float v)
{
    return (int16_t)lroundf(v);
}

static float clampf_local(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

esp_err_t gfx_lobster_emote_validate_assets(const gfx_lobster_emote_assets_t *assets)
{
    if (assets == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (assets->sequence == NULL || assets->sequence_count == 0U) {
        return ESP_ERR_INVALID_STATE;
    }

    if (assets->layout != NULL) {
        if (assets->export_meta == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        if (assets->export_meta->version != GFX_LOBSTER_EMOTE_EXPORT_VERSION &&
                assets->export_meta->version != GFX_LOBSTER_EMOTE_EXPORT_VERSION_LEGACY) {
            return ESP_ERR_INVALID_STATE;
        }
        if (assets->export_meta->export_width <= 0 || assets->export_meta->export_height <= 0 ||
                assets->export_meta->design_viewbox_w <= 0 || assets->export_meta->design_viewbox_h <= 0) {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    return ESP_OK;
}

static void blend_curve14(const int16_t base[6][14], const gfx_lobster_emote_state_t *state, gfx_lobster_curve14_t *out)
{
    for (int i = 0; i < 14; i++) {
        out->pts[i] = iroundf(base[0][i]
            + (base[1][i] - base[0][i]) * state->w_smile / 100.0f
            + (base[2][i] - base[0][i]) * state->w_happy / 100.0f
            + (base[3][i] - base[0][i]) * state->w_sad / 100.0f
            + (base[4][i] - base[0][i]) * state->w_surprise / 100.0f
            + (base[5][i] - base[0][i]) * state->w_angry / 100.0f);
    }
}

static void blend_curve8(const int16_t base[6][8], const gfx_lobster_emote_state_t *state, gfx_lobster_curve8_t *out)
{
    for (int i = 0; i < 8; i++) {
        out->pts[i] = iroundf(base[0][i]
            + (base[1][i] - base[0][i]) * state->w_smile / 100.0f
            + (base[2][i] - base[0][i]) * state->w_happy / 100.0f
            + (base[3][i] - base[0][i]) * state->w_sad / 100.0f
            + (base[4][i] - base[0][i]) * state->w_surprise / 100.0f
            + (base[5][i] - base[0][i]) * state->w_angry / 100.0f);
    }
}

static void blend_curve8_right(const int16_t base[6][8], const gfx_lobster_emote_state_t *state, gfx_lobster_curve8_t *out)
{
    blend_curve8(base, state, out);
}

esp_err_t gfx_lobster_emote_eval_state(const gfx_lobster_emote_assets_t *assets,
                                       const gfx_lobster_emote_state_t *state,
                                       gfx_lobster_transform_t *head_out,
                                       gfx_lobster_transform_t *body_out,
                                       gfx_lobster_transform_t *tail_out,
                                       gfx_lobster_transform_t *claw_l_out,
                                       gfx_lobster_transform_t *claw_r_out,
                                       gfx_lobster_transform_t *antenna_out,
                                       gfx_lobster_transform_t *eye_out,
                                       gfx_lobster_transform_t *pupil_out,
                                       gfx_lobster_transform_t *mouth_out,
                                       gfx_lobster_transform_t *dots_out,
                                       gfx_lobster_curve14_t *eye_curve_out,
                                       gfx_lobster_curve14_t *pupil_curve_out,
                                       gfx_lobster_curve14_t *mouth_curve_out,
                                       gfx_lobster_curve8_t *antenna_curve_l_out,
                                       gfx_lobster_curve8_t *antenna_curve_r_out,
                                       gfx_lobster_pupil_shape_t *pupil_shape_out,
                                       int16_t *look_x_out,
                                       int16_t *look_y_out)
{
    gfx_lobster_emotion_axes_t a = {0};
    const gfx_lobster_emote_semantics_t *semantics = gfx_lobster_emote_get_semantics(assets);
    const gfx_lobster_emote_axis_t *axis_smile = (semantics != NULL) ? &semantics->smile : &s_default_axis_smile;
    const gfx_lobster_emote_axis_t *axis_happy = (semantics != NULL) ? &semantics->happy : &s_default_axis_happy;
    const gfx_lobster_emote_axis_t *axis_sad = (semantics != NULL) ? &semantics->sad : &s_default_axis_sad;
    const gfx_lobster_emote_axis_t *axis_surprise = (semantics != NULL) ? &semantics->surprise : &s_default_axis_surprise;
    const gfx_lobster_emote_axis_t *axis_angry = (semantics != NULL) ? &semantics->angry : &s_default_axis_angry;
    const int16_t (*eye_white_base)[14] = (assets && assets->eye_white_base) ? assets->eye_white_base : s_eye_white_base;
    const int16_t (*pupil_base)[14] = (assets && assets->pupil_base) ? assets->pupil_base : NULL;
    const int16_t (*mouth_base)[14] = (assets && assets->mouth_base) ? assets->mouth_base : s_mouth_base;
    const int16_t (*antenna_left_base)[8] = (assets && assets->antenna_left_base) ? assets->antenna_left_base : s_antenna_base;
    const int16_t (*antenna_right_base)[8] = (assets && assets->antenna_right_base) ? assets->antenna_right_base : s_antenna_right_base;
    float sm, hp, sd, su, an, look_x, look_y;
    ESP_RETURN_ON_FALSE(state != NULL, ESP_ERR_INVALID_ARG, TAG, "state is NULL");

    sm = (float)state->w_smile / 100.0f;
    hp = (float)state->w_happy / 100.0f;
    sd = (float)state->w_sad / 100.0f;
    su = (float)state->w_surprise / 100.0f;
    an = (float)state->w_angry / 100.0f;

    a.pupil_scale = 1.0f;
    gfx_lobster_emote_accumulate_axis(&a, axis_smile, sm);
    gfx_lobster_emote_accumulate_axis(&a, axis_happy, hp);
    gfx_lobster_emote_accumulate_axis(&a, axis_sad, sd);
    gfx_lobster_emote_accumulate_axis(&a, axis_surprise, su);
    gfx_lobster_emote_accumulate_axis(&a, axis_angry, an);

    look_x = a.look_bias_x * semantics->look_scale_x + (float)state->w_look_x;
    look_y = a.look_bias_y * semantics->look_scale_y + (float)state->w_look_y;
    look_x = clampf_local(look_x, semantics->look_x_min, semantics->look_x_max);
    look_y = clampf_local(look_y, semantics->look_y_min, semantics->look_y_max);

    if (head_out) *head_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (body_out) *body_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (tail_out) *tail_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (claw_l_out) *claw_l_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (claw_r_out) *claw_r_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (dots_out) *dots_out = (gfx_lobster_transform_t){ 0, 0, 0, 100 };
    if (eye_out) *eye_out = (gfx_lobster_transform_t){
        iroundf(look_x * semantics->eye_x_from_look),
        iroundf(a.alert * semantics->eye_y_from_alert + a.droop * semantics->eye_y_from_droop),
        iroundf(a.eye_focus * semantics->eye_rot_from_focus + a.eye_soft * semantics->eye_rot_from_soft),
        iroundf((semantics->eye_scale_base + a.eye_open * semantics->eye_scale_from_eye_open + a.droop * semantics->eye_scale_from_droop) * 100.0f)
    };
    if (pupil_out) {
        float pupil_x = a.pupil_x + look_x * semantics->pupil_x_from_look;
        float pupil_y = a.pupil_y + look_y * semantics->pupil_y_from_look;
        pupil_x = clampf_local(pupil_x, semantics->pupil_x_min, semantics->pupil_x_max);
        pupil_y = clampf_local(pupil_y, semantics->pupil_y_min, semantics->pupil_y_max);
        *pupil_out = (gfx_lobster_transform_t){
            iroundf(pupil_x), iroundf(pupil_y), 0,
            iroundf(a.pupil_scale * 100.0f)
        };
    }
    if (mouth_out) *mouth_out = (gfx_lobster_transform_t){
        iroundf(look_x * semantics->mouth_x_from_look), iroundf(look_y * semantics->mouth_y_from_look), 0, 100
    };
    if (antenna_out) *antenna_out = (gfx_lobster_transform_t){
        iroundf(look_x * semantics->antenna_x_from_look),
        iroundf(a.antenna_lift * semantics->antenna_y_from_lift + a.droop * semantics->antenna_y_from_droop),
        iroundf(a.antenna_open * semantics->antenna_rot_from_open + a.antenna_curl * semantics->antenna_rot_from_curl + a.droop * semantics->antenna_rot_from_droop),
        iroundf((semantics->antenna_scale_base + a.alert * semantics->antenna_scale_from_alert + a.antenna_lift * semantics->antenna_scale_from_lift) * 100.0f)
    };
    if (eye_curve_out) {
        blend_curve14(eye_white_base, state, eye_curve_out);
    }
    if (antenna_curve_l_out) {
        blend_curve8(antenna_left_base, state, antenna_curve_l_out);
    }
    if (antenna_curve_r_out) {
        blend_curve8_right(antenna_right_base, state, antenna_curve_r_out);
    }
    if (pupil_curve_out) {
        if (pupil_base != NULL) {
            blend_curve14(pupil_base, state, pupil_curve_out);
        } else {
            const int16_t *src = s_pupil_curve_o;
            gfx_lobster_pupil_shape_t shape = resolve_pupil_shape(state);
            if (shape == GFX_LOBSTER_PUPIL_SHAPE_U) src = s_pupil_curve_u;
            else if (shape == GFX_LOBSTER_PUPIL_SHAPE_N) src = s_pupil_curve_n;
            else if (shape == GFX_LOBSTER_PUPIL_SHAPE_LINE) src = s_pupil_curve_line;
            memcpy(pupil_curve_out->pts, src, sizeof(pupil_curve_out->pts));
        }
    }
    if (mouth_curve_out) {
        blend_curve14(mouth_base, state, mouth_curve_out);
    }
    if (pupil_shape_out) *pupil_shape_out = resolve_pupil_shape(state);
    if (look_x_out) *look_x_out = state->w_look_x;
    if (look_y_out) *look_y_out = state->w_look_y;
    return ESP_OK;
}

static void update_part(gfx_lobster_part_state_t *ps, int16_t div)
{
    ps->cur.x = gfx_lobster_emote_ease(ps->cur.x, ps->tgt.x, div);
    ps->cur.y = gfx_lobster_emote_ease(ps->cur.y, ps->tgt.y, div);
    ps->cur.r = gfx_lobster_emote_ease(ps->cur.r, ps->tgt.r, div);
    ps->cur.s = gfx_lobster_emote_ease(ps->cur.s, ps->tgt.s, div);
}

static void update_shape(gfx_lobster_shape_state_t *shape, int16_t div)
{
    for (size_t i = 0; i < shape->count; i++) {
        shape->cur[i].x = gfx_lobster_emote_ease(shape->cur[i].x, shape->tgt[i].x, div);
        shape->cur[i].y = gfx_lobster_emote_ease(shape->cur[i].y, shape->tgt[i].y, div);
    }
}

static void ease_curve14(gfx_lobster_curve14_t *cur, const gfx_lobster_curve14_t *tgt, int16_t div)
{
    for (int i = 0; i < 14; i++) {
        cur->pts[i] = gfx_lobster_emote_ease(cur->pts[i], tgt->pts[i], div);
    }
}

static void ease_curve8(gfx_lobster_curve8_t *cur, const gfx_lobster_curve8_t *tgt, int16_t div)
{
    for (int i = 0; i < 8; i++) {
        cur->pts[i] = gfx_lobster_emote_ease(cur->pts[i], tgt->pts[i], div);
    }
}

esp_err_t gfx_lobster_emote_update_pose(gfx_obj_t *obj, gfx_lobster_emote_t *lobster)
{
    int16_t div;
    int32_t root_x;
    int32_t root_y;
    int32_t ww, wh, ox, oy;
    int32_t eye_scale_percent;
    int32_t pupil_scale_percent;
    int32_t antenna_scale_percent;
    int32_t antenna_thickness;
    int32_t eye_left_cx;
    int32_t eye_left_cy;
    int32_t eye_right_cx;
    int32_t eye_right_cy;
    int32_t pupil_left_cx;
    int32_t pupil_left_cy;
    int32_t pupil_right_cx;
    int32_t pupil_right_cy;
    int32_t mouth_cx;
    int32_t mouth_cy;
    int32_t antenna_left_cx;
    int32_t antenna_left_cy;
    int32_t eye_segs;
    int32_t pupil_segs;
    int32_t antenna_segs;
    const gfx_lobster_emote_semantics_t *semantics;
    float sc;

    if (lobster == NULL || lobster->assets == NULL) return ESP_OK;
    semantics = gfx_lobster_emote_get_semantics(lobster->assets);
    div = lobster->cfg.damping_div;
    if (div < 1) div = 1;
    gfx_obj_calc_pos_in_parent(obj);
    root_x = obj->geometry.x;
    root_y = obj->geometry.y;

    update_part(&lobster->head, div);
    update_part(&lobster->body, div);
    update_part(&lobster->tail, div);
    update_part(&lobster->claw_l, div);
    update_part(&lobster->claw_r, div);
    update_part(&lobster->antenna, div);
    update_part(&lobster->eye_l, div);
    update_part(&lobster->eye_r, div);
    update_part(&lobster->pupil_l, div);
    update_part(&lobster->pupil_r, div);
    update_part(&lobster->mouth, div);
    update_part(&lobster->dots, div);
    lobster->look_x_cur = gfx_lobster_emote_ease(lobster->look_x_cur, lobster->look_x_tgt, div);
    lobster->look_y_cur = gfx_lobster_emote_ease(lobster->look_y_cur, lobster->look_y_tgt, div);
    ease_curve14(&lobster->eye_white_cur, &lobster->eye_white_tgt, div);
    ease_curve14(&lobster->pupil_cur, &lobster->pupil_tgt, div);
    ease_curve14(&lobster->mouth_cur, &lobster->mouth_tgt, div);
    ease_curve8(&lobster->antenna_curve_l_cur, &lobster->antenna_curve_l_tgt, div);
    ease_curve8(&lobster->antenna_curve_r_cur, &lobster->antenna_curve_r_tgt, div);

    ww = lobster->cfg.display_w;
    wh = lobster->cfg.display_h;
    if (lobster->assets->export_meta != NULL &&
            lobster->assets->export_meta->export_width > 0 &&
            lobster->assets->export_meta->export_height > 0) {
        ww = lobster->assets->export_meta->export_width;
        wh = lobster->assets->export_meta->export_height;
    }
    {
        float viewbox_w = (lobster->assets->export_meta != NULL && lobster->assets->export_meta->design_viewbox_w > 0)
                              ? (float)lobster->assets->export_meta->design_viewbox_w
                              : LOBSTER_VIEWBOX_W;
        float viewbox_h = (lobster->assets->export_meta != NULL && lobster->assets->export_meta->design_viewbox_h > 0)
                              ? (float)lobster->assets->export_meta->design_viewbox_h
                              : LOBSTER_VIEWBOX_H;
        sc = (float)ww / viewbox_w;
        if ((float)wh / viewbox_h < sc) sc = (float)wh / viewbox_h;
    }
    ox = ww / 2;
    oy = wh / 2;
    if (lobster->assets->layout != NULL) {
      eye_left_cx = root_x + lobster->assets->layout->eye_left_cx;
      eye_left_cy = root_y + lobster->assets->layout->eye_left_cy;
      eye_right_cx = root_x + lobster->assets->layout->eye_right_cx;
      eye_right_cy = root_y + lobster->assets->layout->eye_right_cy;
      pupil_left_cx = root_x + lobster->assets->layout->pupil_left_cx;
      pupil_left_cy = root_y + lobster->assets->layout->pupil_left_cy;
      pupil_right_cx = root_x + lobster->assets->layout->pupil_right_cx;
      pupil_right_cy = root_y + lobster->assets->layout->pupil_right_cy;
      mouth_cx = root_x + lobster->assets->layout->mouth_cx;
      mouth_cy = root_y + lobster->assets->layout->mouth_cy;
      antenna_left_cx = root_x + lobster->assets->layout->antenna_left_cx;
      antenna_left_cy = root_y + lobster->assets->layout->antenna_left_cy;
      antenna_right_cx = root_x + lobster->assets->layout->antenna_right_cx;
      antenna_right_cy = root_y + lobster->assets->layout->antenna_right_cy;
    } else {
      eye_left_cx = root_x + ox + iroundf((478.47f - LOBSTER_VIEWBOX_CX) * sc);
      eye_left_cy = root_y + oy + iroundf((652.4f - LOBSTER_VIEWBOX_CY) * sc);
      eye_right_cx = root_x + ox + iroundf((632.96f - LOBSTER_VIEWBOX_CX) * sc);
      eye_right_cy = root_y + oy + iroundf((652.4f - LOBSTER_VIEWBOX_CY) * sc);
      pupil_left_cx = root_x + ox + iroundf((478.47f - LOBSTER_VIEWBOX_CX) * sc);
      pupil_left_cy = root_y + oy + iroundf((665.4f - LOBSTER_VIEWBOX_CY) * sc);
      pupil_right_cx = root_x + ox + iroundf((632.96f - LOBSTER_VIEWBOX_CX) * sc);
      pupil_right_cy = root_y + oy + iroundf((665.4f - LOBSTER_VIEWBOX_CY) * sc);
      mouth_cx = root_x + ox + iroundf((555.0f - LOBSTER_VIEWBOX_CX) * sc);
      mouth_cy = root_y + oy + iroundf((760.0f - LOBSTER_VIEWBOX_CY) * sc);
      antenna_left_cx = root_x + ox + iroundf((526.0f - LOBSTER_VIEWBOX_CX) * sc);
      antenna_left_cy = root_y + oy + iroundf((406.53f - LOBSTER_VIEWBOX_CY) * sc);
      antenna_right_cx = root_x + ox + iroundf((709.0f - LOBSTER_VIEWBOX_CX) * sc);
      antenna_right_cy = root_y + oy + iroundf((406.53f - LOBSTER_VIEWBOX_CY) * sc);
    }
    eye_segs = (semantics != NULL && semantics->eye_segs > 0) ? semantics->eye_segs : GFX_LOBSTER_DEFAULT_EYE_SEGS;
    pupil_segs = (semantics != NULL && semantics->pupil_segs > 0) ? semantics->pupil_segs : GFX_LOBSTER_DEFAULT_PUPIL_SEGS;
    antenna_segs = (semantics != NULL && semantics->antenna_segs > 0) ? semantics->antenna_segs : GFX_LOBSTER_DEFAULT_ANTENNA_SEGS;
    eye_scale_percent = iroundf(sc * (float)lobster->eye_l.cur.s * semantics->eye_scale_multiplier);
    pupil_scale_percent = iroundf(sc * (float)lobster->pupil_l.cur.s);
    antenna_scale_percent = iroundf(sc * (float)lobster->antenna.cur.s);
    antenna_thickness = iroundf(sc * semantics->antenna_thickness_base);
    if (antenna_thickness < 2) antenna_thickness = 2;

    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_stroke(lobster->antenna_l_obj, lobster->antenna_curve_l_cur.pts, false,
                                                           antenna_left_cx + lobster->antenna.cur.x,
                                                           antenna_left_cy + lobster->antenna.cur.y,
                                                           antenna_scale_percent, antenna_thickness, false, antenna_segs),
                        TAG, "apply left antenna failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_stroke(lobster->antenna_r_obj, lobster->antenna_curve_r_cur.pts, false,
                                                           antenna_right_cx + lobster->antenna.cur.x,
                                                           antenna_right_cy + lobster->antenna.cur.y,
                                                           antenna_scale_percent, antenna_thickness, false, antenna_segs),
                        TAG, "apply right antenna failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->eye_l_obj, lobster->eye_white_cur.pts,
                                                         eye_left_cx + lobster->eye_l.cur.x + lobster->look_x_cur,
                                                         eye_left_cy + lobster->eye_l.cur.y + lobster->look_y_cur,
                                                         eye_scale_percent, false, eye_segs),
                        TAG, "apply left eye failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->eye_r_obj, lobster->eye_white_cur.pts,
                                                         eye_right_cx + lobster->eye_r.cur.x + lobster->look_x_cur,
                                                         eye_right_cy + lobster->eye_r.cur.y + lobster->look_y_cur,
                                                         eye_scale_percent, true, eye_segs),
                        TAG, "apply right eye failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->pupil_l_obj, lobster->pupil_cur.pts,
                                                         pupil_left_cx + lobster->pupil_l.cur.x + lobster->look_x_cur,
                                                         pupil_left_cy + lobster->pupil_l.cur.y + lobster->look_y_cur,
                                                         pupil_scale_percent, false, pupil_segs),
                        TAG, "apply left pupil failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->pupil_r_obj, lobster->pupil_cur.pts,
                                                         pupil_right_cx + lobster->pupil_r.cur.x + lobster->look_x_cur,
                                                         pupil_right_cy + lobster->pupil_r.cur.y + lobster->look_y_cur,
                                                         pupil_scale_percent, true, pupil_segs),
                        TAG, "apply right pupil failed");
    ESP_RETURN_ON_ERROR(gfx_face_emote_apply_bezier_fill(lobster->mouth_obj, lobster->mouth_cur.pts,
                                                         mouth_cx + lobster->mouth.cur.x + lobster->look_x_cur / 2,
                                                         mouth_cy + lobster->mouth.cur.y + lobster->look_y_cur / 2,
                                                         eye_scale_percent, false, pupil_segs),
                        TAG, "apply mouth failed");
    return ESP_OK;
}
