/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"

#include "format_demo_app.h"
#include "playground_scene.h"

static const char *const TAG = "gfx565";

static gfx_fs_t *s_assets_fs;

static void init_optional_asset_fs(void)
{
    gfx_err_t err;

    err = gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_PARTITION,
        .access_mode = GFX_FS_ACCESS_DIRECT,
        .path_or_label = "assets_test",
    }, &s_assets_fs);
    if (err != GFX_OK) {
        ESP_LOGW(TAG, "assets_test open failed: %d; flash full project including assets_test.bin", err);
        return;
    }

    ESP_ERROR_CHECK(gfx_format_demo_set_asset_fs(s_assets_fs));
    ESP_LOGI(TAG, "format demo asset fs ready");
}

void app_main(void)
{
    const gfx_format_demo_app_config_t config = {
        .log_tag = TAG,
        .title = "GFX RGB565 Playground",
        .format_tag = "RGB565",
        .color_format = GFX_COLOR_FORMAT_RGB565,
    };

    init_optional_asset_fs();
    ESP_ERROR_CHECK(gfx_format_demo_app_run(&config));
}
