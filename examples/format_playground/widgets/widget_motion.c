/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"
#include "gfx/widgets/progress_bar.h"

#include "claw_motion.inc"

#define DEMO_MOTION_ZOOM_SLIDER_X  746
#define DEMO_MOTION_ZOOM_SLIDER_Y  122
#define DEMO_MOTION_ZOOM_SLIDER_W   32
#define DEMO_MOTION_ZOOM_SLIDER_H  230
#define DEMO_MOTION_CANVAS_X       423
#define DEMO_MOTION_CANVAS_Y       122
#define DEMO_MOTION_CANVAS_W       230
#define DEMO_MOTION_CANVAS_H       230
#define DEMO_MOTION_ZOOM_MIN       700
#define DEMO_MOTION_ZOOM_MAX      2100
#define DEMO_MOTION_ZOOM_DEFAULT   500

const char *const gfx_format_demo_action_names[] = {
    "Move",
    "Connecting",
    "Connect Fail",
    "Connect OK",
    "Disconnect",
    "Command Received",
    "Command Failed",
    "Work",
};

const uint16_t gfx_format_demo_action_name_count =
    (uint16_t)(sizeof(gfx_format_demo_action_names) / sizeof(gfx_format_demo_action_names[0]));

static void demo_motion_apply_zoom(format_playground_scene_t *scene)
{
    uint32_t zoom;
    uint16_t canvas_w;
    uint16_t canvas_h;
    gfx_coord_t canvas_x;
    gfx_coord_t canvas_y;

    if (scene == NULL || scene->motion == NULL) {
        return;
    }

    zoom = DEMO_MOTION_ZOOM_MIN +
           (((uint32_t)scene->motion_zoom_value * (DEMO_MOTION_ZOOM_MAX - DEMO_MOTION_ZOOM_MIN)) / 1000U);
    canvas_w = (uint16_t)(((uint32_t)DEMO_MOTION_CANVAS_W * zoom) / 1000U);
    canvas_h = (uint16_t)(((uint32_t)DEMO_MOTION_CANVAS_H * zoom) / 1000U);
    if (canvas_w < 1U) {
        canvas_w = 1U;
    }
    if (canvas_h < 1U) {
        canvas_h = 1U;
    }
    canvas_x = (gfx_coord_t)(DEMO_MOTION_CANVAS_X +
                             ((gfx_coord_t)DEMO_MOTION_CANVAS_W - (gfx_coord_t)canvas_w) / 2);
    canvas_y = (gfx_coord_t)(DEMO_MOTION_CANVAS_Y +
                             ((gfx_coord_t)DEMO_MOTION_CANVAS_H - (gfx_coord_t)canvas_h) / 2);
    (void)gfx_motion_player_set_canvas(scene->motion, canvas_x, canvas_y, canvas_w, canvas_h);
    (void)gfx_motion_player_sync(scene->motion);
}

static void demo_motion_zoom_changed_cb(gfx_object_t *obj, uint16_t permille, void *user_data)
{
    format_playground_scene_t *scene = (format_playground_scene_t *)user_data;

    (void)obj;
    if (scene == NULL) {
        return;
    }

    scene->motion_zoom_value = permille;
    demo_motion_apply_zoom(scene);
}

gfx_err_t gfx_format_demo_build_motion_demo(format_playground_scene_t *scene)
{
    gfx_object_t *slider;

    scene->motion = gfx_motion_player_create(scene->disp, &claw_motion_scene_asset);
    GFX_RETURN_ON_FALSE(scene->motion != NULL, GFX_ERR_INVALID_ARG, "playground", "create motion player failed");
    (void)gfx_motion_player_set_color(scene->motion, GFX_COLOR_HEX(0xFF4D2B));
    (void)gfx_motion_player_set_action_loop(scene->motion, true);
    (void)gfx_motion_player_set_visible(scene->motion, false);

    scene->motion_zoom_value = DEMO_MOTION_ZOOM_DEFAULT;
    demo_motion_apply_zoom(scene);

    slider = gfx_progress_bar_create(scene->disp);
    GFX_RETURN_ON_FALSE(slider != NULL, GFX_ERR_INVALID_ARG, "playground", "create motion zoom slider failed");
    scene->motion_zoom_slider = slider;
    (void)gfx_object_set_pos(slider, DEMO_MOTION_ZOOM_SLIDER_X, DEMO_MOTION_ZOOM_SLIDER_Y);
    (void)gfx_object_set_size(slider, DEMO_MOTION_ZOOM_SLIDER_W, DEMO_MOTION_ZOOM_SLIDER_H);
    (void)gfx_progress_bar_set_direction(slider, GFX_PROGRESS_BAR_DIR_VERTICAL);
    (void)gfx_progress_bar_set_colors(slider, GFX_COLOR_HEX(0x26313B), GFX_COLOR_HEX(0x2F8CFF));
    (void)gfx_progress_bar_set_thumb_style(slider, GFX_COLOR_HEX(0xF7FBFF), GFX_COLOR_HEX(0xF7FBFF), 0);
    (void)gfx_progress_bar_set_radius(slider, 16);
    (void)gfx_progress_bar_set_fill_pad(slider, 4);
    (void)gfx_progress_bar_set_interactive(slider, true);
    (void)gfx_progress_bar_set_value_changed_cb(slider, demo_motion_zoom_changed_cb, scene);
    (void)gfx_progress_bar_set_value(slider, scene->motion_zoom_value);
    (void)gfx_object_set_visible(slider, false);
    return GFX_OK;
}
