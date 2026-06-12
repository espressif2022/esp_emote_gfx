/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "gfx/widgets/motion.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/** Maximum colour palette entries (colour_idx 1..GFX_MOTION_PALETTE_MAX). */
#define GFX_MOTION_PALETTE_MAX 64U

typedef struct {
    int16_t x;
    int16_t y;
} gfx_motion_point_t;

typedef struct {
    const gfx_motion_asset_t *asset;

    gfx_motion_point_t pose_cur[GFX_MOTION_SCENE_MAX_POINTS];
    gfx_motion_point_t pose_tgt[GFX_MOTION_SCENE_MAX_POINTS];

    uint16_t active_action;
    uint8_t active_step;
    uint16_t step_ticks;
    bool action_loop_override_en;
    bool action_loop_override;
    bool action_ended;
    bool dirty;
} gfx_motion_scene_t;

esp_err_t gfx_motion_scene_init(gfx_motion_scene_t *scene, const gfx_motion_asset_t *asset);
esp_err_t gfx_motion_scene_set_action(gfx_motion_scene_t *scene, uint16_t action_index, bool snap_now);
esp_err_t gfx_motion_scene_set_action_loop(gfx_motion_scene_t *scene, bool loop);
esp_err_t gfx_motion_scene_clear_action_loop_override(gfx_motion_scene_t *scene);
bool gfx_motion_scene_tick(gfx_motion_scene_t *scene);
bool gfx_motion_scene_advance(gfx_motion_scene_t *scene);
void gfx_motion_scene_log_active_step(const gfx_motion_scene_t *scene, const char *reason);

#ifdef __cplusplus
}
#endif
