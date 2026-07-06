/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "unity.h"
#include "common.h"

static const char *const TAG = "test_coverflow";

#define COVERFLOW_ITEM_COUNT 4U
#define COVERFLOW_IMG_W 160U
#define COVERFLOW_IMG_H 106U

typedef struct {
    gfx_object_t *title;
    gfx_object_t *coverflow;
    gfx_object_t *status;
    gfx_object_t *cards[COVERFLOW_ITEM_COUNT];
    gfx_coverflow_card_dsc_t card_dsc[COVERFLOW_ITEM_COUNT];
} test_coverflow_scene_t;

static uint8_t s_cover_pixels[COVERFLOW_ITEM_COUNT][COVERFLOW_IMG_W * COVERFLOW_IMG_H * 3U];
static gfx_image_dsc_t s_cover_images[COVERFLOW_ITEM_COUNT];
static const gfx_image_dsc_t *s_cover_image_ptrs[COVERFLOW_ITEM_COUNT];

static const char *const s_cover_titles[COVERFLOW_ITEM_COUNT] = {
    "Misty Ridge",
    "Warm Harbor",
    "Quiet Trail",
    "Night Lake",
};

static uint8_t cover_mix_u8(uint8_t a, uint8_t b, uint32_t t, uint32_t max_t)
{
    if (max_t == 0U) {
        return b;
    }

    return (uint8_t)(((uint32_t)a * (max_t - t) + (uint32_t)b * t) / max_t);
}

static void cover_put_px(uint8_t item, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    size_t offset;

    if (item >= COVERFLOW_ITEM_COUNT || x >= COVERFLOW_IMG_W || y >= COVERFLOW_IMG_H) {
        return;
    }

    offset = ((size_t)y * COVERFLOW_IMG_W + x) * 3U;
    s_cover_pixels[item][offset + 0U] = r;
    s_cover_pixels[item][offset + 1U] = g;
    s_cover_pixels[item][offset + 2U] = b;
}

static void cover_generate_image(uint8_t item)
{
    const uint8_t sky_top[4][3] = {
        {62, 136, 212},
        {242, 149, 92},
        {80, 150, 132},
        {27, 38, 82},
    };
    const uint8_t sky_bottom[4][3] = {
        {176, 219, 238},
        {253, 203, 138},
        {183, 213, 190},
        {86, 98, 148},
    };
    const uint8_t water[4][3] = {
        {44, 122, 170},
        {60, 124, 150},
        {72, 142, 112},
        {24, 57, 116},
    };
    const uint32_t sky_h = 46U;
    const uint32_t ridge_h = 28U;
    const uint32_t lake_y0 = sky_h + ridge_h;

    for (uint32_t y = 0; y < COVERFLOW_IMG_H; y++) {
        for (uint32_t x = 0; x < COVERFLOW_IMG_W; x++) {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            int32_t xi = (int32_t)x;
            int32_t yi = (int32_t)y;

            if (y < sky_h) {
                r = cover_mix_u8(sky_top[item][0], sky_bottom[item][0], y, sky_h - 1U);
                g = cover_mix_u8(sky_top[item][1], sky_bottom[item][1], y, sky_h - 1U);
                b = cover_mix_u8(sky_top[item][2], sky_bottom[item][2], y, sky_h - 1U);

                int32_t sun_x = 116 - (int32_t)item * 14;
                int32_t sun_y = 20 + (int32_t)item * 3;
                int32_t dx = xi - sun_x;
                int32_t dy = yi - sun_y;
                int32_t dist2 = dx * dx + dy * dy;
                if (dist2 < 14 * 14) {
                    r = 255U;
                    g = (item == 3U) ? 221U : 232U;
                    b = (item == 3U) ? 154U : 108U;
                }
            } else if (y < lake_y0) {
                int32_t peak_a = 74 - abs(xi - (42 + (int32_t)item * 11)) * 34 / 50;
                int32_t peak_b = 78 - abs(xi - (104 - (int32_t)item * 7)) * 32 / 58;
                int32_t ridge = peak_a > peak_b ? peak_a : peak_b;

                if (yi <= ridge) {
                    r = (uint8_t)(52U + item * 12U + (y - sky_h) * 2U);
                    g = (uint8_t)(72U + item * 10U + (y - sky_h) * 2U);
                    b = (uint8_t)(88U + item * 14U + (y - sky_h) * 2U);
                    if ((xi > peak_a - 18 && xi < peak_a + 18 && y < sky_h + 12U) ||
                            (xi > peak_b - 20 && xi < peak_b + 20 && y < sky_h + 10U)) {
                        r = 224U;
                        g = 232U;
                        b = 226U;
                    }
                } else {
                    r = (uint8_t)(132U + item * 11U);
                    g = (uint8_t)(172U + item * 8U);
                    b = (uint8_t)(162U + item * 7U);
                }
            } else if (y < 90U) {
                uint32_t t = y - lake_y0;
                r = cover_mix_u8(water[item][0], 28U, t, 90U - lake_y0);
                g = cover_mix_u8(water[item][1], 82U, t, 90U - lake_y0);
                b = cover_mix_u8(water[item][2], 132U, t, 90U - lake_y0);

                if (((x + y * 3U + item * 7U) % 19U) < 3U) {
                    r = (uint8_t)(r + 18U);
                    g = (uint8_t)(g + 18U);
                    b = (uint8_t)(b + 14U);
                }
            } else if (y < 96U) {
                r = (uint8_t)(168U + item * 10U);
                g = (uint8_t)(136U + item * 6U);
                b = 82U;
            } else {
                r = (uint8_t)(46U + item * 10U);
                g = (uint8_t)(106U + item * 16U);
                b = (uint8_t)(60U + item * 8U);
                if (((x * 5U + y * 3U + item * 13U) % 29U) < 5U) {
                    r = (uint8_t)(r + 24U);
                    g = (uint8_t)(g + 28U);
                }
            }

            cover_put_px(item, x, y, r, g, b);
        }
    }
}

static void cover_images_init(void)
{
    for (uint8_t i = 0; i < COVERFLOW_ITEM_COUNT; i++) {
        cover_generate_image(i);
        s_cover_images[i] = (gfx_image_dsc_t) {
            .header = {
                .magic = GFX_IMAGE_HEADER_MAGIC,
                .cf = GFX_COLOR_FORMAT_RGB888,
                .w = COVERFLOW_IMG_W,
                .h = COVERFLOW_IMG_H,
                .stride = COVERFLOW_IMG_W * 3U,
            },
            .data_size = sizeof(s_cover_pixels[i]),
            .data = s_cover_pixels[i],
        };
        s_cover_image_ptrs[i] = &s_cover_images[i];
    }
}

static gfx_object_t *cover_create_label(gfx_display_t *display, const char *text, gfx_color_t color)
{
    gfx_object_t *label = gfx_label_create(display);

    if (label == NULL) {
        return NULL;
    }

    (void)gfx_label_set_font(label, (gfx_font_t)&font_puhui_16_4);
    (void)gfx_label_set_text(label, text);
    (void)gfx_label_set_color(label, color);
    (void)gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
    return label;
}

static gfx_object_t *cover_create_card(gfx_display_t *display, const gfx_image_dsc_t *image_dsc,
                                       const char *title, gfx_object_t **out_image,
                                       gfx_object_t **out_label)
{
    gfx_object_t *card = gfx_container_create(display);
    gfx_object_t *image = gfx_mesh_img_create(display);
    gfx_object_t *label = cover_create_label(display, title, GFX_COLOR_HEX(0xF3F7FA));

    if (card == NULL || image == NULL || label == NULL) {
        if (card != NULL) {
            gfx_object_delete(card);
        }
        if (image != NULL) {
            gfx_object_delete(image);
        }
        if (label != NULL) {
            gfx_object_delete(label);
        }
        return NULL;
    }

    (void)gfx_container_set_bg_color(card, GFX_COLOR_HEX(0x17212B));
    (void)gfx_container_set_border_color(card, GFX_COLOR_HEX(0x7EC8E3));
    (void)gfx_container_set_border_width(card, 2);

    (void)gfx_mesh_img_set_image_rect(image, image_dsc, 120, 78);

    (void)gfx_object_add_child(card, image);
    (void)gfx_object_add_child(card, label);
    (void)gfx_object_set_visible(card, false);
    if (out_image != NULL) {
        *out_image = image;
    }
    if (out_label != NULL) {
        *out_label = label;
    }
    return card;
}

static void cover_changed_cb(gfx_object_t *obj, int32_t index, void *user_data)
{
    test_coverflow_scene_t *scene = (test_coverflow_scene_t *)user_data;

    (void)obj;
    if (scene == NULL || scene->status == NULL) {
        return;
    }
    if (index < 0 || index >= (int32_t)COVERFLOW_ITEM_COUNT) {
        (void)gfx_label_set_text(scene->status, "coverflow: unknown");
        return;
    }

    (void)gfx_label_set_text_fmt(scene->status, "coverflow: %s  index=%ld",
                                 s_cover_titles[index], (long)index);
}

static void cover_scene_cleanup(test_coverflow_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (scene->title != NULL) {
        gfx_object_delete(scene->title);
        scene->title = NULL;
    }
    if (scene->status != NULL) {
        gfx_object_delete(scene->status);
        scene->status = NULL;
    }
    if (scene->coverflow != NULL) {
        gfx_object_delete(scene->coverflow);
        scene->coverflow = NULL;
    }
}

static void cover_scene_create(test_coverflow_scene_t *scene)
{
    const uint32_t hres = gfx_display_get_h_res(disp_default);
    const uint32_t vres = gfx_display_get_v_res(disp_default);
    const uint16_t cover_w = (hres >= 640U) ? 420U : (uint16_t)(hres - 24U);
    const uint16_t cover_h = (vres >= 480U) ? 250U : (uint16_t)(vres - 82U);

    cover_images_init();

    scene->title = cover_create_label(disp_default, "Coverflow Card Preview", GFX_COLOR_HEX(0xFFFFFF));
    scene->coverflow = gfx_coverflow_create(disp_default);
    scene->status = cover_create_label(disp_default, "coverflow: ready", GFX_COLOR_HEX(0xA8B3BD));

    TEST_ASSERT_NOT_NULL(scene->title);
    TEST_ASSERT_NOT_NULL(scene->coverflow);
    TEST_ASSERT_NOT_NULL(scene->status);

    (void)gfx_object_set_size(scene->title, (uint16_t)(hres > 24U ? hres - 24U : hres), 28);
    (void)gfx_object_align(scene->title, GFX_ALIGN_TOP_MID, 0, 14);

    (void)gfx_object_set_size(scene->coverflow, cover_w, cover_h);
    (void)gfx_object_align(scene->coverflow, GFX_ALIGN_CENTER, 0, 0);
    (void)gfx_coverflow_set_font(scene->coverflow, (gfx_font_t)&font_puhui_16_4);
    (void)gfx_coverflow_set_drag_threshold(scene->coverflow, 8);
    (void)gfx_coverflow_set_page_threshold(scene->coverflow, (uint16_t)(cover_w / 8U));
    (void)gfx_coverflow_set_zoom(scene->coverflow, 112, 58);
    (void)gfx_coverflow_set_spacing(scene->coverflow, 38);
    (void)gfx_coverflow_set_side_dim(scene->coverflow, 92);
    (void)gfx_coverflow_set_bg_color(scene->coverflow, GFX_COLOR_HEX(0x101418));
    (void)gfx_coverflow_set_center_color(scene->coverflow, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_coverflow_set_side_color(scene->coverflow, GFX_COLOR_HEX(0x1F2A35));
    (void)gfx_coverflow_set_text_color(scene->coverflow, GFX_COLOR_HEX(0xF3F7FA));
    (void)gfx_coverflow_set_border_color(scene->coverflow, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_coverflow_set_border_width(scene->coverflow, 2);
    (void)gfx_coverflow_set_changed_cb(scene->coverflow, cover_changed_cb, scene);

    for (uint16_t i = 0; i < COVERFLOW_ITEM_COUNT; i++) {
        gfx_object_t *image = NULL;
        gfx_object_t *label = NULL;
        scene->cards[i] = cover_create_card(disp_default, s_cover_image_ptrs[i], s_cover_titles[i],
                                            &image, &label);
        TEST_ASSERT_NOT_NULL(scene->cards[i]);
        scene->card_dsc[i] = (gfx_coverflow_card_dsc_t) {
            .card = scene->cards[i],
            .image_slot = image,
            .title_slot = label,
        };
    }
    TEST_ASSERT_EQUAL(GFX_OK, gfx_coverflow_set_card_descriptors(scene->coverflow, scene->card_dsc,
                      COVERFLOW_ITEM_COUNT));
    TEST_ASSERT_EQUAL(GFX_OK, gfx_coverflow_set_selected(scene->coverflow, 1));

    (void)gfx_object_set_size(scene->status, (uint16_t)(hres > 24U ? hres - 24U : hres), 28);
    (void)gfx_object_align(scene->status, GFX_ALIGN_BOTTOM_MID, 0, -12);
}

// TEST_CASE("coverflow: card scene preview", "[widget][coverflow][card]")
void test_coverflow_card_scene_preview(void)
{
    test_app_runtime_t runtime;
    test_coverflow_scene_t scene = {0};

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));

    test_app_log_case(TAG, "Coverflow card scene preview");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    (void)gfx_display_set_bg_color(disp_default, GFX_COLOR_HEX(0x080B0F));
    cover_scene_create(&scene);
    test_app_unlock();

    test_app_wait_for_observe(2500);

    for (uint16_t i = 0; i < COVERFLOW_ITEM_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
        TEST_ASSERT_EQUAL(GFX_OK, gfx_coverflow_set_selected(scene.coverflow, (int32_t)i));
        test_app_unlock();
        test_app_wait_for_observe(1700);
    }

    test_app_log_step(TAG, "Touch drag coverflow now");
    // test_app_wait_for_observe(10000);
    test_app_wait_for_observe(1000 * 100000);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    cover_scene_cleanup(&scene);
    test_app_unlock();

    test_app_runtime_close(&runtime);
}
