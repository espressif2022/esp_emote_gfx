/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stdlib.h>

#include "unity.h"
#include "common/gfx_types_priv.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/container.h"
#include "lib/eaf/gfx_eaf_dec.h"
#include "mmap_generate_assets_test.h"

#include "harness/gfx_test_assets_fixture.h"
#include "harness/gfx_test_core_fixture.h"

static const uint16_t TEST_ANIM_OFFSCREEN_W = 320;
static const uint16_t TEST_ANIM_OFFSCREEN_H = 240;
static const uint16_t TEST_ANIM_JPEG_X = 0;
static const uint16_t TEST_ANIM_JPEG_Y = 0;
static const uint16_t TEST_ANIM_PALETTE_X = 160;
static const uint16_t TEST_ANIM_PALETTE_Y = 0;
static const uint16_t TEST_ANIM_UI_X = 0;
static const uint16_t TEST_ANIM_UI_Y = 180;
static const uint16_t TEST_ANIM_UI_W = 48;
static const uint16_t TEST_ANIM_UI_H = 24;
static const gfx_color_t TEST_ANIM_BG_COLOR = {.full = 0x2226};
static const gfx_color_t TEST_ANIM_UI_COLOR = {.full = 0x4bea};

static esp_err_t test_anim_decode_first_frame_rgb565(mmap_assets_handle_t assets_handle, int asset_id,
        uint16_t **out_pixels, uint16_t *out_w, uint16_t *out_h)
{
    const uint8_t *anim_data = (const uint8_t *)mmap_assets_get_mem(assets_handle, asset_id);
    size_t anim_size = mmap_assets_get_size(assets_handle, asset_id);
    eaf_dec_handle_t eaf = NULL;
    eaf_dec_header_t header = {0};
    eaf_dec_type_t type;
    uint16_t *pixels = NULL;
    esp_err_t ret;

    TEST_ASSERT_NOT_NULL(anim_data);
    TEST_ASSERT_GREATER_THAN_UINT32(0, anim_size);
    TEST_ASSERT_NOT_NULL(out_pixels);
    TEST_ASSERT_NOT_NULL(out_w);
    TEST_ASSERT_NOT_NULL(out_h);

    ret = eaf_dec_init(anim_data, anim_size, &eaf);
    if (ret != ESP_OK) {
        return ret;
    }

    type = eaf_dec_get_frame_info(eaf, 0, &header);
    if (type != EAF_DEC_TYPE_VALID) {
        eaf_dec_deinit(eaf);
        return ESP_ERR_INVALID_RESPONSE;
    }

    pixels = calloc((size_t)header.width * header.height, sizeof(uint16_t));
    if (pixels == NULL) {
        eaf_dec_free_header(&header);
        eaf_dec_deinit(eaf);
        return ESP_ERR_NO_MEM;
    }

    ret = eaf_dec_decode_frame(eaf, 0, (uint8_t *)pixels, (size_t)header.width * header.height * sizeof(uint16_t));
    if (ret == ESP_OK) {
        *out_pixels = pixels;
        *out_w = header.width;
        *out_h = header.height;
    } else {
        free(pixels);
    }

    eaf_dec_free_header(&header);
    eaf_dec_deinit(eaf);
    return ret;
}

static void test_anim_find_sample_pixel(const uint16_t *pixels, uint16_t width, uint16_t height,
                                        uint16_t avoid_color, uint16_t *out_x, uint16_t *out_y, uint16_t *out_color)
{
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint16_t color = pixels[(size_t)y * width + x];
            if (color != avoid_color) {
                *out_x = x;
                *out_y = y;
                *out_color = color;
                return;
            }
        }
    }

    *out_x = width / 2U;
    *out_y = height / 2U;
    *out_color = pixels[(size_t)(*out_y) * width + *out_x];
}

static void test_anim_expect_rgb565_pixel(const uint8_t *buf, uint16_t width,
        gfx_color_format_t format, uint16_t x, uint16_t y, uint16_t expected)
{
    const uint8_t *px = buf + (((size_t)y * width) + x) * 2U;
    uint8_t expected_bytes[2];

    gfx_color_write_rgb565_bytes(expected_bytes, format, expected);
    TEST_ASSERT_EQUAL_HEX16(expected, gfx_color_read_rgb565_bytes(px, format));
    TEST_ASSERT_EQUAL_HEX8(expected_bytes[0], px[0]);
    TEST_ASSERT_EQUAL_HEX8(expected_bytes[1], px[1]);
}

static void test_anim_jpeg_palette_mix_assert(gfx_test_core_ctx_t *core,
        mmap_assets_handle_t assets_handle, gfx_color_format_t format)
{
    uint16_t *jpeg_pixels = NULL;
    uint16_t *palette_pixels = NULL;
    uint16_t jpeg_w = 0;
    uint16_t jpeg_h = 0;
    uint16_t palette_w = 0;
    uint16_t palette_h = 0;
    uint16_t jpeg_sample_x = 0;
    uint16_t jpeg_sample_y = 0;
    uint16_t jpeg_sample_color = 0;
    uint16_t palette_sample_x = 0;
    uint16_t palette_sample_y = 0;
    uint16_t palette_sample_color = 0;
    gfx_backend_t *backend = NULL;
    gfx_display_t *disp = NULL;
    gfx_object_t *jpeg_anim = NULL;
    gfx_object_t *palette_anim = NULL;
    gfx_object_t *ui_box = NULL;
    const uint8_t *buf = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, test_anim_decode_first_frame_rgb565(assets_handle,
                      MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF, &jpeg_pixels, &jpeg_w, &jpeg_h));
    TEST_ASSERT_EQUAL(ESP_OK, test_anim_decode_first_frame_rgb565(assets_handle,
                      MMAP_ASSETS_TEST_MI_1_EYE_8BIT_EAF, &palette_pixels, &palette_w, &palette_h));

    test_anim_find_sample_pixel(jpeg_pixels, jpeg_w, jpeg_h, TEST_ANIM_BG_COLOR.full,
                                &jpeg_sample_x, &jpeg_sample_y, &jpeg_sample_color);
    test_anim_find_sample_pixel(palette_pixels, palette_w, palette_h, TEST_ANIM_BG_COLOR.full,
                                &palette_sample_x, &palette_sample_y, &palette_sample_color);

    backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = TEST_ANIM_OFFSCREEN_W,
        .v_res = TEST_ANIM_OFFSCREEN_H,
        .color_format = format,
    });
    TEST_ASSERT_NOT_NULL(backend);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_core_lock(core));
    disp = gfx_display_add(core->handle, &(gfx_display_config_t) {
        .h_res = TEST_ANIM_OFFSCREEN_W,
        .v_res = TEST_ANIM_OFFSCREEN_H,
        .color_format = format,
        .backend = backend,
        .flags = {
        },
        .buffers = {
            .buf_pixels = (size_t)TEST_ANIM_OFFSCREEN_W * TEST_ANIM_OFFSCREEN_H,
        },
    });
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL(GFX_OK, gfx_display_set_bg_color(disp, TEST_ANIM_BG_COLOR));

    jpeg_anim = gfx_anim_create(disp);
    palette_anim = gfx_anim_create(disp);
    ui_box = gfx_container_create(disp);
    TEST_ASSERT_NOT_NULL(jpeg_anim);
    TEST_ASSERT_NOT_NULL(palette_anim);
    TEST_ASSERT_NOT_NULL(ui_box);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_anim_set_src_desc(jpeg_anim, &(gfx_anim_src_t) {
        .type = GFX_ANIM_SRC_TYPE_MEMORY,
        .data = mmap_assets_get_mem(assets_handle, MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF),
        .data_len = mmap_assets_get_size(assets_handle, MMAP_ASSETS_TEST_MI_1_EYE_24BIT_AAF),
    }));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_anim_set_src_desc(palette_anim, &(gfx_anim_src_t) {
        .type = GFX_ANIM_SRC_TYPE_MEMORY,
        .data = mmap_assets_get_mem(assets_handle, MMAP_ASSETS_TEST_MI_1_EYE_8BIT_EAF),
        .data_len = mmap_assets_get_size(assets_handle, MMAP_ASSETS_TEST_MI_1_EYE_8BIT_EAF),
    }));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_pos(jpeg_anim, TEST_ANIM_JPEG_X, TEST_ANIM_JPEG_Y));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_pos(palette_anim, TEST_ANIM_PALETTE_X, TEST_ANIM_PALETTE_Y));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_pos(ui_box, TEST_ANIM_UI_X, TEST_ANIM_UI_Y));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_object_set_size(ui_box, TEST_ANIM_UI_W, TEST_ANIM_UI_H));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_container_set_bg_color(ui_box, TEST_ANIM_UI_COLOR));
    gfx_test_core_unlock(core);

    TEST_ASSERT_EQUAL(GFX_OK, gfx_core_refresh_now(core->handle));

    buf = (const uint8_t *)gfx_memory_backend_get_buffer_data(backend);
    TEST_ASSERT_NOT_NULL(buf);
    test_anim_expect_rgb565_pixel(buf, TEST_ANIM_OFFSCREEN_W, format,
                                  (uint16_t)(TEST_ANIM_JPEG_X + jpeg_sample_x),
                                  (uint16_t)(TEST_ANIM_JPEG_Y + jpeg_sample_y),
                                  jpeg_sample_color);
    test_anim_expect_rgb565_pixel(buf, TEST_ANIM_OFFSCREEN_W, format,
                                  (uint16_t)(TEST_ANIM_PALETTE_X + palette_sample_x),
                                  (uint16_t)(TEST_ANIM_PALETTE_Y + palette_sample_y),
                                  palette_sample_color);
    test_anim_expect_rgb565_pixel(buf, TEST_ANIM_OFFSCREEN_W, format,
                                  (uint16_t)(TEST_ANIM_UI_X + (TEST_ANIM_UI_W / 2U)),
                                  (uint16_t)(TEST_ANIM_UI_Y + (TEST_ANIM_UI_H / 2U)),
                                  TEST_ANIM_UI_COLOR.full);
    test_anim_expect_rgb565_pixel(buf, TEST_ANIM_OFFSCREEN_W, format,
                                  TEST_ANIM_OFFSCREEN_W - 1U, TEST_ANIM_OFFSCREEN_H - 1U,
                                  TEST_ANIM_BG_COLOR.full);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_core_lock(core));
    gfx_display_delete(disp);
    gfx_test_core_unlock(core);

    free(jpeg_pixels);
    free(palette_pixels);
}

TEST_CASE("anim: jpeg and palette frame keep rgb565 output contract", "[unit][integration][anim][format]")
{
    gfx_test_core_ctx_t core = {0};
    gfx_test_assets_ctx_t assets = {0};

    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_assets_open(&assets, GFX_TEST_ASSETS_PARTITION_DEFAULT,
                      MMAP_ASSETS_TEST_FILES, MMAP_ASSETS_TEST_CHECKSUM));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_core_open(&core));

    test_anim_jpeg_palette_mix_assert(&core, assets.handle, GFX_COLOR_FORMAT_RGB565);

    gfx_test_core_close(&core);
    gfx_test_assets_close(&assets);
}

TEST_CASE("anim: jpeg and palette frame keep rgb565 swapped output contract", "[unit][integration][anim][format]")
{
    gfx_test_core_ctx_t core = {0};
    gfx_test_assets_ctx_t assets = {0};

    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_assets_open(&assets, GFX_TEST_ASSETS_PARTITION_DEFAULT,
                      MMAP_ASSETS_TEST_FILES, MMAP_ASSETS_TEST_CHECKSUM));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_test_core_open(&core));

    test_anim_jpeg_palette_mix_assert(&core, assets.handle, GFX_COLOR_FORMAT_RGB565_SWAPPED);

    gfx_test_core_close(&core);
    gfx_test_assets_close(&assets);
}
