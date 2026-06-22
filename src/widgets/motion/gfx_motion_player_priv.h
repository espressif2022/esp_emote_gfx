/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common/gfx_config_internal.h"
#include "common/gfx_mesh_frac.h"
#include "core/gfx_obj.h"
#include "gfx/widgets/mesh_image.h"
#include "gfx/widgets/motion.h"
#include "widgets/motion/gfx_motion_priv.h"
#include "widgets/motion/gfx_motion_scene_priv.h"

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

/** Maximum mesh_img objects per runtime (one per segment). */
#define GFX_MOTION_PLAYER_MAX_SEGMENTS 64U
#define GFX_MOTION_PLAYER_MAX_ICON_SEGMENTS 32U

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

struct gfx_motion_player {
    gfx_motion_scene_t scene;
    gfx_motion_t motion;
    gfx_object_t *seg_objs[GFX_MOTION_PLAYER_MAX_SEGMENTS];
    uint8_t seg_grid_cols[GFX_MOTION_PLAYER_MAX_SEGMENTS];
    uint8_t seg_grid_rows[GFX_MOTION_PLAYER_MAX_SEGMENTS];
    uint8_t seg_obj_count;
    gfx_object_t *icon_objs[GFX_MOTION_PLAYER_MAX_ICON_SEGMENTS];
    uint8_t icon_grid_cols[GFX_MOTION_PLAYER_MAX_ICON_SEGMENTS];
    uint8_t icon_grid_rows[GFX_MOTION_PLAYER_MAX_ICON_SEGMENTS];
    uint8_t icon_obj_count;
    gfx_color_t stroke_color;
    uint32_t layer_mask;
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    uint16_t palette_pixels[GFX_MOTION_PALETTE_MAX];
    gfx_image_dsc_t palette_imgs[GFX_MOTION_PALETTE_MAX];
    gfx_coord_t canvas_x;
    gfx_coord_t canvas_y;
    uint16_t canvas_w;
    uint16_t canvas_h;
    gfx_motion_player_action_end_cb_t action_end_cb;
    void *action_end_user_data;
    bool mesh_dirty;
    bool visible;
    void *scratch;
};

uint8_t gfx_motion_player_ring_segs(float radius);
gfx_err_t gfx_motion_player_apply_capsule(gfx_object_t *obj,
        const gfx_motion_player_screen_point_t *a,
        const gfx_motion_player_screen_point_t *b,
        int32_t thick);
gfx_err_t gfx_motion_player_apply_ring(gfx_object_t *obj,
                                       gfx_motion_player_runtime_scratch_t *scratch,
                                       const gfx_motion_player_screen_point_t *c,
                                       int32_t radius, int32_t thick, uint8_t segs);
gfx_err_t gfx_motion_player_apply_bezier(gfx_object_t *obj,
        gfx_motion_player_runtime_scratch_t *scratch,
        const gfx_motion_player_screen_point_t *ctrl,
        uint8_t n, int32_t thick, bool loop);
gfx_err_t gfx_motion_player_apply_bezier_fill(gfx_object_t *obj,
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
gfx_err_t gfx_motion_player_apply_resource_uv(const gfx_motion_player_t *rt, uint8_t seg_idx,
        gfx_object_t *obj, uint8_t cols, uint8_t rows);
gfx_err_t gfx_motion_player_bind_segment_style(gfx_motion_player_t *player, uint8_t seg_idx,
        gfx_object_t *obj, const gfx_image_src_t *solid_src);
gfx_err_t gfx_motion_player_bind_style_common(gfx_motion_player_t *player,
        const gfx_motion_segment_t *seg, gfx_object_t *obj, const gfx_image_src_t *solid_src);

#ifdef __cplusplus
}
#endif
