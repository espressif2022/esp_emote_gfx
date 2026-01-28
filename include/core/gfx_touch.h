/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"

typedef void *gfx_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GFX_TOUCH_EVENT_PRESS = 0,
    GFX_TOUCH_EVENT_RELEASE,
} gfx_touch_event_type_t;

typedef struct {
    gfx_touch_event_type_t type;
    uint16_t x;
    uint16_t y;
    uint16_t strength;
    uint8_t track_id;
    uint32_t timestamp_ms;
} gfx_touch_event_t;

typedef void (*gfx_touch_event_cb_t)(gfx_handle_t handle, const gfx_touch_event_t *event, void *user_data);

typedef struct {
    esp_lcd_touch_handle_t handle;
    uint32_t poll_ms;
    gfx_touch_event_cb_t event_cb;
    void *user_data;
    gpio_num_t int_gpio_num;  ///< Interrupt GPIO number (GPIO_NUM_NC if not used or to use handle's config)
} gfx_touch_config_t;

/**
 * @brief Configure touch handling at runtime
 *
 * Passing NULL or a config without handle disables touch support.
 *
 * @param handle Graphics handle
 * @param config Touch configuration or NULL to disable
 * @return esp_err_t ESP_OK on success, otherwise error code
 */
esp_err_t gfx_touch_configure(gfx_handle_t handle, const gfx_touch_config_t *config);

#ifdef __cplusplus
}
#endif
