/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

gfx_err_t gfx_subsystem_image_decoder_init(void);
gfx_err_t gfx_subsystem_image_decoder_deinit(void);

gfx_err_t gfx_subsystem_font_init(void);
gfx_err_t gfx_subsystem_font_deinit(void);

gfx_err_t gfx_subsystem_accel_init(void);
gfx_err_t gfx_subsystem_accel_deinit(void);

gfx_err_t gfx_subsystem_jpeg_init(void);
gfx_err_t gfx_subsystem_jpeg_deinit(void);

#ifdef __cplusplus
}
#endif
