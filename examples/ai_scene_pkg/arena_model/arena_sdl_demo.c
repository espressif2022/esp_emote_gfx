/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_arena_sdl_demo — visual host simulation (arena formal path)
 * ===========================================================
 *   ARENA_SDL=1 ./build-host-sdl/gfx_arena_sdl_demo
 *
 * Mouse click OK button → color + title update (no gfx_object tree).
 * Close window to exit.
 *
 * Headless self-check (CI):
 *   ./build-host-sdl/gfx_arena_sdl_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena_demo_scene.h"

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx_display_port.h"
#include "gfx_host_runner.h"

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
    uint32_t ok_count = 0;

    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_demo_pack(&pkg_size);
    CHECK(pkg != NULL, "pack");

    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    CHECK(handle != NULL, "core");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = ARENA_DEMO_SCREEN_W,
        .v_res = ARENA_DEMO_SCREEN_H,
    });
    CHECK(backend != NULL, "backend");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = ARENA_DEMO_SCREEN_W,
        .v_res = ARENA_DEMO_SCREEN_H,
        .backend = backend,
    });
    CHECK(disp != NULL, "display");

    gfx_arena_scene_t scene;
    CHECK(gfx_arena_demo_bind(disp, pkg, pkg_size, (gfx_font_t)&font_puhui_16_4,
                          &scene, &ok_count) == 0, "bind");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh");

    gfx_touch_event_t press = {
        .type = GFX_TOUCH_EVENT_PRESS,
        .x = ARENA_DEMO_SCREEN_W - 100,
        .y = ARENA_DEMO_SCREEN_H - 56,
        .track_id = 0,
    };
    gfx_touch_event_t release = {
        .type = GFX_TOUCH_EVENT_RELEASE,
        .x = ARENA_DEMO_SCREEN_W - 100,
        .y = ARENA_DEMO_SCREEN_H - 56,
        .track_id = 0,
    };
    CHECK(gfx_touch_inject(disp, &press) == GFX_OK, "press");
    CHECK(gfx_touch_inject(disp, &release) == GFX_OK, "release");
    CHECK(ok_count == 1, "ok_count");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh after click");

    gfx_arena_scene_detach(&scene);
    gfx_core_deinit(handle);
    free(pkg);
    return fails == 0 ? 0 : 1;
}

static int run_sdl(void)
{
    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_demo_pack(&pkg_size);
    if (pkg == NULL) {
        fprintf(stderr, "pack failed\n");
        return 1;
    }

    uint32_t ok_count = 0;
    gfx_display_port_t port = {0};
    gfx_err_t err = gfx_display_port_open(&(gfx_display_port_config_t) {
        .h_res = ARENA_DEMO_SCREEN_W,
        .v_res = ARENA_DEMO_SCREEN_H,
        .fps = 30,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
        .sdl = {
            .scale = 1,
            .title = "arena_sdl_demo (formal path)",
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
    if (gfx_arena_demo_bind(port.disp, pkg, pkg_size, (gfx_font_t)&font_puhui_16_4,
                        &scene, &ok_count) != 0) {
        fprintf(stderr, "arena_demo_bind failed\n");
        gfx_display_port_close(&port);
        free(pkg);
        return 1;
    }

    (void)gfx_core_refresh_now(port.gfx);
    printf("SDL arena formal path: click OK (bottom-right). Close window to exit.\n");

    (void)gfx_host_runner_run(port.gfx, port.disp, &(gfx_host_runner_config_t) {
        .frame_delay_ms = 16,
    });

    printf("ok_count=%lu\n", (unsigned long)ok_count);
    gfx_arena_scene_detach(&scene);
    gfx_display_port_close(&port);
    free(pkg);
    return 0;
}

int main(void)
{
    printf("arena_sdl_demo: formal arena path (draw/dirty/touch)\n");
    const char *sdl = getenv("ARENA_SDL");
    if (sdl != NULL && sdl[0] == '1') {
        return run_sdl();
    }
    const int rc = run_headless();
    printf(rc == 0 ? "PASS\n" : "FAIL\n");
    return rc;
}
