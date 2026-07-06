/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

const char *const gfx_format_demo_list_items[] = {
    "JPEG decode", "Status card", "Quick action", "Network", "Display", "Storage", "About",
    "Audio", "Battery", "Weather", "Schedule", "Messages", "System",
};

const uint16_t gfx_format_demo_list_item_count =
    (uint16_t)(sizeof(gfx_format_demo_list_items) / sizeof(gfx_format_demo_list_items[0]));

static void demo_list_demo_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || focused_index < 0) {
        return;
    }

    (void)gfx_list_set_selected(obj, focused_index);

    if (scene->status != NULL) {
        const char *text = gfx_list_get_item_text(obj, (uint16_t)focused_index);
        (void)gfx_label_set_text_fmt(scene->status, "List: %s", text != NULL ? text : "Unknown");
    }
}

static void demo_list_demo_select_cb(gfx_object_t *obj, int32_t selected_index,
                                     bool confirmed, void *user_data)
{
    (void)confirmed;
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || selected_index < 0) {
        return;
    }

    if (scene->status != NULL) {
        const char *text = gfx_list_get_item_text(obj, (uint16_t)selected_index);
        (void)gfx_label_set_text_fmt(scene->status, "List: %s", text != NULL ? text : "Unknown");
    }
}

gfx_err_t gfx_format_demo_build_list_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_list_create(scene->disp);

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create list demo failed");
    scene->list_demo = obj;
    (void)gfx_object_set_pos(obj, 386, 146);
    (void)gfx_object_set_size(obj, 304, 252);
    (void)gfx_list_set_items(obj, gfx_format_demo_list_items, gfx_format_demo_list_item_count);
    (void)gfx_list_set_font(obj, scene->font);
    (void)gfx_list_set_item_height(obj, 46);
    (void)gfx_list_set_text_pad(obj, 14, 8);
    (void)gfx_list_set_bg_color(obj, GFX_COLOR_HEX(0x171D24));
    (void)gfx_list_set_focus_bg_color(obj, GFX_COLOR_HEX(0xF1C40F));
    (void)gfx_list_set_selected_bg_color(obj, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_list_set_pressed_bg_color(obj, GFX_COLOR_HEX(0x3F6FA3));
    (void)gfx_list_set_text_color(obj, GFX_COLOR_HEX(0xDCE4EC));
    (void)gfx_list_set_focus_text_color(obj, GFX_COLOR_HEX(0x101418));
    (void)gfx_list_set_selected_text_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_list_set_pressed_text_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_list_set_border_color(obj, GFX_COLOR_HEX(0x3F5163));
    (void)gfx_list_set_border_width(obj, 2);
    (void)gfx_list_set_drag_threshold(obj, 8);
    (void)gfx_list_set_snap_to_item(obj, true);
    (void)gfx_list_set_focus_cb(obj, demo_list_demo_focus_cb, scene);
    (void)gfx_list_set_select_cb(obj, demo_list_demo_select_cb, scene);
    (void)gfx_list_set_selected(obj, 0);
    (void)gfx_list_set_focus(obj, 0);
    return GFX_OK;
}
