#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_err.h"

#include "common/gfx_mesh_frac.h"
#include "core/gfx_timer.h"
#include "core/gfx_obj.h"
#include "widget/gfx_face_emote.h"
#include "widget/gfx_img.h"
#include "widget/gfx_mesh_img.h"

#define GFX_FACE_EMOTE_REF_COUNT 6U

#define GFX_FACE_EMOTE_SHAPE14_NUM_PTS 14
#define GFX_FACE_EMOTE_SHAPE8_NUM_PTS  8

#define GFX_FACE_EMOTE_BLEND_WEIGHT_DIV 100
#define GFX_FACE_EMOTE_SCALE_PERCENT_DIV 100

#define GFX_FACE_EMOTE_Q8_ONE GFX_MESH_FRAC_ONE
#define GFX_FACE_EMOTE_BEZIER_T_DEN 1000
#define GFX_FACE_EMOTE_BEZIER_STRIDE 6

#define GFX_FACE_EMOTE_LAYOUT_MOUTH_Y_NUM        30
#define GFX_FACE_EMOTE_LAYOUT_EYE_HALF_GAP_NUM   40
#define GFX_FACE_EMOTE_LAYOUT_EYE_Y_NUM          70
#define GFX_FACE_EMOTE_LAYOUT_BROW_Y_EXTRA_NUM   16
#define GFX_FACE_EMOTE_LAYOUT_SCALE_NUM          160
#define GFX_FACE_EMOTE_LAYOUT_BROW_THICK_NUM     3
#define GFX_FACE_EMOTE_LAYOUT_MOUTH_THICK_NUM    4

#define GFX_FACE_EMOTE_DEFAULT_TIMER_PERIOD_MS ((uint16_t)CONFIG_GFX_FACE_EMOTE_DEFAULT_TIMER_PERIOD_MS)
#define GFX_FACE_EMOTE_DEFAULT_EYE_SEGS        ((uint8_t)CONFIG_GFX_FACE_EMOTE_DEFAULT_EYE_SEGS)
#define GFX_FACE_EMOTE_DEFAULT_BROW_SEGS       ((uint8_t)CONFIG_GFX_FACE_EMOTE_DEFAULT_BROW_SEGS)
#define GFX_FACE_EMOTE_DEFAULT_MOUTH_SEGS      ((uint8_t)CONFIG_GFX_FACE_EMOTE_DEFAULT_MOUTH_SEGS)

#define GFX_FACE_EMOTE_MOUTH_LOOK_X_NUM 6
#define GFX_FACE_EMOTE_MOUTH_LOOK_DEN   10
#define GFX_FACE_EMOTE_EASE_SPRING_DIV 4

#define GFX_FACE_EMOTE_CREATE_FALLBACK_RES ((uint32_t)CONFIG_GFX_FACE_EMOTE_CREATE_FALLBACK_RES)

#define GFX_FACE_EMOTE_MAX_STROKE_SEGS     CONFIG_GFX_FACE_EMOTE_MAX_STROKE_SEGS
#define GFX_FACE_EMOTE_MAX_STROKE_POINTS   (GFX_FACE_EMOTE_MAX_STROKE_SEGS + 1)
#define GFX_FACE_EMOTE_MAX_STROKE_MESH_Q8  (GFX_FACE_EMOTE_MAX_STROKE_POINTS * 2)

#define GFX_FACE_EMOTE_MAX_FILL_SEGS     CONFIG_GFX_FACE_EMOTE_MAX_FILL_SEGS
#define GFX_FACE_EMOTE_MAX_FILL_POINTS   (GFX_FACE_EMOTE_MAX_FILL_SEGS + 1)
#define GFX_FACE_EMOTE_MAX_FILL_MESH_Q8  (GFX_FACE_EMOTE_MAX_FILL_POINTS * 2)

typedef struct {
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    gfx_color_t color;
    gfx_obj_t *mouth_obj;
    gfx_obj_t *left_eye_obj;
    gfx_obj_t *right_eye_obj;
    gfx_obj_t *left_brow_obj;
    gfx_obj_t *right_brow_obj;
    gfx_timer_handle_t anim_timer;
    gfx_face_emote_cfg_t cfg;
    const gfx_face_emote_assets_t *assets;
    gfx_face_emote_eye_shape_t eye_current;
    gfx_face_emote_eye_shape_t eye_target;
    gfx_face_emote_brow_shape_t brow_current;
    gfx_face_emote_brow_shape_t brow_target;
    gfx_face_emote_mouth_shape_t mouth_current;
    gfx_face_emote_mouth_shape_t mouth_target;
    int16_t look_x_current;
    int16_t look_x_target;
    int16_t look_y_current;
    int16_t look_y_target;
    bool manual_look_enabled;
    uint32_t anim_tick;
    size_t expr_idx;
} gfx_face_emote_t;

bool gfx_face_emote_assets_valid(const gfx_face_emote_assets_t *assets);
void gfx_face_emote_expr_to_mix(const gfx_face_emote_expr_t *expr, gfx_face_emote_mix_t *mix);
esp_err_t gfx_face_emote_find_expr_index(const gfx_face_emote_assets_t *assets,
                                         const char *name,
                                         size_t *index_out);
esp_err_t gfx_face_emote_eval_mix(const gfx_face_emote_assets_t *assets,
                                  const gfx_face_emote_mix_t *mix,
                                  gfx_face_emote_eye_shape_t *eye_next,
                                  gfx_face_emote_brow_shape_t *brow_next,
                                  gfx_face_emote_mouth_shape_t *mouth_next,
                                  int16_t *look_x_next,
                                  int16_t *look_y_next);
esp_err_t gfx_face_emote_set_target_pose(gfx_obj_t *obj,
                                         gfx_face_emote_t *face,
                                         const gfx_face_emote_eye_shape_t *eye_next,
                                         const gfx_face_emote_brow_shape_t *brow_next,
                                         const gfx_face_emote_mouth_shape_t *mouth_next,
                                         int16_t look_x_next,
                                         int16_t look_y_next,
                                         bool snap_now);
esp_err_t gfx_face_emote_update_pose(gfx_obj_t *obj, gfx_face_emote_t *face);
esp_err_t gfx_face_emote_apply_bezier_stroke(gfx_obj_t *obj,
                                             const int16_t *pts,
                                             bool closed,
                                             int32_t cx,
                                             int32_t cy,
                                             int32_t scale_percent,
                                             int32_t thickness,
                                             bool flip_x,
                                             int32_t segs);
esp_err_t gfx_face_emote_apply_bezier_fill(gfx_obj_t *obj,
                                           const int16_t *pts,
                                           int32_t cx,
                                           int32_t cy,
                                           int32_t scale_percent,
                                           bool flip_x,
                                           int32_t segs);
