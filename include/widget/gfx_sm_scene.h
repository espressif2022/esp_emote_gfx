/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file gfx_sm_scene.h
 *
 * Universal animation scene format — three-layer architecture:
 *
 * Layer 1 — SOURCE FORMAT (this file, ROM-side):
 *   Named joints (2-D control points) grouped into primitives via segments.
 *   All emote types (stickman, face, lobster-style textured) share the same struct:
 *
 *     Segment kind   Joints used      Rendered as
 *     ─────────────  ───────────────  ────────────────────────────────────
 *     CAPSULE        joint_a, joint_b Thick capsule (limb / body segment)
 *     RING           joint_a          Hollow ring (head)
 *     BEZIER_STRIP   joint_a .. +n-1  Open thick Bézier curve (brow)
 *     BEZIER_LOOP    joint_a .. +n-1  Closed thick Bézier loop (mouth outline)
 *     BEZIER_FILL    joint_a .. +n-1  Closed filled Bézier shape (eye sclera)
 *
 *   Stickman: each joint = one skeleton endpoint.
 *   Face:     each joint = one cubic Bézier control point (n = 3k+1 format).
 *   Textured: any segment can reference a ROM image via segment.resource_idx.
 *   Poses store the *actual* target positions (pre-blended for face expressions).
 *
 * Layer 2 — PARSER (gfx_sm_scene.c):
 *   Validates asset, manages runtime pose_cur / pose_tgt interpolation, and
 *   advances clip timelines.  Zero display calls.
 *
 * Layer 3 — RUNTIME (gfx_sm_runtime.c):
 *   Creates one gfx_mesh_img per segment.  On every sync it maps design-space
 *   pose_cur[] to screen pixels and calls the appropriate apply helper
 *   (s_apply_capsule / s_apply_ring / s_apply_bezier) based on segment kind.
 *   No type flag required — segment kind encodes everything.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/gfx_obj.h"
#include "widget/gfx_img.h"
#include "widget/gfx_rig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_SM_SCENE_SCHEMA_VERSION 2U

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
    uint16_t uv_w;                 /**< Source crop width     (0 = full image width)              */
    uint16_t uv_h;                 /**< Source crop height    (0 = full image height)             */
} gfx_sm_resource_t;

/* ------------------------------------------------------------------ */
/*  1. Segment primitives                                              */
/* ------------------------------------------------------------------ */

/**
 * Primitive kind — determines how the renderer draws a segment.
 *
 * CAPSULE / RING use two joints (endpoint pair / center).
 * BEZIER_STRIP / BEZIER_LOOP / BEZIER_FILL use a contiguous range of
 * joints as cubic Bézier control points (n = 3k+1 polygon format).
 */
typedef enum {
    GFX_SM_SEG_CAPSULE      = 0, /**< Thick capsule between joint_a → joint_b        */
    GFX_SM_SEG_RING         = 1, /**< Hollow ring centred at joint_a                  */
    GFX_SM_SEG_BEZIER_STRIP = 2, /**< Open thick Bézier curve  (e.g. brow)            */
    GFX_SM_SEG_BEZIER_LOOP  = 3, /**< Closed thick Bézier loop (e.g. mouth outline)   */
    GFX_SM_SEG_BEZIER_FILL  = 4, /**< Closed filled Bézier shape (e.g. eye sclera)    */
} gfx_sm_seg_kind_t;

/** One rendering primitive wiring joints to a visual element. */
typedef struct {
    gfx_sm_seg_kind_t kind;
    uint16_t          joint_a;       /**< CAPSULE: start; RING: centre; BEZIER: first ctrl pt     */
    uint16_t          joint_b;       /**< CAPSULE: end  ; unused for RING/BEZIER                  */
    uint16_t          joint_count;   /**< BEZIER_*: number of consecutive control points (n=3k+1) */
    uint8_t           stroke_width;  /**< Design-space override; 0 = use layout->stroke_width     */
    uint8_t           layer_bit;     /**< Visibility layer mask bit (0 = always shown)            */
    int16_t           radius_hint;   /**< RING: design-space radius                               */
    /**
     * Texture / resource binding.
     * 0   = solid colour (driven by gfx_sm_runtime_set_color).
     * N>0 = use asset->resources[N-1] as the mesh_img image source.
     */
    uint8_t           resource_idx;
    /**
     * Palette colour index.
     * 0   = use runtime colour (gfx_sm_runtime_set_color), not affected by set_color.
     * N>0 = use asset->color_palette[N-1] (0xRRGGBB) as the fixed segment colour.
     *        set_color() skips palette-coloured segments.
     */
    uint8_t           color_idx;
    /**
     * Segment opacity 0-255.
     * 0 is treated as 255 (fully opaque) for zero-init compatibility.
     */
    uint8_t           opacity;
} gfx_sm_segment_t;

/* ------------------------------------------------------------------ */
/*  2. Poses — flat arrays of joint coordinates                       */
/* ------------------------------------------------------------------ */

/**
 * One pose: flat [x0,y0, x1,y1, …] array, length = joint_count × 2.
 * For stickman: x,y = skeleton joint position in design space.
 * For face: x,y = Bézier control point position in design space
 *           (pre-blended from reference shapes + expression weights).
 */
typedef struct {
    const int16_t *coords;
} gfx_sm_pose_t;

/* ------------------------------------------------------------------ */
/*  3. Clips (animation sequences)                                    */
/* ------------------------------------------------------------------ */

/** Interpolation style when transitioning into a clip step. */
typedef enum {
    GFX_SM_INTERP_HOLD   = 0, /**< Snap immediately to target pose */
    GFX_SM_INTERP_DAMPED = 1, /**< Exponential ease (damping_div)  */
} gfx_sm_interp_t;

/** One step in a clip: selects a target pose and how long to hold it. */
typedef struct {
    uint16_t         pose_index;  /**< Index into gfx_sm_asset_t.poses[]     */
    uint16_t         hold_ticks;  /**< Timer ticks to hold before advancing   */
    gfx_sm_interp_t  interp;     /**< Transition style into this step        */
    int8_t           facing;      /**< 1=right  -1=left (mirrors X)           */
} gfx_sm_clip_step_t;

/** Animation clip: a sequence of steps with loop control. */
typedef struct {
    const gfx_sm_clip_step_t *steps;
    uint8_t                  step_count;
    bool                     loop;
} gfx_sm_clip_t;

/* ------------------------------------------------------------------ */
/*  4. Metadata and layout hints                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t version;    /**< Must equal GFX_SM_SCENE_SCHEMA_VERSION */
    int32_t  viewbox_x;
    int32_t  viewbox_y;
    int32_t  viewbox_w;
    int32_t  viewbox_h;
} gfx_sm_meta_t;

/**
 * Rendering parameters.  Separated from geometry so they can be
 * overridden without touching the ROM asset.
 */
typedef struct {
    int16_t  stroke_width;    /**< Default capsule / Bézier stroke thickness (design units) */
    int16_t  mirror_x;        /**< X axis for facing=-1 horizontal mirroring               */
    int16_t  ground_y;        /**< Informational floor position                             */
    uint16_t timer_period_ms; /**< Clip-advance timer period                                */
    int16_t  damping_div;     /**< Divisor for INTERP_DAMPED easing (1 = snap)             */
} gfx_sm_layout_t;

/* ------------------------------------------------------------------ */
/*  5. Top-level asset bundle                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const gfx_sm_meta_t    *meta;

    /** Joint name table (joint_count entries). */
    const char *const       *joint_names;
    uint16_t                 joint_count;

    /** Segment wiring (segment_count entries; 0 is valid). */
    const gfx_sm_segment_t *segments;
    uint8_t                  segment_count;

    /** Pose library. */
    const gfx_sm_pose_t    *poses;
    uint16_t                 pose_count;

    /** Clip library. */
    const gfx_sm_clip_t    *clips;
    uint16_t                 clip_count;

    /** Default playback sequence (clip indices). */
    const uint16_t          *sequence;
    uint16_t                 sequence_count;

    /** Rendering hints. */
    const gfx_sm_layout_t  *layout;

    /**
     * Optional texture/image resource table.
     * Segments reference entries here via segment.resource_idx (1-based).
     * NULL and resource_count=0 are valid (all segments use solid colour).
     */
    const gfx_sm_resource_t *resources;
    uint8_t                   resource_count;

    /**
     * Optional per-segment colour palette.
     * Stored as 0xRRGGBB 24-bit values; converted to native pixel at runtime init.
     * Segments reference entries via segment.color_idx (1-based).
     * NULL and color_palette_count=0 are valid (all non-resource segments use
     * the runtime colour set by gfx_sm_runtime_set_color).
     */
    const uint32_t *color_palette;
    uint8_t         color_palette_count;
} gfx_sm_asset_t;

/* ------------------------------------------------------------------ */
/*  Layer 2 — PARSER runtime state                                    */
/* ------------------------------------------------------------------ */

/**
 * Maximum total joints per asset (sum of all segment control points).
 * Stickman ≤ 12, face ≤ 64, rig ≤ 311 — keep at 512 to accommodate rigs.
 * NOTE: per-segment buffer (SM_BEZIER_MAX_PTS in gfx_sm_runtime.c) is
 * independent and capped separately to limit stack usage.
 */
#define GFX_SM_SCENE_MAX_JOINTS 512U

typedef struct {
    int16_t x;
    int16_t y;
} gfx_sm_pt_t;

typedef struct {
    const gfx_sm_asset_t *asset;

    gfx_sm_pt_t  pose_cur[GFX_SM_SCENE_MAX_JOINTS]; /**< Current (animated) positions */
    gfx_sm_pt_t  pose_tgt[GFX_SM_SCENE_MAX_JOINTS]; /**< Target positions              */

    uint16_t     active_clip;
    uint8_t      active_step;
    uint16_t     step_ticks;
    bool         dirty;
} gfx_sm_scene_t;

esp_err_t gfx_sm_scene_init(gfx_sm_scene_t *scene, const gfx_sm_asset_t *asset);
esp_err_t gfx_sm_scene_set_clip(gfx_sm_scene_t *scene, uint16_t clip_index, bool snap_now);

/** Ease pose_cur toward pose_tgt one tick.  Returns true if any coord changed. */
bool gfx_sm_scene_tick(gfx_sm_scene_t *scene);

/** Advance the clip timeline (hold_ticks countdown and step transitions). */
void gfx_sm_scene_advance(gfx_sm_scene_t *scene);

/* ------------------------------------------------------------------ */
/*  Layer 3 — RUNTIME (unified renderer)                              */
/* ------------------------------------------------------------------ */

/** Maximum mesh_img objects per runtime (one per segment). */
#define GFX_SM_RUNTIME_MAX_SEGS 64U

/** Maximum colour palette entries (colour_idx 1..GFX_SM_PALETTE_MAX). */
#define GFX_SM_PALETTE_MAX 16U

/**
 * Unified animation runtime.
 *
 * Owns a gfx_sm_scene_t (parser) + gfx_rig_t (timer) + one gfx_mesh_img
 * per segment.  Dispatches rendering based on segment kind — no separate
 * "stickman renderer" vs "face renderer".
 *
 * Usage:
 *   gfx_sm_runtime_t rt = {0};
 *   gfx_sm_runtime_init(&rt, disp, &my_asset);
 *   gfx_sm_runtime_set_color(&rt, GFX_COLOR_HEX(0xFFFFFF));
 *   gfx_sm_runtime_set_clip(&rt, clip_index, false);
 */
typedef struct {
    gfx_sm_scene_t  scene;
    gfx_rig_t       rig;
    /* ── private ── */
    gfx_obj_t      *seg_objs[GFX_SM_RUNTIME_MAX_SEGS]; /**< One mesh_img per segment */
    uint8_t         seg_obj_count;
    gfx_color_t     stroke_color;
    uint16_t        solid_pixel;
    gfx_image_dsc_t solid_img;
    /** Per-palette-entry native pixels and their 1×1 image descriptors. */
    uint16_t        palette_pixels[GFX_SM_PALETTE_MAX];
    gfx_image_dsc_t palette_imgs[GFX_SM_PALETTE_MAX];
    gfx_coord_t     canvas_x;
    gfx_coord_t     canvas_y;
    uint16_t        canvas_w;
    uint16_t        canvas_h;
    bool            mesh_dirty;
} gfx_sm_runtime_t;

/**
 * Initialise the runtime: parse asset, create mesh_img objects, start rig timer.
 * Canvas defaults to full display; override with gfx_sm_runtime_set_canvas().
 */
esp_err_t gfx_sm_runtime_init(gfx_sm_runtime_t *rt,
                               gfx_disp_t *disp,
                               const gfx_sm_asset_t *asset);

/** Destroy all mesh_img objects and stop the rig timer. */
void      gfx_sm_runtime_deinit(gfx_sm_runtime_t *rt);

/** Change the stroke colour for all segments. */
esp_err_t gfx_sm_runtime_set_color(gfx_sm_runtime_t *rt, gfx_color_t color);

/** Override the canvas region the scene is scaled into. */
esp_err_t gfx_sm_runtime_set_canvas(gfx_sm_runtime_t *rt,
                                     gfx_coord_t x, gfx_coord_t y,
                                     uint16_t w, uint16_t h);

/** Jump to a clip by index. */
esp_err_t gfx_sm_runtime_set_clip(gfx_sm_runtime_t *rt, uint16_t clip_idx, bool snap);

#ifdef __cplusplus
}
#endif
