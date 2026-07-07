/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "common/gfx_check.h"
#include "gfx.h"
#include "lvgl.h"

#include "claw_motion.inc"
#include "playground_scene.h"
#include "playground_scene_priv.h"

#define GFX_FORMAT_DEMO_LOOSE_MOUNT_PREFIX  "/spiffs"
#define GFX_FORMAT_DEMO_LOOSE_HOST_ROOT     "examples/esp/format_rgb565/spiffs_anim"
#define GFX_FORMAT_DEMO_SPIFFS_PARTITION    "storage"

#ifndef GFX_HOST_BUILD
#include "esp_spiffs.h"
#endif

#define DEMO_PREVIEW_X              276
#define DEMO_PREVIEW_BUTTON_W       190
#define DEMO_PREVIEW_BUTTON_H        42
#define DEMO_PREVIEW_BOTTOM_MARGIN   22
#define DEMO_PREVIEW_FPS_W          150

extern const lv_font_t font_puhui_16_4;

static format_playground_scene_t s_scene;

static gfx_asset_source_t *s_demo_asset_fs;
static gfx_asset_source_t *s_demo_loose_fs;

gfx_err_t gfx_format_demo_set_asset_fs(gfx_asset_source_t *fs)
{
    if (fs != s_demo_asset_fs) {
        s_demo_asset_fs = fs;
        (void)gfx_fs_mount("", fs);
    }
    return GFX_OK;
}

gfx_err_t gfx_format_demo_mount_loose_assets(void)
{
    gfx_err_t err;

    if (s_demo_loose_fs != NULL) {
        return GFX_OK;
    }

#ifndef GFX_HOST_BUILD
    if (!esp_spiffs_mounted(GFX_FORMAT_DEMO_SPIFFS_PARTITION)) {
        const esp_vfs_spiffs_conf_t spiffs_conf = {
            .base_path = GFX_FORMAT_DEMO_LOOSE_MOUNT_PREFIX,
            .partition_label = GFX_FORMAT_DEMO_SPIFFS_PARTITION,
            .max_files = 4,
            .format_if_mount_failed = false,
        };
        esp_err_t esp_err = esp_vfs_spiffs_register(&spiffs_conf);
        if (esp_err != ESP_OK) {
            return GFX_FAIL;
        }
    }
    err = gfx_fs_open_dir(GFX_FORMAT_DEMO_LOOSE_MOUNT_PREFIX, &s_demo_loose_fs);
#else
    err = gfx_fs_open_dir(GFX_FORMAT_DEMO_LOOSE_HOST_ROOT, &s_demo_loose_fs);
#endif
    if (err != GFX_OK) {
        return err;
    }

    err = gfx_fs_mount(GFX_FORMAT_DEMO_LOOSE_MOUNT_PREFIX, s_demo_loose_fs);
    if (err != GFX_OK) {
        gfx_fs_close(s_demo_loose_fs);
        s_demo_loose_fs = NULL;
    }
    return err;
}

static gfx_err_t demo_apply(gfx_err_t err)
{
    return err == GFX_OK ? GFX_OK : GFX_FAIL;
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

static void demo_apply_action(format_playground_scene_t *scene, bool snap);

static gfx_object_t *demo_create_label(gfx_display_t *display, gfx_font_t font,
                                       gfx_coord_t x, gfx_coord_t y, uint16_t w, uint16_t h,
                                       const char *text, gfx_color_t color,
                                       gfx_label_long_mode_t long_mode)
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
    (void)gfx_label_set_long_mode(label, long_mode);
    if (long_mode == GFX_LABEL_LONG_SCROLL) {
        (void)gfx_label_set_scroll_loop(label, true);
        (void)gfx_label_set_scroll_speed(label, 40);
    }
    return label;
}

static void demo_preview_button_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    if (obj == NULL || scene == NULL || event == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    switch ((demo_widget_id_t)scene->widget_idx) {
    case DEMO_WIDGET_IMAGE:
        (void)gfx_format_demo_next_image_clip(scene);
        break;
    case DEMO_WIDGET_ANIM:
        (void)gfx_format_demo_next_anim_clip(scene);
        break;
    case DEMO_WIDGET_MOTION:
        scene->action_idx = (uint16_t)((scene->action_idx + 1U) % gfx_format_demo_action_name_count);
        demo_apply_action(scene, true);
        break;
    default:
        break;
    }
}

static gfx_object_t *demo_create_preview_button(gfx_display_t *display, gfx_font_t font,
        uint16_t preview_w, gfx_coord_t y)
{
    gfx_object_t *button = gfx_button_create(display);
    gfx_coord_t x;

    if (button == NULL) {
        return NULL;
    }

    x = DEMO_PREVIEW_X;
    if (preview_w > DEMO_PREVIEW_BUTTON_W) {
        x += (gfx_coord_t)((preview_w - DEMO_PREVIEW_BUTTON_W) / 2U);
    }
    (void)gfx_object_set_pos(button, x, y);
    (void)gfx_object_set_size(button, DEMO_PREVIEW_BUTTON_W, DEMO_PREVIEW_BUTTON_H);
    (void)gfx_button_set_font(button, font);
    (void)gfx_button_set_bg_color(button, GFX_COLOR_HEX(0x245C8F));
    (void)gfx_button_set_bg_color_pressed(button, GFX_COLOR_HEX(0x2E7D32));
    (void)gfx_button_set_border_color(button, GFX_COLOR_HEX(0x76B7E8));
    (void)gfx_button_set_border_width(button, 2);
    (void)gfx_button_set_text_color(button, GFX_COLOR_HEX(0xFFFFFF));
    (void)gfx_button_set_text_align(button, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_object_set_touch_cb(button, demo_preview_button_cb, &s_scene);
    (void)gfx_object_set_visible(button, false);
    return button;
}

static void demo_set_preview_note(format_playground_scene_t *scene, const char *text)
{
    if (scene != NULL && scene->preview_note != NULL) {
        (void)gfx_label_set_text(scene->preview_note, text != NULL ? text : "");
        demo_set_object_visible(scene->preview_note, true);
    }
}

static void demo_set_preview_next_button(format_playground_scene_t *scene, const char *label,
        uint16_t index, uint16_t count)
{
    if (scene == NULL || scene->preview_button == NULL) {
        return;
    }

    if (count <= 1U || label == NULL) {
        demo_set_object_visible(scene->preview_button, false);
        return;
    }

    (void)gfx_button_set_text_fmt(scene->preview_button, "%s  %u/%u",
                                  label, (unsigned)(index + 1U), (unsigned)count);
    demo_set_object_visible(scene->preview_button, true);
}

static void demo_hide_preview_objects(format_playground_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    demo_set_object_visible(scene->button_demo, false);
    demo_set_object_visible(scene->image_button_demo, false);
    demo_set_object_visible(scene->progress_bar_demo, false);
    demo_set_object_visible(scene->list_demo, false);
    demo_set_object_visible(scene->image_demo, false);
    demo_set_object_visible(scene->anim_demo, false);
    demo_set_object_visible(scene->wheel_demo, false);
    demo_set_object_visible(scene->pick_stamp_demo, false);
    for (size_t i = 0; i < sizeof(scene->pick_stamp_date) / sizeof(scene->pick_stamp_date[0]); i++) {
        demo_set_object_visible(scene->pick_stamp_date[i], false);
    }
    for (size_t i = 0; i < sizeof(scene->pick_stamp_time) / sizeof(scene->pick_stamp_time[0]); i++) {
        demo_set_object_visible(scene->pick_stamp_time[i], false);
    }
    demo_set_object_visible(scene->pageflow_demo, false);
    demo_set_object_visible(scene->coverflow_demo, false);
    demo_set_object_visible(scene->motion_zoom_slider, false);
    demo_set_object_visible(scene->preview_note, false);
    demo_set_object_visible(scene->preview_button, false);
    if (scene->motion != NULL) {
        (void)gfx_motion_player_set_visible(scene->motion, false);
    }
}

void gfx_format_demo_update_status(format_playground_scene_t *scene)
{
    (void)scene;
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
    if (scene->widget_idx == DEMO_WIDGET_MOTION) {
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text_fmt(scene->preview_note, "Current action: %s",
                                         gfx_format_demo_action_name(scene->action_idx));
        }
        demo_set_preview_next_button(scene, "Next Action",
                                     scene->action_idx,
                                     gfx_format_demo_action_name_count);
    }
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
        demo_set_preview_note(scene, "Tap the button to confirm touch and color.");
        break;
    case DEMO_WIDGET_IMAGE_BUTTON:
        demo_set_object_visible(scene->image_button_demo, true);
        demo_set_preview_note(scene, "Press to swap the background image while keeping text and touch feedback.");
        break;
    case DEMO_WIDGET_PROGRESS_BAR:
        demo_set_object_visible(scene->progress_bar_demo, true);
        demo_set_preview_note(scene, "Single widget version of the care scene long-press progress track and fill.");
        break;
    case DEMO_WIDGET_LIST:
        demo_set_object_visible(scene->list_demo, true);
        demo_set_preview_note(scene, "Scroll and select list rows.");
        break;
    case DEMO_WIDGET_IMAGE:
        demo_set_object_visible(scene->image_demo, true);
        demo_set_preview_note(scene, gfx_format_demo_image_clip_note());
        demo_set_preview_next_button(scene, "Next Image",
                                     (uint16_t)gfx_format_demo_image_clip_index(),
                                     (uint16_t)gfx_format_demo_image_clip_count());
        break;
    case DEMO_WIDGET_ANIM:
        demo_set_object_visible(scene->anim_demo, true);
        demo_set_preview_note(scene, gfx_format_demo_anim_clip_note());
        demo_set_preview_next_button(scene, "Next Anim",
                                     (uint16_t)gfx_format_demo_anim_clip_index(),
                                     (uint16_t)gfx_format_demo_anim_clip_count());
        break;
    case DEMO_WIDGET_MOTION:
        demo_set_object_visible(scene->motion_zoom_slider, true);
        if (scene->motion != NULL) {
            (void)gfx_motion_player_set_visible(scene->motion, true);
        }
        if (scene->preview_note != NULL) {
            (void)gfx_label_set_text_fmt(scene->preview_note, "Current action: %s",
                                         gfx_format_demo_action_name(scene->action_idx));
            demo_set_object_visible(scene->preview_note, true);
        }
        demo_set_preview_next_button(scene, "Next Action",
                                     scene->action_idx,
                                     gfx_format_demo_action_name_count);
        break;
    case DEMO_WIDGET_WHEEL:
        demo_set_object_visible(scene->wheel_demo, true);
        demo_set_preview_note(scene, "Spin the wheel and watch the selected row snap to center.");
        break;
    case DEMO_WIDGET_PICK_STAMP:
        demo_set_object_visible(scene->pick_stamp_demo, true);
        for (size_t i = 0; i < sizeof(scene->pick_stamp_date) / sizeof(scene->pick_stamp_date[0]); i++) {
            demo_set_object_visible(scene->pick_stamp_date[i], true);
        }
        for (size_t i = 0; i < sizeof(scene->pick_stamp_time) / sizeof(scene->pick_stamp_time[0]); i++) {
            demo_set_object_visible(scene->pick_stamp_time[i], true);
        }
        demo_set_preview_note(scene, "MY PICK stamp: BBH Bogle oblique 27 date, 20 time.");
        break;
    case DEMO_WIDGET_PAGEFLOW:
        demo_set_object_visible(scene->pageflow_demo, true);
        demo_set_preview_note(scene, "Swipe to the format probe image and compare color banding.");
        break;
    case DEMO_WIDGET_COVERFLOW:
        demo_set_object_visible(scene->coverflow_demo, true);
        demo_set_preview_note(scene, "Center card starts on the format probe image for color-depth comparison.");
        break;
    default:
        break;
    }

    gfx_format_demo_update_status(scene);
}

void gfx_format_demo_update_perf_label(uint32_t fps, uint32_t frame_ms)
{
    if (s_scene.fps_label == NULL) {
        return;
    }
    (void)gfx_label_set_text_fmt(s_scene.fps_label, "FPS: %u  %u ms",
                                 (unsigned)fps, (unsigned)frame_ms);
}

gfx_err_t gfx_format_demo_build_playground_scene(gfx_display_t *disp, const char *title_text,
        const char *format_tag)
{
    uint32_t hres;
    uint32_t vres;
    uint16_t preview_w;
    uint16_t preview_title_w;
    uint16_t fps_w;
    gfx_coord_t preview_button_y;

    GFX_RETURN_ON_FALSE(disp != NULL, GFX_ERR_INVALID_ARG, "playground", "display is null");

    memset(&s_scene, 0, sizeof(s_scene));
    s_scene.disp = disp;
    s_scene.font = (gfx_font_t)&font_puhui_16_4;
    s_scene.format_tag = format_tag;

    hres = gfx_display_get_h_res(disp);
    vres = gfx_display_get_v_res(disp);
    preview_w = (hres > DEMO_PREVIEW_X) ? (uint16_t)(hres - DEMO_PREVIEW_X) : DEMO_PREVIEW_BUTTON_W;
    fps_w = (preview_w > DEMO_PREVIEW_FPS_W) ? DEMO_PREVIEW_FPS_W : preview_w;
    preview_title_w = (preview_w > (fps_w + 8U)) ? (uint16_t)(preview_w - fps_w - 8U) : preview_w;
    preview_button_y = (vres > (DEMO_PREVIEW_BUTTON_H + DEMO_PREVIEW_BOTTOM_MARGIN))
                       ? (gfx_coord_t)(vres - DEMO_PREVIEW_BUTTON_H - DEMO_PREVIEW_BOTTOM_MARGIN)
                       : 0;

    GFX_RETURN_ON_ERROR(demo_apply(gfx_display_set_bg_color(disp, GFX_COLOR_HEX(0x101418))),
                        "playground", "set bg failed");

    s_scene.header_title = demo_create_label(disp, s_scene.font, 22, 18, (uint16_t)(hres - 44U), 34,
                           title_text != NULL ? title_text : "GFX Format Playground",
                           GFX_COLOR_HEX(0xF3F7FA), GFX_LABEL_LONG_CLIP);
    s_scene.header_tag = demo_create_label(disp, s_scene.font, 22, 56, 220, 24, "Widgets",
                                           GFX_COLOR_HEX(0xA8B3BD), GFX_LABEL_LONG_CLIP);
    s_scene.preview_title = demo_create_label(disp, s_scene.font, DEMO_PREVIEW_X, 56,
                            preview_title_w, 24, "Button Preview",
                            GFX_COLOR_HEX(0xA8B3BD), GFX_LABEL_LONG_SCROLL);
    s_scene.fps_label = demo_create_label(disp, s_scene.font,
                                          (gfx_coord_t)(DEMO_PREVIEW_X + preview_w - fps_w), 56,
                                          fps_w, 24, "FPS: --",
                                          GFX_COLOR_HEX(0x76B7E8), GFX_LABEL_LONG_CLIP);
    s_scene.preview_note = demo_create_label(disp, s_scene.font, DEMO_PREVIEW_X, 88, preview_w, 42, "",
                           GFX_COLOR_HEX(0xDCE4EC), GFX_LABEL_LONG_SCROLL);
    GFX_RETURN_ON_FALSE(s_scene.header_title != NULL && s_scene.header_tag != NULL &&
                        s_scene.preview_title != NULL && s_scene.preview_note != NULL &&
                        s_scene.fps_label != NULL,
                        GFX_ERR_INVALID_ARG, "playground", "create labels failed");
    (void)gfx_label_set_text_align(s_scene.fps_label, GFX_TEXT_ALIGN_RIGHT);

    if (s_scene.header_tag != NULL && format_tag != NULL) {
        (void)gfx_label_set_text_fmt(s_scene.header_tag, "Widgets   %s", format_tag);
    }

    GFX_RETURN_ON_ERROR(gfx_format_demo_build_widget_list(&s_scene), "playground", "build widget list failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_button(&s_scene), "playground", "build button failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_image_button(&s_scene), "playground", "build image button failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_progress_bar(&s_scene), "playground", "build progress bar failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_list_demo(&s_scene), "playground", "build list failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_image_demo(&s_scene), "playground", "build image failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_anim_demo(&s_scene), "playground", "build anim failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_motion_demo(&s_scene), "playground", "build motion failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_wheel_demo(&s_scene), "playground", "build wheel failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_pick_stamp_demo(&s_scene), "playground", "build pick stamp failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_pageflow_demo(&s_scene), "playground", "build pageflow failed");
    GFX_RETURN_ON_ERROR(gfx_format_demo_build_coverflow_demo(&s_scene), "playground", "build coverflow failed");

    s_scene.preview_button = demo_create_preview_button(disp, s_scene.font, preview_w, preview_button_y);
    GFX_RETURN_ON_FALSE(s_scene.preview_button != NULL, GFX_ERR_INVALID_ARG,
                        "playground", "create preview button failed");

    s_scene.action_idx = 0;
    s_scene.progress_value = 0;
    demo_apply_action(&s_scene, true);
    gfx_format_demo_apply_widget_focus(&s_scene, 0);
    return GFX_OK;
}
