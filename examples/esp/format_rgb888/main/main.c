/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "format_demo_app.h"
#include "playground_scene.h"

static const char *const TAG = "gfx888";

static gfx_asset_source_t *s_assets_fs;

static void init_format_demo_assets(void)
{
    gfx_err_t err = gfx_fs_open_partition("assets_test", &s_assets_fs);
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "assets_test open failed: %d; flash full project including assets_test.bin", err);
        return;
    }

    ESP_ERROR_CHECK(gfx_format_demo_set_asset_fs(s_assets_fs));
}

static void init_loose_assets(void)
{
    gfx_err_t err = gfx_format_demo_mount_loose_assets();
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "loose assets mount failed: %d; SPIFFS clips will be skipped", err);
    }
}

void app_main(void)
{
    const gfx_format_demo_app_config_t config = {
        .log_tag = TAG,
        .title = "GFX BGR888 Playground",
        .format_tag = "BGR888",
        .color_format = GFX_COLOR_FORMAT_BGR888,
        .log_running = true,
    };

    init_format_demo_assets();
    init_loose_assets();
    ESP_ERROR_CHECK(gfx_format_demo_app_run(&config));
}
