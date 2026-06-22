/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "format_demo_app.h"

#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hmi_rgb_board.h"
#include "playground_scene.h"

typedef struct {
    gfx_handle_t gfx;
    gfx_display_t *disp;
    gfx_touch_t *touch;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch_panel;
    void *panel_fbs[2];
    gfx_timer_handle_t perf_timer;
    const gfx_format_demo_app_config_t *config;
} format_demo_app_runtime_t;

static format_demo_app_runtime_t s_app;

static void format_demo_perf_timer_cb(void *user_data)
{
    format_demo_app_runtime_t *app = (format_demo_app_runtime_t *)user_data;
    gfx_display_perf_stats_t stats;
    uint32_t fps;
    uint32_t frame_ms = 0;

    if (app == NULL || app->gfx == NULL || app->disp == NULL) {
        return;
    }

    fps = gfx_timer_get_actual_fps(app->gfx);
    if (gfx_display_get_perf_stats(app->disp, &stats) == GFX_OK) {
        frame_ms = (uint32_t)((stats.frame_time_us + 500U) / 1000U);
        if (fps == 0U && stats.frame_time_us > 0U) {
            fps = (uint32_t)(1000000ULL / stats.frame_time_us);
        }
    }
    gfx_format_demo_update_perf_label(fps, frame_ms);
}

static bool format_demo_rgb_panel_frame_done(esp_lcd_panel_handle_t panel,
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

static void format_demo_flush_cb(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1,
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

static esp_err_t format_demo_board_init(format_demo_app_runtime_t *app)
{
    ESP_RETURN_ON_ERROR(hmi_rgb_board_backlight_init(), app->config->log_tag, "backlight init failed");
    hmi_rgb_board_backlight_set(false);
    ESP_RETURN_ON_ERROR(hmi_rgb_board_rgb_panel_create(&app->panel, 2), app->config->log_tag, "panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(app->panel, 2,
                        &app->panel_fbs[0],
                        &app->panel_fbs[1]),
                        app->config->log_tag, "get frame buffers failed");
    ESP_RETURN_ON_ERROR(hmi_rgb_board_rgb_panel_boot(app->panel), app->config->log_tag, "panel boot failed");

    esp_err_t touch_ret = hmi_rgb_board_touch_new(&app->touch_panel);
    if (touch_ret != ESP_OK) {
        ESP_LOGW(app->config->log_tag, "touch init failed: %s", esp_err_to_name(touch_ret));
        app->touch_panel = NULL;
    }
    hmi_rgb_board_backlight_set(true);
    return ESP_OK;
}

static esp_err_t format_demo_gfx_init(format_demo_app_runtime_t *app)
{
    app->gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    ESP_RETURN_ON_FALSE(app->gfx != NULL, ESP_FAIL, app->config->log_tag, "gfx core init failed");

    app->disp = gfx_display_add(app->gfx, &(gfx_display_config_t) {
        .h_res = HMI_RGB_LCD_H_RES,
        .v_res = HMI_RGB_LCD_V_RES,
        .color_format = app->config->color_format,
        .flush_cb = format_demo_flush_cb,
        .user_data = app->panel,
        .flags = {
            .full_frame = true,
        },
        .buffers = {
            .buf1 = app->panel_fbs[0],
            .buf2 = app->panel_fbs[1],
            .buf_pixels = HMI_RGB_LCD_H_RES * HMI_RGB_LCD_V_RES,
        },
    });
    ESP_RETURN_ON_FALSE(app->disp != NULL, ESP_FAIL, app->config->log_tag, "display add failed");

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = format_demo_rgb_panel_frame_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(app->panel, &cbs, app->disp),
                        app->config->log_tag, "register panel callback failed");

    if (app->touch_panel != NULL) {
        app->touch = gfx_touch_add(app->gfx, &(gfx_touch_config_t) {
            .driver_handle = app->touch_panel,
            .disp = app->disp,
            .poll_ms = 30,
        });
        ESP_RETURN_ON_FALSE(app->touch != NULL, ESP_FAIL, app->config->log_tag, "touch add failed");
    }
    return ESP_OK;
}

esp_err_t gfx_format_demo_app_run(const gfx_format_demo_app_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, "format_demo", "config is null");
    ESP_RETURN_ON_FALSE(config->log_tag != NULL, ESP_ERR_INVALID_ARG, "format_demo", "log tag is null");

    ESP_LOGI(config->log_tag, "start format demo");
    s_app = (format_demo_app_runtime_t) {
        .config = config,
    };

    ESP_RETURN_ON_ERROR(format_demo_board_init(&s_app), config->log_tag, "board init failed");
    ESP_RETURN_ON_ERROR(format_demo_gfx_init(&s_app), config->log_tag, "gfx init failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_playground_scene(s_app.disp, config->title, config->format_tag),
                        config->log_tag, "build playground scene failed");
    s_app.perf_timer = gfx_timer_create(s_app.gfx, format_demo_perf_timer_cb, 500, &s_app);
    ESP_RETURN_ON_FALSE(s_app.perf_timer != NULL, ESP_ERR_NO_MEM, config->log_tag, "create perf timer failed");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (config->log_running) {
            ESP_LOGI(config->log_tag, "running: color_format=%d", (int)gfx_display_get_color_format(s_app.disp));
        }
    }
}
