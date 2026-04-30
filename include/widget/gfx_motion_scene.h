/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file gfx_motion_scene.h
 *
 * Motion Scene Asset — three-layer architecture:
 *
 * Layer 1 — GENERATED ASSET (firmware-side):
 *   Named control points grouped into visual parts via segments.  The public
 *   field names still use joint_* for ABI compatibility, but semantically they
 *   are generic control points: skeleton endpoints, Bézier controls, or mesh
 *   anchors depending on the segment kind.
 *   All emote types (stickman, face, lobster-style textured) share the same
 *   Motion Scene Asset layout:
 *
 *     Segment kind   Control points   Rendered as
 *     ─────────────  ───────────────  ────────────────────────────────────
 *     CAPSULE        joint_a, joint_b Thick capsule (limb / body segment)
 *     RING           joint_a          Hollow ring (head)
 *     BEZIER_STRIP   joint_a .. +n-1  Open thick Bézier curve (brow)
 *     BEZIER_LOOP    joint_a .. +n-1  Closed thick Bézier loop (mouth outline)
 *     BEZIER_FILL    joint_a .. +n-1  Closed fill: n=7 eye, n=13 ellipse quad, else any n=3k+1 (hub mesh)
 *
 *   Stickman: each control point = one skeleton endpoint.
 *   Face:     each control point = one cubic Bézier point (n = 3k+1 format).
 *   Textured: any segment can reference a ROM image via segment.resource_idx.
 *   Poses store the *actual* target positions (pre-blended for face expressions).
 *
 * Layer 2 — PARSER (gfx_motion_scene.c):
 *   Validates asset, manages runtime pose_cur / pose_tgt interpolation, and
 *   advances action timelines. Zero display calls.
 *
 * Layer 3 — RUNTIME (gfx_motion_player.c):
 *   Creates one gfx_mesh_img per segment.  On every sync it maps design-space
 *   pose_cur[] to screen pixels and calls the appropriate primitive helper
 *   (capsule / ring / bezier) based on segment kind.
 *   No type flag required — segment kind encodes everything.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/gfx_obj.h"
#include "widget/gfx_img.h"
#include "widget/gfx_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_MOTION_SCENE_SCHEMA_VERSION 2U

/* ------------------------------------------------------------------ */
/*  0. Resource table (textures / image assets)                       */
/* ------------------------------------------------------------------ */

/**
 * One entry in the asset's resource table.
 *
 * A segment with resource_idx > 0 uses resources[resource_idx - 1]
 * as its mesh_img texture source instead of the runtime solid colour.
 * This allows texture-mapped segments (e.g. lobster body) to live in
 * the same unified asset format as solid-colour vector segments.
 *
 * resource_idx = 0  → solid colour (default, zero-init compatible)
 * resource_idx = N  → resources[N-1]
 */
typedef struct {
    const gfx_image_dsc_t *image;  /**< Pointer to the image descriptor (ROM .inc array)         */
    uint16_t uv_x;                 /**< Source crop origin X  (0 = full image)                   */
    uint16_t uv_y;                 /**< Source crop origin Y  (0 = full image)                   */
    uint16_t uv_w;                 /**< Source crop width     (0 = image width from uv_x)         */
    uint16_t uv_h;                 /**< Source crop height    (0 = image height from uv_y)        */
} gfx_motion_resource_t;

/* ------------------------------------------------------------------ */
/*  1. Segment primitives                                              */
/* ------------------------------------------------------------------ */

/**
 * Primitive kind — determines how the renderer draws a segment.
 *
 * CAPSULE / RING use control points as endpoint pair / center.
 * BEZIER_STRIP / BEZIER_LOOP / BEZIER_FILL use a contiguous range of
 * control points as cubic Bézier controls (n = 3k+1 polygon format).
 */
typedef enum {
    GFX_MOTION_SEG_CAPSULE      = 0, /**< Thick capsule between joint_a → joint_b        */
    GFX_MOTION_SEG_RING         = 1, /**< Hollow ring centred at joint_a                  */
    GFX_MOTION_SEG_BEZIER_STRIP = 2, /**< Open thick Bézier curve  (e.g. brow)            */
    GFX_MOTION_SEG_BEZIER_LOOP  = 3, /**< Closed thick Bézier loop (e.g. mouth outline)   */
    GFX_MOTION_SEG_BEZIER_FILL  = 4, /**< Closed filled Bézier shape (e.g. eye sclera)    */
} gfx_motion_segment_kind_t;

/** One visual part wiring control points to a rendering primitive. */
typedef struct {
    gfx_motion_segment_kind_t kind;
    uint16_t          joint_a;       /**< CAPSULE: start; RING: centre; BEZIER: first ctrl pt     */
    uint16_t          joint_b;       /**< CAPSULE: end  ; unused for RING/BEZIER                  */
    uint16_t          joint_count;   /**< BEZIER_*: number of consecutive control points (n=3k+1) */
    uint8_t           stroke_width;  /**< Design-space override; 0 = use layout->stroke_width     */
    uint8_t           layer_bit;     /**< Visibility layer mask bit (0 = always shown)            */
    int16_t           radius_hint;   /**< RING: design-space radius                               */
    /**
     * Texture / resource binding.
     * 0   = solid colour (driven by gfx_motion_player_set_color).
     * N>0 = use asset->resources[N-1] as the mesh_img image source.
     */
    uint8_t           resource_idx;
    /**
     * Palette colour index.
     * 0   = use runtime colour (gfx_motion_player_set_color), not affected by set_color.
     * N>0 = use asset->color_palette[N-1] (0xRRGGBB) as the fixed segment colour.
     *        set_color() skips palette-coloured segments.
     */
    uint8_t           color_idx;
    /**
     * Segment opacity 0-255.
     * 0 is treated as 255 (fully opaque) for zero-init compatibility.
     */
    uint8_t           opacity;
} gfx_motion_segment_t;

/* ------------------------------------------------------------------ */
/*  2. Poses — flat arrays of control point coordinates               */
/* ------------------------------------------------------------------ */

/**
 * One pose: flat [x0,y0, x1,y1, …] array, length = joint_count × 2.
 * joint_count is the ABI field name; conceptually it is the number of
 * generated control points.
 * For stickman: x,y = skeleton endpoint position in design space.
 * For face: x,y = Bézier control point position in design space
 *           (pre-blended from reference shapes + expression weights).
 */
typedef struct {
    const int16_t *coords;
} gfx_motion_pose_t;

/* ------------------------------------------------------------------ */
/*  3. Actions (animation sequences)                                  */
/* ------------------------------------------------------------------ */

/** Interpolation style when transitioning into an action step. */
typedef enum {
    GFX_MOTION_INTERP_HOLD   = 0, /**< Snap immediately to target pose */
    GFX_MOTION_INTERP_DAMPED = 1, /**< Exponential ease (damping_div)  */
} gfx_motion_interp_t;

/** One step in an action: selects a target pose and how long to hold it. */
typedef struct {
    uint16_t         pose_index;  /**< Index into gfx_motion_asset_t.poses[]     */
    uint16_t         hold_ticks;  /**< Timer ticks to hold before advancing      */
    gfx_motion_interp_t  interp;  /**< Transition style into this step           */
    int8_t           facing;      /**< 1=right  -1=left (mirrors X)              */
} gfx_motion_action_step_t;

/** Animation action: a sequence of steps with loop control. */
typedef struct {
    const gfx_motion_action_step_t *steps;
    uint8_t                  step_count;
    bool                     loop;
} gfx_motion_action_t;

/* ------------------------------------------------------------------ */
/*  4. Metadata and layout hints                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t version;    /**< Must equal GFX_MOTION_SCENE_SCHEMA_VERSION */
    int32_t  viewbox_x;
    int32_t  viewbox_y;
    int32_t  viewbox_w;
    int32_t  viewbox_h;
} gfx_motion_meta_t;

/**
 * Rendering parameters.  Separated from geometry so they can be
 * overridden without touching the ROM asset.
 */
typedef struct {
    int16_t  stroke_width;    /**< Default capsule / Bézier stroke thickness (design units) */
    int16_t  mirror_x;        /**< X axis for facing=-1 horizontal mirroring               */
    int16_t  ground_y;        /**< Informational floor position                             */
    uint16_t timer_period_ms; /**< Action-advance timer period                              */
    int16_t  damping_div;     /**< Divisor for INTERP_DAMPED easing (1 = snap)             */
} gfx_motion_layout_t;

/* ------------------------------------------------------------------ */
/*  5. Top-level asset bundle                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const gfx_motion_meta_t    *meta;

    /** Control point name table (joint_count entries; field name kept for ABI). */
    const char *const       *joint_names;
    uint16_t                 joint_count;

    /** Segment wiring (segment_count entries; 0 is valid). */
    const gfx_motion_segment_t *segments;
    uint8_t                  segment_count;

    /** Pose library. */
    const gfx_motion_pose_t    *poses;
    uint16_t                 pose_count;

    /** Action library. */
    const gfx_motion_action_t    *actions;
    uint16_t                 action_count;

    /** Default playback sequence (action indices). */
    const uint16_t          *sequence;
    uint16_t                 sequence_count;

    /** Rendering hints. */
    const gfx_motion_layout_t  *layout;

    /**
     * Optional texture/image resource table.
     * Segments reference entries here via segment.resource_idx (1-based).
     * NULL and resource_count=0 are valid (all segments use solid colour).
     */
    const gfx_motion_resource_t *resources;
    uint8_t                   resource_count;

    /**
     * Optional per-segment colour palette.
     * Stored as 0xRRGGBB 24-bit values; converted to native pixel at runtime init.
     * Segments reference entries via segment.color_idx (1-based).
     * NULL and color_palette_count=0 are valid (all non-resource segments use
     * the runtime colour set by gfx_motion_player_set_color).
     */
    const uint32_t *color_palette;
    uint8_t         color_palette_count;
} gfx_motion_asset_t;

/* ------------------------------------------------------------------ */
/*  Layer 2 — PARSER runtime state                                    */
/* ------------------------------------------------------------------ */

/**
 * Maximum total control points per asset.
 * Raised beyond 512 so closed-loop rigs can duplicate outline control points
 * for BEZIER_FILL companions without immediately exhausting the budget.
 */
#define GFX_MOTION_SCENE_MAX_POINTS 640U

/**
 * Maximum control points in a single BEZIER_* segment.
 * Shared by scene/player code so invalid assets fail early at compile/import time.
 */
#define GFX_MOTION_SCENE_MAX_SEG_CTRL_POINTS 64U

typedef struct {
    int16_t x;
    int16_t y;
} gfx_motion_point_t;

typedef struct {
    const gfx_motion_asset_t *asset;

    gfx_motion_point_t  pose_cur[GFX_MOTION_SCENE_MAX_POINTS]; /**< Current (animated) positions */
    gfx_motion_point_t  pose_tgt[GFX_MOTION_SCENE_MAX_POINTS]; /**< Target positions              */

    uint16_t     active_action;
    uint8_t      active_step;
    uint16_t     step_ticks;
    bool         action_loop_override_en;
    bool         action_loop_override;
    bool         dirty;
} gfx_motion_scene_t;

esp_err_t gfx_motion_scene_init(gfx_motion_scene_t *scene, const gfx_motion_asset_t *asset);
esp_err_t gfx_motion_scene_set_action(gfx_motion_scene_t *scene, uint16_t action_index, bool snap_now);
esp_err_t gfx_motion_scene_set_action_loop(gfx_motion_scene_t *scene, bool loop);
esp_err_t gfx_motion_scene_clear_action_loop_override(gfx_motion_scene_t *scene);

/** Ease pose_cur toward pose_tgt one tick.  Returns true if any coord changed. */
bool gfx_motion_scene_tick(gfx_motion_scene_t *scene);

/** Advance the action timeline (hold_ticks countdown and step transitions). */
void gfx_motion_scene_advance(gfx_motion_scene_t *scene);

/**
 * Debug: print active action index, step index, pose index, hold ticks, facing, and interp.
 * Generated Motion Scene Assets expose action enums in their .inc files; the
 * parser only sees numeric action/pose indices at runtime.
 */
void gfx_motion_scene_log_active_step(const gfx_motion_scene_t *scene, const char *reason);

/* ------------------------------------------------------------------ */
/*  Layer 3 — RUNTIME (unified renderer)                              */
/* ------------------------------------------------------------------ */

/** Maximum mesh_img objects per runtime (one per segment). */
#define GFX_MOTION_PLAYER_MAX_SEGMENTS 64U

/** Maximum colour palette entries (colour_idx 1..GFX_MOTION_PALETTE_MAX). */
#define GFX_MOTION_PALETTE_MAX 16U

/**
 * Unified animation runtime.
 *
 * Owns a gfx_motion_scene_t (scene state) + gfx_motion_t (timer driver) + one gfx_mesh_img
 * per segment.  Dispatches rendering based on segment kind — no separate
 * "stickman renderer" vs "face renderer".
 *
 * Usage:
 *   gfx_motion_player_t player = {0};
 *   gfx_motion_player_init(&player, disp, &my_asset);
 *   gfx_motion_player_set_color(&player, GFX_COLOR_HEX(0xFFFFFF));
 *   gfx_motion_player_set_action(&player, action_index, false);
 */
typedef struct {
    gfx_motion_scene_t  scene;
    gfx_motion_t        motion;
    /* ── private ── */
    gfx_obj_t      *seg_objs[GFX_MOTION_PLAYER_MAX_SEGMENTS]; /**< One mesh_img per segment */
    uint8_t         seg_grid_cols[GFX_MOTION_PLAYER_MAX_SEGMENTS];
    uint8_t         seg_grid_rows[GFX_MOTION_PLAYER_MAX_SEGMENTS];
    uint8_t         seg_obj_count;
    gfx_color_t     stroke_color;
    uint32_t        layer_mask;
    uint16_t        solid_pixel;
    gfx_image_dsc_t solid_img;
    /** Per-palette-entry native pixels and their 1×1 image descriptors. */
    uint16_t        palette_pixels[GFX_MOTION_PALETTE_MAX];
    gfx_image_dsc_t palette_imgs[GFX_MOTION_PALETTE_MAX];
    gfx_coord_t     canvas_x;
    gfx_coord_t     canvas_y;
    uint16_t        canvas_w;
    uint16_t        canvas_h;
    bool            mesh_dirty;
    void           *scratch;
} gfx_motion_player_t;

/**
 * Initialise the player: parse the asset, create mesh objects, and start the motion timer.
 * Canvas defaults to full display; override with gfx_motion_player_set_canvas().
 */
esp_err_t gfx_motion_player_init(gfx_motion_player_t *player,
                                 gfx_disp_t *disp,
                                 const gfx_motion_asset_t *asset);

/** Destroy all mesh_img objects and stop the motion timer. */
void      gfx_motion_player_deinit(gfx_motion_player_t *player);

/** Change the stroke colour for all segments. */
esp_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color);

/** Override the canvas region the scene is scaled into. */
esp_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h);

/**
 * Set the visible segment layer mask.
 *
 * Segment layer_bit == 0 is always visible. Segment layer_bit N (1..32)
 * is visible when BIT(N - 1) is set in layer_mask.
 */
esp_err_t gfx_motion_player_set_layer_mask(gfx_motion_player_t *player, uint32_t layer_mask);

/** Force the current player state to be applied immediately without advancing time. */
esp_err_t gfx_motion_player_sync(gfx_motion_player_t *player);

/** Switch to an action by index. */
esp_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap);
esp_err_t gfx_motion_player_set_action_loop(gfx_motion_player_t *player, bool loop);
esp_err_t gfx_motion_player_clear_action_loop_override(gfx_motion_player_t *player);

#ifdef __cplusplus
}
#endif
