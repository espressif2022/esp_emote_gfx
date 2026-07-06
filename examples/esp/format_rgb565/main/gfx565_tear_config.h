/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "gfx/backends/esp_lcd.h"
#include "hmi_rgb_board.h"

/**
 * RGB565 HMI RGB board — tear mode verification presets.
 *
 * Change GFX565_TEAR_VERIFY_MODE, then rebuild.
 *
 * Naming: <tear_kind>_<panel_fb_count>_PIPELINE
 *
 *   0  DOUBLE_FULL     — 2 panel FB, gfx_buf_pipeline_t, 整屏 draw
 *   1  TRIPLE_FULL     — 3 panel FB, gfx_buf_pipeline_t, 整屏 draw
 *   2  TRIPLE_PARTIAL  — 3 panel FB, partition + STAGING + clean copy
 *
 * Not listed (gfx_tear_mode_t exists, not for this RGB demo):
 *   DOUBLE_PARTIAL — 2-FB partial; on RGB use TRIPLE_PARTIAL instead.
 *   NONE / DOUBLE_DIRECT — SPI per-chunk blit, not for RGB scan-out.
 *   TE_SYNC — stub only (no TE GPIO).
 */
// #define GFX565_TEAR_VERIFY_MODE   GFX565_TEAR_VERIFY_TRIPLE_PARTIAL //OK
#define GFX565_TEAR_VERIFY_MODE   GFX565_TEAR_VERIFY_TRIPLE_FULL //OK
// #define GFX565_TEAR_VERIFY_MODE   GFX565_TEAR_VERIFY_DOUBLE_FULL //OK

#define GFX565_TEAR_VERIFY_DOUBLE_FULL       0
#define GFX565_TEAR_VERIFY_TRIPLE_FULL       1
#define GFX565_TEAR_VERIFY_TRIPLE_PARTIAL    2

#define GFX565_H_RES              HMI_RGB_LCD_H_RES
#define GFX565_V_RES              HMI_RGB_LCD_V_RES
#define GFX565_PARTITION_LINES    48U
#define GFX565_PARTITION_PIXELS   (GFX565_H_RES * GFX565_PARTITION_LINES)

#if GFX565_TEAR_VERIFY_MODE == GFX565_TEAR_VERIFY_DOUBLE_FULL
#define GFX565_PANEL_NUM_FBS      2U
#define GFX565_TEAR_MODE          GFX_TEAR_DOUBLE_FULL
#define GFX565_FLUSH_MODE         GFX_BACKEND_ESP_LCD_FLUSH_DIRECT
#define GFX565_BUF_PIXELS         (GFX565_H_RES * GFX565_V_RES)
#define GFX565_USE_PANEL_AS_DRAW  1
#define GFX565_TEAR_MODE_NAME     "DOUBLE_FULL"

#elif GFX565_TEAR_VERIFY_MODE == GFX565_TEAR_VERIFY_TRIPLE_FULL
#define GFX565_PANEL_NUM_FBS      3U
#define GFX565_TEAR_MODE          GFX_TEAR_TRIPLE_FULL
#define GFX565_FLUSH_MODE         GFX_BACKEND_ESP_LCD_FLUSH_DIRECT
#define GFX565_BUF_PIXELS         (GFX565_H_RES * GFX565_V_RES)
#define GFX565_USE_PANEL_AS_DRAW  1
#define GFX565_TEAR_MODE_NAME     "TRIPLE_FULL"

#elif GFX565_TEAR_VERIFY_MODE == GFX565_TEAR_VERIFY_TRIPLE_PARTIAL
#define GFX565_PANEL_NUM_FBS      3U
#define GFX565_TEAR_MODE          GFX_TEAR_TRIPLE_PARTIAL
#define GFX565_FLUSH_MODE         GFX_BACKEND_ESP_LCD_FLUSH_STAGING
#define GFX565_BUF_PIXELS         GFX565_PARTITION_PIXELS
#define GFX565_USE_PANEL_AS_DRAW  0
#define GFX565_TEAR_MODE_NAME     "TRIPLE_PARTIAL"

#else
#error "Invalid GFX565_TEAR_VERIFY_MODE (valid: 0..2, see header comments)"
#endif
