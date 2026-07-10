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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/input.h"
#include "gfx/scene/arena.h"
#include "gfx/timer.h"
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

/**
 * Full-screen horizontal pager (drag between pages).
 *
 * N pages over a sliding window of at most 3 full-frame snapshots
 * (previous / current / next); drag and snap frames compose the current
 * and neighbor snapshots row-wise instead of redrawing the node tree, so
 * drag cost is independent of page count and scene complexity. On page
 * settle the vacated far slot is refilled with the new neighbor on the
 * next pager timer tick, off the drag-critical path.
 *
 * load_page rebuilds the arena content for a page (e.g. arena_pack +
 * arena_load + arena_scene_replace_arena). It runs in gfx task context on
 * page settle and in the caller's context during enable; it must NOT take
 * the gfx core lock. Pages are rebuilt on switch and on neighbor prefetch
 * via the fallback snapshot path (widget state resets).
 *
 * snapshot_page (optional) generates a page snapshot directly into buf
 * (w*h RGB565), e.g. arena_scene_pager_snapshot_baked over a compile-time
 * baked static layer. Return 0 on success; nonzero falls back to
 * load_page + full node-tree render.
 */
typedef struct {
    gfx_handle_t gfx;          /* core handle (snap animation timer) */
    uint8_t  page_count;       /* 2..ARENA_PAGER_MAX_PAGES */
    uint8_t  current;          /* page currently loaded in the arena */
    uint16_t drag_threshold;   /* px to enter drag mode; 0 = 8 */
    uint16_t snap_ms;          /* snap animation length; 0 = 200 */
    int (*load_page)(uint8_t page, void *user_data);
    int (*snapshot_page)(uint8_t page, uint16_t *buf, uint16_t w, uint16_t h,
                         void *user_data);
    void *user_data;
} arena_pager_config_t;

#define ARENA_PAGER_MAX_PAGES  32U
#define ARENA_PAGER_SNAP_SLOTS 3U

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
    /* Pager state; internal, mutated on the gfx task (touch/timer/render). */
    struct {
        bool     enabled;
        bool     touch_active;  /* finger down since PRESS */
        bool     dragging;      /* past threshold: compose mode */
        bool     animating;     /* snap animation running */
        bool     snap_stale;    /* current page mutated since snapshot */
        bool     prefetch_pending; /* fill missing neighbor on next tick */
        uint8_t  track;
        uint8_t  current;       /* page index */
        uint8_t  page_count;
        uint8_t  snap_slots;    /* allocated snapshot slots (2 or 3) */
        uint16_t drag_threshold;
        uint16_t snap_ms;
        uint16_t w, h;          /* snapshot dimensions (display res) */
        int16_t  start_x, start_y;
        int32_t  drag_dx;       /* current page shift; <0 next page, >0 prev */
        int32_t  snap_from, snap_to;
        uint32_t snap_start;    /* gfx_timer_tick_get() at snap start */
        gfx_handle_t gfx;
        gfx_timer_handle_t snap_timer;
        uint16_t *snap[ARENA_PAGER_SNAP_SLOTS];    /* slot buffers (owned) */
        int16_t  snap_page[ARENA_PAGER_SNAP_SLOTS];/* page per slot; -1 empty */
        int (*load_page)(uint8_t page, void *user_data);
        int (*snapshot_page)(uint8_t page, uint16_t *buf, uint16_t w, uint16_t h,
                             void *user_data);
        void *user_data;
    } pager;
} gfx_arena_scene_t;

/** Snapshot buffer of the slot holding `page`, or NULL if not resident. */
static inline uint16_t *arena_scene_pager_snap_of(gfx_arena_scene_t *scene, int page)
{
    for (uint8_t i = 0; i < scene->pager.snap_slots; i++) {
        if ((int)scene->pager.snap_page[i] == page) {
            return scene->pager.snap[i];
        }
    }
    return NULL;
}

/** True while draw must compose pager snapshots instead of the node tree. */
static inline bool arena_scene_pager_composing(const gfx_arena_scene_t *scene)
{
    return scene->pager.enabled &&
           (scene->pager.dragging || scene->pager.animating || scene->pager.drag_dx != 0);
}

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

/**
 * Replace scene arena content in place (frees the old arena, takes ownership
 * of the new one; font/actions/pager state are kept). Caller must hold the
 * gfx core lock or run in gfx task context.
 */
int arena_scene_replace_arena(gfx_arena_scene_t *scene, arena_t *arena);

/* ---- pager (full-screen horizontal drag between N pages) ---- */

/**
 * Enable the pager and pre-render the snapshot window (current page plus
 * valid neighbors). The arena must already hold cfg->current, and the
 * display render format must be 16bpp. Allocates min(page_count, 3) *
 * w * h * 2 bytes for snapshots (freed on disable/detach). Caller must
 * hold the gfx core lock.
 */
int arena_scene_pager_enable(gfx_arena_scene_t *scene, const arena_pager_config_t *cfg);

/**
 * snapshot_page helper for compile-time baked pages: decode the ARS1 RLE
 * static layer into buf, then overlay runtime-bound text from the page's
 * ARN blob (ARENA_DRAW_MODE_OVERLAY over ARENA_F_TEXT_DYN nodes, scene
 * font). buf is pager w*h RGB565. Usable from
 * arena_pager_config_t.snapshot_page.
 */
int arena_scene_pager_snapshot_baked(gfx_arena_scene_t *scene,
                                     const uint8_t *rle, size_t rle_size,
                                     const uint8_t *pkg, size_t pkg_size,
                                     uint16_t *buf);

/** Free snapshots/timer and return to normal node-tree rendering. */
void arena_scene_pager_disable(gfx_arena_scene_t *scene);

/**
 * Programmatic drag for benches: set the page shift directly (clamped to the
 * valid direction) and mark the screen dirty. Caller must hold the gfx lock.
 */
int arena_scene_pager_set_offset(gfx_arena_scene_t *scene, int32_t dx);

/** End a programmatic drag: offset 0, compose mode off, screen dirty. */
int arena_scene_pager_cancel(gfx_arena_scene_t *scene);

/* ---- test / debug hooks (keep for CI demos) ---- */
uint8_t arena_scene_test_dirty_count(gfx_display_t *disp);
int arena_scene_test_dirty_area(gfx_display_t *disp, uint8_t index, gfx_area_t *out);

#ifdef __cplusplus
}
#endif
