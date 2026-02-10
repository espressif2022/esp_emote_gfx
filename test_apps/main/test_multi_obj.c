/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_multi";

static void test_multiple_objects_function(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Multiple Objects Interaction ===");

    gfx_emote_lock(emote_handle);

    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_obj_t *anim_obj = gfx_anim_create(disp_default);
    gfx_obj_t *img_obj = gfx_img_create(disp_default);
    gfx_obj_t *label_obj = gfx_label_create(disp_default);
    gfx_timer_handle_t timer = gfx_timer_create(emote_handle, clock_tm_callback, 5000, label_obj);

    TEST_ASSERT_NOT_NULL(anim_obj);
    TEST_ASSERT_NOT_NULL(label_obj);
    TEST_ASSERT_NOT_NULL(img_obj);
    TEST_ASSERT_NOT_NULL(timer);

    const void *anim_data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);

    gfx_anim_set_src(anim_obj, anim_data, anim_size);
    gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj, 0, 30, 15, true);
    gfx_anim_start(anim_obj);

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .mem_size = (size_t)mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .font_size = 20,
    };

    gfx_font_t font_DejaVuSans;
    esp_err_t ret = gfx_label_new_font(&font_cfg, &font_DejaVuSans);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    gfx_label_set_font(label_obj, font_DejaVuSans);
#else
    gfx_label_set_font(label_obj, (gfx_font_t)&font_puhui_16_4);
#endif

    gfx_obj_set_size(label_obj, 200, 49);
    gfx_label_set_text(label_obj, "Multi-Object Test");
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0xFF0000));
    gfx_obj_align(label_obj, GFX_ALIGN_BOTTOM_MID, 0, 0);
    gfx_label_set_text_align(label_obj, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(label_obj, GFX_LABEL_LONG_SCROLL);

    gfx_image_dsc_t img_dsc;
    load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565_BIN, &img_dsc);
    gfx_img_set_src(img_obj, (void *)&img_dsc); // Use BIN format image
    gfx_obj_align(img_obj, GFX_ALIGN_TOP_MID, 0, 0);

    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(10 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_timer_delete(emote_handle, timer);
    gfx_obj_delete(anim_obj);
    gfx_obj_delete(label_obj);
    gfx_obj_delete(img_obj);
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_label_delete_font(font_DejaVuSans);
#endif
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test function obj multi", "")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = display_and_graphics_init("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_multiple_objects_function(assets_handle);

    display_and_graphics_clean(assets_handle);
}
