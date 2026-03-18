/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core/gfx_log.h"

#ifndef GFX_LOG_MODULE
#error "GFX_LOG_MODULE must be defined before including common/gfx_log_priv.h"
#endif

#define GFX_LOG_WRITE(level, tag, format, ...)                                \
    do {                                                                      \
        if (gfx_log_should_output(GFX_LOG_MODULE, level)) {                   \
            gfx_log_write(GFX_LOG_MODULE, level, tag, format, ##__VA_ARGS__); \
        }                                                                     \
    } while (0)

#define GFX_LOGE(tag, format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define GFX_LOGW(tag, format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define GFX_LOGI(tag, format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define GFX_LOGD(tag, format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define GFX_LOGV(tag, format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)
