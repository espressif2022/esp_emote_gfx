/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "gfx/scene/arena.h"
#include "gfx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_display gfx_display_t;

/** RGB565 framebuffer target for gfx_arena_draw(). */
typedef struct {
    uint16_t *pixels;   /* RGB565, row-major */
    uint16_t  width;
    uint16_t  height;
    uint16_t  clear_rgb565;
} gfx_arena_fb_t;

/**
 * @brief Draw the full arena tree into an RGB565 framebuffer
 * @param arena Loaded arena
 * @param fb Target framebuffer; cleared first with clear_rgb565
 * @return 0 on success, negative on error
 */
int gfx_arena_draw(const gfx_arena_t *arena, gfx_arena_fb_t *fb);

/**
 * @brief Draw visible arena nodes into the current render surface
 *
 * Uses surf->clip_area / buf_area (half-open). No gfx_object_t for ARN nodes;
 * ANIM/MOTION hosts may still draw via the composite object tree.
 *
 * @param disp Display handle (for font / img_rt / scene side tables)
 * @param arena Loaded arena
 * @param render_surface Opaque gfx_render_surface_t pointer
 * @return 0 on success, negative on error
 */
int gfx_arena_draw_clipped(gfx_display_t *disp, const gfx_arena_t *arena,
                       const void *render_surface /* gfx_render_surface_t* */);

#ifdef __cplusplus
}
#endif
