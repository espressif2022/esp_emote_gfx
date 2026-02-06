/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "unity.h"
#include "test_common.h"

static const char *TAG = "test_image";

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

static void multi_disp1_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
    // ESP_LOGI(TAG, "disp_1 flush(%p): (%d, %d), (%d, %d)", panel, (int)x1, (int)y1, (int)x2, (int)y2);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
}

static void multi_disp2_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
    // ESP_LOGI(TAG, "disp_2 flush(%p): (%d, %d), (%d, %d)", panel, (int)x1, (int)y1, (int)x2, (int)y2);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
}

static void test_multi_disp_function(mmap_assets_handle_t assets_handle)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "=== Testing Image Function ===");

    gfx_emote_lock(emote_handle);

    gfx_disp_config_t disp_cfg1 = {
        .h_res = 320,
        .v_res = 240,
        .flush_cb = multi_disp1_flush_callback,
        .user_data = (void *)panel_handle,
        .flags = { .swap = true },
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels =  320 * 16},
    };
    gfx_disp_t *new_disp1 = gfx_emote_add_disp(emote_handle, &disp_cfg1);
    TEST_ASSERT_NOT_NULL(new_disp1);

    gfx_disp_config_t disp_cfg2 = {
        .h_res = 320,
        .v_res = 240,
        .flush_cb = multi_disp2_flush_callback,
        .user_data = (void *)panel_handle,
        .flags = { .swap = true },
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = 320 * 16 },
    };

    gfx_disp_t *new_disp2 = gfx_disp_add(emote_handle, &disp_cfg2);
    TEST_ASSERT_NOT_NULL(new_disp2);

    gfx_obj_t *anim_obj1 = gfx_anim_create(new_disp1);
    TEST_ASSERT_NOT_NULL(anim_obj1);

    const void *anim_data1 = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    size_t anim_size1 = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    gfx_anim_set_src(anim_obj1, anim_data1, anim_size1);
    gfx_obj_align(anim_obj1, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj1, 0, 0xFFFF, 15, true);
    gfx_anim_start(anim_obj1);

    gfx_obj_t *anim_obj2 = gfx_anim_create(new_disp2);
    TEST_ASSERT_NOT_NULL(anim_obj2);

    const void *anim_data2 = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_TRANSPARENT_EAF);
    size_t anim_size2 = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_TRANSPARENT_EAF);
    gfx_anim_set_src(anim_obj2, anim_data2, anim_size2);
    gfx_obj_align(anim_obj2, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj2, 0, 0xFFFF, 15, true);
    gfx_anim_start(anim_obj2);
    gfx_emote_unlock(emote_handle);
    
    vTaskDelay(pdMS_TO_TICKS(3 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(anim_obj1);
    gfx_obj_delete(anim_obj2);
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test multi disp1 function", "[multi_disp]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_multi_disp_function(assets_handle);

    test_cleanup_display_and_graphics(assets_handle);
}
