/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "unity.h"
#include "common.h"

static const char *TAG = "test_label";

typedef struct {
    gfx_obj_t *title;
    gfx_obj_t *wrap_label;
    gfx_obj_t *scroll_label;
    gfx_obj_t *snap_label;
    gfx_obj_t *status_label;
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_font_t ft_font;
#endif
} test_label_scene_t;

static void test_label_delete_scene(test_label_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->title != NULL) {
        gfx_obj_delete(scene->title);
        scene->title = NULL;
    }
    if (scene->wrap_label != NULL) {
        gfx_obj_delete(scene->wrap_label);
        scene->wrap_label = NULL;
    }
    if (scene->scroll_label != NULL) {
        gfx_obj_delete(scene->scroll_label);
        scene->scroll_label = NULL;
    }
    if (scene->snap_label != NULL) {
        gfx_obj_delete(scene->snap_label);
        scene->snap_label = NULL;
    }
    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
}

static esp_err_t test_label_create_scene(mmap_assets_handle_t assets_handle, bool use_freetype, test_label_scene_t *scene)
{
    TEST_ASSERT_NOT_NULL(scene);
    memset(scene, 0, sizeof(*scene));

    TEST_ASSERT_NOT_NULL(disp_default);

    scene->title = gfx_label_create(disp_default);
    scene->wrap_label = gfx_label_create(disp_default);
    scene->scroll_label = gfx_label_create(disp_default);
    scene->snap_label = gfx_label_create(disp_default);
    scene->status_label = gfx_label_create(disp_default);

    TEST_ASSERT_NOT_NULL(scene->title);
    TEST_ASSERT_NOT_NULL(scene->wrap_label);
    TEST_ASSERT_NOT_NULL(scene->scroll_label);
    TEST_ASSERT_NOT_NULL(scene->snap_label);
    TEST_ASSERT_NOT_NULL(scene->status_label);

    gfx_font_t font = (gfx_font_t)&font_puhui_16_4;

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    if (use_freetype) {
        gfx_label_cfg_t font_cfg = {
            .name = "DejaVuSans.ttf",
            .mem = mmap_assets_get_mem(assets_handle, MMAP_ASSETS_TEST_DEJAVUSANS_TTF),
            .mem_size = (size_t)mmap_assets_get_size(assets_handle, MMAP_ASSETS_TEST_DEJAVUSANS_TTF),
            .font_size = 20,
        };

        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_new_font(&font_cfg, &scene->ft_font));
        font = scene->ft_font;
    }
#else
    (void)assets_handle;
    (void)use_freetype;
#endif

    gfx_label_set_font(scene->title, font);
    gfx_label_set_font(scene->wrap_label, font);
    gfx_label_set_font(scene->scroll_label, font);
    gfx_label_set_font(scene->snap_label, font);
    gfx_label_set_font(scene->status_label, font);

    gfx_obj_set_size(scene->title, 300, 28);
    gfx_obj_set_size(scene->wrap_label, 280, 78);
    gfx_obj_set_size(scene->scroll_label, 190, 36);
    gfx_obj_set_size(scene->snap_label, 190, 36);
    gfx_obj_set_size(scene->status_label, 280, 40);

    gfx_label_set_bg_enable(scene->title, true);
    gfx_label_set_bg_enable(scene->wrap_label, true);
    gfx_label_set_bg_enable(scene->scroll_label, true);
    gfx_label_set_bg_enable(scene->snap_label, true);
    gfx_label_set_bg_enable(scene->status_label, true);

    gfx_label_set_bg_color(scene->title, GFX_COLOR_HEX(0x10243F));
    gfx_label_set_bg_color(scene->wrap_label, GFX_COLOR_HEX(0x251B3A));
    gfx_label_set_bg_color(scene->scroll_label, GFX_COLOR_HEX(0x1D3A2D));
    gfx_label_set_bg_color(scene->snap_label, GFX_COLOR_HEX(0x3A2416));
    gfx_label_set_bg_color(scene->status_label, GFX_COLOR_HEX(0x2C2C2C));

    gfx_label_set_color(scene->title, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_color(scene->wrap_label, GFX_COLOR_HEX(0xF7D7FF));
    gfx_label_set_color(scene->scroll_label, GFX_COLOR_HEX(0xCFFFE0));
    gfx_label_set_color(scene->snap_label, GFX_COLOR_HEX(0xFFE6B8));
    gfx_label_set_color(scene->status_label, GFX_COLOR_HEX(0xFFFFFF));

    gfx_label_set_text(scene->title, use_freetype ? "Label Validation / FreeType" : "Label Validation / Bitmap");
    gfx_label_set_text_align(scene->title, GFX_TEXT_ALIGN_CENTER);

    gfx_label_set_text(scene->wrap_label,
                       "WRAP: AAAA AAAA 乐鑫乐鑫 AAAA BBBB CCCC 乐鑫乐鑫 DDDD EEEE FFFF");
    gfx_label_set_long_mode(scene->wrap_label, GFX_LABEL_LONG_WRAP);

    gfx_label_set_text(scene->scroll_label,
                       "SCROLL: AAAA乐鑫AAAA乐鑫BBBB乐鑫BBBB乐鑫CCCC乐鑫CCCC");
    gfx_label_set_long_mode(scene->scroll_label, GFX_LABEL_LONG_SCROLL);

    gfx_label_set_text(scene->snap_label,
                       "SNAP: alpha beta gamma delta 乐鑫 epsilon zeta eta theta");
    gfx_label_set_long_mode(scene->snap_label, GFX_LABEL_LONG_SCROLL_SNAP);

    gfx_label_set_text(scene->status_label,
                       "Observe wrap / scroll / snap smoothness.\nThen watch text update.");
    gfx_label_set_long_mode(scene->status_label, GFX_LABEL_LONG_WRAP);

    gfx_obj_align(scene->title, GFX_ALIGN_TOP_MID, 0, 8);
    gfx_obj_align(scene->wrap_label, GFX_ALIGN_TOP_MID, 0, 48);
    gfx_obj_align(scene->scroll_label, GFX_ALIGN_TOP_MID, 0, 136);
    gfx_obj_align(scene->snap_label, GFX_ALIGN_TOP_MID, 0, 184);
    gfx_obj_align(scene->status_label, GFX_ALIGN_TOP_MID, 0, 232);

    return ESP_OK;
}

static void test_label_validation_run(mmap_assets_handle_t assets_handle, bool use_freetype)
{
    test_label_scene_t scene = {0};

    test_app_log_case(TAG, use_freetype ? "Label Validation: FreeType" : "Label Validation: Bitmap");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, test_label_create_scene(assets_handle, use_freetype, &scene));
    test_app_unlock();

    test_app_wait_for_observe(2500);

    test_app_log_step(TAG, "Update repeated-text content to trigger glyph-cache reuse");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_text(scene.scroll_label,
                       "SCROLL: 乐鑫乐鑫乐鑫 AAAA AAAA BBBB BBBB CCCC CCCC 乐鑫乐鑫");
    gfx_label_set_text(scene.wrap_label,
                       "WRAP: cache cache cache AAAA AAAA 乐鑫乐鑫 cache cache cache");
    gfx_label_set_text(scene.status_label,
                       "Step 2: repeated glyphs updated.\nCheck whether redraw remains stable.");
    test_app_unlock();

    test_app_wait_for_observe(2500);

    test_app_log_step(TAG, "Switch colors and snap content");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_label_set_color(scene.scroll_label, GFX_COLOR_HEX(0xFFF07A));
    gfx_label_set_color(scene.snap_label, GFX_COLOR_HEX(0x90E7FF));
    gfx_label_set_text(scene.snap_label,
                       "SNAP: one two three four five 乐鑫 six seven eight nine ten");
    gfx_label_set_text(scene.status_label,
                       "Step 3: snap content changed.\nWatch word-boundary paging.");
    test_app_unlock();

    test_app_wait_for_observe(3000);

    test_app_log_step(TAG, "Cleanup label validation scene");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_label_delete_scene(&scene);
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    if (use_freetype && scene.ft_font != NULL) {
        gfx_label_delete_font(scene.ft_font);
        scene.ft_font = NULL;
    }
#endif
    test_app_unlock();
}

TEST_CASE("label: bitmap font scene", "[widget][label][bitmap]")
{
    test_app_runtime_t runtime;
    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));

    test_label_validation_run(runtime.assets_handle, false);

    test_app_runtime_close(&runtime);
}

TEST_CASE("label: freetype renderer", "[widget][label][freetype]")
{
    test_app_runtime_t runtime;
    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));

    test_label_validation_run(runtime.assets_handle, true);

    test_app_runtime_close(&runtime);
}
