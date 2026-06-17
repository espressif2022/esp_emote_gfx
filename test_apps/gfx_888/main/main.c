/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx.h"
#include "playground_scene.h"
#include "hmi_rgb_board.h"

static const char *const TAG = "gfx888";

static gfx_handle_t s_gfx;
static gfx_display_t *s_disp;
static gfx_touch_t *s_touch;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch_panel;
static void *s_panel_fbs[2];

static bool rgb_panel_frame_done(esp_lcd_panel_handle_t panel,
                                 const esp_lcd_rgb_panel_event_data_t *edata,
                                 void *user_ctx)
{
    (void)panel;
    (void)edata;
    gfx_display_t *disp = (gfx_display_t *)user_ctx;
    if (disp != NULL) {
        gfx_display_flush_ready(disp, true);
    }
    return false;
}

static void gfx_flush_cb(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1,
                         gfx_coord_t x2, gfx_coord_t y2, const void *data)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;

    if (!gfx_display_is_flushing_last(disp)) {
        gfx_display_flush_ready(disp, false);
        return;
    }

    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, 0, 0,
                              gfx_display_get_h_res(disp),
                              gfx_display_get_v_res(disp),
                              data);
}

void app_main(void)
{
    ESP_LOGI(TAG, "start gfx_888");

    ESP_ERROR_CHECK(hmi_rgb_board_backlight_init());
    hmi_rgb_board_backlight_set(false);
    ESP_ERROR_CHECK(hmi_rgb_board_rgb_panel_create(&s_panel, 2));
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &s_panel_fbs[0], &s_panel_fbs[1]));
    ESP_ERROR_CHECK(hmi_rgb_board_rgb_panel_boot(s_panel));
    esp_err_t touch_ret = hmi_rgb_board_touch_new(&s_touch_panel);
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(touch_ret));
        s_touch_panel = NULL;
    }
    hmi_rgb_board_backlight_set(true);

    s_gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    ESP_ERROR_CHECK(s_gfx != NULL ? ESP_OK : ESP_FAIL);

    s_disp = gfx_display_add(s_gfx, &(gfx_display_config_t) {
        .h_res = HMI_RGB_LCD_H_RES,
        .v_res = HMI_RGB_LCD_V_RES,
        .color_format = GFX_COLOR_FORMAT_BGR888,
        .flush_cb = gfx_flush_cb,
        .user_data = s_panel,
        .flags = {
            .full_frame = true,
        },
        .buffers = {
            .buf1 = s_panel_fbs[0],
            .buf2 = s_panel_fbs[1],
            .buf_pixels = HMI_RGB_LCD_H_RES * HMI_RGB_LCD_V_RES,
        },
    });
    ESP_ERROR_CHECK(s_disp != NULL ? ESP_OK : ESP_FAIL);

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = rgb_panel_frame_done,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(s_panel, &cbs, s_disp));

    if (s_touch_panel != NULL) {
        s_touch = gfx_touch_add(s_gfx, &(gfx_touch_config_t) {
            .driver_handle = s_touch_panel,
            .disp = s_disp,
            .poll_ms = 30,
        });
        ESP_ERROR_CHECK(s_touch != NULL ? ESP_OK : ESP_FAIL);
    }

    ESP_ERROR_CHECK(gfx_format_demo_build_playground_scene(s_disp, "GFX BGR888 Playground", "BGR888"));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "running: color_format=%d", (int)gfx_display_get_color_format(s_disp));
    }
}
