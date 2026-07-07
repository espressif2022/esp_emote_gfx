/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gfx/error.h"
#include "gfx.h"
#include "playground_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEMO_WIDGET_COUNT 11U
#define DEMO_CARD_COUNT   5U

#define DEMO_FLOW_IMAGE_C5_DEVKITC     "01_esp32_c5_devkitc_320x240.jpg"
#define DEMO_FLOW_IMAGE_C6_DEVKITC     "02_esp32_c6_devkitc_320x240.jpg"
#define DEMO_FLOW_IMAGE_S3_DEVKITC     "03_esp32_s3_devkitc_320x240.jpg"
#define DEMO_FLOW_IMAGE_P4_EYE_FRONT   "04_esp32_p4_eye_front_320x240.jpg"
#define DEMO_FLOW_IMAGE_P4_EYE_BACK    "05_esp32_p4_eye_back_320x240.jpg"

typedef enum {
    DEMO_WIDGET_MOTION = 0,
    DEMO_WIDGET_ANIM,
    DEMO_WIDGET_COVERFLOW,
    DEMO_WIDGET_PAGEFLOW,
    DEMO_WIDGET_IMAGE,
    DEMO_WIDGET_BUTTON,
    DEMO_WIDGET_IMAGE_BUTTON,
    DEMO_WIDGET_PROGRESS_BAR,
    DEMO_WIDGET_LIST,
    DEMO_WIDGET_WHEEL,
    DEMO_WIDGET_PICK_STAMP,
} demo_widget_id_t;

typedef struct {
    gfx_display_t *disp;
    gfx_object_t *header_title;
    gfx_object_t *header_tag;
    gfx_object_t *widget_list;
    gfx_object_t *preview_title;
    gfx_object_t *preview_note;
    gfx_object_t *status;
    gfx_object_t *button_demo;
    gfx_object_t *image_button_demo;
    gfx_object_t *progress_bar_demo;
    gfx_object_t *list_demo;
    gfx_object_t *image_demo;
    gfx_object_t *anim_demo;
    gfx_object_t *fps_label;
    gfx_object_t *preview_button;
    gfx_object_t *wheel_demo;
    gfx_object_t *pick_stamp_demo;
    gfx_object_t *pick_stamp_date[5];
    gfx_object_t *pick_stamp_time[8];
    gfx_object_t *pageflow_demo;
    gfx_object_t *coverflow_demo;
    gfx_object_t *cover_cards[DEMO_CARD_COUNT];
    gfx_coverflow_card_dsc_t cover_card_dsc[DEMO_CARD_COUNT];
    gfx_motion_player_t *motion;
    gfx_object_t *motion_zoom_slider;
    uint16_t progress_value;
    uint16_t motion_zoom_value;
    uint16_t widget_idx;
    uint16_t action_idx;
    gfx_font_t font;
    const char *format_tag;
} format_playground_scene_t;

extern const char *const gfx_format_demo_widget_names[DEMO_WIDGET_COUNT];
extern const char *const gfx_format_demo_action_names[];
extern const uint16_t gfx_format_demo_action_name_count;
extern const char *const gfx_format_demo_cover_card_titles[DEMO_CARD_COUNT];
extern const char *const gfx_format_demo_flow_image_names[DEMO_CARD_COUNT];
extern const char *const gfx_format_demo_list_items[];
extern const uint16_t gfx_format_demo_list_item_count;
extern const char *const gfx_format_demo_wheel_items[];
extern const uint16_t gfx_format_demo_wheel_item_count;

size_t gfx_format_demo_image_clip_count(void);
size_t gfx_format_demo_anim_clip_count(void);
size_t gfx_format_demo_image_clip_index(void);
size_t gfx_format_demo_anim_clip_index(void);
const char *gfx_format_demo_image_clip_note(void);
const char *gfx_format_demo_anim_clip_note(void);
gfx_err_t gfx_format_demo_next_image_clip(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_next_anim_clip(format_playground_scene_t *scene);

const char *gfx_format_demo_widget_name(uint16_t widget_idx);
const char *gfx_format_demo_action_name(uint16_t action_idx);
void gfx_format_demo_update_status(format_playground_scene_t *scene);
void gfx_format_demo_apply_widget_focus(format_playground_scene_t *scene, uint16_t widget_idx);

gfx_err_t gfx_format_demo_build_widget_list(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_button(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_image_button(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_progress_bar(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_list_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_image_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_anim_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_motion_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_wheel_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_pick_stamp_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_pageflow_demo(format_playground_scene_t *scene);
gfx_err_t gfx_format_demo_build_coverflow_demo(format_playground_scene_t *scene);

#ifdef __cplusplus
}
#endif
