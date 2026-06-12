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

#include "gfx/display.h"
#include "gfx/error.h"
#include "gfx/widgets/image.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_MOTION_SCENE_SCHEMA_VERSION 3U

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
    uint8_t          icon_enabled;   /**< 1 = render a step-local icon overlay       */
    uint16_t         icon_index;     /**< Index into gfx_motion_asset_t.icons[]       */
    int16_t          icon_x;         /**< Icon centre X in design-space coordinates   */
    int16_t          icon_y;         /**< Icon centre Y in design-space coordinates   */
    uint16_t         icon_scale_q8;  /**< Icon scale in Q8.8 (256 = 1.0x)             */
} gfx_motion_action_step_t;

/** Animation action: a sequence of steps with loop control. */
typedef struct {
    const gfx_motion_action_step_t *steps;
    uint8_t                  step_count;
    bool                     loop;
} gfx_motion_action_t;

/**
 * Static icon overlay geometry.
 *
 * Icon control points are local to the icon centre. At runtime, the active
 * action step supplies icon_x / icon_y / icon_scale_q8 and the renderer places
 * the icon directly on top of the base motion scene without consuming pose
 * joint budget.
 */
typedef struct {
    const char *name;
    const gfx_motion_segment_t *segments;
    uint8_t segment_count;
    const int16_t *coords;   /**< Flat [x0,y0, x1,y1, ...] local control-point array */
    uint16_t joint_count;
} gfx_motion_icon_t;

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

    /** Optional static icon overlays. */
    const gfx_motion_icon_t *icons;
    uint16_t icon_count;

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
/*  Layer 3 — RUNTIME (unified renderer)                              */
/* ------------------------------------------------------------------ */

/**
 * Opaque animation runtime.
 *
 * It owns the parser state, timer driver, scratch buffers, and one mesh object
 * per segment. The fields are intentionally private so applications cannot
 * depend on runtime layout.
 *
 * Usage:
 *   gfx_motion_player_t *player = gfx_motion_player_create(disp, &my_asset);
 *   gfx_motion_player_set_color(player, GFX_COLOR_HEX(0xFFFFFF));
 *   gfx_motion_player_set_action(player, action_index, false);
 *   gfx_motion_player_delete(player);
 */
typedef struct gfx_motion_player gfx_motion_player_t;

/**
 * @brief Motion action finished callback.
 *
 * Called once when a non-looping action reaches its final step. Looping actions
 * do not emit this callback.
 *
 * @param player Motion player that finished an action.
 * @param action_idx Finished action index.
 * @param user_data User data passed to gfx_motion_player_set_action_end_cb().
 */
typedef void (*gfx_motion_player_action_end_cb_t)(gfx_motion_player_t *player,
        uint16_t action_idx,
        void *user_data);

/**
 * @brief Create a motion player for a display.
 *
 * The player parses the asset, creates one mesh object per segment, and starts
 * the internal motion timer. The canvas defaults to the full display; use
 * gfx_motion_player_set_canvas() to override it.
 *
 * @param disp Display that owns the generated segment objects.
 * @param asset Motion scene asset descriptor.
 * @return Motion player handle on success, or NULL on failure.
 */
gfx_motion_player_t *gfx_motion_player_create(gfx_display_t *disp, const gfx_motion_asset_t *asset);

/**
 * @brief Delete a motion player.
 *
 * This stops the internal timer and destroys all mesh objects owned by the
 * player. Passing NULL is allowed.
 *
 * @param player Motion player handle returned from gfx_motion_player_create().
 */
void gfx_motion_player_delete(gfx_motion_player_t *player);

/**
 * @brief Set the runtime color used by solid-color segments.
 *
 * Segments bound to a texture resource or fixed asset palette color keep their
 * own source/color and are not changed by this call.
 *
 * @param player Motion player handle.
 * @param color Runtime segment color.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color);

/**
 * @brief Set the canvas region used to scale and place the scene.
 *
 * The asset viewbox is mapped into this rectangle before segment meshes are
 * updated.
 *
 * @param player Motion player handle.
 * @param x Canvas origin X in screen coordinates.
 * @param y Canvas origin Y in screen coordinates.
 * @param w Canvas width in pixels; must be greater than 0.
 * @param h Canvas height in pixels; must be greater than 0.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h);

/**
 * @brief Set the visible segment layer mask.
 *
 * Segment layer_bit == 0 is always visible. Segment layer_bit N (1..32)
 * is visible when BIT(N - 1) is set in layer_mask.
 *
 * @param player Motion player handle.
 * @param layer_mask Visibility mask for segment layers.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_layer_mask(gfx_motion_player_t *player, uint32_t layer_mask);

/**
 * @brief Set whole motion player visibility.
 *
 * This hides or shows all segment and icon objects owned by the player.
 * Segment layer visibility is still controlled separately by layer_mask.
 *
 * @param player Motion player handle.
 * @param visible true to show the player, false to hide it.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_visible(gfx_motion_player_t *player, bool visible);

/**
 * @brief Apply the current player state immediately without advancing time.
 *
 * @param player Motion player handle.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_sync(gfx_motion_player_t *player);

/**
 * @brief Reset the internal motion timer.
 *
 * @param player Motion player handle.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_reset_timer(gfx_motion_player_t *player);

/**
 * @brief Switch to an action by index.
 *
 * @param player Motion player handle.
 * @param action_idx Action index in gfx_motion_asset_t.actions.
 * @param snap Whether to snap directly to the first target pose.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap);

/**
 * @brief Set the callback invoked when a non-looping action finishes.
 *
 * Passing NULL disables the callback.
 *
 * @param player Motion player handle.
 * @param cb Callback to invoke from the motion timer context.
 * @param user_data User data passed to the callback.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_action_end_cb(gfx_motion_player_t *player,
        gfx_motion_player_action_end_cb_t cb,
        void *user_data);

/**
 * @brief Override the loop setting of the active action.
 *
 * The override remains active across action switches until cleared with
 * gfx_motion_player_clear_action_loop_override().
 *
 * @param player Motion player handle.
 * @param loop true to force looping, false to force one-shot playback.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_set_action_loop(gfx_motion_player_t *player, bool loop);

/**
 * @brief Clear the action loop override.
 *
 * After this call, each action uses its asset-defined loop flag again.
 *
 * @param player Motion player handle.
 * @return GFX_OK on success, or an GFX_ERR_* code on failure.
 */
gfx_err_t gfx_motion_player_clear_action_loop_override(gfx_motion_player_t *player);

#ifdef __cplusplus
}
#endif
