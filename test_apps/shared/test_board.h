/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "gfx/backends/esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

/** @brief LCD interface type for the current BSP target. */
gfx_backend_esp_lcd_interface_t test_board_lcd_interface(void);

/** @brief Touch driver handle created by test_board_init(). */
esp_lcd_touch_handle_t test_board_touch(void);

esp_err_t test_board_init(void);
void test_board_deinit(void);

#ifdef __cplusplus
}
#endif
