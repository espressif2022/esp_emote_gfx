/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "gfx.h"
#include "mmap_generate_test_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    mmap_assets_handle_t assets_handle;
} test_app_runtime_t;

#define TEST_APP_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/**********************
 *  EXTERNAL SYMBOLS
 **********************/

extern const gfx_image_dsc_t icon_rgb565;
extern const gfx_image_dsc_t icon_rgb565A8;
extern const lv_font_t font_puhui_16_4;

/**********************
 *  SHARED GLOBALS
 **********************/

extern gfx_handle_t emote_handle;
extern gfx_disp_t *disp_default;
extern gfx_touch_t *touch_default;

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

/**********************
 *   TEST SUPPORT API
 **********************/

esp_err_t test_app_runtime_open(test_app_runtime_t *runtime);
void test_app_runtime_close(test_app_runtime_t *runtime);
esp_err_t test_app_lock(void);
void test_app_unlock(void);
void test_app_wait_ms(uint32_t delay_ms);
void test_app_wait_for_observe(uint32_t delay_ms);
void test_app_log_case(const char *tag, const char *case_name);
void test_app_log_step(const char *tag, const char *step_name);

/**********************
 *   PLATFORM HELPERS
 **********************/

esp_err_t display_and_graphics_init(const char *partition_label, uint32_t max_files, uint32_t checksum, mmap_assets_handle_t *assets_handle);
void display_and_graphics_clean(mmap_assets_handle_t assets_handle);
esp_err_t load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc);
void clock_tm_callback(void *user_data);

#ifdef __cplusplus
}
#endif
