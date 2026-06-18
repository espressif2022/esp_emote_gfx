/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gfx_format_demo_build_playground_scene(gfx_display_t *disp, const char *title_text,
        const char *format_tag);
esp_err_t gfx_format_demo_set_asset_store(gfx_asset_store_t *store);

#ifdef __cplusplus
}
#endif
