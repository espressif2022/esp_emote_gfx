/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    GFX_LOG_LEVEL_NONE = 0,
    GFX_LOG_LEVEL_ERROR,
    GFX_LOG_LEVEL_WARN,
    GFX_LOG_LEVEL_INFO,
    GFX_LOG_LEVEL_DEBUG,
    GFX_LOG_LEVEL_VERBOSE,
} gfx_log_level_t;

typedef enum {
    GFX_LOG_MODULE_CORE = 0,
    GFX_LOG_MODULE_DISP,
    GFX_LOG_MODULE_OBJ,
    GFX_LOG_MODULE_REFR,
    GFX_LOG_MODULE_RENDER,
    GFX_LOG_MODULE_TIMER,
    GFX_LOG_MODULE_TOUCH,
    GFX_LOG_MODULE_IMG_DEC,
    GFX_LOG_MODULE_LABEL,
    GFX_LOG_MODULE_LABEL_OBJ,
    GFX_LOG_MODULE_DRAW_LABEL,
    GFX_LOG_MODULE_FONT_LV,
    GFX_LOG_MODULE_FONT_FT,
    GFX_LOG_MODULE_IMG,
    GFX_LOG_MODULE_QRCODE,
    GFX_LOG_MODULE_BUTTON,
    GFX_LOG_MODULE_ANIM,
    GFX_LOG_MODULE_ANIM_DEC,
    GFX_LOG_MODULE_MOTION,
    GFX_LOG_MODULE_EAF_DEC,
    GFX_LOG_MODULE_QRCODE_LIB,
    GFX_LOG_MODULE_COUNT,
} gfx_log_module_t;

/**********************
 *   PUBLIC API
 **********************/

void gfx_log_set_level(gfx_log_module_t module, gfx_log_level_t level);
gfx_log_level_t gfx_log_get_level(gfx_log_module_t module);
void gfx_log_set_level_all(gfx_log_level_t level);
bool gfx_log_should_output(gfx_log_module_t module, gfx_log_level_t level);
const char *gfx_log_module_name(gfx_log_module_t module);
void gfx_log_write(gfx_log_module_t module, gfx_log_level_t level, const char *tag, const char *format, ...);
void gfx_log_writev(gfx_log_module_t module, gfx_log_level_t level, const char *tag, const char *format, va_list args);

#ifdef __cplusplus
}
#endif
