/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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
    GFX_LOG_MODULE_MESH_IMG,
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

/**
 * @brief Set the log level for one module.
 *
 * Messages with a level numerically higher than the configured level are
 * suppressed. Invalid modules are ignored.
 *
 * @param module Log module to configure.
 * @param level Maximum level to output.
 */
void gfx_log_set_level(gfx_log_module_t module, gfx_log_level_t level);

/**
 * @brief Get the configured log level for one module.
 *
 * @param module Log module to query.
 * @return Configured log level, or GFX_LOG_LEVEL_NONE if module is invalid.
 */
gfx_log_level_t gfx_log_get_level(gfx_log_module_t module);

/**
 * @brief Set the log level for all modules.
 *
 * @param level Maximum level to output for every module.
 */
void gfx_log_set_level_all(gfx_log_level_t level);

#ifdef __cplusplus
}
#endif
