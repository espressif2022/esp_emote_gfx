/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform/gfx_platform_accel_priv.h"

esp_err_t gfx_platform_accel_init(void)
{
    return ESP_OK;
}

void gfx_platform_accel_deinit(void)
{
}

const gfx_draw_ops_t *gfx_platform_accel_get_draw_ops(void)
{
    return NULL;
}

uint32_t gfx_platform_accel_get_caps(void)
{
    return GFX_BACKEND_CAP_NONE;
}

gfx_render_alignment_t gfx_platform_accel_get_alignment(void)
{
    return (gfx_render_alignment_t) {
        .width_px = 1,
        .height_px = 1,
        .stride_bytes = 1,
        .addr_bytes = 1,
    };
}
