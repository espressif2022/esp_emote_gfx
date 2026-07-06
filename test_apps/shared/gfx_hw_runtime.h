/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_mmap_assets.h"
#include "gfx.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"

#include "gfx_test_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mmap_assets_handle_t assets_handle;
} test_app_runtime_t;

typedef void (*test_app_touch_event_cb_t)(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);
typedef void (*test_app_disp_update_cb_t)(gfx_display_t *disp, gfx_display_event_t event, const void *obj, void *user_data);

#define TEST_APP_ASSETS_PARTITION_DEFAULT "assets_test"

extern const gfx_image_dsc_t icon_rgb565;
extern const gfx_image_dsc_t icon_rgb565A8;
extern const gfx_image_dsc_t icon_rgb888;
extern const gfx_image_dsc_t icon_rgb888a8;
extern const lv_font_t font_puhui_16_4;

extern gfx_handle_t emote_handle;
extern gfx_display_t *disp_default;
extern gfx_touch_t *touch_default;

esp_err_t test_app_runtime_open(test_app_runtime_t *runtime, const char *partition_label);
void test_app_runtime_close(test_app_runtime_t *runtime);
esp_err_t test_app_lock(void);
void test_app_unlock(void);
void test_app_set_touch_event_cb(test_app_touch_event_cb_t cb, void *user_data);
void test_app_set_disp_update_cb(test_app_disp_update_cb_t cb, void *user_data);

esp_err_t display_and_graphics_init(const char *partition_label, uint32_t max_files, uint32_t checksum,
                                    mmap_assets_handle_t *assets_handle);
void display_and_graphics_clean(mmap_assets_handle_t assets_handle);
esp_err_t load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc);
void clock_tm_callback(void *user_data);

#ifdef __cplusplus
}
#endif
