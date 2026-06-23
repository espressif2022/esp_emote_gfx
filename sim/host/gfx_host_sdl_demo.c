/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/gfx_err.h"
#include "gfx/base.h"
#include "gfx/fs.h"

#include "gfx_display_port.h"
#include "gfx_host_runner.h"
#include "playground_scene.h"

#define HOST_LCD_H_RES 800U
#define HOST_LCD_V_RES 480U

typedef struct {
    gfx_display_port_t port;
    gfx_timer_handle_t perf_timer;
} host_demo_t;

static void host_perf_timer_cb(void *user_data)
{
    host_demo_t *demo = (host_demo_t *)user_data;
    gfx_display_perf_stats_t stats;
    uint32_t fps;
    uint32_t frame_ms = 0;

    if (demo == NULL || demo->port.gfx == NULL || demo->port.disp == NULL) {
        return;
    }

    fps = gfx_timer_get_actual_fps(demo->port.gfx);
    if (gfx_display_get_perf_stats(demo->port.disp, &stats) == GFX_OK) {
        frame_ms = (uint32_t)((stats.frame_time_us + 500U) / 1000U);
        if (fps == 0U && stats.frame_time_us > 0U) {
            fps = (uint32_t)(1000000ULL / stats.frame_time_us);
        }
    }
    gfx_format_demo_update_perf_label(fps, frame_ms);
}

static int host_demo_open_port(host_demo_t *demo)
{
    const char *asset_root = getenv("GFX_FS_ROOT");

    if (asset_root == NULL || asset_root[0] == '\0') {
        asset_root = "examples/assets/format";
    }

    gfx_err_t err = gfx_display_port_open(&(gfx_display_port_config_t) {
        .h_res = HOST_LCD_H_RES,
        .v_res = HOST_LCD_V_RES,
        .fps = 30,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
        .sdl = {
            .scale = 1,
            .title = "GFX RGB565 Playground SDL",
        },
        .runtime = {
            .manual_tick = true,
            .core = {
                .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
            },
        },
        .display = {
            .full_frame = true,
            .double_buffer = true,
        },
        .fs = {
            .type = GFX_DISPLAY_PORT_FS_DIR,
            .path_or_label = asset_root,
        },
    }, &demo->port);
    if (err != GFX_OK) {
        fprintf(stderr, "display port open failed: %d root=%s\n", err, asset_root);
        return 1;
    }

    if (gfx_format_demo_set_asset_fs(demo->port.fs) != GFX_OK) {
        fprintf(stderr, "format demo asset fs setup failed\n");
        gfx_display_port_close(&demo->port);
        return 1;
    }

    return 0;
}

int main(void)
{
    host_demo_t demo = {0};

    if (host_demo_open_port(&demo) != 0) {
        return 1;
    }

    if (gfx_format_demo_build_playground_scene(demo.port.disp,
            "GFX RGB565 Playground", "RGB565") != GFX_OK) {
        fprintf(stderr, "failed to build format playground scene\n");
        gfx_display_port_close(&demo.port);
        return 1;
    }

    demo.perf_timer = gfx_timer_create(demo.port.gfx, host_perf_timer_cb, 500, &demo);
    if (demo.perf_timer == NULL) {
        fprintf(stderr, "failed to create perf timer\n");
        gfx_display_port_close(&demo.port);
        return 1;
    }

    (void)gfx_core_refresh_now(demo.port.gfx);
    (void)gfx_host_runner_run(demo.port.gfx, demo.port.disp, &(gfx_host_runner_config_t) {
        .frame_delay_ms = 16,
    });

    gfx_display_port_close(&demo.port);
    return 0;
}
