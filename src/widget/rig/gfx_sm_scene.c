/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layer 2 — PARSER
 *
 * Responsibilities:
 *  - Validate the ROM asset (gfx_sm_asset_t).
 *  - Initialize the mutable runtime state (gfx_sm_scene_t).
 *  - On each tick: ease pose_cur toward pose_tgt using the layout's damping_div.
 *  - On each advance: count hold_ticks and step through clip steps, applying
 *    facing/mirroring when loading the next target pose.
 *
 * This file deliberately contains NO gfx_obj / mesh / display calls.
 */

#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_IMG
#include "common/gfx_log_priv.h"

#include "widget/gfx_sm_scene.h"

static const char *TAG = "gfx_sm_scene";

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static inline int16_t s_ease(int16_t cur, int16_t tgt, int16_t div)
{
    int32_t diff = (int32_t)tgt - (int32_t)cur;
    int32_t step;

    if (diff == 0) {
        return cur;
    }
    if (div < 1) {
        div = 1;
    }
    step = diff / div;
    if (step == 0) {
        step = (diff > 0) ? 1 : -1;
    }
    return (int16_t)(cur + step);
}

/**
 * Copy the target pose from the asset into pose_tgt[], applying facing mirror
 * around layout->mirror_x when facing == -1.
 */
static void s_load_target(gfx_sm_scene_t *scene, uint16_t pose_index, int8_t facing)
{
    const gfx_sm_asset_t   *asset  = scene->asset;
    const gfx_sm_pose_t    *pose   = &asset->poses[pose_index];
    const gfx_sm_layout_t  *layout = asset->layout;
    uint8_t                 n      = asset->joint_count;

    for (uint8_t i = 0; i < n; i++) {
        int16_t x = pose->coords[i * 2 + 0];
        int16_t y = pose->coords[i * 2 + 1];
        if (facing < 0) {
            x = (int16_t)(layout->mirror_x * 2 - x);
        }
        scene->pose_tgt[i].x = x;
        scene->pose_tgt[i].y = y;
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t gfx_sm_scene_init(gfx_sm_scene_t *scene, const gfx_sm_asset_t *asset)
{
    const gfx_sm_clip_t      *clip;
    const gfx_sm_clip_step_t *step;

    ESP_RETURN_ON_FALSE(scene  != NULL, ESP_ERR_INVALID_ARG, TAG, "scene is NULL");
    ESP_RETURN_ON_FALSE(asset  != NULL, ESP_ERR_INVALID_ARG, TAG, "asset is NULL");
    ESP_RETURN_ON_FALSE(asset->meta != NULL, ESP_ERR_INVALID_ARG, TAG, "meta is NULL");
    ESP_RETURN_ON_FALSE(asset->meta->version == GFX_SM_SCENE_SCHEMA_VERSION,
                        ESP_ERR_INVALID_ARG, TAG, "schema version mismatch");
    ESP_RETURN_ON_FALSE(asset->joint_names != NULL && asset->joint_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "joints empty");
    ESP_RETURN_ON_FALSE(asset->joint_count <= GFX_SM_SCENE_MAX_JOINTS,
                        ESP_ERR_INVALID_ARG, TAG, "too many joints");
    /* segment_count == 0 is valid for non-skeletal assets (e.g. face emote). */
    if (asset->segment_count > 0U) {
        ESP_RETURN_ON_FALSE(asset->segments != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "segments not NULL but pointer is NULL");
    }
    ESP_RETURN_ON_FALSE(asset->poses != NULL && asset->pose_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "poses empty");
    ESP_RETURN_ON_FALSE(asset->clips != NULL && asset->clip_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "clips empty");
    ESP_RETURN_ON_FALSE(asset->layout != NULL, ESP_ERR_INVALID_ARG, TAG, "layout is NULL");

    for (uint8_t i = 0; i < asset->segment_count; i++) {
        ESP_RETURN_ON_FALSE(asset->segments[i].joint_a < asset->joint_count &&
                            asset->segments[i].joint_b < asset->joint_count,
                            ESP_ERR_INVALID_ARG, TAG, "segment[%u] joint index out of range", i);
    }
    /* Validate clip step pose indices */
    for (uint16_t c = 0; c < asset->clip_count; c++) {
        clip = &asset->clips[c];
        ESP_RETURN_ON_FALSE(clip->name != NULL, ESP_ERR_INVALID_ARG, TAG, "clip[%u] name is NULL", c);
        ESP_RETURN_ON_FALSE(clip->steps != NULL && clip->step_count > 0U,
                            ESP_ERR_INVALID_ARG, TAG, "clip[%u] has no steps", c);
        for (uint8_t s = 0; s < clip->step_count; s++) {
            ESP_RETURN_ON_FALSE(clip->steps[s].pose_index < asset->pose_count,
                                ESP_ERR_INVALID_ARG, TAG, "clip[%u] step[%u] pose out of range", c, s);
        }
    }

    memset(scene, 0, sizeof(*scene));
    scene->asset = asset;

    /* Bootstrap: start at clip 0, step 0, snap immediately */
    clip = &asset->clips[0];
    step = &clip->steps[0];
    s_load_target(scene, step->pose_index, step->facing);
    memcpy(scene->pose_cur, scene->pose_tgt, sizeof(gfx_sm_pt_t) * asset->joint_count);
    scene->dirty = true;

    return ESP_OK;
}

esp_err_t gfx_sm_scene_set_clip(gfx_sm_scene_t *scene, uint16_t clip_index, bool snap_now)
{
    const gfx_sm_clip_step_t *step;

    ESP_RETURN_ON_FALSE(scene != NULL && scene->asset != NULL, ESP_ERR_INVALID_STATE, TAG, "scene not ready");
    ESP_RETURN_ON_FALSE(clip_index < scene->asset->clip_count,
                        ESP_ERR_INVALID_ARG, TAG, "clip index out of range");

    scene->active_clip = clip_index;
    scene->active_step = 0;
    scene->step_ticks  = 0;

    step = &scene->asset->clips[clip_index].steps[0];
    s_load_target(scene, step->pose_index, step->facing);

    if (snap_now) {
        memcpy(scene->pose_cur, scene->pose_tgt,
               sizeof(gfx_sm_pt_t) * scene->asset->joint_count);
        scene->dirty = true;
    }

    return ESP_OK;
}

esp_err_t gfx_sm_scene_set_clip_name(gfx_sm_scene_t *scene, const char *name, bool snap_now)
{
    ESP_RETURN_ON_FALSE(scene != NULL && scene->asset != NULL, ESP_ERR_INVALID_STATE, TAG, "scene not ready");
    ESP_RETURN_ON_FALSE(name  != NULL, ESP_ERR_INVALID_ARG, TAG, "name is NULL");

    for (uint16_t i = 0; i < scene->asset->clip_count; i++) {
        if (strcmp(scene->asset->clips[i].name, name) == 0) {
            return gfx_sm_scene_set_clip(scene, i, snap_now);
        }
    }

    return ESP_ERR_NOT_FOUND;
}

bool gfx_sm_scene_tick(gfx_sm_scene_t *scene)
{
    uint8_t  n;
    int16_t  div;
    bool     changed = false;

    if (scene == NULL || scene->asset == NULL) {
        return false;
    }

    n   = scene->asset->joint_count;
    div = (scene->asset->layout->damping_div > 0) ? scene->asset->layout->damping_div : 4;

    for (uint8_t i = 0; i < n; i++) {
        int16_t nx = s_ease(scene->pose_cur[i].x, scene->pose_tgt[i].x, div);
        int16_t ny = s_ease(scene->pose_cur[i].y, scene->pose_tgt[i].y, div);

        if (nx != scene->pose_cur[i].x || ny != scene->pose_cur[i].y) {
            scene->pose_cur[i].x = nx;
            scene->pose_cur[i].y = ny;
            changed = true;
        }
    }

    if (changed) {
        scene->dirty = true;
    }

    return changed;
}

void gfx_sm_scene_advance(gfx_sm_scene_t *scene)
{
    const gfx_sm_clip_t      *clip;
    const gfx_sm_clip_step_t *step;
    uint8_t                   next_step;

    if (scene == NULL || scene->asset == NULL) {
        return;
    }

    clip = &scene->asset->clips[scene->active_clip];
    step = &clip->steps[scene->active_step];

    scene->step_ticks++;
    if (scene->step_ticks < step->hold_ticks) {
        return;
    }

    scene->step_ticks = 0;
    next_step = (uint8_t)(scene->active_step + 1U);

    if (next_step >= clip->step_count) {
        if (!clip->loop) {
            return;
        }
        next_step = 0;
    }

    scene->active_step = next_step;
    step = &clip->steps[next_step];
    s_load_target(scene, step->pose_index, step->facing);
}
