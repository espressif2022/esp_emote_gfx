/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "gfx.h"
#include "mmap_generate_test_assets.h"

#ifdef __cplusplus
extern "C" {
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
