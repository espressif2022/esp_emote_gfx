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

static const char *TAG = "test_image";

static void test_image_run(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc;

    test_app_log_case(TAG, "Image Widget Demo");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());

    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_obj_t *img_obj_c_array = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj_c_array);

    gfx_img_set_src(img_obj_c_array, (void *)&icon_rgb565);
    gfx_obj_set_pos(img_obj_c_array, 100, 100);

    test_app_unlock();
    test_app_wait_ms(2000);

    test_app_log_step(TAG, "Move image with set_pos");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_set_pos(img_obj_c_array, 200, 100);
    test_app_unlock();
    test_app_wait_ms(2000);

    test_app_log_step(TAG, "Reposition image with align");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_align(img_obj_c_array, GFX_ALIGN_CENTER, 0, 0);
    test_app_unlock();
    test_app_wait_ms(2000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_delete(img_obj_c_array);

    gfx_obj_t *img_obj_bin = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj_bin);

    esp_err_t ret = load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565A8_BIN, &img_dsc);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    gfx_img_set_src(img_obj_bin, (void *)&img_dsc);
    gfx_obj_set_pos(img_obj_bin, 100, 180);
    test_app_unlock();
    test_app_wait_ms(2000);

    test_app_log_step(TAG, "Reload mmap-backed image");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565A8_BIN, &img_dsc);
    gfx_img_set_src(img_obj_bin, (void *)&img_dsc);
    test_app_unlock();
    test_app_wait_ms(6000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_delete(img_obj_bin);

    test_app_log_step(TAG, "Show multiple image formats");
    gfx_obj_t *img_obj1 = gfx_img_create(disp_default);
    gfx_obj_t *img_obj2 = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(img_obj1);
    TEST_ASSERT_NOT_NULL(img_obj2);

    gfx_img_set_src(img_obj1, (void *)&icon_rgb565A8);

    ret = load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565_BIN, &img_dsc);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    gfx_img_set_src(img_obj2, (void *)&img_dsc);

    gfx_obj_set_pos(img_obj1, 150, 100);
    gfx_obj_set_pos(img_obj2, 150, 180);

    test_app_unlock();
    test_app_wait_ms(3000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_delete(img_obj1);
    gfx_obj_delete(img_obj2);
    test_app_unlock();
}

TEST_CASE("gfx demo: image widget", "[demo][image]")
{
    test_app_runtime_t runtime;
    esp_err_t ret = test_app_runtime_open(&runtime);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_image_run(runtime.assets_handle);

    test_app_runtime_close(&runtime);
}
