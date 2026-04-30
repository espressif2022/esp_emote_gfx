/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "gfx.h"
#include "mmap_generate_assets_test.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mmap_assets_handle_t assets_handle;
} test_app_runtime_t;

typedef void (*test_app_touch_event_cb_t)(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);
typedef void (*test_app_disp_update_cb_t)(gfx_disp_t *disp, gfx_disp_event_t event, const void *obj, void *user_data);

#define TEST_APP_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_APP_ASSETS_PARTITION_DEFAULT "assets_test"

/**
 * Periodic SRAM/PSRAM monitor period (ms) when explicitly enabled.
 * Set to 0 to disable monitor startup.
 */
#ifndef TEST_APP_MEM_MONITOR_PERIOD_MS
#define TEST_APP_MEM_MONITOR_PERIOD_MS 10000
#endif

/* External declarations */
extern const gfx_image_dsc_t icon_rgb565;
extern const gfx_image_dsc_t icon_rgb565A8;
extern const lv_font_t font_puhui_16_4;

/* Shared global variables */
extern gfx_handle_t emote_handle;
extern gfx_disp_t *disp_default;  /* First display (from gfx_emote_add_disp in test_init) */
extern gfx_touch_t *touch_default;

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

esp_err_t test_app_runtime_open(test_app_runtime_t *runtime, const char *partition_label);
void test_app_runtime_close(test_app_runtime_t *runtime);
esp_err_t test_app_lock(void);
void test_app_unlock(void);
void test_app_wait_ms(uint32_t delay_ms);
void test_app_wait_for_observe(uint32_t delay_ms);
void test_app_log_case(const char *tag, const char *case_name);
void test_app_log_step(const char *tag, const char *step_name);
void test_app_set_touch_event_cb(test_app_touch_event_cb_t cb, void *user_data);
void test_app_set_disp_update_cb(test_app_disp_update_cb_t cb, void *user_data);
void test_app_mem_log_snapshot(const char *tag, const char *label);
void test_app_mem_monitor_start(uint32_t period_ms);
void test_app_mem_monitor_stop(void);

/**
 * @brief Initialize display and graphics system
 *
 * @param partition_label Partition label for assets
 * @param max_files Maximum number of files
 * @param checksum Checksum value
 * @param assets_handle Output parameter for assets handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t display_and_graphics_init(const char *partition_label, uint32_t max_files, uint32_t checksum, mmap_assets_handle_t *assets_handle);

/**
 * @brief Cleanup display and graphics system
 *
 * @param assets_handle Assets handle to cleanup
 */
void display_and_graphics_clean(mmap_assets_handle_t assets_handle);

/**
 * @brief Load image from mmap assets and prepare image descriptor
 *
 * @param assets_handle Handle to mmap assets
 * @param asset_id Asset ID in mmap
 * @param img_dsc Pointer to image descriptor to be filled
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc);

/**
 * @brief Clock timer callback function
 *
 * @param user_data User data pointer (label object)
 */
void clock_tm_callback(void *user_data);

#ifdef __cplusplus
}
#endif
