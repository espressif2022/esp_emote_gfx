/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gfx.h"
#include "playground_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEMO_WIDGET_COUNT 8U
#define DEMO_CARD_COUNT   5U

typedef enum {
    DEMO_WIDGET_BUTTON = 0,
    DEMO_WIDGET_LIST,
    DEMO_WIDGET_IMAGE,
    DEMO_WIDGET_ANIM_JPG,
    DEMO_WIDGET_MOTION,
    DEMO_WIDGET_WHEEL,
    DEMO_WIDGET_PAGEFLOW,
    DEMO_WIDGET_COVERFLOW,
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
    gfx_object_t *list_demo;
    gfx_object_t *image_demo;
    gfx_object_t *image_button;
    gfx_object_t *anim_jpg_demo;
    gfx_object_t *anim_jpg_button;
    gfx_object_t *wheel_demo;
    gfx_object_t *pageflow_demo;
    gfx_object_t *coverflow_demo;
    gfx_object_t *cover_cards[DEMO_CARD_COUNT];
    gfx_coverflow_card_dsc_t cover_card_dsc[DEMO_CARD_COUNT];
    gfx_motion_player_t *motion;
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

size_t gfx_format_demo_anim_asset_count(void);

const char *gfx_format_demo_widget_name(uint16_t widget_idx);
const char *gfx_format_demo_action_name(uint16_t action_idx);
void gfx_format_demo_update_status(format_playground_scene_t *scene);
void gfx_format_demo_apply_widget_focus(format_playground_scene_t *scene, uint16_t widget_idx);

esp_err_t gfx_format_demo_build_widget_list(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_button(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_list_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_image_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_anim_jpg_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_motion_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_wheel_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_pageflow_demo(format_playground_scene_t *scene);
esp_err_t gfx_format_demo_build_coverflow_demo(format_playground_scene_t *scene);

#ifdef __cplusplus
}
#endif
