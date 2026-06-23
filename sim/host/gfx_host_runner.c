/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "gfx_host_runner.h"

#include <time.h>

#include "gfx/backends/sdl.h"

#define HOST_RUNNER_DEFAULT_DELAY_MS 16U

uint64_t gfx_host_runner_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

void gfx_host_runner_sleep_ms(uint32_t ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0) {
    }
}

int gfx_host_runner_run(gfx_handle_t gfx, gfx_display_t *disp,
                        const gfx_host_runner_config_t *cfg)
{
    uint32_t delay_ms = HOST_RUNNER_DEFAULT_DELAY_MS;
    gfx_host_runner_loop_cb_t loop_cb = NULL;
    void *user_data = NULL;

    if (cfg != NULL) {
        if (cfg->frame_delay_ms > 0U) {
            delay_ms = cfg->frame_delay_ms;
        }
        loop_cb = cfg->loop_cb;
        user_data = cfg->user_data;
    }

    while (!gfx_backend_sdl_poll_events(disp)) {
        if (gfx != NULL) {
            (void)gfx_core_tick(gfx);
        }
        if (loop_cb != NULL) {
            loop_cb(user_data, gfx_host_runner_now_ms());
        }
        gfx_host_runner_sleep_ms(delay_ms);
    }

    return 0;
}
