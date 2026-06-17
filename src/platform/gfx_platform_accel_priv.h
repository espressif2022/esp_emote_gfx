/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#include "core/display/gfx_backend_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gfx_platform_accel_init(void);
void gfx_platform_accel_deinit(void);

const gfx_draw_ops_t *gfx_platform_accel_get_draw_ops(void);
uint32_t gfx_platform_accel_get_caps(void);
gfx_render_alignment_t gfx_platform_accel_get_alignment(void);

#ifdef __cplusplus
}
#endif
