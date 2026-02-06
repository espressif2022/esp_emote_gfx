/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "core/gfx_touch.h"
#include "core/gfx_timer_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gfx_core_context;

/**
 * @brief Touch module internal state (one per graphics context)
 *
 * Full definition of struct gfx_touch; public API uses opaque gfx_touch_t.
 */
struct gfx_touch {
    struct gfx_core_context *ctx;
    esp_lcd_touch_handle_t handle;
    gfx_timer_handle_t poll_timer;
    gfx_touch_event_cb_t event_cb;
    void *user_data;
    uint32_t poll_ms;

    bool pressed;
    uint16_t last_x;
    uint16_t last_y;
    uint16_t last_strength;
    uint8_t last_id;

    gpio_num_t int_gpio_num;
    bool irq_enabled;
    volatile bool irq_pending;
    void *isr_ctx;
};

/* Internal API (used by gfx_core.c and gfx_touch.c) */
esp_err_t gfx_touch_start(gfx_touch_t *touch, const gfx_touch_config_t *cfg);
void gfx_touch_deinit(gfx_touch_t *touch);

#ifdef __cplusplus
}
#endif
