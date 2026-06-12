/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_lcd_touch_t *esp_lcd_touch_handle_t;

typedef struct {
    int int_gpio_num;
    void *user_data;
} esp_lcd_touch_config_t;

typedef struct esp_lcd_touch_t {
    esp_lcd_touch_config_t config;
} esp_lcd_touch_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t strength;
    uint8_t track_id;
} esp_lcd_touch_point_data_t;

typedef void (*esp_lcd_touch_interrupt_cb_t)(esp_lcd_touch_handle_t tp);

static inline esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t tp)
{
    (void)tp;
    return ESP_OK;
}

static inline esp_err_t esp_lcd_touch_get_data(esp_lcd_touch_handle_t tp,
        esp_lcd_touch_point_data_t *point_data,
        uint8_t *point_num,
        uint8_t max_point_num)
{
    (void)tp;
    (void)point_data;
    (void)max_point_num;
    if (point_num != NULL) {
        *point_num = 0;
    }
    return ESP_OK;
}

static inline esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp,
        esp_lcd_touch_interrupt_cb_t cb)
{
    (void)tp;
    (void)cb;
    return ESP_OK;
}

static inline esp_err_t esp_lcd_touch_register_interrupt_callback_with_data(esp_lcd_touch_handle_t tp,
        esp_lcd_touch_interrupt_cb_t cb,
        void *user_data)
{
    if (tp != NULL) {
        tp->config.user_data = user_data;
    }
    (void)cb;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
