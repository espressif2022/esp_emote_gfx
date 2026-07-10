/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*
 * DEPRECATED — A/B compare only.
 * --------------------------------
 * Formal package path: gfx_arena_scene_attach + gfx_arena_draw (no gfx_object tree).
 * Prefer gfx_arena_sdl_demo / examples/esp/arena_demo.
 *
 * Kept solely for gfx_arena_compare_demo / ESP ARENA_DEMO_COMPARE=1
 * (materialize parallel gfx_object_t tree). Do not use in product code.
 */

#include "gfx/display.h"
#include "gfx/object.h"
#include "gfx/scene/arena.h"
#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gfx_arena_t        arena;
    gfx_display_t *disp;
    gfx_object_t **objs;       /* [node_count], parallel to arena nodes */
    uint16_t       obj_count;
    gfx_font_t     font;       /* optional; NULL = widget default */
} gfx_arena_gfx_scene_t;

/**
 * Load package into writable arena and build a gfx object tree that mirrors it.
 * Package bytes must outlive this call only until load returns (copied into arena).
 */
int gfx_arena_gfx_bind(const uint8_t *pkg, size_t pkg_size, gfx_display_t *disp,
                   gfx_font_t font, gfx_arena_gfx_scene_t *out);

/** Push one arena node's fields into its gfx object (geometry/colors/text/visible). */
int gfx_arena_gfx_sync_node(gfx_arena_gfx_scene_t *scene, uint32_t node_off);

/** Sync every node. Call after batch in-place arena mutations. */
int gfx_arena_gfx_sync_all(gfx_arena_gfx_scene_t *scene);

gfx_object_t *gfx_arena_gfx_find_obj(gfx_arena_gfx_scene_t *scene, const char *name);

void gfx_arena_gfx_free(gfx_arena_gfx_scene_t *scene);

#ifdef __cplusplus
}
#endif
