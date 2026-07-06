/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/gfx_subsystem_init_priv.h"
#include "codecs/image/gfx_image_decoder_priv.h"
#include "platform/gfx_platform_accel_priv.h"
#include "platform/gfx_platform_jpeg_priv.h"

gfx_err_t gfx_subsystem_image_decoder_init(void)
{
    return gfx_image_decoder_init();
}

gfx_err_t gfx_subsystem_image_decoder_deinit(void)
{
    return gfx_image_decoder_deinit();
}

gfx_err_t gfx_subsystem_font_init(void)
{
    return GFX_OK;
}

gfx_err_t gfx_subsystem_font_deinit(void)
{
    return GFX_OK;
}

gfx_err_t gfx_subsystem_accel_init(void)
{
    return gfx_platform_accel_init();
}

gfx_err_t gfx_subsystem_accel_deinit(void)
{
    gfx_platform_accel_deinit();
    return GFX_OK;
}

gfx_err_t gfx_subsystem_jpeg_init(void)
{
    return gfx_platform_jpeg_init();
}

gfx_err_t gfx_subsystem_jpeg_deinit(void)
{
    gfx_platform_jpeg_deinit();
    return GFX_OK;
}
