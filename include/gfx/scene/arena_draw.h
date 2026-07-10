/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
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

/**
 * Draw pass selector for static-layer baking / overlay composition.
 * BAKE renders fills, images and static text — everything except
 * ARENA_F_TEXT_DYN glyphs (host bake of the static layer; the bake tool
 * must use the same font source as the device, e.g. a shared LVGL font C
 * array, for pixel-identical glyphs). OVERLAY renders only
 * ARENA_F_TEXT_DYN glyphs (device pass over a decoded static layer).
 */
typedef enum {
    ARENA_DRAW_MODE_ALL = 0,
    ARENA_DRAW_MODE_BAKE,
    ARENA_DRAW_MODE_OVERLAY,
} arena_draw_mode_t;

/**
 * Like arena_draw_clipped but with an explicit draw pass and no pager
 * compose redirect (always draws the given arena's node tree).
 */
int arena_draw_clipped_mode(gfx_display_t *disp, const arena_t *arena,
                            const void *render_surface, arena_draw_mode_t mode);

/**
 * True if the attached arena scene is guaranteed to paint every pixel of
 * clip (half-open) opaquely: pager compose frames always do; node-tree
 * frames do when the bottom root is an opaque square container covering
 * clip. Lets the render core skip the background clear.
 */
bool arena_draw_covers_clip(gfx_display_t *disp, const gfx_area_t *clip);

#ifdef __cplusplus
}
#endif
