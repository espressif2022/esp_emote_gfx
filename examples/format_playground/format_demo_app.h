/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *log_tag;
    const char *title;
    const char *format_tag;
    gfx_color_format_t color_format;
    bool log_running;
} gfx_format_demo_app_config_t;

esp_err_t gfx_format_demo_app_run(const gfx_format_demo_app_config_t *config);

#ifdef __cplusplus
}
#endif
