/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "common/gfx_check.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_LABEL
#include "common/gfx_log_priv.h"
#include "fonts/gfx_font_priv.h"

#if GFX_HOST_BUILD
#include "platform/host/host_font_priv.h"
#endif

static const char *const TAG = "font_adapter";

gfx_err_t gfx_font_init_adapter(gfx_font_handle_t font_adapter, const void *font)
{
    GFX_RETURN_ON_FALSE(font_adapter != NULL && font != NULL, GFX_ERR_INVALID_ARG, TAG, "invalid font adapter args");

#if GFX_HOST_BUILD
    if (gfx_is_lvgl_font(font)) {
        gfx_font_lv_init_adapter(font_adapter, font);
        return GFX_OK;
    }

    if (gfx_host_font_is_builtin(font)) {
        gfx_host_font_init_adapter(font_adapter, font);
        return GFX_OK;
    }
    gfx_host_font_init_adapter(font_adapter, gfx_host_font_default());
    return GFX_OK;
#else
    if (gfx_is_lvgl_font(font)) {
        gfx_font_lv_init_adapter(font_adapter, font);
        return GFX_OK;
    }

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_font_ft_init_adapter(font_adapter, font);
    return GFX_OK;
#endif
#endif

    GFX_LOGW(TAG, "font adapter: unsupported font handle");
    return GFX_ERR_NOT_SUPPORTED;
}
