/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/gfx_obj.h"
#include "core/gfx_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lightweight motion driver.
 *
 * Goal: let higher-level scene/player code focus on state changes only.
 * - The driver owns a timer and calls `tick_cb` periodically.
 * - When `tick_cb` reports changes (or `force_apply`), the driver calls `apply_cb`.
 */

typedef struct gfx_motion_cfg_t {
    uint16_t timer_period_ms;
    int16_t damping_div;
} gfx_motion_cfg_t;

typedef struct gfx_motion_t gfx_motion_t;

typedef bool (*gfx_motion_tick_cb_t)(gfx_motion_t *motion, void *user_data);
typedef esp_err_t (*gfx_motion_apply_cb_t)(gfx_motion_t *motion, void *user_data, bool force_apply);

struct gfx_motion_t {
    gfx_timer_handle_t timer;
    gfx_motion_cfg_t cfg;
    gfx_disp_t *disp;
    gfx_obj_t *anchor;
    gfx_motion_tick_cb_t tick_cb;
    gfx_motion_apply_cb_t apply_cb;
    void *user_data;
};

void gfx_motion_cfg_init(gfx_motion_cfg_t *cfg, uint16_t timer_period_ms, int16_t damping_div);

esp_err_t gfx_motion_init(gfx_motion_t *motion,
                          gfx_disp_t *disp,
                          gfx_obj_t *anchor,
                          const gfx_motion_cfg_t *cfg,
                          gfx_motion_tick_cb_t tick_cb,
                          gfx_motion_apply_cb_t apply_cb,
                          void *user_data);

void gfx_motion_deinit(gfx_motion_t *motion);

esp_err_t gfx_motion_set_period(gfx_motion_t *motion, uint16_t period_ms);

/** Run one tick immediately (no wait). */
esp_err_t gfx_motion_step(gfx_motion_t *motion, bool force_apply);

/** Utility: damped step for int16 values (same policy as existing widgets). */
int16_t gfx_motion_ease_i16(int16_t cur, int16_t tgt, int16_t div);

#ifdef __cplusplus
}
#endif
