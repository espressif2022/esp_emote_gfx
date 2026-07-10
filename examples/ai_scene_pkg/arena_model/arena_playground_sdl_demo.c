/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_arena_playground_sdl_demo — PC sim for Arena playground
 * =======================================================
 *   ARENA_SDL=1 ./build-host-sdl/gfx_arena_playground_sdl_demo
 *
 * Compare with object path:
 *   ./build-host-sdl/gfx_host_sdl_demo
 *
 * Headless (CI):
 *   ./build-host-sdl/gfx_arena_playground_sdl_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena_playground_scene.h"

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/fs.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx_display_port.h"
#include "gfx_host_runner.h"

#ifndef ARENA_PG_ASSETS_DIR
/* Shared with format_playground: JPEG + EAF for Image/Coverflow/Anim. */
#define ARENA_PG_ASSETS_DIR "examples/assets/format"
#endif

static gfx_asset_source_t *s_assets_fs;

static int gfx_arena_pg_mount_assets(void)
{
    gfx_err_t err;
    if (s_assets_fs != NULL) {
        return 0;
    }
    err = gfx_fs_open_dir(ARENA_PG_ASSETS_DIR, &s_assets_fs);
    if (err != GFX_OK || s_assets_fs == NULL) {
        fprintf(stderr, "WARN: gfx_fs_open_dir(%s) failed (%d); JPEG demos skipped\n",
                ARENA_PG_ASSETS_DIR, (int)err);
        return -1;
    }
    err = gfx_fs_mount("", s_assets_fs);
    if (err != GFX_OK) {
        fprintf(stderr, "WARN: gfx_fs_mount failed (%d)\n", (int)err);
        gfx_fs_close(s_assets_fs);
        s_assets_fs = NULL;
        return -1;
    }
    return 0;
}

static void gfx_arena_pg_unmount_assets(void)
{
    if (s_assets_fs != NULL) {
        gfx_fs_close(s_assets_fs);
        s_assets_fs = NULL;
    }
}

extern const lv_font_t font_puhui_16_4;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        fails++; \
    } else { \
        printf("OK  %s\n", msg); \
    } \
} while (0)

static int run_headless(void)
{
    int fails = 0;

    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_playground_pack(&pkg_size);
    CHECK(pkg != NULL, "pack");

    (void)gfx_arena_pg_mount_assets();

    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    CHECK(handle != NULL, "core");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = ARENA_PG_SCREEN_W,
        .v_res = ARENA_PG_SCREEN_H,
    });
    CHECK(backend != NULL, "backend");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = ARENA_PG_SCREEN_W,
        .v_res = ARENA_PG_SCREEN_H,
        .backend = backend,
    });
    CHECK(disp != NULL, "display");

    gfx_arena_scene_t scene;
    gfx_arena_playground_t pg;
    CHECK(gfx_arena_playground_bind(disp, pkg, pkg_size, (gfx_font_t)&font_puhui_16_4,
                                &scene, &pg) == 0, "bind");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh");

    /* Tap visible nav row Coverflow (index 2): nav at y=112, item_height 46.
     * List is index 8 and needs scroll; verify that via set_focus. */
    {
        const int nav_y = 112;
        const int item_h = 46;
        const int tap_idx = ARENA_PG_NAV_COVERFLOW;
        const int tap_y = nav_y + tap_idx * item_h + item_h / 2;
        gfx_touch_event_t press = {
            .type = GFX_TOUCH_EVENT_PRESS,
            .x = 80,
            .y = (gfx_coord_t)tap_y,
            .track_id = 0,
        };
        gfx_touch_event_t release = {
            .type = GFX_TOUCH_EVENT_RELEASE,
            .x = 80,
            .y = (gfx_coord_t)tap_y,
            .track_id = 0,
        };
        CHECK(gfx_touch_inject(disp, &press) == GFX_OK, "nav press");
        CHECK(gfx_touch_inject(disp, &release) == GFX_OK, "nav release");
    }
    CHECK(pg.focus == ARENA_PG_NAV_COVERFLOW, "focus=Coverflow");
    CHECK(gfx_arena_playground_poll_nav(&pg) == 0, "poll no-op after action");
    CHECK(gfx_arena_playground_set_focus(&pg, ARENA_PG_NAV_LIST) == 0, "set_focus List");
    CHECK(pg.focus == ARENA_PG_NAV_LIST, "focus=List");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh after nav");

    gfx_arena_playground_unbind(&pg);
    gfx_arena_scene_detach(&scene);
    gfx_core_deinit(handle);
    gfx_arena_pg_unmount_assets();
    free(pkg);
    return fails == 0 ? 0 : 1;
}

typedef struct {
    gfx_handle_t gfx;
    gfx_display_t *disp;
    gfx_arena_playground_t *pg;
    gfx_timer_handle_t perf_timer;
} pg_tick_ctx_t;

static void on_pg_loop(void *user_data, uint64_t now_ms)
{
    (void)now_ms;
    pg_tick_ctx_t *ctx = (pg_tick_ctx_t *)user_data;
    if (ctx == NULL || ctx->pg == NULL) {
        return;
    }
    if (gfx_arena_playground_poll_nav(ctx->pg) && ctx->gfx != NULL) {
        (void)gfx_core_refresh_now(ctx->gfx);
    }
}

static void gfx_arena_perf_timer_cb(void *user_data)
{
    pg_tick_ctx_t *ctx = (pg_tick_ctx_t *)user_data;
    gfx_display_perf_stats_t stats;
    uint32_t fps;
    uint32_t frame_ms = 0;

    if (ctx == NULL || ctx->gfx == NULL || ctx->disp == NULL || ctx->pg == NULL) {
        return;
    }

    fps = gfx_timer_get_actual_fps(ctx->gfx);
    if (gfx_display_get_perf_stats(ctx->disp, &stats) == GFX_OK) {
        frame_ms = (uint32_t)((stats.frame_time_us + 500U) / 1000U);
        if (fps == 0U && stats.frame_time_us > 0U) {
            fps = (uint32_t)(1000000ULL / stats.frame_time_us);
        }
    }
    gfx_arena_playground_update_perf_label(ctx->pg, fps, frame_ms);
}

static int run_sdl(void)
{
    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_playground_pack(&pkg_size);
    if (pkg == NULL) {
        fprintf(stderr, "pack failed\n");
        return 1;
    }

    (void)gfx_arena_pg_mount_assets();

    gfx_display_port_t port = {0};
    gfx_err_t err = gfx_display_port_open(&(gfx_display_port_config_t) {
        .h_res = ARENA_PG_SCREEN_W,
        .v_res = ARENA_PG_SCREEN_H,
        .fps = 30,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
        .sdl = {
            .scale = 1,
            .title = "GFX Arena Playground SDL",
        },
        .runtime = {
            .manual_tick = true,
            .core = {
                .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
            },
        },
        .display = {
            .double_buffer = true,
        },
    }, &port);
    if (err != GFX_OK) {
        fprintf(stderr, "SDL port open failed: %d\n", (int)err);
        free(pkg);
        return 1;
    }

    gfx_arena_scene_t scene;
    gfx_arena_playground_t pg;
    if (gfx_arena_playground_bind(port.disp, pkg, pkg_size, (gfx_font_t)&font_puhui_16_4,
                              &scene, &pg) != 0) {
        fprintf(stderr, "arena_playground_bind failed\n");
        gfx_display_port_close(&port);
        free(pkg);
        return 1;
    }

    pg_tick_ctx_t tick = {
        .gfx = port.gfx,
        .disp = port.disp,
        .pg = &pg,
        .perf_timer = NULL,
    };
    tick.perf_timer = gfx_timer_create(port.gfx, gfx_arena_perf_timer_cb, 500, &tick);
    if (tick.perf_timer == NULL) {
        fprintf(stderr, "failed to create perf timer\n");
        gfx_arena_playground_unbind(&pg);
        gfx_arena_scene_detach(&scene);
        gfx_display_port_close(&port);
        free(pkg);
        return 1;
    }

    (void)gfx_core_refresh_now(port.gfx);
    printf("SDL Arena playground (nav order = object format_playground).\n");
    printf("Compare FPS with: ./build-host-sdl/gfx_host_sdl_demo\n");

    (void)gfx_host_runner_run(port.gfx, port.disp, &(gfx_host_runner_config_t) {
        .frame_delay_ms = 16,
        .loop_cb = on_pg_loop,
        .user_data = &tick,
    });

    gfx_arena_playground_unbind(&pg);
    gfx_arena_scene_detach(&scene);
    gfx_display_port_close(&port);
    gfx_arena_pg_unmount_assets();
    free(pkg);
    return 0;
}

int main(void)
{
    printf("arena_playground_sdl_demo: Arena playground (parity shell)\n");
    const char *sdl = getenv("ARENA_SDL");
    if (sdl != NULL && sdl[0] == '1') {
        return run_sdl();
    }
    const int rc = run_headless();
    printf(rc == 0 ? "PASS\n" : "FAIL\n");
    return rc;
}
