/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "unity.h"
#include "common.h"
#include "esp_timer.h"
#include "esp_jpeg_dec.h"

static const char *TAG = "test_image";

static lv_res_t jpeg_decode_memory(uint8_t **out, uint32_t *w, uint32_t *h, const uint8_t *in, uint32_t insize)
{
    jpeg_error_t ret;
    jpeg_dec_config_t config = {
#if  LV_COLOR_DEPTH == 32
        .output_type = JPEG_PIXEL_FORMAT_RGB888,
#elif  LV_COLOR_DEPTH == 16
#if  LV_BIG_ENDIAN_SYSTEM == 1 || LV_COLOR_16_SWAP == 1
        .output_type = JPEG_PIXEL_FORMAT_RGB565_BE,
#else
        .output_type = JPEG_PIXEL_FORMAT_RGB565_LE,
#endif
#else
#error Unsupported LV_COLOR_DEPTH
#endif
        .rotate = JPEG_ROTATE_0D,
    };

    jpeg_dec_handle_t jpeg_dec = NULL;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    jpeg_dec_open(&config, &jpeg_dec);
    ESP_GOTO_ON_FALSE(jpeg_dec != NULL, ESP_FAIL, err, TAG, "Failed to open jpeg decoder");

    jpeg_io = malloc(sizeof(jpeg_dec_io_t));
    out_info = malloc(sizeof(jpeg_dec_header_info_t));
    ESP_GOTO_ON_FALSE(jpeg_io != NULL && out_info != NULL, ESP_FAIL, err_dec, TAG, "Failed to allocate memory for jpeg decoder");

    jpeg_io->inbuf = (unsigned char *)in;
    jpeg_io->inbuf_len = insize;

    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    ESP_GOTO_ON_FALSE(ret == JPEG_ERR_OK, ESP_FAIL, err_alloc, TAG, "Failed to parse jpeg header");

    *w = out_info->width;
    *h = out_info->height;

    if (out) {
        *out = (uint8_t *)heap_caps_aligned_alloc(16, out_info->height * out_info->width * 2, MALLOC_CAP_DEFAULT);
        ESP_GOTO_ON_FALSE(*out != NULL, ESP_FAIL, err_alloc, TAG, "Failed to allocate memory for output buffer");

        jpeg_io->outbuf = *out;
        ret = jpeg_dec_process(jpeg_dec, jpeg_io);
        ESP_GOTO_ON_FALSE(ret == JPEG_ERR_OK, ESP_FAIL, err_out, TAG, "Failed to decode jpeg: %d", (int)ret);
    }

    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return LV_RES_OK;

err_out:
    if (out && *out) {
        free(*out);
        *out = NULL;
    }
err_alloc:
    free(jpeg_io);
    free(out_info);
    jpeg_dec_close(jpeg_dec);
    return LV_RES_INV;
err_dec:
    jpeg_dec_close(jpeg_dec);
    return LV_RES_INV;
err:
    return LV_RES_INV;
}

void decode_jpeg_rgb565_from_memory(const unsigned char *jpeg_data, size_t jpeg_size, gfx_obj_t *image)
{

    static uint8_t *output_buffer = NULL;
    uint32_t width, height;

    int64_t start_time = esp_timer_get_time();
    lv_res_t res = jpeg_decode_memory(&output_buffer, &width, &height, jpeg_data, jpeg_size);
    if (res != LV_RES_OK) {
        ESP_LOGE(TAG, "JPEG decode failed");
        return;
    }
    ESP_LOGW("####", "[1]JPEG: %" PRIu64 " ms", (esp_timer_get_time() - start_time) / 1000);

    gfx_emote_lock(emote_handle);

    static gfx_image_dsc_t img_dec = {
        .header.magic = C_ARRAY_HEADER_MAGIC,
        .header.w = 0,
        .header.h = 0,
        .data_size = 0,
        .header.cf = GFX_COLOR_FORMAT_RGB565,
        .data = NULL,
    };

    img_dec.header.w = width;
    img_dec.header.h = height;
    img_dec.data_size = width * height * 2; // RGB565 is 2 bytes per pixel
    img_dec.data = (const uint8_t *)output_buffer;

    gfx_img_set_src_desc(image, &(gfx_img_src_t) {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &img_dec,
    });

    start_time = esp_timer_get_time();
    gfx_refr_now(emote_handle);
    ESP_LOGW("####", "[2]refr end: %" PRIu64 " ms", (esp_timer_get_time() - start_time) / 1000);

    gfx_emote_unlock(emote_handle); //
    vTaskDelay(pdMS_TO_TICKS(10));

    gfx_emote_lock(emote_handle);
    // Clean up
    if (output_buffer) {
        ESP_LOGI(TAG, "Free");
        free(output_buffer);
    }
    gfx_emote_unlock(emote_handle);
}

static void test_image_function(mmap_assets_handle_t assets_handle)
{

    ESP_LOGI(TAG, "=== Testing Image Function ===");

    gfx_emote_lock(emote_handle);

    gfx_obj_t *img_obj_bin = gfx_img_create(disp_default);
    gfx_obj_align(img_obj_bin, GFX_ALIGN_CENTER, 0, 0);
    gfx_disp_set_bg_enable(disp_default, false);

    // gfx_obj_t *label_obj_1 = gfx_label_create(disp_default);

    // gfx_obj_set_size(label_obj_1, 150, 60);
    // gfx_label_set_font(label_obj_1, (gfx_font_t)&font_puhui_16_4);

    // gfx_label_set_text(label_obj_1, "AAAAAAAAAAAA");
    // gfx_label_set_color(label_obj_1, GFX_COLOR_HEX(0x0000FF));
    // gfx_label_set_long_mode(label_obj_1, GFX_LABEL_LONG_SCROLL);
    // gfx_label_set_bg_color(label_obj_1, GFX_COLOR_HEX(0xFF0000));
    // gfx_label_set_bg_enable(label_obj_1, true);
    // gfx_obj_align(label_obj_1, GFX_ALIGN_TOP_MID, 0, 100);

    gfx_emote_unlock(emote_handle);

RETRY:
    int total_files = mmap_assets_get_stored_files(assets_handle);
    for (int i = 0; i < total_files; i++) {
        // for (int i = 0; i < 1; i++) {
        const void *img_data = mmap_assets_get_mem(assets_handle, i);
        // if (!img_data) {
        //     ESP_LOGE(TAG, "Failed to get image data: %d", i);
        //     continue;
        // }
        size_t img_size = mmap_assets_get_size(assets_handle, i);
        // if (img_size < sizeof(gfx_image_header_t)) {
        //     ESP_LOGE(TAG, "Failed to get image size: %d", i);
        //     continue;
        // }

        decode_jpeg_rgb565_from_memory(
            (const unsigned char *)img_data, img_size, img_obj_bin);
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
    goto RETRY;

    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);

    // gfx_obj_set_pos(img_obj_bin, 0, 0);

    gfx_obj_t *label_obj = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(label_obj);

    gfx_obj_set_size(label_obj, 150, 100);
    gfx_label_set_font(label_obj, (gfx_font_t)&font_puhui_16_4);

    gfx_label_set_text(label_obj, "AAAAAAAAAAAA");
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x0000FF));
    gfx_label_set_long_mode(label_obj, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_bg_color(label_obj, GFX_COLOR_HEX(0xFF0000));
    gfx_label_set_bg_enable(label_obj, true);
    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 100);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

RERTY2:

    gfx_emote_lock(emote_handle);

    gfx_label_set_text(label_obj, "BBBBBBBBBBBB");
    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 300);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);

    gfx_label_set_text(label_obj, "CCCCCCCCCCCC");
    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 100);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    goto RERTY2;
}

// TEST_CASE("test function obj image", "")


void test_image_function_main(void)
{
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = display_and_graphics_init("test_assets", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_image_function(assets_handle);

    display_and_graphics_clean(assets_handle);
}
