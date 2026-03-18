/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"

static const char *TAG = "test_multi_obj";

typedef struct {
    gfx_obj_t *anim_obj;
    gfx_obj_t *img_obj;
    gfx_obj_t *label_obj;
    gfx_timer_handle_t timer;
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_font_t ft_font;
#endif
} test_multi_obj_scene_t;

static void test_multi_obj_scene_cleanup(test_multi_obj_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->timer != NULL) {
        gfx_timer_delete(emote_handle, scene->timer);
        scene->timer = NULL;
    }
    if (scene->anim_obj != NULL) {
        gfx_obj_delete(scene->anim_obj);
        scene->anim_obj = NULL;
    }
    if (scene->label_obj != NULL) {
        gfx_obj_delete(scene->label_obj);
        scene->label_obj = NULL;
    }
    if (scene->img_obj != NULL) {
        gfx_obj_delete(scene->img_obj);
        scene->img_obj = NULL;
    }
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    if (scene->ft_font != NULL) {
        gfx_label_delete_font(scene->ft_font);
        scene->ft_font = NULL;
    }
#endif
}

static void test_multi_obj_configure_font(test_multi_obj_scene_t *scene, mmap_assets_handle_t assets_handle)
{
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .mem_size = (size_t)mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .font_size = 20,
    };

    TEST_ASSERT_EQUAL(ESP_OK, gfx_label_new_font(&font_cfg, &scene->ft_font));
    gfx_label_set_font(scene->label_obj, scene->ft_font);
#else
    (void)assets_handle;
    gfx_label_set_font(scene->label_obj, (gfx_font_t)&font_puhui_16_4);
#endif
}

static void test_multi_obj_run(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc = {0};
    test_multi_obj_scene_t scene = {0};
    const void *anim_data = NULL;
    size_t anim_size = 0;

    test_app_log_case(TAG, "Multi-widget scene validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene.anim_obj = gfx_anim_create(disp_default);
    scene.img_obj = gfx_img_create(disp_default);
    scene.label_obj = gfx_label_create(disp_default);
    scene.timer = gfx_timer_create(emote_handle, clock_tm_callback, 5000, scene.label_obj);

    TEST_ASSERT_NOT_NULL(scene.anim_obj);
    TEST_ASSERT_NOT_NULL(scene.img_obj);
    TEST_ASSERT_NOT_NULL(scene.label_obj);
    TEST_ASSERT_NOT_NULL(scene.timer);

    anim_data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    anim_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(scene.anim_obj, anim_data, anim_size));
    gfx_obj_align(scene.anim_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(scene.anim_obj, 0, 30, 15, true);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(scene.anim_obj));

    test_multi_obj_configure_font(&scene, assets_handle);
    gfx_obj_set_size(scene.label_obj, 220, 52);
    gfx_label_set_text(scene.label_obj, "Multi-object scene");
    gfx_label_set_color(scene.label_obj, GFX_COLOR_HEX(0xFF5A36));
    gfx_label_set_text_align(scene.label_obj, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(scene.label_obj, GFX_LABEL_LONG_SCROLL);
    gfx_obj_align(scene.label_obj, GFX_ALIGN_BOTTOM_MID, 0, -4);

    TEST_ASSERT_EQUAL(ESP_OK, load_image(assets_handle, MMAP_TEST_ASSETS_ICON_RGB565_BIN, &img_dsc));
    gfx_img_set_src(scene.img_obj, (void *)&img_dsc);
    gfx_obj_align(scene.img_obj, GFX_ALIGN_TOP_MID, 0, 8);
    test_app_unlock();

    test_app_wait_for_observe(5000);

    test_app_log_step(TAG, "Update label and image placement");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.label_obj, "Animation + image + timer");
    gfx_obj_align(scene.img_obj, GFX_ALIGN_TOP_LEFT, 12, 12);
    test_app_unlock();

    test_app_wait_for_observe(3500);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_multi_obj_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("multi widget scene", "[widget][multi]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime));
    test_multi_obj_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
