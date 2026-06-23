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
#include "gfx_display_port.h"
#include "gfx/backends/esp_lcd_rgb.h"
#include "playground_scene.h"

typedef struct {
    gfx_display_port_t port;
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

    if (app == NULL || app->port.gfx == NULL || app->port.disp == NULL) {
        return;
    }

    fps = gfx_timer_get_actual_fps(app->port.gfx);
    if (gfx_display_get_perf_stats(app->port.disp, &stats) == GFX_OK) {
        frame_ms = (uint32_t)((stats.frame_time_us + 500U) / 1000U);
        if (fps == 0U && stats.frame_time_us > 0U) {
            fps = (uint32_t)(1000000ULL / stats.frame_time_us);
        }
    }
    gfx_format_demo_update_perf_label(fps, frame_ms);
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
    gfx_backend_t *backend;
    gfx_err_t err;

    backend = gfx_backend_esp_lcd_rgb_create(&(gfx_backend_esp_lcd_rgb_config_t) {
        .panel = app->panel,
    });
    ESP_RETURN_ON_FALSE(backend != NULL, ESP_FAIL, app->config->log_tag, "display backend create failed");

    err = gfx_display_port_open(&(gfx_display_port_config_t) {
        .h_res = HMI_RGB_LCD_H_RES,
        .v_res = HMI_RGB_LCD_V_RES,
        .fps = 30,
        .color_format = app->config->color_format,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_EXTERNAL,
        .backend = backend,
        .runtime = {
            .core = {
                .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
            },
        },
        .display = {
            .full_frame = true,
            .buf1 = app->panel_fbs[0],
            .buf2 = app->panel_fbs[1],
            .buf_pixels = HMI_RGB_LCD_H_RES * HMI_RGB_LCD_V_RES,
        },
    }, &app->port);
    if (err != GFX_OK) {
        gfx_backend_esp_lcd_rgb_delete(backend);
        ESP_LOGE(app->config->log_tag, "display port open failed: %d", (int)err);
        return ESP_FAIL;
    }

    if (app->touch_panel != NULL) {
        app->touch = gfx_touch_add(app->port.gfx, &(gfx_touch_config_t) {
            .driver_handle = app->touch_panel,
            .disp = app->port.disp,
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
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_playground_scene(s_app.port.disp, config->title, config->format_tag),
                        config->log_tag, "build playground scene failed");
    s_app.perf_timer = gfx_timer_create(s_app.port.gfx, format_demo_perf_timer_cb, 500, &s_app);
    ESP_RETURN_ON_FALSE(s_app.perf_timer != NULL, ESP_ERR_NO_MEM, config->log_tag, "create perf timer failed");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (config->log_running) {
            ESP_LOGI(config->log_tag, "running: color_format=%d", (int)gfx_display_get_color_format(s_app.port.disp));
        }
    }
}
