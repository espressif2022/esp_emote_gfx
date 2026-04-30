/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "common.h"
#include "widget/gfx_qrcode.h"

static const char *TAG = "test_qrcode";

typedef struct {
    const char *step_name;
    const char *payload;
    uint16_t size;
    gfx_qrcode_ecc_t ecc;
    uint32_t fg_rgb;
    uint32_t bg_rgb;
    uint8_t align;
    gfx_coord_t x_ofs;
    gfx_coord_t y_ofs;
    uint32_t observe_ms;
} test_qrcode_case_t;

typedef struct {
    gfx_obj_t *code_obj;
    gfx_obj_t *status_label;
} test_qrcode_scene_t;

static void test_qrcode_scene_cleanup(test_qrcode_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->status_label != NULL) {
        gfx_obj_delete(scene->status_label);
        scene->status_label = NULL;
    }
    if (scene->code_obj != NULL) {
        gfx_obj_delete(scene->code_obj);
        scene->code_obj = NULL;
    }
}

static void test_qrcode_apply_case(test_qrcode_scene_t *scene, const test_qrcode_case_t *test_case)
{
    TEST_ASSERT_EQUAL(ESP_OK, gfx_qrcode_set_data(scene->code_obj, test_case->payload));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_qrcode_set_size(scene->code_obj, test_case->size));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_qrcode_set_ecc(scene->code_obj, test_case->ecc));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_qrcode_set_color(scene->code_obj, GFX_COLOR_HEX(test_case->fg_rgb)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_qrcode_set_bg_color(scene->code_obj, GFX_COLOR_HEX(test_case->bg_rgb)));
    gfx_obj_align(scene->code_obj, test_case->align, test_case->x_ofs, test_case->y_ofs);
    gfx_label_set_text(scene->status_label, test_case->step_name);
}

static void test_qrcode_run(void)
{
    static const test_qrcode_case_t s_cases[] = {
        {
            .step_name = "Basic URL / ECC-M",
            .payload = "https://www.espressif.com",
            .size = 150,
            .ecc = GFX_QRCODE_ECC_MEDIUM,
            .fg_rgb = 0x000000,
            .bg_rgb = 0xFFFFFF,
            .align = GFX_ALIGN_CENTER,
            .x_ofs = 0,
            .y_ofs = -12,
            .observe_ms = 1800,
        },
        {
            .step_name = "ECC-H / warning palette",
            .payload = "Hello, QR Code!",
            .size = 128,
            .ecc = GFX_QRCODE_ECC_HIGH,
            .fg_rgb = 0xFF5A36,
            .bg_rgb = 0xFFF5E8,
            .align = GFX_ALIGN_CENTER,
            .x_ofs = 0,
            .y_ofs = -12,
            .observe_ms = 1800,
        },
        {
            .step_name = "Compact layout / ECC-L",
            .payload = "Size Test",
            .size = 96,
            .ecc = GFX_QRCODE_ECC_LOW,
            .fg_rgb = 0x1C5D99,
            .bg_rgb = 0xF2F7FF,
            .align = GFX_ALIGN_TOP_LEFT,
            .x_ofs = 12,
            .y_ofs = 12,
            .observe_ms = 1800,
        },
        {
            .step_name = "Large layout / ECC-Q",
            .payload = "Alignment Test",
            .size = 180,
            .ecc = GFX_QRCODE_ECC_QUARTILE,
            .fg_rgb = 0x0F7B0F,
            .bg_rgb = 0xF7FFF0,
            .align = GFX_ALIGN_BOTTOM_RIGHT,
            .x_ofs = -12,
            .y_ofs = -56,
            .observe_ms = 1800,
        },
        {
            .step_name = "Long payload / ECC-H",
            .payload = "This is a longer text payload used to validate QR generation stability.",
            .size = 196,
            .ecc = GFX_QRCODE_ECC_HIGH,
            .fg_rgb = 0x000000,
            .bg_rgb = 0xFFFFFF,
            .align = GFX_ALIGN_CENTER,
            .x_ofs = 0,
            .y_ofs = -12,
            .observe_ms = 2200,
        },
    };

    test_qrcode_scene_t scene = {0};

    test_app_log_case(TAG, "QR code widget validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    scene.code_obj = gfx_qrcode_create(disp_default);
    scene.status_label = gfx_label_create(disp_default);
    TEST_ASSERT_NOT_NULL(scene.code_obj);
    TEST_ASSERT_NOT_NULL(scene.status_label);

    gfx_obj_set_size(scene.status_label, 260, 28);
    gfx_obj_align(scene.status_label, GFX_ALIGN_BOTTOM_MID, 0, -10);
    gfx_label_set_font(scene.status_label, (gfx_font_t)&font_puhui_16_4);
    gfx_label_set_text_align(scene.status_label, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(scene.status_label, GFX_LABEL_LONG_WRAP);
    test_app_unlock();

    for (size_t i = 0; i < TEST_APP_ARRAY_SIZE(s_cases); ++i) {
        test_app_log_step(TAG, s_cases[i].step_name);
        TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
        test_qrcode_apply_case(&scene, &s_cases[i]);
        test_app_unlock();
        test_app_wait_for_observe(s_cases[i].observe_ms);
    }

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_qrcode_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("qrcode: render test case", "[widget][qrcode]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_qrcode_run();
    test_app_runtime_close(&runtime);
}
