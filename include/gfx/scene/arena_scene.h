/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Arena scene backend attached to a gfx_display.
 * Package path: mutate arena -> mark_dirty -> existing dirty[]/merge/refresh.
 * Hand-written object UI does not use this API.
 */

#include <stddef.h>
#include <stdint.h>

#include "gfx/input.h"
#include "gfx/scene/arena.h"
#include "gfx/types.h"
#include "gfx/widgets/label.h" /* gfx_font_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_display gfx_display_t;

typedef void (*arena_action_cb_t)(arena_t *arena, arena_node_t *node,
                                  const gfx_touch_event_t *event, void *user_data);

typedef struct {
    const char       *name;
    arena_action_cb_t cb;
    void             *user_data;
} arena_action_entry_t;

typedef struct gfx_arena_scene {
    arena_t                 arena;
    gfx_display_t          *disp;
    const arena_action_entry_t *actions;
    size_t                  action_count;
    uint32_t                pressed_off; /* ARENA_NO_NODE if none */
    uint8_t                 pressed_track;
    gfx_font_t              font;        /* optional; label/button text */
    void                   *font_adapter; /* gfx_font_handle_t, owned */
    /* Glyph alpha scratch reused across draws (owned, freed on detach). */
    uint8_t                *glyph_scratch;
    size_t                  glyph_scratch_cap;
} gfx_arena_scene_t;

/** Attach writable arena as the display's scene backend (takes ownership of arena bytes). */
int arena_scene_attach(gfx_display_t *disp, arena_t *arena, gfx_arena_scene_t *out);

/** Detach and free arena RAM; clears display arena pointer. */
void arena_scene_detach(gfx_arena_scene_t *scene);

gfx_arena_scene_t *arena_scene_from_disp(gfx_display_t *disp);

void arena_scene_set_actions(gfx_arena_scene_t *scene,
                             const arena_action_entry_t *actions, size_t count);

/** Optional font for LABEL/BUTTON text (lvgl/FT source). NULL clears. */
void arena_scene_set_font(gfx_arena_scene_t *scene, gfx_font_t font);

/** Mark node abs rect dirty via gfx_invalidate_area_disp (no fullscreen by default). */
int arena_scene_mark_dirty(gfx_arena_scene_t *scene, uint32_t node_off);

/** Mark entire screen dirty (explicit full refresh only). */
int arena_scene_mark_dirty_all(gfx_arena_scene_t *scene);

/**
 * Hit-test: deepest visible CLICKABLE node containing (x,y).
 * Sibling order: later siblings are above earlier ones.
 * Returns node offset or ARENA_NO_NODE.
 */
uint32_t arena_scene_hit_test(gfx_arena_scene_t *scene, uint16_t x, uint16_t y);

/**
 * Dispatch touch into arena (press/move/release).
 * On RELEASE over the pressed clickable node, runs matching action by name.
 * Returns 1 if handled by arena scene, 0 if no arena / not handled.
 */
int arena_scene_handle_touch(gfx_display_t *disp, const gfx_touch_event_t *event);

/* ---- test / debug hooks (keep for CI demos) ---- */
uint8_t arena_scene_test_dirty_count(gfx_display_t *disp);
int arena_scene_test_dirty_area(gfx_display_t *disp, uint8_t index, gfx_area_t *out);

#ifdef __cplusplus
}
#endif
