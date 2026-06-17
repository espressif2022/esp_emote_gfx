/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/gfx_subsystem_init_priv.h"
#include "codecs/image/gfx_image_decoder_priv.h"
#include "platform/gfx_platform_accel_priv.h"

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
    return ESP_OK;
}

esp_err_t gfx_subsystem_font_deinit(void)
{
    return ESP_OK;
}

esp_err_t gfx_subsystem_accel_init(void)
{
    return gfx_platform_accel_init();
}

esp_err_t gfx_subsystem_accel_deinit(void)
{
    gfx_platform_accel_deinit();
    return ESP_OK;
}
