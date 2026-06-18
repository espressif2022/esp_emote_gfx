/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "playground_scene_priv.h"

#include "esp_check.h"

const char *const gfx_format_demo_cover_card_titles[DEMO_CARD_COUNT] = {
    "Misty Ridge",
    "Format Probe",
    "Warm Harbor",
    "Quiet Trail",
    "Night Lake",
};

static gfx_object_t *demo_create_cover_card(gfx_display_t *display, gfx_font_t font,
        const gfx_image_src_t *image_src, const char *title,
        gfx_object_t **out_image, gfx_object_t **out_label)
{
    gfx_object_t *card = gfx_container_create(display);
    gfx_object_t *image = gfx_mesh_img_create(display);
    gfx_object_t *label = gfx_label_create(display);

    if (card == NULL || image == NULL || label == NULL) {
        return NULL;
    }

    (void)gfx_container_set_bg_color(card, GFX_COLOR_HEX(0x17212B));
    (void)gfx_container_set_border_color(card, GFX_COLOR_HEX(0x7EC8E3));
    (void)gfx_container_set_border_width(card, 2);
    (void)gfx_mesh_img_set_source_rect(image, image_src, 120, 78);
    (void)gfx_label_set_font(label, font);
    (void)gfx_label_set_text(label, title);
    (void)gfx_label_set_color(label, GFX_COLOR_HEX(0xF3F7FA));
    (void)gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);

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

static void demo_coverflow_changed_cb(gfx_object_t *obj, int32_t index, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    (void)obj;
    if (scene != NULL && scene->status != NULL) {
        (void)gfx_label_set_text_fmt(scene->status, "Coverflow: item %ld", (long)index);
    }
}

esp_err_t gfx_format_demo_build_coverflow_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_coverflow_create(scene->disp);

    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, "playground", "create coverflow failed");
    scene->coverflow_demo = obj;
    (void)gfx_object_set_pos(obj, 258, 142);
    (void)gfx_object_set_size(obj, 374, 238);
    (void)gfx_coverflow_set_font(obj, scene->font);
    (void)gfx_coverflow_set_drag_threshold(obj, 8);
    (void)gfx_coverflow_set_page_threshold(obj, 50);
    (void)gfx_coverflow_set_zoom(obj, 112, 58);
    (void)gfx_coverflow_set_spacing(obj, 38);
    (void)gfx_coverflow_set_side_dim(obj, 92);
    (void)gfx_coverflow_set_bg_color(obj, GFX_COLOR_HEX(0x101418));
    (void)gfx_coverflow_set_center_color(obj, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_coverflow_set_side_color(obj, GFX_COLOR_HEX(0x1F2A35));
    (void)gfx_coverflow_set_text_color(obj, GFX_COLOR_HEX(0xF3F7FA));
    (void)gfx_coverflow_set_border_color(obj, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_coverflow_set_border_width(obj, 2);
    (void)gfx_coverflow_set_changed_cb(obj, demo_coverflow_changed_cb, scene);

    for (uint16_t i = 0; i < DEMO_CARD_COUNT; i++) {
        gfx_object_t *image = NULL;
        gfx_object_t *label = NULL;
        const gfx_image_src_t image_src = {
            .type = GFX_IMAGE_SRC_TYPE_FILE,
            .data = gfx_format_demo_flow_image_names[i],
        };
        scene->cover_cards[i] = demo_create_cover_card(scene->disp, scene->font, &image_src,
                                gfx_format_demo_cover_card_titles[i], &image, &label);
        ESP_RETURN_ON_FALSE(scene->cover_cards[i] != NULL, ESP_ERR_NO_MEM,
                            "playground", "create cover card failed");
        scene->cover_card_dsc[i] = (gfx_coverflow_card_dsc_t) {
            .card = scene->cover_cards[i],
            .image_slot = image,
            .title_slot = label,
        };
    }

    (void)gfx_coverflow_set_card_descriptors(obj, scene->cover_card_dsc, DEMO_CARD_COUNT);
    (void)gfx_coverflow_set_selected(obj, 1);
    return ESP_OK;
}
