/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

static void demo_button_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    if (scene->status != NULL) {
        (void)gfx_label_set_text(scene->status, "Button: pressed");
    }
}

gfx_err_t gfx_format_demo_build_button(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_button_create(scene->disp);

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create button failed");
    scene->button_demo = obj;
    (void)gfx_object_set_pos(obj, 298, 206);
    (void)gfx_object_set_size(obj, 250, 74);
    (void)gfx_button_set_text(obj, "Press Button");
    (void)gfx_button_set_font(obj, scene->font);
    (void)gfx_button_set_bg_color(obj, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_button_set_bg_color_pressed(obj, GFX_COLOR_HEX(0x2E7D32));
    (void)gfx_button_set_border_color(obj, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_button_set_border_width(obj, 2);
    (void)gfx_button_set_text_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_button_set_text_align(obj, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_object_set_touch_cb(obj, demo_button_touch_cb, scene);
    return GFX_OK;
}
