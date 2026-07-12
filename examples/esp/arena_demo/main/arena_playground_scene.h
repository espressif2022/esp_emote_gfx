/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Arena playground shell — nav order + demo content aligned with format_playground.
 *
 * Anim/Motion are native GFX_ARENA_NODE_* with scene side-table hosts.
 * Pick Stamp remains an object overlay (no ARN type yet).
 *
 * FPS compare vs object path: use List / Wheel / Progress / Button / Image only
 * (tier A). Anim / Motion / Pick Stamp include object-host overhead.
 */

#include <stddef.h>
#include <stdint.h>

#include "gfx/scene/arena.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/label.h"
#include "gfx/object.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARENA_PG_SCREEN_W 800
#define ARENA_PG_SCREEN_H 480
#define ARENA_PG_NAV_COUNT 11

/* Match demo_widget_id_t / gfx_format_demo_widget_names order. */
#define ARENA_PG_NAV_MOTION        0
#define ARENA_PG_NAV_ANIM          1
#define ARENA_PG_NAV_COVERFLOW     2
#define ARENA_PG_NAV_PAGEFLOW      3
#define ARENA_PG_NAV_IMAGE         4
#define ARENA_PG_NAV_BUTTON        5
#define ARENA_PG_NAV_IMAGE_BUTTON  6
#define ARENA_PG_NAV_PROGRESS_BAR  7
#define ARENA_PG_NAV_LIST          8
#define ARENA_PG_NAV_WHEEL         9
#define ARENA_PG_NAV_PICK_STAMP   10

typedef struct {
    gfx_arena_scene_t *scene;
    gfx_display_t     *disp;
    uint32_t nav_off;
    uint32_t panel_offs[ARENA_PG_NAV_COUNT];
    uint32_t title_off;
    uint32_t fps_off;
    uint32_t preview_btn_off;
    uint32_t anim_off;
    uint32_t motion_off;
    uint32_t progress_off;
    uint32_t progress_label_off;
    uint32_t imgbtn_status_off;
    uint16_t focus;

    /* Pick Stamp remains object overlay (no ARN type). */
    gfx_object_t         *pick_stamp;
    gfx_object_t         *pick_stamp_date[5];
    gfx_object_t         *pick_stamp_time[8];
    /* Motion zoom slider (object); canvas size applied via motion_sync. */
    gfx_object_t         *motion_zoom;
    uint16_t              motion_zoom_value;
    uint16_t              motion_action;
    size_t                anim_clip;
    size_t                image_clip;
} gfx_arena_playground_t;

/**
 * @brief Pack the playground ARN package
 * @param out_size Receives packed byte size on success
 * @return Newly allocated package buffer, or NULL on failure; caller frees with free()
 */
uint8_t *gfx_arena_playground_pack(size_t *out_size);

/**
 * @brief Load, attach, and bind playground ARN hosts
 *
 * Binds ANIM/MOTION scene hosts and creates Pick Stamp / zoom overlays.
 *
 * @param disp Target display
 * @param pkg ARN package bytes
 * @param pkg_size Package size
 * @param font Default font
 * @param out_scene Scene handle to initialize
 * @param out_pg Playground handle to initialize
 * @return 0 on success, negative on error
 */
int gfx_arena_playground_bind(gfx_display_t *disp, const uint8_t *pkg, size_t pkg_size,
                          gfx_font_t font, gfx_arena_scene_t *out_scene,
                          gfx_arena_playground_t *out_pg);

/**
 * @brief Tear down playground overlays before gfx_arena_scene_detach
 * @param pg Playground handle; NULL-safe
 */
void gfx_arena_playground_unbind(gfx_arena_playground_t *pg);

/**
 * @brief Show one nav panel and toggle ARN hosts / overlays
 * @param pg Playground handle
 * @param index Nav index in [0, ARENA_PG_NAV_COUNT)
 * @return 0 on success, negative on error
 */
int gfx_arena_playground_set_focus(gfx_arena_playground_t *pg, uint16_t index);

/**
 * @brief Poll nav list selection and apply focus if it changed
 * @param pg Playground handle
 * @return 1 if focus changed, 0 otherwise
 */
int gfx_arena_playground_poll_nav(gfx_arena_playground_t *pg);

/**
 * @brief Update the FPS label text
 * @param pg Playground handle
 * @param fps Frames per second
 * @param frame_ms Frame time in milliseconds
 */
void gfx_arena_playground_update_perf_label(gfx_arena_playground_t *pg, uint32_t fps, uint32_t frame_ms);

#ifdef __cplusplus
}
#endif
