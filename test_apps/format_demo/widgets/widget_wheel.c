/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "playground_scene_priv.h"

#include "esp_check.h"

const char *const gfx_format_demo_wheel_items[] = {
    "Low", "Medium", "High", "Turbo", "Sleep", "Focus", "Play",
};

const uint16_t gfx_format_demo_wheel_item_count =
    (uint16_t)(sizeof(gfx_format_demo_wheel_items) / sizeof(gfx_format_demo_wheel_items[0]));

static void demo_wheel_value_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;
    const char *text;

    if (obj == NULL || scene == NULL || selected_index < 0) {
        return;
    }

    text = gfx_wheel_get_item_text(obj, (uint16_t)selected_index);
    if (scene->status != NULL) {
        (void)gfx_label_set_text_fmt(scene->status, "Wheel: %s   index=%ld",
                                     text != NULL ? text : "Unknown", (long)selected_index);
    }
}

esp_err_t gfx_format_demo_build_wheel_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_wheel_create(scene->disp);

    ESP_RETURN_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, "playground", "create wheel failed");
    scene->wheel_demo = obj;
    (void)gfx_object_set_pos(obj, 292, 146);
    (void)gfx_object_set_size(obj, 300, 252);
    (void)gfx_wheel_set_items(obj, gfx_format_demo_wheel_items, gfx_format_demo_wheel_item_count);
    (void)gfx_wheel_set_font(obj, scene->font);
    (void)gfx_wheel_set_item_height(obj, 48);
    (void)gfx_wheel_set_visible_rows(obj, 5);
    (void)gfx_wheel_set_cyclic(obj, true);
    (void)gfx_wheel_set_drag_threshold(obj, 8);
    (void)gfx_wheel_set_bg_color(obj, GFX_COLOR_HEX(0x171D24));
    (void)gfx_wheel_set_text_color(obj, GFX_COLOR_HEX(0x9AA7B2));
    (void)gfx_wheel_set_center_bg_color(obj, GFX_COLOR_HEX(0xF1C40F));
    (void)gfx_wheel_set_center_text_color(obj, GFX_COLOR_HEX(0x101418));
    (void)gfx_wheel_set_border_color(obj, GFX_COLOR_HEX(0x3F5163));
    (void)gfx_wheel_set_border_width(obj, 2);
    (void)gfx_wheel_set_value_cb(obj, demo_wheel_value_cb, scene);
    (void)gfx_wheel_set_selected(obj, 1);
    return ESP_OK;
}
