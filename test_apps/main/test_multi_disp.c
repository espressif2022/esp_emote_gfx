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
#include "bsp/esp-bsp.h"
#include "common.h"

static const char *TAG = "test_multi_disp";


static gfx_disp_t *disp_1 = NULL;
static gfx_disp_t *disp_2 = NULL;

static void disp_flush_callback(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
    gfx_disp_flush_ready(disp, true);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static int32_t s_grab_off_x = 0;
static int32_t s_grab_off_y = 0;
static bool s_grab_active = false;

static void touch_obj_event_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    gfx_coord_t obj_x, obj_y;
    gfx_obj_get_pos(obj, &obj_x, &obj_y);
    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        s_grab_off_x = (int32_t)event->x - obj_x;
        s_grab_off_y = (int32_t)event->y - obj_y;
        s_grab_active = true;
    }
    if (s_grab_active) {
        gfx_obj_set_pos(obj, (int32_t)event->x - s_grab_off_x, (int32_t)event->y - s_grab_off_y);
    }
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        s_grab_active = false;
    }
}

static void test_multi_disp_run(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing multi-display ===");

    gfx_emote_lock(emote_handle);

    gfx_disp_config_t disp_cfg_1 = {
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .flush_cb = disp_flush_callback,
        .update_cb = NULL,
        .user_data = (void *)panel_handle,
        .flags = { .swap = true },
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16 },
    };
    disp_1 = gfx_disp_add(emote_handle, &disp_cfg_1);
    TEST_ASSERT_NOT_NULL(disp_1);

    gfx_disp_config_t disp_cfg_2 = {
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .flush_cb = disp_flush_callback,
        .update_cb = NULL,
        .user_data = (void *)panel_handle,
        .flags = { .swap = true },
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16 },
    };
    disp_2 = gfx_disp_add(emote_handle, &disp_cfg_2);
    TEST_ASSERT_NOT_NULL(disp_2);

    gfx_obj_t *anim_obj_1 = gfx_anim_create(disp_1);
    TEST_ASSERT_NOT_NULL(anim_obj_1);
    const void *anim_data_1 = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    size_t anim_size_1 = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    gfx_anim_set_src(anim_obj_1, anim_data_1, anim_size_1);
    gfx_obj_align(anim_obj_1, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj_1, 0, 0xFFFF, 15, true);
    gfx_anim_start(anim_obj_1);

    gfx_obj_t *anim_obj_2 = gfx_anim_create(disp_2);
    TEST_ASSERT_NOT_NULL(anim_obj_2);
    const void *anim_data_2 = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_TRANSPARENT_EAF);
    size_t anim_size_2 = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_TRANSPARENT_EAF);
    gfx_anim_set_src(anim_obj_2, anim_data_2, anim_size_2);
    gfx_obj_align(anim_obj_2, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj_2, 0, 0xFFFF, 15, true);
    gfx_anim_start(anim_obj_2);

    gfx_touch_set_disp(touch_default, disp_2);

    gfx_obj_set_touch_cb(anim_obj_1, touch_obj_event_cb, NULL);
    gfx_obj_set_touch_cb(anim_obj_2, touch_obj_event_cb, NULL);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(1000 * 10));

    ESP_LOGI(TAG, "=== test multi disp completed ===");
    gfx_emote_lock(emote_handle);
    gfx_obj_delete(anim_obj_1);
    gfx_obj_delete(anim_obj_2);
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test function disp multi", "")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = display_and_graphics_init("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    test_multi_disp_run(assets_handle);
    display_and_graphics_clean(assets_handle);
}
