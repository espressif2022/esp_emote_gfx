/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include "common/gfx_subsystem_init_priv.h"
#include "widget/img/gfx_img_dec_priv.h"
#include "widget/font/gfx_font_priv.h"

esp_err_t gfx_subsystem_image_decoder_init(void)
{
    return gfx_image_decoder_init();
}

esp_err_t gfx_subsystem_image_decoder_deinit(void)
{
    return gfx_image_decoder_deinit();
}

esp_err_t gfx_subsystem_font_init(void)
{
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    return gfx_ft_lib_create();
#else
    return ESP_OK;
#endif
}

esp_err_t gfx_subsystem_font_deinit(void)
{
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    return gfx_ft_lib_cleanup();
#else
    return ESP_OK;
#endif
}
