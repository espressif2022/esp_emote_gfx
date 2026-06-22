/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

#include "claw_motion.inc"

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

gfx_err_t gfx_format_demo_build_motion_demo(format_playground_scene_t *scene)
{
    scene->motion = gfx_motion_player_create(scene->disp, &claw_motion_scene_asset);
    GFX_RETURN_ON_FALSE(scene->motion != NULL, GFX_ERR_INVALID_ARG, "playground", "create motion player failed");
    (void)gfx_motion_player_set_canvas(scene->motion, 284, 122, 260, 230);
    (void)gfx_motion_player_set_color(scene->motion, GFX_COLOR_HEX(0xFF4D2B));
    (void)gfx_motion_player_set_action_loop(scene->motion, true);
    (void)gfx_motion_player_set_visible(scene->motion, false);
    return GFX_OK;
}
