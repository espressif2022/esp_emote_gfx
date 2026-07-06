/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "driver/spi_common.h"

#include "test_board.h"

static const char *const TAG = "test_board";

esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;

gfx_backend_esp_lcd_interface_t test_board_lcd_interface(void)
{
#if CONFIG_IDF_TARGET_ESP32P4
    return GFX_BACKEND_ESP_LCD_IF_MIPI_DPI;
#else
    return GFX_BACKEND_ESP_LCD_IF_PANEL_IO;
#endif
}

esp_lcd_touch_handle_t test_board_touch(void)
{
    return s_touch_handle;
}

esp_err_t test_board_init(void)
{
#if CONFIG_IDF_TARGET_ESP32S3
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 100) * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle), TAG, "display init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG, "panel on failed");
#elif CONFIG_IDF_TARGET_ESP32P4
    const bsp_display_config_t bsp_disp_cfg = {
        .hdmi_resolution = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .dsi_bus = {
            .phy_clk_src = 0,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        },
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle), TAG, "display init failed");
#endif
    bsp_display_backlight_on();

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c init failed");
    ESP_RETURN_ON_ERROR(bsp_touch_new(NULL, &s_touch_handle), TAG, "touch init failed");
    ESP_RETURN_ON_FALSE(s_touch_handle != NULL, ESP_FAIL, TAG, "touch handle is NULL");

    return ESP_OK;
}

void test_board_deinit(void)
{
#if CONFIG_IDF_TARGET_ESP32S3
    if (panel_handle != NULL) {
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
    if (io_handle != NULL) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }
    spi_bus_free(BSP_LCD_SPI_NUM);

    /*[lack mem] can't delete tp_io_handle here (created by esp_lcd_new_panel_io_i2c) */
#elif CONFIG_IDF_TARGET_ESP32P4
    bsp_display_delete();
    bsp_touch_delete();
#endif
    if (s_touch_handle != NULL) {
        esp_lcd_touch_del(s_touch_handle);
        s_touch_handle = NULL;
    }
    bsp_i2c_deinit();
}
