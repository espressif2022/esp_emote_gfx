/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "format_demo_app.h"
#include "playground_scene.h"

static const char *const TAG = "gfx565";

static gfx_fs_t *s_assets_store;

static void init_optional_asset_store(void)
{
    gfx_err_t err;
    const gfx_fs_mmap_config_t asset_config = {
        .partition_label = "assets_test",
        .full_check = true,
    };

    err = gfx_fs_open_mmap(&asset_config, &s_assets_store);
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "assets_test open failed: %d; flash full project including assets_test.bin", err);
        return;
    }

    ESP_ERROR_CHECK(gfx_format_demo_set_asset_store(s_assets_store));
    ESP_LOGI(TAG, "format demo assets store ready");
}

void app_main(void)
{
    const gfx_format_demo_app_config_t config = {
        .log_tag = TAG,
        .title = "GFX RGB565 Playground",
        .format_tag = "RGB565",
        .color_format = GFX_COLOR_FORMAT_RGB565,
    };

    init_optional_asset_store();
    ESP_ERROR_CHECK(gfx_format_demo_app_run(&config));
}
