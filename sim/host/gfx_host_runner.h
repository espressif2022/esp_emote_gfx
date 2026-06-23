/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/core.h"
#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gfx_host_runner_loop_cb_t)(void *user_data, uint64_t now_ms);

typedef struct {
    uint32_t frame_delay_ms;              /**< Sleep after each host loop; 0 uses 16 ms. */
    gfx_host_runner_loop_cb_t loop_cb;    /**< Optional app callback after SDL events/tick. */
    void *user_data;                      /**< App callback user data. */
} gfx_host_runner_config_t;

uint64_t gfx_host_runner_now_ms(void);
void gfx_host_runner_sleep_ms(uint32_t ms);
int gfx_host_runner_run(gfx_handle_t gfx, gfx_display_t *disp,
                        const gfx_host_runner_config_t *cfg);

#ifdef __cplusplus
}
#endif
