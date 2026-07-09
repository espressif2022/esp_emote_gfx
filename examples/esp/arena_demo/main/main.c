/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP arena formal-path demo
 * --------------------------
 * Default: hand-packed ARN demo scene (arena_demo_pack).
 * Optional:
 *   ARENA_DEMO_FROM_GSP=1  — GSP home.inc → ARN
 *   ARENA_DEMO_COMPARE=1   — A/B timing: arena_draw_clipped vs object bridge
 *   ARENA_DEMO_SWEEP=1     — dual-board visual: row-by-row recolor loop
 *     -DARENA_DEMO_PATH=A  — formal arena (board 1)
 *     -DARENA_DEMO_PATH=B  — object bridge (board 2)
 *     (no artificial delay — refresh as fast as the path allows)
 *
 * Dual-board visual (same scene, side-by-side):
 *   # board 1 — arena
 *   idf.py -B build-a -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=A reconfigure build flash
 *   # board 2 — object
 *   idf.py -B build-b -DARENA_DEMO_SWEEP=1 -DARENA_DEMO_PATH=B reconfigure build flash
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "arena_demo_scene.h"
#include "gfx/backends/esp_lcd.h"
#include "gfx/base.h"
#include "gfx/input.h"
#include "gfx/scene/arena.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx_display_port.h"
#include "hmi_rgb_board.h"

#ifndef ARENA_DEMO_FROM_GSP
#define ARENA_DEMO_FROM_GSP 0
#endif
#ifndef ARENA_DEMO_COMPARE
#define ARENA_DEMO_COMPARE 0
#endif
#ifndef ARENA_DEMO_SWEEP
#define ARENA_DEMO_SWEEP 0
#endif
#ifndef ARENA_DEMO_PATH_IS_OBJECT
#define ARENA_DEMO_PATH_IS_OBJECT 0
#endif
#ifndef ARENA_COMPARE_ITERS
#define ARENA_COMPARE_ITERS 30
#endif
#if ARENA_DEMO_COMPARE || ARENA_DEMO_SWEEP
#include "arena_compare_run.h"
#include "arena_gfx_bridge.h"
#endif

#if ARENA_DEMO_FROM_GSP && !ARENA_DEMO_COMPARE && !ARENA_DEMO_SWEEP
#ifndef GSP_SCENE_INC
#define GSP_SCENE_INC "inc/home.inc"
#endif
#include GSP_SCENE_INC
#include "gfx/scene/gsp_to_arena.h"
#endif

extern const lv_font_t font_puhui_16_4;

static const char *const TAG = "arena_demo";

typedef struct {
    gfx_display_port_t port;
    gfx_touch_t *touch;
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch_panel;
    void *panel_fbs[2];
    gfx_arena_scene_t scene;
#if ARENA_DEMO_SWEEP
    arena_gfx_scene_t obj_scene;
    int sweep_use_object;
    int sweep_row;
    uint32_t sweep_tick;
#endif
    uint8_t *pkg;
    size_t pkg_size;
    uint32_t ok_count;
} arena_demo_app_t;

static arena_demo_app_t s_app;

static esp_err_t board_init(arena_demo_app_t *app)
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

static esp_err_t gfx_init(arena_demo_app_t *app)
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
                    .task_stack = 16 * 1024,
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

#if !ARENA_DEMO_COMPARE && !ARENA_DEMO_SWEEP
    if (app->touch_panel != NULL) {
        app->touch = gfx_touch_add(app->port.gfx, &(gfx_touch_config_t) {
            .driver_handle = app->touch_panel,
            .disp = app->port.disp,
            .poll_ms = 30,
        });
        ESP_RETURN_ON_FALSE(app->touch != NULL, ESP_FAIL, TAG, "touch add failed");
    }
#else
    (void)app->touch_panel;
#endif
    return ESP_OK;
}

#if ARENA_DEMO_SWEEP
static esp_err_t scene_load(arena_demo_app_t *app)
{
    app->sweep_use_object = ARENA_DEMO_PATH_IS_OBJECT;
    app->sweep_row = 0;
    app->sweep_tick = 0;

    uint16_t nodes = 0;
    app->pkg = arena_compare_pack_grid(HMI_RGB_LCD_H_RES, HMI_RGB_LCD_V_RES,
                                       &app->pkg_size, &nodes);
    ESP_RETURN_ON_FALSE(app->pkg != NULL, ESP_ERR_NO_MEM, TAG, "pack grid failed");

    ESP_LOGI(TAG, "row-sweep visual: path=%s  %ux%u  nodes=%u  (max rate)",
             app->sweep_use_object ? "B object bridge" : "A formal arena",
             (unsigned)HMI_RGB_LCD_H_RES, (unsigned)HMI_RGB_LCD_V_RES,
             (unsigned)nodes);

    ESP_RETURN_ON_FALSE(gfx_core_lock(app->port.gfx) == GFX_OK, ESP_FAIL, TAG, "gfx lock failed");

    if (app->sweep_use_object) {
        if (arena_gfx_bind(app->pkg, app->pkg_size, app->port.disp,
                           (gfx_font_t)&font_puhui_16_4, &app->obj_scene) != 0) {
            (void)gfx_core_unlock(app->port.gfx);
            ESP_LOGE(TAG, "arena_gfx_bind failed");
            return ESP_FAIL;
        }
        gfx_display_refresh_all(app->port.disp);
    } else {
        arena_t arena = {0};
        if (arena_load(app->pkg, app->pkg_size, &arena) != 0 ||
                arena_scene_attach(app->port.disp, &arena, &app->scene) != 0) {
            (void)gfx_core_unlock(app->port.gfx);
            arena_free(&arena);
            ESP_LOGE(TAG, "arena load/attach failed");
            return ESP_FAIL;
        }
        arena_scene_set_font(&app->scene, (gfx_font_t)&font_puhui_16_4);
        (void)arena_scene_mark_dirty_all(&app->scene);
    }

    (void)gfx_core_unlock(app->port.gfx);
    (void)gfx_core_refresh_now(app->port.gfx);
    ESP_LOGI(TAG, "sweep running — place boards side by side");
    return ESP_OK;
}

static void sweep_step(arena_demo_app_t *app)
{
    const int row = app->sweep_row;
    const uint32_t color = 0xff0040u + ((app->sweep_tick * 0x030507u) & 0x00ffffu);

    if (app->sweep_use_object) {
        arena_compare_recolor_row(&app->obj_scene.arena, row, color);
        const arena_hdr_t *hdr = arena_hdr(&app->obj_scene.arena);
        const uint16_t first =
            (uint16_t)(1u + (uint16_t)ARENA_COMPARE_GRID_ROWS +
                       (uint16_t)row * (uint16_t)ARENA_COMPARE_GRID_COLS);
        for (int c = 0; c < ARENA_COMPARE_GRID_COLS; c++) {
            const uint32_t off =
                hdr->nodes_off +
                (uint32_t)(first + (uint16_t)c) * (uint32_t)sizeof(arena_node_t);
            (void)arena_gfx_sync_node(&app->obj_scene, off);
        }
    } else {
        arena_compare_recolor_row(&app->scene.arena, row, color);
        (void)arena_scene_mark_dirty(
            &app->scene, arena_compare_row_container_off(&app->scene.arena, row));
    }

    (void)gfx_core_refresh_now(app->port.gfx);

    app->sweep_row++;
    if (app->sweep_row >= ARENA_COMPARE_GRID_ROWS) {
        app->sweep_row = 0;
        app->sweep_tick++;
    }
}

#elif ARENA_DEMO_COMPARE
static void log_avg(const char *label, int iters, int64_t us)
{
    ESP_LOGI(TAG, "  %-14s  total=%lld us  avg=%.1f us",
             label, (long long)us, (double)us / (double)iters);
}

static void log_ratio(const char *label, int64_t arena_us, int64_t object_us)
{
    if (arena_us <= 0) {
        ESP_LOGI(TAG, "  %-14s  n/a", label);
        return;
    }
    ESP_LOGI(TAG, "  %-14s  object/arena = %.2fx  (>1 ⇒ arena faster)",
             label, (double)object_us / (double)arena_us);
}

static void log_path(const char *title, const char *load_name,
                     const arena_compare_path_stats_t *s, int iters)
{
    ESP_LOGI(TAG, "=== %s ===", title);
    log_avg(load_name, iters, s->load_us);
    log_avg("full wall", iters, s->full_wall_us);
    log_avg("full render", iters, s->full_render_us);
    log_avg("full flush", iters, s->full_flush_us);
    log_avg("dirty wall", iters, s->dirty_wall_us);
    log_avg("dirty render", iters, s->dirty_render_us);
    log_avg("dirty flush", iters, s->dirty_flush_us);
}

static esp_err_t scene_load(arena_demo_app_t *app)
{
    const int iters = ARENA_COMPARE_ITERS;
    ESP_LOGI(TAG, "A/B compare on device: %ux%u, iters=%d",
             (unsigned)HMI_RGB_LCD_H_RES, (unsigned)HMI_RGB_LCD_V_RES, iters);
    ESP_LOGI(TAG, "A=arena_draw_clipped  B=object bridge (draw_child_objects)");
    ESP_LOGI(TAG, "wall=refresh_now; render/flush from display perf stats");

    arena_compare_result_t r = {0};
    ESP_RETURN_ON_FALSE(gfx_core_lock(app->port.gfx) == GFX_OK, ESP_FAIL, TAG, "gfx lock failed");
    int rc = arena_compare_run(app->port.gfx, app->port.disp,
                               (gfx_font_t)&font_puhui_16_4,
    &(arena_compare_config_t) {
        .screen_w = HMI_RGB_LCD_H_RES,
        .screen_h = HMI_RGB_LCD_V_RES,
        .iters = iters,
    }, &r);
    (void)gfx_core_unlock(app->port.gfx);
    ESP_RETURN_ON_FALSE(rc == 0, ESP_FAIL, TAG, "arena_compare_run failed: %d", rc);

    ESP_LOGI(TAG, "pkg=%u B nodes=%u", (unsigned)r.pkg_size, (unsigned)r.node_count);
    log_path("A formal arena", "load+attach", &r.arena, r.iters);
    log_path("B object bridge", "load+bind", &r.object, r.iters);

    ESP_LOGI(TAG, "=== ratio (object / arena) ===");
    log_ratio("load", r.arena.load_us, r.object.load_us);
    log_ratio("full wall", r.arena.full_wall_us, r.object.full_wall_us);
    log_ratio("full render", r.arena.full_render_us, r.object.full_render_us);
    log_ratio("full flush", r.arena.full_flush_us, r.object.full_flush_us);
    log_ratio("dirty wall", r.arena.dirty_wall_us, r.object.dirty_wall_us);
    log_ratio("dirty render", r.arena.dirty_render_us, r.object.dirty_render_us);
    log_ratio("dirty flush", r.arena.dirty_flush_us, r.object.dirty_flush_us);
    ESP_LOGI(TAG, "compare done — look at render ratio; flush often dominates wall");
    return ESP_OK;
}

#elif ARENA_DEMO_FROM_GSP
static void on_ok_gsp(arena_t *arena, arena_node_t *node,
                      const gfx_touch_event_t *event, void *user_data)
{
    arena_demo_app_t *app = (arena_demo_app_t *)user_data;
    (void)arena;
    if (app == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }
    if (node != NULL) {
        node->bg_rgb = 0xff6644;
        (void)arena_scene_mark_dirty(&app->scene, arena_node_offset(&app->scene.arena, node));
    }
    app->ok_count++;
}

static esp_err_t scene_load(arena_demo_app_t *app)
{
    gsp_to_arena_info_t info = {0};
    int64_t t_pack0 = esp_timer_get_time();
    int rc = gsp_to_arena(home_scene_pkg, sizeof(home_scene_pkg),
                          &app->pkg, &app->pkg_size, &info);
    int64_t t_pack1 = esp_timer_get_time();
    ESP_RETURN_ON_FALSE(rc == GSP_TO_ARENA_OK && app->pkg != NULL, ESP_FAIL, TAG,
                        "gsp_to_arena failed: %d", rc);

    ESP_LOGI(TAG, "GSP→ARN: %u nodes (skip %u) %u B, pack=%lld us",
             info.out_node_count, info.skipped, (unsigned)app->pkg_size,
             (long long)(t_pack1 - t_pack0));

    ESP_RETURN_ON_FALSE(gfx_core_lock(app->port.gfx) == GFX_OK, ESP_FAIL, TAG, "gfx lock failed");

    arena_t arena = {0};
    int64_t t_bind0 = esp_timer_get_time();
    if (arena_load(app->pkg, app->pkg_size, &arena) != 0 ||
            arena_scene_attach(app->port.disp, &arena, &app->scene) != 0) {
        (void)gfx_core_unlock(app->port.gfx);
        arena_free(&arena);
        ESP_LOGE(TAG, "arena load/attach failed");
        return ESP_FAIL;
    }
    arena_scene_set_font(&app->scene, (gfx_font_t)&font_puhui_16_4);
    static arena_action_entry_t actions[1];
    actions[0] = (arena_action_entry_t) {
        .name = "on_ok",
        .cb = on_ok_gsp,
        .user_data = app,
    };
    arena_scene_set_actions(&app->scene, actions, 1);
    (void)arena_scene_mark_dirty_all(&app->scene);
    int64_t t_bind1 = esp_timer_get_time();

    (void)gfx_core_unlock(app->port.gfx);

    int64_t t_draw0 = esp_timer_get_time();
    (void)gfx_core_refresh_now(app->port.gfx);
    int64_t t_draw1 = esp_timer_get_time();

    ESP_LOGI(TAG, "timing: bind(load+attach)=%lld us  first_refresh=%lld us",
             (long long)(t_bind1 - t_bind0),
             (long long)(t_draw1 - t_draw0));
    ESP_LOGI(TAG, "GSP home via arena; tap Next (on_ok) to recolor");
    return ESP_OK;
}

#else
static esp_err_t scene_load(arena_demo_app_t *app)
{
    int64_t t_pack0 = esp_timer_get_time();
    app->pkg = arena_demo_pack(&app->pkg_size);
    int64_t t_pack1 = esp_timer_get_time();
    ESP_RETURN_ON_FALSE(app->pkg != NULL, ESP_ERR_NO_MEM, TAG, "arena_demo_pack failed");

    ESP_LOGI(TAG, "arena pkg %u bytes, screen %ux%u, pack=%lld us",
             (unsigned)app->pkg_size,
             (unsigned)HMI_RGB_LCD_H_RES,
             (unsigned)HMI_RGB_LCD_V_RES,
             (long long)(t_pack1 - t_pack0));

    ESP_RETURN_ON_FALSE(gfx_core_lock(app->port.gfx) == GFX_OK, ESP_FAIL, TAG, "gfx lock failed");

    int64_t t_bind0 = esp_timer_get_time();
    int rc = arena_demo_bind(app->port.disp, app->pkg, app->pkg_size,
                             (gfx_font_t)&font_puhui_16_4,
                             &app->scene, &app->ok_count);
    int64_t t_bind1 = esp_timer_get_time();
    if (rc != 0) {
        (void)gfx_core_unlock(app->port.gfx);
        ESP_LOGE(TAG, "arena_demo_bind failed: %d", rc);
        return ESP_FAIL;
    }

    gfx_display_refresh_all(app->port.disp);
    (void)gfx_core_unlock(app->port.gfx);

    int64_t t_draw0 = esp_timer_get_time();
    (void)gfx_core_refresh_now(app->port.gfx);
    int64_t t_draw1 = esp_timer_get_time();

    ESP_LOGI(TAG, "timing: bind(load+attach)=%lld us  first_refresh=%lld us",
             (long long)(t_bind1 - t_bind0),
             (long long)(t_draw1 - t_draw0));
    ESP_LOGI(TAG, "arena scene attached (formal path, no gfx_object tree)");
    ESP_LOGI(TAG, "tap OK (bottom-right) to mutate button + title");
    return ESP_OK;
}
#endif

void app_main(void)
{
#if ARENA_DEMO_SWEEP
    ESP_LOGI(TAG, "start ESP arena row-sweep (%s)",
             ARENA_DEMO_PATH_IS_OBJECT ? "B object" : "A arena");
#elif ARENA_DEMO_COMPARE
    ESP_LOGI(TAG, "start ESP arena A/B compare");
#else
    ESP_LOGI(TAG, "start ESP arena formal-path demo");
#endif

    ESP_ERROR_CHECK(board_init(&s_app));
    ESP_ERROR_CHECK(gfx_init(&s_app));
    ESP_ERROR_CHECK(scene_load(&s_app));

#if ARENA_DEMO_SWEEP
    /* No sleep: each board refreshes as fast as its path can. */
    while (1) {
        sweep_step(&s_app);
    }
#elif ARENA_DEMO_COMPARE
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
    uint32_t last_ok = 0;
    while (1) {
        if (s_app.ok_count != last_ok) {
            last_ok = s_app.ok_count;
            ESP_LOGI(TAG, "OK pressed, count=%lu", (unsigned long)last_ok);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
#endif
}
