/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

static void demo_pageflow_changed_cb(gfx_object_t *obj, int32_t page_index, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    (void)obj;
    if (scene != NULL && scene->status != NULL) {
        (void)gfx_label_set_text_fmt(scene->status, "Pageflow: page %ld", (long)page_index);
    }
}

gfx_err_t gfx_format_demo_build_pageflow_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj = gfx_pageflow_create(scene->disp);
    gfx_image_src_t sources[DEMO_CARD_COUNT];

    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_INVALID_ARG, "playground", "create pageflow failed");
    for (uint16_t i = 0; i < DEMO_CARD_COUNT; i++) {
        sources[i] = (gfx_image_src_t) {
            .type = GFX_IMAGE_SRC_TYPE_FILE,
            .data = gfx_format_demo_flow_image_names[i],
        };
    }
    scene->pageflow_demo = obj;
    (void)gfx_object_set_pos(obj, 372, 138);
    (void)gfx_object_set_size(obj, 332, 244);
    GFX_RETURN_ON_ERROR(gfx_pageflow_set_image_sources(obj, sources, DEMO_CARD_COUNT),
                        "playground", "set pageflow image sources failed");
    (void)gfx_pageflow_set_font(obj, scene->font);
    (void)gfx_pageflow_set_drag_threshold(obj, 8);
    (void)gfx_pageflow_set_page_threshold(obj, 56);
    (void)gfx_pageflow_set_bg_color(obj, GFX_COLOR_HEX(0x101418));
    (void)gfx_pageflow_set_page_color(obj, GFX_COLOR_HEX(0x1F2A35));
    (void)gfx_pageflow_set_text_color(obj, GFX_COLOR_HEX(0xF3F7FA));
    (void)gfx_pageflow_set_border_color(obj, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_pageflow_set_border_width(obj, 2);
    (void)gfx_pageflow_set_changed_cb(obj, demo_pageflow_changed_cb, scene);
    (void)gfx_pageflow_set_page(obj, 1);
    return GFX_OK;
}
