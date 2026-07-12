/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Arena scene backend attached to a gfx_display.
 * Package path: mutate arena -> mark_dirty -> existing dirty[]/merge/refresh.
 *
 * Dynamics use runtime side tables (list/wheel scroll, coverflow/pageflow drag) —
 * package ABI unchanged.
 */

#include <stddef.h>
#include <stdint.h>

#include "gfx/input.h"
#include "gfx/scene/arena.h"
#include "gfx/types.h"
#include "gfx/widgets/anim.h"   /* gfx_anim_src_t */
#include "gfx/widgets/image.h"  /* gfx_image_src_t */
#include "gfx/widgets/label.h"  /* gfx_font_t */
#include "gfx/widgets/motion.h" /* gfx_motion_asset_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_display gfx_display_t;

typedef void (*gfx_arena_action_cb_t)(gfx_arena_t *arena, gfx_arena_node_t *node,
                                  const gfx_touch_event_t *event, void *user_data);

typedef struct {
    const char       *name;
    gfx_arena_action_cb_t cb;
    void             *user_data;
} gfx_arena_action_entry_t;

/** Runtime scroll for LIST / WHEEL (not in package). */
typedef struct {
    uint32_t node_off;
    int32_t  scroll_y;
    int32_t  velocity_y;
    int32_t  start_scroll_y;
    int32_t  tween_from;     /* WHEEL snap tween */
    int32_t  tween_to;
    uint32_t tween_start_ms;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t last_y;
    uint32_t last_ms;
    uint8_t  pressed;
    uint8_t  dragging;
    uint8_t  inertia;
    uint8_t  snap_tweening;  /* WHEEL: ease-out scroll snap (gfx_wheel) */
    uint8_t  used;
} gfx_arena_list_rt_t;

/**
 * Runtime drag/tween for COVERFLOW (not in package).
 * Mirrors gfx_coverflow: selection commits only after tween completes.
 */
typedef struct {
    uint32_t node_off;
    int32_t  drag_offset;
    int32_t  tween_from;
    int32_t  tween_to;
    int32_t  tween_target_index;
    uint32_t tween_start_ms;
    uint16_t start_x;
    uint16_t start_y;
    uint8_t  pressed;
    uint8_t  dragging;
    uint8_t  tweening;
    uint8_t  tween_commit_pending;
    uint8_t  used;
} gfx_arena_coverflow_rt_t;

/**
 * Runtime drag/tween for PAGEFLOW (not in package).
 * Page selection commits when tween starts, matching gfx_pageflow.
 */
typedef struct {
    uint32_t node_off;
    int32_t  drag_offset;
    int32_t  tween_from;
    int32_t  tween_to;
    uint32_t tween_start_ms;
    uint16_t start_x;
    uint16_t start_y;
    uint8_t  pressed;
    uint8_t  dragging;
    uint8_t  tweening;
    uint8_t  used;
} gfx_arena_pageflow_rt_t;

#define GFX_ARENA_LIST_RT_MAX      8
#define GFX_ARENA_COVERFLOW_RT_MAX 4
#define GFX_ARENA_PAGEFLOW_RT_MAX  4
#define GFX_ARENA_IMG_RT_MAX       24
#define GFX_ARENA_ANIM_RT_MAX      4
#define GFX_ARENA_MOTION_RT_MAX    4

/**
 * Runtime decoded image (not in package).
 * Shares gfx_image_resource with the object path (file / JPEG / IMAGE_DSC).
 * Opaque storage lives in gfx_arena_scene.c; draw uses gfx_arena_scene_img_rt_*.
 */
typedef struct gfx_arena_img_rt gfx_arena_img_rt_t;

/**
 * Runtime ANIM player (not in package).
 * Hosts a gfx_anim object sized/positioned from the ARN node each sync.
 */
typedef struct {
    uint32_t      node_off;
    gfx_object_t *obj;
    uint8_t       playing;
    uint8_t       used;
} gfx_arena_anim_rt_t;

/**
 * Runtime MOTION player (not in package).
 * Hosts gfx_motion_player; canvas synced from ARN node abs area.
 */
typedef struct {
    uint32_t              node_off;
    gfx_motion_player_t  *player;
    const gfx_motion_asset_t *asset;
    uint8_t               used;
} gfx_arena_motion_rt_t;

typedef struct gfx_arena_scene {
    gfx_arena_t                 arena;
    gfx_display_t          *disp;
    const gfx_arena_action_entry_t *actions;
    size_t                  action_count;
    uint32_t                pressed_off; /* GFX_ARENA_NO_NODE if none */
    uint8_t                 pressed_track;
    gfx_font_t              font;
    void                   *font_adapter;
    gfx_arena_list_rt_t         list_rt[GFX_ARENA_LIST_RT_MAX];
    gfx_arena_coverflow_rt_t    cover_rt[GFX_ARENA_COVERFLOW_RT_MAX];
    gfx_arena_pageflow_rt_t     page_rt[GFX_ARENA_PAGEFLOW_RT_MAX];
    gfx_arena_anim_rt_t         anim_rt[GFX_ARENA_ANIM_RT_MAX];
    gfx_arena_motion_rt_t       motion_rt[GFX_ARENA_MOTION_RT_MAX];
    gfx_arena_img_rt_t         *img_rt;      /* GFX_ARENA_IMG_RT_MAX slots */
    uint16_t                img_rt_count;
} gfx_arena_scene_t;

/**
 * @brief Attach a loaded arena to a display as the scene backend
 * @param disp Target display
 * @param arena Loaded arena; ownership of RAM copy moves into @p out on success
 * @param out Scene handle to initialize
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_attach(gfx_display_t *disp, gfx_arena_t *arena, gfx_arena_scene_t *out);

/**
 * @brief Detach a scene, free side tables and the arena RAM copy
 * @param scene Scene handle; NULL-safe
 */
void gfx_arena_scene_detach(gfx_arena_scene_t *scene);

/**
 * @brief Look up the arena scene attached to a display
 * @param disp Display handle
 * @return Scene pointer, or NULL if none
 */
gfx_arena_scene_t *gfx_arena_scene_from_disp(gfx_display_t *disp);

/**
 * @brief Install action callbacks matched by name
 * @param scene Scene handle
 * @param actions Action table (not copied; must outlive scene)
 * @param count Number of entries
 */
void gfx_arena_scene_set_actions(gfx_arena_scene_t *scene,
                             const gfx_arena_action_entry_t *actions, size_t count);

/**
 * @brief Set the default font used by arena label/list text draw
 * @param scene Scene handle
 * @param font Font handle; NULL clears
 */
void gfx_arena_scene_set_font(gfx_arena_scene_t *scene, gfx_font_t font);

/**
 * @brief Mark one node dirty by absolute area
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_mark_dirty(gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Mark the full screen dirty
 * @param scene Scene handle
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_mark_dirty_all(gfx_arena_scene_t *scene);

/**
 * @brief Advance list/wheel/cover/page tweens and sync ANIM/MOTION hosts
 * @param scene Scene handle
 * @return 1 if any animation is still active, 0 otherwise
 */
int gfx_arena_scene_tick(gfx_arena_scene_t *scene);

/**
 * @brief Get runtime scroll_y for a LIST/WHEEL node
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @return scroll_y in pixels, or 0 if none
 */
int32_t gfx_arena_scene_list_scroll_y(const gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Get runtime drag_offset for a COVERFLOW node
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @return drag_offset in pixels, or 0 if none
 */
int32_t gfx_arena_scene_coverflow_drag(const gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Get runtime drag_offset for a PAGEFLOW node
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @return drag_offset in pixels, or 0 if none
 */
int32_t gfx_arena_scene_pageflow_drag(const gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Bind a decoded image source to a node or coverflow/pageflow item
 *
 * Uses the same gfx_image_resource / gfx_fs / JPEG stack as gfx_image.
 *
 * @param scene Scene handle
 * @param node_off IMAGE / IMAGE_BUTTON / COVERFLOW / PAGEFLOW node offset
 * @param item_index GFX_ARENA_IMG_ITEM_NONE for IMAGE / IMAGE_BUTTON;
 *                   item index for COVERFLOW / PAGEFLOW cards
 * @param face 0 = normal / card image; 1 = IMAGE_BUTTON pressed face
 * @param src Image source descriptor
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_bind_image_src(gfx_arena_scene_t *scene, uint32_t node_off,
                               uint16_t item_index, uint8_t face,
                               const gfx_image_src_t *src);

/**
 * @brief Unbind one image runtime slot
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @param item_index Item index or GFX_ARENA_IMG_ITEM_NONE
 * @param face Face index, or 0xFF to clear all faces for the node/item
 */
void gfx_arena_scene_unbind_image(gfx_arena_scene_t *scene, uint32_t node_off,
                              uint16_t item_index, uint8_t face);

/**
 * @brief Ensure package FILE_PATH images for a node are opened into img_rt
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @return 0 on success or already open; negative on error
 */
int gfx_arena_scene_ensure_pkg_images(gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Look up an open runtime image for draw
 * @param scene Scene handle
 * @param node_off Node byte offset
 * @param item_index Item index or GFX_ARENA_IMG_ITEM_NONE
 * @param face Face index
 * @return Runtime image slot, or NULL if none
 */
const gfx_arena_img_rt_t *gfx_arena_scene_img_rt_find(const gfx_arena_scene_t *scene,
                                             uint32_t node_off,
                                             uint16_t item_index, uint8_t face);

/**
 * @brief Return whether a runtime image resource is open
 * @param rt Runtime image slot
 * @return true if open
 */
bool gfx_arena_img_rt_is_open(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image width in pixels
 * @param rt Runtime image slot
 * @return Width, or 0 if closed
 */
uint16_t gfx_arena_img_rt_width(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image height in pixels
 * @param rt Runtime image slot
 * @return Height, or 0 if closed
 */
uint16_t gfx_arena_img_rt_height(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image color format
 * @param rt Runtime image slot
 * @return Color format
 */
gfx_color_format_t gfx_arena_img_rt_format(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image pixel buffer
 * @param rt Runtime image slot
 * @return Pixel pointer, or NULL if closed
 */
const uint8_t *gfx_arena_img_rt_pixels(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image stride in pixels
 * @param rt Runtime image slot
 * @return Stride in pixels
 */
gfx_coord_t gfx_arena_img_rt_stride_px(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image alpha mask buffer
 * @param rt Runtime image slot
 * @return Alpha pointer, or NULL if none
 */
const gfx_opa_t *gfx_arena_img_rt_alpha(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image alpha stride
 * @param rt Runtime image slot
 * @return Alpha stride
 */
gfx_coord_t gfx_arena_img_rt_alpha_stride(const gfx_arena_img_rt_t *rt);

/**
 * @brief Get runtime image bytes per pixel
 * @param rt Runtime image slot
 * @return Pixel size in bytes
 */
uint8_t gfx_arena_img_rt_pixel_size(const gfx_arena_img_rt_t *rt);

/**
 * @brief Bind / open ANIM node playback (gfx_anim host in side table)
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_ANIM offset
 * @param src Animation source; NULL uses package FILE_PATH from gfx_arena_anim_hdr_t
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_bind_anim_src(gfx_arena_scene_t *scene, uint32_t node_off,
                              const gfx_anim_src_t *src);

/**
 * @brief Ensure package ANIM FILE_PATH is opened into anim_rt
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_ANIM offset
 * @return 0 on success or already open; negative on error
 */
int gfx_arena_scene_ensure_pkg_anims(gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Start or stop ANIM host playback
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_ANIM offset
 * @param playing true to start, false to stop
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_anim_set_playing(gfx_arena_scene_t *scene, uint32_t node_off, bool playing);

/**
 * @brief Bind a MOTION node to a firmware motion asset
 *
 * Player canvas and visibility follow the ARN node each tick.
 *
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_MOTION offset
 * @param asset Firmware motion asset (e.g. claw_motion_scene_asset)
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_bind_motion_asset(gfx_arena_scene_t *scene, uint32_t node_off,
                                  const gfx_motion_asset_t *asset);

/**
 * @brief Set the current MOTION action index
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_MOTION offset
 * @param action_idx Action index
 * @param snap true to snap immediately, false to transition
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_motion_set_action(gfx_arena_scene_t *scene, uint32_t node_off,
                                  uint16_t action_idx, bool snap);

/**
 * @brief Sync MOTION canvas and visibility from the ARN node
 * @param scene Scene handle
 * @param node_off GFX_ARENA_NODE_MOTION offset
 * @return 0 on success, negative on error
 */
int gfx_arena_scene_motion_sync(gfx_arena_scene_t *scene, uint32_t node_off);

/**
 * @brief Hit-test a point against visible clickable arena nodes
 * @param scene Scene handle
 * @param x Screen X
 * @param y Screen Y
 * @return Node offset, or GFX_ARENA_NO_NODE if none
 */
uint32_t gfx_arena_scene_hit_test(gfx_arena_scene_t *scene, uint16_t x, uint16_t y);

/**
 * @brief Dispatch a touch event into the arena scene
 * @param disp Display that owns the scene
 * @param event Touch event
 * @return 0 if handled / ignored cleanly, negative on error
 */
int gfx_arena_scene_handle_touch(gfx_display_t *disp, const gfx_touch_event_t *event);

#ifdef __cplusplus
}
#endif
