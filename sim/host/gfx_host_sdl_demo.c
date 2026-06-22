/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/gfx_err.h"
#include "gfx/backends/sdl.h"
#include "gfx/base.h"
#include "gfx/fs.h"

#include "playground_scene.h"

#define HOST_LCD_H_RES 800U
#define HOST_LCD_V_RES 480U

typedef struct {
    gfx_handle_t gfx;
    gfx_display_t *display;
    gfx_fs_t *asset_fs;
    gfx_timer_handle_t perf_timer;
} host_demo_t;

static void host_sleep_ms(unsigned ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0) {
    }
}

static void host_perf_timer_cb(void *user_data)
{
    host_demo_t *demo = (host_demo_t *)user_data;
    gfx_display_perf_stats_t stats;
    uint32_t fps;
    uint32_t frame_ms = 0;

    if (demo == NULL || demo->gfx == NULL || demo->display == NULL) {
        return;
    }

    fps = gfx_timer_get_actual_fps(demo->gfx);
    if (gfx_display_get_perf_stats(demo->display, &stats) == GFX_OK) {
        frame_ms = (uint32_t)((stats.frame_time_us + 500U) / 1000U);
        if (fps == 0U && stats.frame_time_us > 0U) {
            fps = (uint32_t)(1000000ULL / stats.frame_time_us);
        }
    }
    gfx_format_demo_update_perf_label(fps, frame_ms);
}

static int host_demo_open_assets(host_demo_t *demo)
{
    const char *asset_root = getenv("GFX_FS_ROOT");
    gfx_err_t err;

    if (asset_root == NULL || asset_root[0] == '\0') {
        asset_root = "examples/assets/format";
    }

    err = gfx_fs_open_dir(asset_root, &demo->asset_fs);
    if (err != GFX_OK) {
        fprintf(stderr, "asset fs open failed: %d root=%s\n", err, asset_root);
        return 1;
    }

    if (gfx_format_demo_set_asset_fs(demo->asset_fs) != GFX_OK) {
        fprintf(stderr, "format demo asset fs setup failed\n");
        gfx_fs_close(demo->asset_fs);
        demo->asset_fs = NULL;
        return 1;
    }

    return 0;
}

int main(void)
{
    host_demo_t demo = {0};
    gfx_backend_t *backend;

    if (host_demo_open_assets(&demo) != 0) {
        return 1;
    }

    demo.gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    if (demo.gfx == NULL) {
        fprintf(stderr, "failed to init gfx core\n");
        gfx_fs_close(demo.asset_fs);
        return 1;
    }

    backend = gfx_backend_sdl_create(&(gfx_backend_sdl_config_t) {
        .h_res = HOST_LCD_H_RES,
        .v_res = HOST_LCD_V_RES,
        .scale = 1,
        .title = "GFX RGB565 Playground SDL",
    });
    if (backend == NULL) {
        fprintf(stderr, "failed to create SDL backend\n");
        gfx_core_deinit(demo.gfx);
        gfx_fs_close(demo.asset_fs);
        return 1;
    }

    demo.display = gfx_display_add(demo.gfx, &(gfx_display_config_t) {
        .h_res = HOST_LCD_H_RES,
        .v_res = HOST_LCD_V_RES,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend = backend,
        .flags = {
            .full_frame = true,
            .double_buffer = true,
        },
    });
    if (demo.display == NULL) {
        fprintf(stderr, "failed to create display\n");
        gfx_backend_sdl_delete(backend);
        gfx_core_deinit(demo.gfx);
        gfx_fs_close(demo.asset_fs);
        return 1;
    }

    if (gfx_format_demo_build_playground_scene(demo.display,
            "GFX RGB565 Playground", "RGB565") != GFX_OK) {
        fprintf(stderr, "failed to build format playground scene\n");
        gfx_core_deinit(demo.gfx);
        gfx_fs_close(demo.asset_fs);
        return 1;
    }

    demo.perf_timer = gfx_timer_create(demo.gfx, host_perf_timer_cb, 500, &demo);
    if (demo.perf_timer == NULL) {
        fprintf(stderr, "failed to create perf timer\n");
        gfx_core_deinit(demo.gfx);
        gfx_fs_close(demo.asset_fs);
        return 1;
    }

    (void)gfx_core_refresh_now(demo.gfx);
    while (!gfx_backend_sdl_poll(demo.display)) {
        host_sleep_ms(16);
    }

    gfx_core_deinit(demo.gfx);
    gfx_fs_close(demo.asset_fs);
    return 0;
}
