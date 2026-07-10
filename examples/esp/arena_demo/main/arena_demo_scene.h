/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gfx/scene/arena.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default visual demo resolution (matches HMI RGB board). */
#define ARENA_DEMO_SCREEN_W 800
#define ARENA_DEMO_SCREEN_H 480

/**
 * Pack a simple interactive home-like arena scene.
 * Caller frees returned buffer with free().
 */
uint8_t *gfx_arena_demo_pack(size_t *out_size);

/**
 * Load package into scene, attach to disp, set font + on_ok action.
 * on_ok: button turns orange and title text updates via user callback data.
 */
int gfx_arena_demo_bind(gfx_display_t *disp, const uint8_t *pkg, size_t pkg_size,
                    gfx_font_t font, gfx_arena_scene_t *out_scene,
                    uint32_t *ok_count_inout);

#ifdef __cplusplus
}
#endif
