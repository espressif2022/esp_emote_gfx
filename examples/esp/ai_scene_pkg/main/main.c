/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "gfx/backends/esp_lcd.h"
#include "gfx/base.h"
#include "gfx/fs.h"
#include "gfx/input.h"
#include "gfx/scene/gsp.h"
#include "gfx/widgets/label.h"
#include "gfx_display_port.h"
#include "hmi_rgb_board.h"

#ifndef GSP_SCENE_INC
#define GSP_SCENE_INC "inc/home_control_v4.inc"
#endif

#include GSP_SCENE_INC

#if defined(HOME_CONTROL_SCREEN_W)
#define GSP_DEMO_SCREEN_W       HOME_CONTROL_SCREEN_W
#define GSP_DEMO_SCREEN_H       HOME_CONTROL_SCREEN_H
#define GSP_DEMO_FONT_COUNT     HOME_CONTROL_FONT_COUNT
#define GSP_DEMO_FONT_DESCS     home_control_fonts
#define GSP_DEMO_SCENE_PKG      home_control_scene_pkg
#define GSP_DEMO_SCENE_PKG_LEN  home_control_scene_pkg_len
#elif defined(HOME_SCREEN_W)
#define GSP_DEMO_SCREEN_W       HOME_SCREEN_W
#define GSP_DEMO_SCREEN_H       HOME_SCREEN_H
#define GSP_DEMO_FONT_COUNT     HOME_FONT_COUNT
#define GSP_DEMO_FONT_DESCS     home_fonts
#define GSP_DEMO_SCENE_PKG      home_scene_pkg
#define GSP_DEMO_SCENE_PKG_LEN  home_scene_pkg_len
#else
#error "Unsupported GSP scene include: missing HOME_* or HOME_CONTROL_* symbols"
#endif

extern const lv_font_t font_puhui_16_4;

static const char *const TAG = "gsp_board";

typedef struct {
    gfx_display_port_t port;
    gfx_touch_t *touch;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch_panel;
    void *panel_fbs[2];
    gfx_gsp_scene_t scene;
    gfx_gsp_font_binding_t font_bindings[GSP_DEMO_FONT_COUNT];
    gfx_font_t fonts[GSP_DEMO_FONT_COUNT];
    gfx_fs_blob_t font_blobs[GSP_DEMO_FONT_COUNT];
    const char *font_blob_names[GSP_DEMO_FONT_COUNT];
    uint16_t font_blob_count;
    gfx_asset_source_t *font_fs;
    gfx_font_t default_font;
    uint32_t ok_count;
} ai_scene_pkg_app_t;

static ai_scene_pkg_app_t s_app;

static void on_action_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    ai_scene_pkg_app_t *app = (ai_scene_pkg_app_t *)user_data;

    (void)obj;
    if (app == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    app->ok_count++;
    ESP_LOGI(TAG, "GSP action release, count=%lu", (unsigned long)app->ok_count);

    gfx_object_t *title = gfx_gsp_scene_find_by_name(&app->scene, "title");
    if (title != NULL) {
        (void)gfx_label_set_text_fmt(title, "ESP GSP scene #%lu",
                                     (unsigned long)app->ok_count);
    }
}

static esp_err_t board_init(ai_scene_pkg_app_t *app)
{
    ESP_RETURN_ON_ERROR(hmi_rgb_board_backlight_init(), TAG, "backlight init failed");
    hmi_rgb_board_backlight_set(false);

    ESP_RETURN_ON_ERROR(hmi_rgb_board_rgb_panel_create(&app->panel, 2),
                        TAG, "panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(app->panel, 2,
                        &app->panel_fbs[0], &app->panel_fbs[1]),
                        TAG, "get frame buffers failed");
    ESP_RETURN_ON_ERROR(hmi_rgb_board_rgb_panel_boot(app->panel), TAG, "panel boot failed");

    esp_err_t touch_ret = hmi_rgb_board_touch_new(&app->touch_panel);
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(touch_ret));
        app->touch_panel = NULL;
    }

    hmi_rgb_board_backlight_set(true);
    return ESP_OK;
}

static esp_err_t gfx_init(ai_scene_pkg_app_t *app)
{
    gfx_backend_esp_lcd_config_t backend_cfg = {
        .panel = app->panel,
        .panel_io = NULL,
        .interface = GFX_BACKEND_ESP_LCD_IF_RGB,
        .panel_fb_count = 2,
        .panel_fb = {
            app->panel_fbs[0],
            app->panel_fbs[1],
        },
    };
    gfx_backend_t *backend = gfx_backend_esp_lcd_create(&backend_cfg);
    ESP_RETURN_ON_FALSE(backend != NULL, ESP_FAIL, TAG, "display backend create failed");

    gfx_display_port_config_t port_cfg = {
        .h_res = HMI_RGB_LCD_H_RES,
        .v_res = HMI_RGB_LCD_V_RES,
        .fps = 30,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_EXTERNAL,
        .backend = backend,
        .runtime = {
            .core = {
                .task = {
                    .task_priority = 4,
                    .task_stack = 24 * 1024,
                    .task_affinity = -1,
                    .task_stack_caps = GFX_CORE_TASK_STACK_CAP_DEFAULT,
                },
            },
        },
        .display = {
            .buf1 = app->panel_fbs[0],
            .buf2 = app->panel_fbs[1],
            .buf_pixels = HMI_RGB_LCD_H_RES * HMI_RGB_LCD_V_RES,
        },
    };

    gfx_err_t err = gfx_display_port_open(&port_cfg, &app->port);
    if (err != GFX_OK) {
        gfx_backend_esp_lcd_delete(backend);
        ESP_LOGE(TAG, "display port open failed: %d", (int)err);
        return ESP_FAIL;
    }

    if (app->touch_panel != NULL) {
        app->touch = gfx_touch_add(app->port.gfx, &(gfx_touch_config_t) {
            .driver_handle = app->touch_panel,
            .disp = app->port.disp,
            .poll_ms = 30,
        });
        ESP_RETURN_ON_FALSE(app->touch != NULL, ESP_FAIL, TAG, "touch add failed");
    }
    return ESP_OK;
}

static const char *path_basename(const char *path)
{
    const char *base;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    base = strrchr(path, '/');
    if (base != NULL) {
        return base + 1;
    }

    return path;
}

static gfx_err_t load_font_blob(ai_scene_pkg_app_t *app, const char *name,
                                gfx_fs_blob_t **out_blob)
{
    char mounted_name[96];

    if (app == NULL || name == NULL || out_blob == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    for (uint16_t i = 0; i < app->font_blob_count; i++) {
        if (app->font_blob_names[i] != NULL &&
                strcmp(app->font_blob_names[i], name) == 0) {
            *out_blob = &app->font_blobs[i];
            return GFX_OK;
        }
    }

    if (app->font_blob_count >= GSP_DEMO_FONT_COUNT) {
        return GFX_ERR_NO_MEM;
    }

    (void)snprintf(mounted_name, sizeof(mounted_name), "/fonts/%s", name);
    gfx_err_t err = gfx_fs_load(mounted_name, &app->font_blobs[app->font_blob_count]);
    if (err != GFX_OK || app->font_blobs[app->font_blob_count].data == NULL) {
        return err != GFX_OK ? err : GFX_ERR_NOT_FOUND;
    }

    app->font_blob_names[app->font_blob_count] = name;
    *out_blob = &app->font_blobs[app->font_blob_count];
    app->font_blob_count++;
    return GFX_OK;
}

static gfx_font_t create_freetype_font(ai_scene_pkg_app_t *app,
                                       const gfx_gsp_font_desc_t *desc,
                                       const char **out_asset_name)
{
    static const char *const fallback_font_name = "KaiTi.ttf";
    const char *candidates[3] = {
        path_basename(desc != NULL ? desc->path : NULL),
        desc != NULL ? desc->family : NULL,
        fallback_font_name,
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        const char *asset_name = candidates[i];
        gfx_fs_blob_t *blob = NULL;

        if (asset_name == NULL || asset_name[0] == '\0') {
            continue;
        }
        if (strchr(asset_name, '.') == NULL) {
            continue;
        }

        gfx_err_t err = load_font_blob(app, asset_name, &blob);
        if (err != GFX_OK || blob == NULL) {
            continue;
        }

        gfx_font_t font = NULL;
        const gfx_label_cfg_t cfg = {
            .name = asset_name,
            .mem = blob->data,
            .mem_size = blob->size,
            .font_size = desc != NULL ? desc->size_px : 16,
        };

        err = gfx_label_font_create(&cfg, &font);
        if (err == GFX_OK && font != NULL) {
            if (out_asset_name != NULL) {
                *out_asset_name = asset_name;
            }
            return font;
        }

        ESP_LOGW(TAG, "create FreeType font failed: %s size=%u err=%d",
                 asset_name,
                 (unsigned)(desc != NULL ? desc->size_px : 16),
                 (int)err);
    }

    return NULL;
}

static void init_font_assets(ai_scene_pkg_app_t *app)
{
    gfx_err_t err = gfx_fs_open_partition("fonts", &app->font_fs);
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "font partition open failed: %d; fallback to font_puhui_16_4",
                 (int)err);
        return;
    }

    err = gfx_fs_mount("/fonts", app->font_fs);
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "font partition mount failed: %d; fallback to font_puhui_16_4",
                 (int)err);
        gfx_fs_close(app->font_fs);
        app->font_fs = NULL;
        return;
    }
}

static void init_font_bindings(ai_scene_pkg_app_t *app)
{
    for (uint16_t i = 0; i < GSP_DEMO_FONT_COUNT; i++) {
        const gfx_gsp_font_desc_t *desc = &GSP_DEMO_FONT_DESCS[i];
        const char *asset_name = NULL;
        gfx_font_t font = (gfx_font_t)&font_puhui_16_4;

        if (app->font_fs != NULL) {
            gfx_font_t ft_font = create_freetype_font(app, desc, &asset_name);
            if (ft_font != NULL) {
                font = ft_font;
                app->fonts[i] = ft_font;
                if (app->default_font == NULL) {
                    app->default_font = ft_font;
                }
            }
        }

        app->font_bindings[i].id = desc->id;
        app->font_bindings[i].font = font;
        if (asset_name != NULL) {
            ESP_LOGI(TAG, "font_id=%u -> FreeType %s (%upx, requested %s)",
                     (unsigned)desc->id,
                     asset_name,
                     (unsigned)desc->size_px,
                     desc->family != NULL ? desc->family : "-");
        } else {
            ESP_LOGW(TAG, "font_id=%u -> font_puhui_16_4 fallback (requested %s, %upx)",
                     (unsigned)desc->id,
                     desc->family != NULL ? desc->family : "-",
                     (unsigned)desc->size_px);
        }
    }
}

static esp_err_t scene_load(ai_scene_pkg_app_t *app)
{
    static const gfx_gsp_cb_binding_t callbacks[] = {
        { .name = "on_ok", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_ac_power", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_ac_temp_up", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_ac_temp_down", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_humid_power", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_humid_up", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_humid_down", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_purify_power", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_more_devices", .cb = on_action_cb, .user_data = &s_app },
        { .name = "on_more_info", .cb = on_action_cb, .user_data = &s_app },
    };

    init_font_assets(app);
    init_font_bindings(app);
    ESP_LOGI(TAG, "load %s: screen=%ux%u bytes=%u fonts=%u",
             GSP_SCENE_INC,
             (unsigned)GSP_DEMO_SCREEN_W,
             (unsigned)GSP_DEMO_SCREEN_H,
             (unsigned)GSP_DEMO_SCENE_PKG_LEN,
             (unsigned)GSP_DEMO_FONT_COUNT);

    ESP_RETURN_ON_FALSE(gfx_core_lock(app->port.gfx) == GFX_OK,
                        ESP_FAIL, TAG, "gfx lock failed");

    int rc = gfx_gsp_load_with_fonts(GSP_DEMO_SCENE_PKG, GSP_DEMO_SCENE_PKG_LEN,
                                     app->port.disp,
                                     app->font_bindings,
                                     GSP_DEMO_FONT_COUNT,
                                     app->default_font != NULL ?
                                     app->default_font : (gfx_font_t)&font_puhui_16_4,
                                     callbacks,
                                     sizeof(callbacks) / sizeof(callbacks[0]),
                                     &app->scene);
    if (rc != GFX_GSP_OK) {
        (void)gfx_core_unlock(app->port.gfx);
        ESP_LOGE(TAG, "gfx_gsp_load failed: %d", rc);
        return ESP_FAIL;
    }

    gfx_display_refresh_all(app->port.disp);
    (void)gfx_core_unlock(app->port.gfx);
    (void)gfx_core_refresh_now(app->port.gfx);

    ESP_LOGI(TAG, "scene loaded: objects=%u blobs=%u actions=%u",
             (unsigned)app->scene.obj_count,
             (unsigned)app->scene.blob_count,
             (unsigned)app->scene.action_count);
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "start ESP ai_scene_pkg legacy GSP object-loader");
    ESP_LOGI(TAG, "scene include: %s", GSP_SCENE_INC);

    ESP_ERROR_CHECK(board_init(&s_app));
    ESP_ERROR_CHECK(gfx_init(&s_app));
    ESP_ERROR_CHECK(scene_load(&s_app));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
