/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_err.h"

#include "core/gfx_timer.h"
#include "core/gfx_obj.h"
#include "widget/gfx_mesh_img.h"
#include "widget/gfx_stickman_emote.h"

#define GFX_STICKMAN_DEFAULT_TIMER_PERIOD_MS 33U
#define GFX_STICKMAN_DEFAULT_DAMPING_DIV 4
#define GFX_STICKMAN_SEGMENT_COUNT 10U

typedef struct {
    gfx_stickman_pose_t pose_cur;
    gfx_stickman_pose_t pose_tgt;
    gfx_timer_handle_t anim_timer;
    gfx_stickman_emote_cfg_t cfg;
    const gfx_stickman_export_t *export_data;
    uint16_t solid_pixel;
    gfx_image_dsc_t solid_img;
    gfx_obj_t *head_obj;
    gfx_obj_t *segment_objs[GFX_STICKMAN_SEGMENT_COUNT];
    gfx_color_t stroke_color;
    gfx_coord_t last_root_x;
    gfx_coord_t last_root_y;
    uint16_t last_root_w;
    uint16_t last_root_h;
    uint8_t head_segs;
    bool mesh_dirty;
    size_t action_idx;
} gfx_stickman_emote_t;

void gfx_stickman_emote_anim_cb(void *user_data);
esp_err_t gfx_stickman_emote_sync_meshes(gfx_obj_t *obj, gfx_stickman_emote_t *stickman);

esp_err_t gfx_stickman_emote_validate_export(const gfx_stickman_export_t *export_data);
esp_err_t gfx_stickman_emote_find_action_index(const gfx_stickman_export_t *export_data,
                                               const char *name,
                                               size_t *index_out);
esp_err_t gfx_stickman_emote_resolve_action_pose(const gfx_stickman_export_t *export_data,
                                                 size_t action_index,
                                                 gfx_stickman_pose_t *pose_out);
bool gfx_stickman_emote_update_pose(gfx_obj_t *obj, gfx_stickman_emote_t *stickman);
