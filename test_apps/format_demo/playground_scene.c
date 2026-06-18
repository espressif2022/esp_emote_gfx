/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>

#include "esp_check.h"
#include "gfx.h"
#include "lvgl.h"

#include "../main/claw_motion.inc"
#include "playground_scene.h"
#include "playground_scene_priv.h"

extern const lv_font_t font_puhui_16_4;

static format_playground_scene_t s_scene;

static gfx_asset_store_t *s_demo_asset_store;

esp_err_t gfx_format_demo_set_asset_store(gfx_asset_store_t *store)
{
    if (store != s_demo_asset_store) {
        s_demo_asset_store = store;
        gfx_asset_set_default_store(store);
    }
    return ESP_OK;
}

static esp_err_t demo_apply(gfx_err_t err)
{
    return err == GFX_OK ? ESP_OK : ESP_FAIL;
}

const char *gfx_format_demo_widget_name(uint16_t widget_idx)
{
    if (widget_idx < DEMO_WIDGET_COUNT) {
        return gfx_format_demo_widget_names[widget_idx];
    }
    return "Unknown";
}

const char *gfx_format_demo_action_name(uint16_t action_idx)
{
    if (action_idx < gfx_format_demo_action_name_count) {
        return gfx_format_demo_action_names[action_idx];
    }
    return "Unknown";
}

static void demo_set_object_visible(gfx_object_t *obj, bool visible)
{
    if (obj != NULL) {
        (void)gfx_object_set_visible(obj, visible);
    }
}

static gfx_object_t *demo_create_label(gfx_display_t *display, gfx_font_t font,
                                       gfx_coord_t x, gfx_coord_t y, uint16_t w, uint16_t h,
                                       const char *text, gfx_color_t color)
{
    gfx_object_t *label = gfx_label_create(display);

    if (label == NULL) {
        return NULL;
    }

    (void)gfx_object_set_pos(label, x, y);
    (void)gfx_object_set_size(label, w, h);
    (void)gfx_label_set_text(label, text);
    (void)gfx_label_set_font(label, font);
    (void)gfx_label_set_color(label, color);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
    return label;
}

static void demo_hide_preview_objects(format_playground_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    demo_set_object_visible(scene->button_demo, false);
    demo_set_object_visible(scene->list_demo, false);
    demo_set_object_visible(scene->image_demo, false);
    demo_set_object_visible(scene->image_button, false);
    demo_set_object_visible(scene->anim_jpg_demo, false);
    demo_set_object_visible(scene->anim_jpg_button, false);
    demo_set_object_visible(scene->wheel_demo, false);
    demo_set_object_visible(scene->pageflow_demo, false);
    demo_set_object_visible(scene->coverflow_demo, false);
    demo_set_object_visible(scene->preview_note, false);
    if (scene->motion != NULL) {
        (void)gfx_motion_player_set_visible(scene->motion, false);
    }
}

void gfx_format_demo_update_status(format_playground_scene_t *scene)
{
    if (scene == NULL || scene->status == NULL) {
        return;
    }

    (void)gfx_label_set_text_fmt(scene->status,
                                 "Widget: %s   Motion: %s   Output: %s",
                                 gfx_format_demo_widget_name(scene->widget_idx),
                                 gfx_format_demo_action_name(scene->action_idx),
                                 scene->format_tag != NULL ? scene->format_tag : "Unknown");
}

static void demo_apply_action(format_playground_scene_t *scene, bool snap)
{
    if (scene == NULL || scene->motion == NULL) {
        return;
    }

    if (scene->action_idx >= CLAW_MOTION_ACTION_COUNT) {
        scene->action_idx = 0;
    }
    (void)gfx_motion_player_set_action(scene->motion, scene->action_idx, snap);
    gfx_format_demo_update_status(scene);
}

void gfx_format_demo_apply_widget_focus(format_playground_scene_t *scene, uint16_t widget_idx)
{
    if (scene == NULL) {
        return;
    }

    if (widget_idx >= DEMO_WIDGET_COUNT) {
        widget_idx = 0;
    }

    scene->widget_idx = widget_idx;
    demo_hide_preview_objects(scene);

    if (scene->preview_title != NULL) {
        (void)gfx_label_set_text_fmt(scene->preview_title, "%s Preview", gfx_format_demo_widget_name(widget_idx));
    }

    switch ((demo_widget_id_t)widget_idx) {
    case DEMO_WIDGET_BUTTON:
        demo_set_object_visible(scene->button_demo, true);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "Tap the button to confirm touch and color.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_LIST:
        demo_set_object_visible(scene->list_demo, true);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "Scroll and select list rows.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_IMAGE:
        demo_set_object_visible(scene->image_demo, true);
        demo_set_object_visible(scene->image_button, true);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "JPEG from mmap store (FILE), SPIFFS (fread) or assets_test mapping (MEM); tap to switch.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_ANIM_JPG:
        demo_set_object_visible(scene->anim_jpg_demo, true);
        demo_set_object_visible(scene->anim_jpg_button, gfx_format_demo_anim_asset_count() > 1U);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "AAF/EAF anim from mmap store, SPIFFS (fread) or mmap mapping (MEM); tap to switch.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_MOTION:
        if (scene->motion != NULL) {
            (void)gfx_motion_player_set_visible(scene->motion, true);
        }
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text_fmt(scene->preview_note, "Current action: %s",
                                         gfx_format_demo_action_name(scene->action_idx));
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_WHEEL:
        demo_set_object_visible(scene->wheel_demo, true);
        break;
    case DEMO_WIDGET_PAGEFLOW:
        demo_set_object_visible(scene->pageflow_demo, true);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "Swipe to the format probe image and compare RGB565 vs RGB888 banding.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    case DEMO_WIDGET_COVERFLOW:
        demo_set_object_visible(scene->coverflow_demo, true);
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text(scene->preview_note, "Center card starts on the format probe image for color-depth comparison.");
            demo_set_object_visible(scene->preview_note, true);
        }
        break;
    default:
        break;
    }

    gfx_format_demo_update_status(scene);
}

esp_err_t gfx_format_demo_build_playground_scene(gfx_display_t *disp, const char *title_text,
        const char *format_tag)
{
    uint32_t hres;
    uint32_t vres;

    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, "playground", "display is null");

    memset(&s_scene, 0, sizeof(s_scene));
    s_scene.disp = disp;
    s_scene.font = (gfx_font_t)&font_puhui_16_4;
    s_scene.format_tag = format_tag;

    hres = gfx_display_get_h_res(disp);
    vres = gfx_display_get_v_res(disp);
    (void)vres;

    ESP_RETURN_ON_ERROR(demo_apply(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x101418))),
                        "playground", "set bg failed");

    s_scene.header_title = demo_create_label(disp, s_scene.font, 22, 18, (uint16_t)(hres - 44U), 34,
                           title_text != NULL ? title_text : "GFX Format Playground",
                           GFX_COLOR_HEX(0xF3F7FA));
    s_scene.header_tag = demo_create_label(disp, s_scene.font, 22, 56, 220, 24, "Widgets",
                                           GFX_COLOR_HEX(0xA8B3BD));
    s_scene.preview_title = demo_create_label(disp, s_scene.font, 276, 56, 340, 24, "Button Preview",
                            GFX_COLOR_HEX(0xA8B3BD));
    s_scene.preview_note = demo_create_label(disp, s_scene.font, 276, 88, 360, 42, "",
                           GFX_COLOR_HEX(0xDCE4EC));
    s_scene.status = demo_create_label(disp, s_scene.font, 276, 430, 360, 26, "",
                                       GFX_COLOR_HEX(0xA8B3BD));

    ESP_RETURN_ON_FALSE(s_scene.header_title != NULL && s_scene.header_tag != NULL &&
                        s_scene.preview_title != NULL && s_scene.preview_note != NULL &&
                        s_scene.status != NULL, ESP_ERR_NO_MEM, "playground", "create labels failed");

    if (s_scene.header_tag != NULL && format_tag != NULL) {
        (void)gfx_label_set_text_fmt(s_scene.header_tag, "Widgets   %s", format_tag);
    }

    ESP_RETURN_ON_ERROR(gfx_format_demo_build_widget_list(&s_scene), "playground", "build widget list failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_button(&s_scene), "playground", "build button failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_list_demo(&s_scene), "playground", "build list failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_image_demo(&s_scene), "playground", "build image failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_anim_jpg_demo(&s_scene), "playground", "build anim jpg failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_motion_demo(&s_scene), "playground", "build motion failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_wheel_demo(&s_scene), "playground", "build wheel failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_pageflow_demo(&s_scene), "playground", "build pageflow failed");
    ESP_RETURN_ON_ERROR(gfx_format_demo_build_coverflow_demo(&s_scene), "playground", "build coverflow failed");

    s_scene.action_idx = 0;
    demo_apply_action(&s_scene, true);
    gfx_format_demo_apply_widget_focus(&s_scene, 0);
    return ESP_OK;
}
