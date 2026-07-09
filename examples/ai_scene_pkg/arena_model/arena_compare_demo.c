/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * arena_compare_demo — A/B: formal arena path vs object tree path (host)
 * =====================================================================
 *   cmake --build build-host-sdl --target gfx_arena_compare_demo
 *   ./build-host-sdl/gfx_arena_compare_demo
 *   ARENA_COMPARE_ITERS=400 ./build-host-sdl/gfx_arena_compare_demo
 *
 * ESP device: examples/esp/arena_demo with -DARENA_DEMO_COMPARE=1
 */

#include <stdio.h>
#include <stdlib.h>

#include "arena_compare_run.h"

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/font_lvgl.h"

extern const lv_font_t font_puhui_16_4;

#define SCREEN_W 320
#define SCREEN_H 240

static int bench_iters(void)
{
    const char *env = getenv("ARENA_COMPARE_ITERS");
    if (env != NULL && env[0] != '\0') {
        int v = atoi(env);
        if (v > 0) {
            return v;
        }
    }
    return 200;
}

static void print_avg(const char *label, int iters, int64_t us)
{
    printf("  %-22s  total=%8.2f ms  avg=%7.2f us\n",
           label, (double)us / 1000.0, (double)us / (double)iters);
}

static void print_ratio(const char *label, int64_t arena_us, int64_t object_us)
{
    if (arena_us <= 0) {
        printf("  %-22s  n/a\n", label);
        return;
    }
    printf("  %-22s  object/arena = %.2fx  ( >1 ⇒ arena faster )\n",
           label, (double)object_us / (double)arena_us);
}

static void print_path(const char *title, const char *load_name,
                       const arena_compare_path_stats_t *s, int iters)
{
    printf("=== %s ===\n", title);
    print_avg(load_name, iters, s->load_us);
    print_avg("full wall", iters, s->full_wall_us);
    print_avg("full render", iters, s->full_render_us);
    print_avg("full flush", iters, s->full_flush_us);
    print_avg("dirty wall", iters, s->dirty_wall_us);
    print_avg("dirty render", iters, s->dirty_render_us);
    print_avg("dirty flush", iters, s->dirty_flush_us);
    printf("\n");
}

int main(void)
{
    const int iters = bench_iters();

    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
    });
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
        .backend = backend,
    });
    if (handle == NULL || backend == NULL || disp == NULL) {
        fprintf(stderr, "gfx init failed\n");
        return 1;
    }

    arena_compare_result_t r = {0};
    const int rc = arena_compare_run(handle, disp, (gfx_font_t)&font_puhui_16_4,
    &(arena_compare_config_t) {
        .screen_w = SCREEN_W,
        .screen_h = SCREEN_H,
        .iters = iters,
    }, &r);
    if (rc != 0) {
        fprintf(stderr, "arena_compare_run failed: %d\n", rc);
        gfx_core_deinit(handle);
        return 1;
    }

    printf("arena_compare_demo: formal arena vs object bridge\n");
    printf("  screen=%ux%u  nodes=%u  pkg=%zu B  iters=%d\n\n",
           SCREEN_W, SCREEN_H, (unsigned)r.node_count, r.pkg_size, r.iters);

    print_path("A formal arena", "load+attach", &r.arena, r.iters);
    print_path("B object bridge", "load+bind", &r.object, r.iters);

    printf("=== ratio (object / arena) ===\n");
    print_ratio("load", r.arena.load_us, r.object.load_us);
    print_ratio("full wall", r.arena.full_wall_us, r.object.full_wall_us);
    print_ratio("full render", r.arena.full_render_us, r.object.full_render_us);
    print_ratio("full flush", r.arena.full_flush_us, r.object.full_flush_us);
    print_ratio("dirty wall", r.arena.dirty_wall_us, r.object.dirty_wall_us);
    print_ratio("dirty render", r.arena.dirty_render_us, r.object.dirty_render_us);
    print_ratio("dirty flush", r.arena.dirty_flush_us, r.object.dirty_flush_us);

    printf("\nNotes:\n");
    printf("  - wall = refresh_now wall clock; render/flush from display perf stats\n");
    printf("  - On device, flush often dominates wall; compare render for draw path\n");
    printf("  - ESP: idf.py -B build -DARENA_DEMO_COMPARE=1 reconfigure build flash monitor\n");

    gfx_core_deinit(handle);
    return 0;
}
