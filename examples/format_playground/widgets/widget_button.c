/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

extern const gfx_image_dsc_t gfx_demo_image_button_released;
extern const gfx_image_dsc_t gfx_demo_image_button_pressed;

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

static void demo_image_button_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    if (scene->status != NULL) {
        (void)gfx_label_set_text(scene->status, "Image Button: released");
    }
}

static void demo_progress_bar_changed_cb(gfx_object_t *obj, uint16_t permille, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    (void)obj;
    if (scene == NULL) {
        return;
    }

    scene->progress_value = permille;
    if (scene->status != NULL) {
        (void)gfx_label_set_text_fmt(scene->status, "Progress: %u%%", (unsigned)((permille + 5U) / 10U));
    }
}

gfx_err_t gfx_format_demo_build_button(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_button_create(scene->disp);

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create button failed");
    scene->button_demo = obj;
    (void)gfx_object_set_pos(obj, 413, 206);
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

gfx_err_t gfx_format_demo_build_image_button(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_image_button_create(scene->disp);
    gfx_image_src_t released_src = {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &gfx_demo_image_button_released,
    };
    gfx_image_src_t pressed_src = {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &gfx_demo_image_button_pressed,
    };

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create image button failed");
    scene->image_button_demo = obj;
    (void)gfx_object_set_pos(obj, 408, 188);
    (void)gfx_object_set_size(obj, 260, 112);
    GFX_RETURN_ON_ERROR(gfx_image_button_set_src_released(obj, &released_src),
                        "playground", "set image button released src failed");
    GFX_RETURN_ON_ERROR(gfx_image_button_set_src_pressed(obj, &pressed_src),
                        "playground", "set image button pressed src failed");
    (void)gfx_image_button_set_text(obj, "Image Button");
    (void)gfx_image_button_set_font(obj, scene->font);
    (void)gfx_image_button_set_text_color(obj, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_image_button_set_text_align(obj, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_image_button_set_text_padding(obj, 18, 14);
    (void)gfx_object_set_touch_cb(obj, demo_image_button_touch_cb, scene);
    (void)gfx_object_set_visible(obj, false);
    return GFX_OK;
}

gfx_err_t gfx_format_demo_build_progress_bar(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_progress_bar_create(scene->disp);

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create progress bar failed");
    scene->progress_bar_demo = obj;
    (void)gfx_object_set_pos(obj, 404, 210);
    (void)gfx_object_set_size(obj, 268, 40);
    (void)gfx_progress_bar_set_colors(obj, GFX_COLOR_HEX(0x26313B), GFX_COLOR_HEX(0x2F8CFF));
    (void)gfx_progress_bar_set_thumb_style(obj, GFX_COLOR_HEX(0xF7FBFF), GFX_COLOR_HEX(0x86C8FF), 2);
    (void)gfx_progress_bar_set_radius(obj, 20);
    (void)gfx_progress_bar_set_fill_pad(obj, 4);
    (void)gfx_progress_bar_set_interactive(obj, true);
    (void)gfx_progress_bar_set_value_changed_cb(obj, demo_progress_bar_changed_cb, scene);
    scene->progress_value = 420;
    (void)gfx_progress_bar_set_value(obj, scene->progress_value);
    (void)gfx_object_set_visible(obj, false);
    return GFX_OK;
}
