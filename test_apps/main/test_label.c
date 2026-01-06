/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "test_common.h"

static const char *TAG = "test_label";

static void test_label_map_function(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Label Map Function ===");

    gfx_emote_lock(emote_handle);

    gfx_obj_t *label_obj = gfx_label_create(emote_handle);
    TEST_ASSERT_NOT_NULL(label_obj);

    gfx_obj_set_size(label_obj, 150, 100);
    gfx_label_set_font(label_obj, (gfx_font_t)&font_puhui_16_4);

    gfx_label_set_text(label_obj, "AAA乐鑫BBB乐鑫CCC乐鑫CCC乐鑫BBB乐鑫AAA");
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x0000FF));
    gfx_label_set_long_mode(label_obj, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_bg_color(label_obj, GFX_COLOR_HEX(0xFF0000));
    gfx_label_set_bg_enable(label_obj, true);
    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 100);

    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x00FF00));
    gfx_emote_unlock(emote_handle);

    ESP_LOGI(TAG, "--- Re-render label end ---");
    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(label_obj);
    gfx_emote_unlock(emote_handle);
}

static void test_label_freetype_function(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Label Function ===");

    gfx_emote_lock(emote_handle);

    gfx_obj_t *label_obj = gfx_label_create(emote_handle);
    TEST_ASSERT_NOT_NULL(label_obj);

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
#endif

    gfx_label_set_bg_enable(label_obj, true);
    gfx_label_set_bg_color(label_obj, GFX_COLOR_HEX(0xFF0000));
    gfx_label_set_long_mode(label_obj, GFX_LABEL_LONG_WRAP);
    gfx_label_set_text(label_obj, "Hello World");
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x00FF00));
    gfx_obj_set_pos(label_obj, 100, 200);
    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 100);
    gfx_obj_set_size(label_obj, 200, 100);

    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(1000 * 3));

    gfx_emote_lock(emote_handle);
    gfx_label_set_text_fmt(label_obj, "Count: %d, Float: %.2f", 42, 3.14);
    gfx_label_set_long_mode(label_obj, GFX_LABEL_LONG_SCROLL);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x0000FF));
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(2 * 1000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(label_obj);
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_label_delete_font(font_DejaVuSans);
#endif
    gfx_emote_unlock(emote_handle);
}

TEST_CASE("test label function", "[label][freetype]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_label_freetype_function(assets_handle);

    test_cleanup_display_and_graphics(assets_handle);
}

TEST_CASE("test label function", "[label][map]")
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = test_init_display_and_graphics("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_label_map_function(assets_handle);

    test_cleanup_display_and_graphics(assets_handle);
}
