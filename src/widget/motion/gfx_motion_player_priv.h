/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "common/gfx_config_internal.h"
#include "common/gfx_mesh_frac.h"
#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"
#include "widget/gfx_motion_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Max control points in a SINGLE segment (not total joints across all segments).
 * Cubic-bezier closed loop with N anchors has 3N+1 control points; N=20 -> 61.
 * Decoupled from GFX_MOTION_SCENE_MAX_POINTS to keep per-call scratch bounded.
 */
#define MOTION_BEZIER_MAX_PTS       GFX_MOTION_SCENE_MAX_SEG_CTRL_POINTS
/*
 * Control points use the cubic-Bezier polygon format: n = 3k+1
 * (k segments, adjacent segments share one endpoint).
 * Only pts[0], pts[3], pts[6], ... lie on the curve; interior pts are handles.
 */
#define MOTION_BEZIER_SEGS_PER_SEG  GFX_MOTION_BEZIER_STROKE_SEGS_PER_SEG
#define MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG GFX_MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG
#define MOTION_BEZIER_FILL_SEGS     GFX_MOTION_BEZIER_FILL_SEGS
#define MOTION_HUB_FILL_MAX_PTS     GFX_MOTION_HUB_FILL_MAX_POINTS
#define MOTION_BEZIER_FILL_USE_SCANLINE GFX_MOTION_BEZIER_FILL_USE_SCANLINE

#define MOTION_BEZIER_FILL_MAX_TESS   ((((MOTION_BEZIER_MAX_PTS - 1U) / 3U) * MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG) + 1U)
#define MOTION_BEZIER_STROKE_MAX_TESS ((((MOTION_BEZIER_MAX_PTS - 1U) / 3U) * MOTION_BEZIER_SEGS_PER_SEG) + 1U)
#define MOTION_BEZIER_MAX_TESS        ((MOTION_BEZIER_FILL_MAX_TESS > MOTION_BEZIER_STROKE_MAX_TESS) ? \
                                       MOTION_BEZIER_FILL_MAX_TESS : MOTION_BEZIER_STROKE_MAX_TESS)

#if (MOTION_BEZIER_FILL_MAX_TESS * 2U) > MOTION_HUB_FILL_MAX_PTS
#error "MOTION_HUB_FILL_MAX_PTS is too small for generic BEZIER_FILL tessellation"
#endif

typedef struct {
    int32_t x;
    int32_t y;
} gfx_motion_player_screen_point_t;

typedef struct {
    gfx_mesh_img_point_q8_t ring_pts[(GFX_MOTION_RING_SEGS_MAX + 1U) * 2U];
    gfx_mesh_img_point_q8_t bezier_pts[(MOTION_BEZIER_MAX_TESS + 1U) * 2U];
    int32_t bezier_ox[MOTION_BEZIER_MAX_TESS + 1U];
    int32_t bezier_oy[MOTION_BEZIER_MAX_TESS + 1U];
    int32_t bezier_ix[MOTION_BEZIER_MAX_TESS + 1U];
    int32_t bezier_iy[MOTION_BEZIER_MAX_TESS + 1U];
    float hub_ox[MOTION_BEZIER_MAX_TESS + 1U];
    float hub_oy[MOTION_BEZIER_MAX_TESS + 1U];
    gfx_mesh_img_point_q8_t hub_pts[MOTION_HUB_FILL_MAX_PTS];
    gfx_mesh_img_point_q8_t fill_pts[(MOTION_BEZIER_FILL_SEGS + 1U) * 2U];
    gfx_motion_player_screen_point_t fill_upper[MOTION_BEZIER_FILL_SEGS + 1U];
    gfx_motion_player_screen_point_t fill_lower[MOTION_BEZIER_FILL_SEGS + 1U];
    gfx_motion_player_screen_point_t ctrl_pts[MOTION_BEZIER_MAX_PTS];
} gfx_motion_player_runtime_scratch_t;

uint8_t gfx_motion_player_ring_segs(float radius);
esp_err_t gfx_motion_player_apply_capsule(gfx_obj_t *obj,
        const gfx_motion_player_screen_point_t *a,
        const gfx_motion_player_screen_point_t *b,
        int32_t thick);
esp_err_t gfx_motion_player_apply_ring(gfx_obj_t *obj,
                                       gfx_motion_player_runtime_scratch_t *scratch,
                                       const gfx_motion_player_screen_point_t *c,
                                       int32_t radius, int32_t thick, uint8_t segs);
esp_err_t gfx_motion_player_apply_bezier(gfx_obj_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n, int32_t thick, bool loop);
esp_err_t gfx_motion_player_apply_bezier_fill(gfx_obj_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n);

uint16_t gfx_motion_player_layout_timer_period_ms(const gfx_motion_layout_t *layout);
int16_t gfx_motion_player_layout_damping_div(const gfx_motion_layout_t *layout);
gfx_opa_t gfx_motion_player_segment_opacity(const gfx_motion_segment_t *seg);
gfx_color_t gfx_motion_player_resolve_fill_color(const gfx_motion_player_t *rt,
        const gfx_motion_segment_t *seg);
bool gfx_motion_player_segment_layer_visible(const gfx_motion_player_t *rt,
        const gfx_motion_segment_t *seg);
esp_err_t gfx_motion_player_apply_resource_uv(const gfx_motion_player_t *rt, uint8_t seg_idx,
        gfx_obj_t *obj, uint8_t cols, uint8_t rows);
esp_err_t gfx_motion_player_bind_segment_style(gfx_motion_player_t *player, uint8_t seg_idx,
        gfx_obj_t *obj, const gfx_img_src_t *solid_src);

#ifdef __cplusplus
}
#endif
