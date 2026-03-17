/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

#include "core/gfx_log.h"

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *s_module_names[GFX_LOG_MODULE_COUNT] = {
    [GFX_LOG_MODULE_CORE] = "core",
    [GFX_LOG_MODULE_DISP] = "disp",
    [GFX_LOG_MODULE_OBJ] = "obj",
    [GFX_LOG_MODULE_REFR] = "refr",
    [GFX_LOG_MODULE_RENDER] = "render",
    [GFX_LOG_MODULE_TIMER] = "timer",
    [GFX_LOG_MODULE_TOUCH] = "touch",
    [GFX_LOG_MODULE_IMG_DEC] = "img_dec",
    [GFX_LOG_MODULE_LABEL] = "label",
    [GFX_LOG_MODULE_LABEL_OBJ] = "label_obj",
    [GFX_LOG_MODULE_DRAW_LABEL] = "draw_label",
    [GFX_LOG_MODULE_FONT_LV] = "font_lv",
    [GFX_LOG_MODULE_FONT_FT] = "font_ft",
    [GFX_LOG_MODULE_IMG] = "img",
    [GFX_LOG_MODULE_QRCODE] = "qrcode",
    [GFX_LOG_MODULE_ANIM] = "anim",
    [GFX_LOG_MODULE_ANIM_DEC] = "anim_dec",
    [GFX_LOG_MODULE_EAF_DEC] = "eaf_dec",
    [GFX_LOG_MODULE_QRCODE_LIB] = "qrcode_lib",
};

static gfx_log_level_t s_module_levels[GFX_LOG_MODULE_COUNT];
static bool s_log_levels_initialized;

/**********************
 *   STATIC FUNCTIONS
 **********************/

static char gfx_log_level_to_char(gfx_log_level_t level)
{
    switch (level) {
    case GFX_LOG_LEVEL_ERROR:
        return 'E';
    case GFX_LOG_LEVEL_WARN:
        return 'W';
    case GFX_LOG_LEVEL_INFO:
        return 'I';
    case GFX_LOG_LEVEL_DEBUG:
        return 'D';
    case GFX_LOG_LEVEL_VERBOSE:
        return 'V';
    case GFX_LOG_LEVEL_NONE:
    default:
        return 'N';
    }
}

static void gfx_log_init_levels(void)
{
    if (s_log_levels_initialized) {
        return;
    }

    for (int i = 0; i < GFX_LOG_MODULE_COUNT; i++) {
        s_module_levels[i] = GFX_LOG_LEVEL_INFO;
    }

    s_log_levels_initialized = true;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_log_set_level(gfx_log_module_t module, gfx_log_level_t level)
{
    gfx_log_init_levels();

    if (module < 0 || module >= GFX_LOG_MODULE_COUNT) {
        return;
    }

    s_module_levels[module] = level;
}

gfx_log_level_t gfx_log_get_level(gfx_log_module_t module)
{
    gfx_log_init_levels();

    if (module < 0 || module >= GFX_LOG_MODULE_COUNT) {
        return GFX_LOG_LEVEL_NONE;
    }

    return s_module_levels[module];
}

void gfx_log_set_level_all(gfx_log_level_t level)
{
    gfx_log_init_levels();

    for (int i = 0; i < GFX_LOG_MODULE_COUNT; i++) {
        s_module_levels[i] = level;
    }
}

bool gfx_log_should_output(gfx_log_module_t module, gfx_log_level_t level)
{
    gfx_log_init_levels();

    if (module < 0 || module >= GFX_LOG_MODULE_COUNT) {
        return false;
    }

    if (level == GFX_LOG_LEVEL_NONE) {
        return false;
    }

    return level <= s_module_levels[module];
}

const char *gfx_log_module_name(gfx_log_module_t module)
{
    if (module < 0 || module >= GFX_LOG_MODULE_COUNT) {
        return "unknown";
    }

    return s_module_names[module];
}

void gfx_log_writev(gfx_log_module_t module, gfx_log_level_t level, const char *tag, const char *format, va_list args)
{
    const char *module_name;
    int64_t ts_us;

    if (!gfx_log_should_output(module, level)) {
        return;
    }

    module_name = gfx_log_module_name(module);
    ts_us = esp_timer_get_time();

    if (tag != NULL && tag[0] != '\0' && strcmp(tag, module_name) != 0) {
        printf("%c (%" PRIi64 ") %s/%s: ", gfx_log_level_to_char(level), ts_us / 1000, module_name, tag);
    } else {
        printf("%c (%" PRIi64 ") %s: ", gfx_log_level_to_char(level), ts_us / 1000, module_name);
    }

    vprintf(format, args);
    printf("\n");
}

void gfx_log_write(gfx_log_module_t module, gfx_log_level_t level, const char *tag, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    gfx_log_writev(module, level, tag, format, args);
    va_end(args);
}
