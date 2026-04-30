/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"

static const char *TAG = "test_image";

typedef struct {
    gfx_obj_t *img_primary;
    gfx_obj_t *img_secondary;
} test_image_scene_t;

static void test_image_scene_cleanup(test_image_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->img_primary != NULL) {
        gfx_obj_delete(scene->img_primary);
        scene->img_primary = NULL;
    }
    if (scene->img_secondary != NULL) {
        gfx_obj_delete(scene->img_secondary);
        scene->img_secondary = NULL;
    }
}

static void test_image_run(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc = {0};
    const gfx_img_src_t c_array_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &icon_rgb565,
    };
    const gfx_img_src_t c_array_a8_src = {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &icon_rgb565A8,
    };
    test_image_scene_t scene = {0};

    test_app_log_case(TAG, "Image widget validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene.img_primary = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.img_primary);
    gfx_img_set_src_desc(scene.img_primary, &c_array_src);
    gfx_obj_set_pos(scene.img_primary, 100, 100);
    test_app_unlock();

    test_app_wait_for_observe(1500);

    test_app_log_step(TAG, "Move C-array image");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_set_pos(scene.img_primary, 200, 100);
    test_app_unlock();

    test_app_wait_for_observe(1500);

    test_app_log_step(TAG, "Center align C-array image");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_align(scene.img_primary, GFX_ALIGN_CENTER, 0, -40);
    test_app_unlock();

    test_app_wait_for_observe(1500);

    test_app_log_step(TAG, "Switch to mmap-backed source");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_obj_delete(scene.img_primary);
    scene.img_primary = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.img_primary);
    TEST_ASSERT_EQUAL(ESP_OK, load_image(assets_handle, MMAP_ASSETS_TEST_ICON_RGB565A8_BIN, &img_dsc));
    gfx_img_set_src_desc(scene.img_primary, &(gfx_img_src_t) {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &img_dsc,
    });
    gfx_obj_set_pos(scene.img_primary, 100, 160);
    test_app_unlock();

    test_app_wait_for_observe(1800);

    test_app_log_step(TAG, "Reload mmap-backed source");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, load_image(assets_handle, MMAP_ASSETS_TEST_ICON_RGB565A8_BIN, &img_dsc));
    gfx_img_set_src_desc(scene.img_primary, &(gfx_img_src_t) {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &img_dsc,
    });
    test_app_unlock();

    test_app_wait_for_observe(1800);

    test_app_log_step(TAG, "Compare RGB565 and RGB565A8");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    scene.img_secondary = gfx_img_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.img_secondary);
    gfx_img_set_src_desc(scene.img_primary, &c_array_a8_src);
    TEST_ASSERT_EQUAL(ESP_OK, load_image(assets_handle, MMAP_ASSETS_TEST_ICON_RGB565_BIN, &img_dsc));
    gfx_img_set_src_desc(scene.img_secondary, &(gfx_img_src_t) {
        .type = GFX_IMG_SRC_TYPE_IMAGE_DSC,
        .data = &img_dsc,
    });
    gfx_obj_set_pos(scene.img_primary, 90, 90);
    gfx_obj_set_pos(scene.img_secondary, 90, 180);
    test_app_unlock();

    test_app_wait_for_observe(2500);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_image_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("image: source render set", "[widget][image]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_image_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
