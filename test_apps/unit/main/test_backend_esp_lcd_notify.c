/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "unity.h"

#include "gfx/backends/esp_lcd.h"
#include "core/runtime/gfx_core_priv.h"
#include "platform/gfx_platform.h"

static gfx_handle_t test_core_create(void)
{
    return gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
}

TEST_CASE("esp_lcd backend: notify flush done signals display", "[unit][backend][esp_lcd]")
{
    gfx_handle_t handle = test_core_create();
    static esp_lcd_panel_handle_t s_fake_panel = (esp_lcd_panel_handle_t)0x1000;
    gfx_backend_t *backend = gfx_backend_esp_lcd_create(&(gfx_backend_esp_lcd_config_t) {
        .panel = s_fake_panel,
        .interface = GFX_BACKEND_ESP_LCD_IF_RGB,
        .skip_panel_callbacks = true,
    });
    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 64,
        .v_res = 64,
        .backend = backend,
        .buffers = {
            .buf_pixels = 64 * 16,
        },
    });

    TEST_ASSERT_NOT_NULL(backend);
    TEST_ASSERT_NOT_NULL(disp);

    TEST_ASSERT_TRUE(gfx_backend_esp_lcd_notify_flush_done(backend, disp));
    TEST_ASSERT_TRUE(gfx_platform_event_wait(disp->sync.event_group, WAIT_FLUSH_DONE, false, false, 0));

    gfx_display_delete(disp);
    gfx_core_deinit(handle);
}

TEST_CASE("esp_lcd backend: draw bitmap hook can be installed", "[unit][backend][esp_lcd]")
{
    static esp_lcd_panel_handle_t s_fake_panel = (esp_lcd_panel_handle_t)0x2000;
    gfx_backend_t *backend = gfx_backend_esp_lcd_create(&(gfx_backend_esp_lcd_config_t) {
        .panel = s_fake_panel,
        .interface = GFX_BACKEND_ESP_LCD_IF_RGB,
        .skip_panel_callbacks = true,
    });

    TEST_ASSERT_NOT_NULL(backend);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_backend_esp_lcd_set_draw_bitmap_callback(backend, NULL, NULL));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_backend_esp_lcd_set_notify_task(backend, NULL));
    gfx_backend_esp_lcd_delete(backend);
}
