/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/display.h"
#include "driver/spi_common.h"

#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "common.h"

static const char *TAG = "common";

/* Shared globals (declared in common.h) */
gfx_handle_t emote_handle = NULL;
gfx_disp_t *disp_default = NULL;
gfx_touch_t *touch_default = NULL;

esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

static esp_lcd_touch_handle_t touch_handle = NULL;   // LCD touch handle

#if CONFIG_IDF_TARGET_ESP32S3
static bool flush_io_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    gfx_disp_t *disp = (gfx_disp_t *)user_ctx;
    if (disp) {
        gfx_disp_flush_ready(disp, true);
    }
    return true;
}
#elif CONFIG_IDF_TARGET_ESP32P4
static bool flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io,
        esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    gfx_disp_t *disp = (gfx_disp_t *)user_ctx;
    if (disp) {
        gfx_disp_flush_ready(disp, true);
    }
    return true;
}
#endif

static void disp_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
}

static void touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        ESP_LOGI(TAG, "touch press  : %p, (%d, %d)", touch, event->x, event->y);
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        ESP_LOGI(TAG, "touch release: %p, (%d, %d)", touch, event->x, event->y);
        break;
    default:
        break;
    }
}

void clock_tm_callback(void *user_data)
{
    gfx_obj_t *label_obj = (gfx_obj_t *)user_data;
    ESP_LOGI(TAG, "FPS: %d*%d: %" PRIu32, BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    if (label_obj) {
        gfx_label_set_text_fmt(label_obj, "%d*%d: %d", BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    }
}

esp_err_t load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc)
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

esp_err_t display_and_graphics_init(const char *partition_label, uint32_t max_files, uint32_t checksum, mmap_assets_handle_t *assets_handle)
{
    esp_err_t ret = ESP_OK;

    const mmap_assets_config_t asset_config = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {.mmap_enable = true, .full_check = true}
    };
    ret = mmap_assets_new(&asset_config, assets_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize assets");
#if CONFIG_IDF_TARGET_ESP32S3
    /* Initialize display and panel */
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 100) * sizeof(uint16_t),
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
#elif CONFIG_IDF_TARGET_ESP32P4
    const bsp_display_config_t bsp_disp_cfg = {
        .hdmi_resolution = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .dsi_bus = {
            .phy_clk_src = 0,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        },
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);
#endif
    bsp_display_backlight_on();

    /* Initialize touch */
    bsp_i2c_init();
    bsp_touch_new(NULL, &touch_handle);
    ESP_GOTO_ON_FALSE(touch_handle != NULL, ESP_FAIL, err_assets, TAG, "Failed to initialize touch");

    /* Initialize graphics system */
    gfx_core_config_t gfx_cfg = {
        .fps = 30,
        .task = GFX_EMOTE_INIT_CONFIG()
    };
    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 7;
    gfx_cfg.task.task_stack = 20 * 1024;
    emote_handle = gfx_emote_init(&gfx_cfg);
    ESP_GOTO_ON_FALSE(emote_handle != NULL, ESP_FAIL, err_assets, TAG, "Failed to initialize graphics system");

    /* Add default display */
    gfx_disp_config_t disp_cfg = {
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .flush_cb = disp_flush_callback,
        .update_cb = NULL,
        .user_data = (void *)panel_handle,
#if CONFIG_IDF_TARGET_ESP32S3
        .flags = { .swap = true, .buff_dma = true, .buff_spiram = false, .double_buffer = true },
#elif CONFIG_IDF_TARGET_ESP32P4
        .flags = { .swap = false, .buff_dma = true, .buff_spiram = false, .double_buffer = true },
#endif
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16 },
    };
    disp_default = gfx_disp_add(emote_handle, &disp_cfg);
    ESP_GOTO_ON_FALSE(disp_default != NULL, ESP_FAIL, err_gfx, TAG, "Failed to add display");

#if CONFIG_IDF_TARGET_ESP32S3
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = flush_io_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp_default);
#elif CONFIG_IDF_TARGET_ESP32P4
    esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
    cbs.on_color_trans_done = flush_dpi_panel_ready_callback;
    esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, disp_default);
#endif
    /* Add touch */
    gfx_touch_config_t touch_cfg = {
        .handle = touch_handle,
        .event_cb = touch_event_cb,
        .disp = disp_default,
        .poll_ms = 50,
        .user_data = emote_handle,
    };
    touch_default = gfx_touch_add(emote_handle, &touch_cfg);
    ESP_GOTO_ON_FALSE(touch_default != NULL, ESP_FAIL, err_gfx, TAG, "Failed to add touch");

    return ESP_OK;

err_gfx:
    if (emote_handle != NULL) {
        gfx_emote_deinit(emote_handle);
        emote_handle = NULL;
        disp_default = NULL;
        touch_default = NULL;
    }
err_assets:
    mmap_assets_del(*assets_handle);
    return ret;
}

void display_and_graphics_clean(mmap_assets_handle_t assets_handle)
{
    if (emote_handle != NULL) {
        gfx_emote_deinit(emote_handle);
        emote_handle = NULL;
        disp_default = NULL;
        touch_default = NULL;
    }
    if (assets_handle != NULL) {
        mmap_assets_del(assets_handle);
    }
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

    /*[lack mem] can't delete tp_io_handle here( create by esp_lcd_new_panel_io_i2c) */
#elif CONFIG_IDF_TARGET_ESP32P4
    bsp_display_delete();
    bsp_touch_delete();
#endif
    if (touch_handle != NULL) {
        esp_lcd_touch_del(touch_handle);
        touch_handle = NULL;
    }
    bsp_i2c_deinit();

    vTaskDelay(pdMS_TO_TICKS(1000));
}
