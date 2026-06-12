/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "esp_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL
#include "common/gfx_log_priv.h"
#include "fonts/gfx_font_priv.h"

#if GFX_HOST_BUILD
#include "platform/host/gfx_font_host_priv.h"
#endif

static const char *const TAG = "font_adapter";

esp_err_t gfx_font_init_adapter(gfx_font_handle_t font_adapter, const void *font)
{
    ESP_RETURN_ON_FALSE(font_adapter != NULL && font != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid font adapter args");

#if GFX_HOST_BUILD
    if (gfx_host_font_is_builtin(font)) {
        gfx_host_font_init_adapter(font_adapter, font);
        return ESP_OK;
    }
#else
    if (gfx_is_lvgl_font(font)) {
        gfx_font_lv_init_adapter(font_adapter, font);
        return ESP_OK;
    }

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_font_ft_init_adapter(font_adapter, font);
    return ESP_OK;
#endif
#endif

    GFX_LOGW(TAG, "font adapter: unsupported font handle");
    return ESP_ERR_NOT_SUPPORTED;
}
