/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gfx_subsystem_image_decoder_init(void);
esp_err_t gfx_subsystem_image_decoder_deinit(void);

esp_err_t gfx_subsystem_font_init(void);
esp_err_t gfx_subsystem_font_deinit(void);

#ifdef __cplusplus
}
#endif
