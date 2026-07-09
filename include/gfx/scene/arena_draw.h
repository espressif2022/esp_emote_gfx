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

typedef struct {
    uint16_t *pixels;   /* RGB565, row-major */
    uint16_t  width;
    uint16_t  height;
    uint16_t  clear_rgb565;
} arena_fb_t;

/** Full-tree draw into RGB565 FB (test / bench). Clears FB first. */
int arena_draw(const arena_t *arena, arena_fb_t *fb);

/**
 * Draw visible container/button backgrounds into the current render surface.
 * Uses surf->clip_area / buf_area (half-open). No gfx_object_t.
 */
int arena_draw_clipped(gfx_display_t *disp, const arena_t *arena,
                       const void *render_surface /* gfx_render_surface_t* */);

#ifdef __cplusplus
}
#endif
