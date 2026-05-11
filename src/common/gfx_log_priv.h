/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "core/gfx_log.h"

bool gfx_log_should_output(gfx_log_module_t module, gfx_log_level_t level);
void gfx_log_write(gfx_log_module_t module, gfx_log_level_t level, const char *tag, const char *format, ...);

#ifdef GFX_LOG_MODULE

#ifdef GFX_LOG_TAG
#ifndef TAG
static const char *TAG = GFX_LOG_TAG;
#endif

/* Newer files can define GFX_LOG_TAG and call GFX_LOGI("msg") directly.
 * Older files keep the ESP_LOG-like GFX_LOGI(TAG, "msg") form below. */
#define GFX_LOG_WRITE(level, format, ...)                                      \
    do {                                                                      \
        if (gfx_log_should_output(GFX_LOG_MODULE, level)) {                   \
            gfx_log_write(GFX_LOG_MODULE, level, GFX_LOG_TAG,                 \
                          format, ##__VA_ARGS__);                            \
        }                                                                     \
    } while (0)

#define GFX_LOGE(format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define GFX_LOGW(format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define GFX_LOGI(format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define GFX_LOGD(format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define GFX_LOGV(format, ...) GFX_LOG_WRITE(GFX_LOG_LEVEL_VERBOSE, format, ##__VA_ARGS__)

#else

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

#endif

#endif /* GFX_LOG_MODULE */
