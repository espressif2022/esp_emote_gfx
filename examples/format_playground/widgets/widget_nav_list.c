/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

const char *const gfx_format_demo_widget_names[DEMO_WIDGET_COUNT] = {
    "Button",
    "List",
    "Image",
    "Anim",
    "Motion",
    "Wheel",
    "Pageflow",
    "Coverflow",
};

static void demo_widget_list_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || focused_index < 0 || focused_index >= (int32_t)DEMO_WIDGET_COUNT) {
        return;
    }

    scene->widget_idx = (uint16_t)focused_index;
    (void)gfx_list_set_selected(obj, focused_index);
    gfx_format_demo_apply_widget_focus(scene, scene->widget_idx);
}

static void demo_widget_list_select_cb(gfx_object_t *obj, int32_t selected_index,
                                       bool confirmed, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || selected_index < 0 || selected_index >= (int32_t)DEMO_WIDGET_COUNT) {
        return;
    }

    (void)confirmed;
    scene->widget_idx = (uint16_t)selected_index;
    gfx_format_demo_apply_widget_focus(scene, scene->widget_idx);
}

gfx_err_t gfx_format_demo_build_widget_list(format_playground_scene_t *scene)
{
    gfx_object_t *list = gfx_list_create(scene->disp);

    GFX_RETURN_ON_FALSE(list != NULL, GFX_ERR_INVALID_ARG, "playground", "create widget list failed");

    scene->widget_list = list;
    (void)gfx_object_set_pos(list, 22, 112);
    (void)gfx_object_set_size(list, 214, 348);
    (void)gfx_list_set_item_height(list, 46);
    (void)gfx_list_set_text_pad(list, 14, 8);
    (void)gfx_list_set_font(list, scene->font);
    (void)gfx_list_set_bg_color(list, GFX_COLOR_HEX(0x161C22));
    (void)gfx_list_set_focus_bg_color(list, GFX_COLOR_HEX(0x2E7D32));
    (void)gfx_list_set_selected_bg_color(list, GFX_COLOR_HEX(0x24476A));
    (void)gfx_list_set_pressed_bg_color(list, GFX_COLOR_HEX(0x375D7D));
    (void)gfx_list_set_text_color(list, GFX_COLOR_HEX(0xDCE4EC));
    (void)gfx_list_set_focus_text_color(list, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_list_set_selected_text_color(list, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_list_set_pressed_text_color(list, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_list_set_border_color(list, GFX_COLOR_HEX(0x40515F));
    (void)gfx_list_set_border_width(list, 2);
    (void)gfx_list_set_drag_threshold(list, 8);
    (void)gfx_list_set_snap_to_item(list, true);
    (void)gfx_list_set_focus_cb(list, demo_widget_list_focus_cb, scene);
    (void)gfx_list_set_select_cb(list, demo_widget_list_select_cb, scene);
    for (uint16_t i = 0; i < DEMO_WIDGET_COUNT; i++) {
        (void)gfx_list_add_item(list, gfx_format_demo_widget_names[i]);
    }
    (void)gfx_list_set_focus(list, 0);
    (void)gfx_list_set_selected(list, 0);
    return GFX_OK;
}
