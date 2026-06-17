/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "hmi_rgb_board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hmi_rgb_board_backlight_init(void);
void hmi_rgb_board_backlight_set(bool on);
esp_err_t hmi_rgb_board_rgb_panel_create(esp_lcd_panel_handle_t *out_panel, uint8_t num_fbs);
esp_err_t hmi_rgb_board_rgb_panel_boot(esp_lcd_panel_handle_t panel);
esp_err_t hmi_rgb_board_touch_new(esp_lcd_touch_handle_t *out_touch);

#ifdef __cplusplus
}
#endif
