/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Shared A/B compare + dual-board row-sweep helpers.
 *
 * Timing (gfx_arena_compare_run):
 *   wall = refresh_now wall clock
 *   render / flush = gfx_display_get_perf_stats()
 *
 * Visual dual-board (ESP ARENA_DEMO_SWEEP=1):
 *   same 16×12 grid; board A uses arena path, board B uses object bridge;
 *   both recolor one row per frame and scroll rows.
 */

#include <stddef.h>
#include <stdint.h>

#include "gfx/base.h"
#include "gfx/scene/arena.h"
#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARENA_COMPARE_GRID_COLS 16
#define ARENA_COMPARE_GRID_ROWS 12

typedef struct {
    uint16_t screen_w;
    uint16_t screen_h;
    int      iters; /* full/dirty loops; load loops use same count */
} gfx_arena_compare_config_t;

typedef struct {
    int64_t load_us;
    int64_t full_wall_us;
    int64_t full_render_us;
    int64_t full_flush_us;
    int64_t dirty_wall_us;
    int64_t dirty_render_us;
    int64_t dirty_flush_us;
} gfx_arena_compare_path_stats_t;

typedef struct {
    uint16_t node_count;
    size_t   pkg_size;
    int      iters;
    gfx_arena_compare_path_stats_t arena;
    gfx_arena_compare_path_stats_t object;
} gfx_arena_compare_result_t;

/** Pack stress scene: root → row containers → 16×12 captioned buttons. Caller frees. */
uint8_t *gfx_arena_compare_pack_grid(uint16_t screen_w, uint16_t screen_h,
                                 size_t *out_size, uint16_t *out_count);

/** Row container node offset (row in [0, GRID_ROWS)). */
uint32_t gfx_arena_compare_row_container_off(gfx_arena_t *arena, int row);

/** Recolor all buttons in one row (bg_rgb = color_base + col). */
void gfx_arena_compare_recolor_row(gfx_arena_t *arena, int row, uint32_t color_base);

/**
 * Pack stress scene, run A then B on disp.
 * Dirty path marks one full button row. Leaves display clean. Returns 0 on success.
 */
int gfx_arena_compare_run(gfx_handle_t gfx, gfx_display_t *disp, gfx_font_t font,
                      const gfx_arena_compare_config_t *cfg,
                      gfx_arena_compare_result_t *out);

#ifdef __cplusplus
}
#endif
