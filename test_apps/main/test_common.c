/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/display.h"
#include "driver/spi_common.h"
#include "bsp/esp-bsp.h"
#include "test_common.h"

static const char *TAG = "test_common";

/* Shared global variables */
gfx_handle_t emote_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
gfx_obj_t *label_tips = NULL;

static bool flush_io_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    gfx_handle_t emote_handle = (gfx_handle_t)user_ctx;
    if (emote_handle) {
        gfx_emote_flush_ready(emote_handle, true);
    }
    return true;
}

static void flush_callback(gfx_handle_t emote_handle, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_emote_get_user_data(emote_handle);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
}

void test_clock_tm_callback(void *user_data)
{
    gfx_obj_t *label_obj = (gfx_obj_t *)user_data;
    ESP_LOGI(TAG, "FPS: %d*%d: %" PRIu32, BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    if (label_obj) {
        gfx_label_set_text_fmt(label_obj, "%d*%d: %d", BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    }
}

esp_err_t test_load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc)
{
    if (img_dsc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const void *img_data = mmap_assets_get_mem(assets_handle, asset_id);
    if (img_data == NULL) {
        return ESP_FAIL;
    }

    size_t img_size = mmap_assets_get_size(assets_handle, asset_id);
    if (img_size < sizeof(gfx_image_header_t)) {
        return ESP_FAIL;
    }

    // Copy header from the beginning of the data
    memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));

    // Set data pointer after the header
    img_dsc->data = (const uint8_t *)img_data + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);

    return ESP_OK;
}

esp_err_t test_init_display_and_graphics(const char *partition_label, uint32_t max_files, uint32_t checksum, mmap_assets_handle_t *assets_handle)
{
    const mmap_assets_config_t asset_config = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    esp_err_t ret = mmap_assets_new(&asset_config, assets_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize assets");
        return ret;
    }

    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 100) * sizeof(uint16_t),
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    bsp_display_backlight_on();

    gfx_core_config_t gfx_cfg = {
        .flush_cb = flush_callback,
        .update_cb = NULL,
        .user_data = panel_handle,
        .flags = {.swap = true, .double_buffer = true},
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .fps = 30,
        .buffers = {.buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16},
        .task = GFX_EMOTE_INIT_CONFIG()
    };
    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 7;
    gfx_cfg.task.task_stack = 20 * 1024;

    emote_handle = gfx_emote_init(&gfx_cfg);
    if (emote_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize graphics system");
        mmap_assets_del(*assets_handle);
        return ESP_FAIL;
    }

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = flush_io_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, emote_handle);

    return ESP_OK;
}

void test_cleanup_display_and_graphics(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Cleanup display and graphics ===");
    if (emote_handle != NULL) {
        gfx_emote_deinit(emote_handle);
        emote_handle = NULL;
    }
    if (assets_handle != NULL) {
        mmap_assets_del(assets_handle);
    }

    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);

    vTaskDelay(pdMS_TO_TICKS(1000));
}
