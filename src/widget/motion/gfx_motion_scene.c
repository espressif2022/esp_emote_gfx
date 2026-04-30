/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layer 2 — PARSER
 *
 * Responsibilities:
 *  - Validate the generated Motion Scene Asset (gfx_motion_asset_t).
 *  - Initialize the mutable runtime state (gfx_motion_scene_t).
 *  - On each tick: ease pose_cur toward pose_tgt using the layout's damping_div.
 *  - On each advance: count hold_ticks and step through action steps, applying
 *    facing/mirroring when loading the next target pose.
 *
 * This file deliberately contains NO gfx_obj / mesh / display calls.
 */

#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_MOTION
#include "common/gfx_config_internal.h"
#include "common/gfx_log_priv.h"

#include "widget/gfx_motion.h"
#include "widget/gfx_motion_scene.h"

static const char *TAG = "gfx_motion_scene";

static const char *s_interp_str(gfx_motion_interp_t i)
{
    return (i == GFX_MOTION_INTERP_HOLD) ? "HOLD" : "DAMPED";
}

static bool s_action_loop_enabled(const gfx_motion_scene_t *scene, const gfx_motion_action_t *action)
{
    if (scene == NULL || action == NULL) {
        return false;
    }
    return scene->action_loop_override_en ? scene->action_loop_override : action->loop;
}

static bool s_interp_is_valid(gfx_motion_interp_t interp)
{
    return interp == GFX_MOTION_INTERP_HOLD ||
           interp == GFX_MOTION_INTERP_DAMPED;
}

static void s_snap_current_to_target(gfx_motion_scene_t *scene)
{
    if (scene == NULL || scene->asset == NULL) {
        return;
    }

    memcpy(scene->pose_cur, scene->pose_tgt,
           sizeof(gfx_motion_point_t) * scene->asset->joint_count);
    scene->dirty = true;
}

void gfx_motion_scene_log_active_step(const gfx_motion_scene_t *scene, const char *reason)
{
    const gfx_motion_action_t      *action;
    const gfx_motion_action_step_t *step;

    if (scene == NULL || scene->asset == NULL) {
        return;
    }
    if (scene->active_action >= scene->asset->action_count) {
        GFX_LOGW(TAG, "%s: invalid active_action=%u",
                 reason ? reason : "?", (unsigned)scene->active_action);
        return;
    }
    action = &scene->asset->actions[scene->active_action];
    if (action->step_count == 0U || action->steps == NULL ||
            scene->active_step >= action->step_count) {
        GFX_LOGW(TAG, "%s: action[%u] bad step=%u count=%u",
                 reason ? reason : "?", (unsigned)scene->active_action,
                 (unsigned)scene->active_step,
                 (unsigned)action->step_count);
        return;
    }
    step = &action->steps[scene->active_step];
    GFX_LOGI(TAG,
             "%s | action_idx=%u/%u step_idx=%u/%u pose_idx=%u hold_ticks=%u step_tick=%u/%u facing=%d interp=%s loop=%d",
             reason ? reason : "",
             (unsigned)scene->active_action, (unsigned)scene->asset->action_count,
             (unsigned)scene->active_step, (unsigned)action->step_count,
             (unsigned)step->pose_index, (unsigned)step->hold_ticks,
             (unsigned)scene->step_ticks, (unsigned)step->hold_ticks,
             (int)step->facing, s_interp_str(step->interp),
             s_action_loop_enabled(scene, action) ? 1 : 0);
}

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static esp_err_t s_validate_segment(const gfx_motion_asset_t *asset, uint8_t seg_index)
{
    const gfx_motion_segment_t *seg = &asset->segments[seg_index];

    ESP_RETURN_ON_FALSE(seg->resource_idx == 0U ||
                        (asset->resources != NULL &&
                         seg->resource_idx <= asset->resource_count &&
                         asset->resources[seg->resource_idx - 1U].image != NULL),
                        ESP_ERR_INVALID_ARG, TAG,
                        "segment[%u] resource index out of range", seg_index);
    ESP_RETURN_ON_FALSE(seg->color_idx == 0U ||
                        (asset->color_palette != NULL &&
                         seg->color_idx <= asset->color_palette_count &&
                         seg->color_idx <= GFX_MOTION_PALETTE_MAX),
                        ESP_ERR_INVALID_ARG, TAG,
                        "segment[%u] color index out of range", seg_index);
    ESP_RETURN_ON_FALSE(seg->layer_bit <= 32U,
                        ESP_ERR_INVALID_ARG, TAG,
                        "segment[%u] layer bit out of range", seg_index);

    switch (seg->kind) {
    case GFX_MOTION_SEG_CAPSULE:
        ESP_RETURN_ON_FALSE(seg->joint_a < asset->joint_count &&
                            seg->joint_b < asset->joint_count,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] capsule control point index out of range", seg_index);
        return ESP_OK;

    case GFX_MOTION_SEG_RING:
        ESP_RETURN_ON_FALSE(seg->joint_a < asset->joint_count,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] ring control point index out of range", seg_index);
        return ESP_OK;

    case GFX_MOTION_SEG_BEZIER_STRIP:
    case GFX_MOTION_SEG_BEZIER_LOOP:
    case GFX_MOTION_SEG_BEZIER_FILL:
        ESP_RETURN_ON_FALSE(seg->joint_count >= 4U,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] bezier control point count must be >= 4", seg_index);
        ESP_RETURN_ON_FALSE(seg->joint_count <= GFX_MOTION_SCENE_MAX_SEG_CTRL_POINTS,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] bezier control point count exceeds max %u",
                            seg_index, GFX_MOTION_SCENE_MAX_SEG_CTRL_POINTS);
        ESP_RETURN_ON_FALSE(((seg->joint_count - 1U) % 3U) == 0U,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] bezier control point count must satisfy 3k+1", seg_index);
        ESP_RETURN_ON_FALSE(seg->joint_a < asset->joint_count &&
                            (uint32_t)seg->joint_a + (uint32_t)seg->joint_count <= asset->joint_count,
                            ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] bezier control range out of bounds", seg_index);
        return ESP_OK;

    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG,
                            "segment[%u] has unknown kind %d", seg_index, (int)seg->kind);
        return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t s_validate_resource_crop(const gfx_motion_resource_t *res, uint8_t resource_index)
{
    uint32_t img_w;
    uint32_t img_h;
    uint32_t crop_w;
    uint32_t crop_h;

    ESP_RETURN_ON_FALSE(res != NULL && res->image != NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "resource[%u] image is NULL", resource_index);

    img_w = res->image->header.w;
    img_h = res->image->header.h;
    ESP_RETURN_ON_FALSE(img_w > 0U && img_h > 0U,
                        ESP_ERR_INVALID_ARG, TAG,
                        "resource[%u] image size is invalid", resource_index);
    ESP_RETURN_ON_FALSE(res->uv_x < img_w && res->uv_y < img_h,
                        ESP_ERR_INVALID_ARG, TAG,
                        "resource[%u] uv origin out of range", resource_index);

    crop_w = (res->uv_w > 0U) ? res->uv_w : (img_w - res->uv_x);
    crop_h = (res->uv_h > 0U) ? res->uv_h : (img_h - res->uv_y);
    ESP_RETURN_ON_FALSE(crop_w > 0U && crop_h > 0U &&
                        (uint32_t)res->uv_x + crop_w <= img_w &&
                        (uint32_t)res->uv_y + crop_h <= img_h,
                        ESP_ERR_INVALID_ARG, TAG,
                        "resource[%u] uv crop out of range", resource_index);

    return ESP_OK;
}

/**
 * Copy the target pose from the asset into pose_tgt[], applying facing mirror
 * around layout->mirror_x when facing == -1.
 */
static void s_load_target(gfx_motion_scene_t *scene, uint16_t pose_index, int8_t facing)
{
    const gfx_motion_asset_t   *asset  = scene->asset;
    const gfx_motion_pose_t    *pose   = &asset->poses[pose_index];
    const gfx_motion_layout_t  *layout = asset->layout;
    uint16_t                n      = asset->joint_count;

    for (uint16_t i = 0; i < n; i++) {
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

esp_err_t gfx_motion_scene_init(gfx_motion_scene_t *scene, const gfx_motion_asset_t *asset)
{
    const gfx_motion_action_t      *action;
    const gfx_motion_action_step_t *step;

    ESP_RETURN_ON_FALSE(scene  != NULL, ESP_ERR_INVALID_ARG, TAG, "scene is NULL");
    ESP_RETURN_ON_FALSE(asset  != NULL, ESP_ERR_INVALID_ARG, TAG, "asset is NULL");
    ESP_RETURN_ON_FALSE(asset->meta != NULL, ESP_ERR_INVALID_ARG, TAG, "meta is NULL");
    ESP_RETURN_ON_FALSE(asset->meta->version == GFX_MOTION_SCENE_SCHEMA_VERSION,
                        ESP_ERR_INVALID_ARG, TAG, "schema version mismatch");
    ESP_RETURN_ON_FALSE(asset->meta->viewbox_w > 0 && asset->meta->viewbox_h > 0,
                        ESP_ERR_INVALID_ARG, TAG, "viewbox size must be positive");
    ESP_RETURN_ON_FALSE(asset->joint_names != NULL && asset->joint_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "control points empty");
    ESP_RETURN_ON_FALSE(asset->joint_count <= GFX_MOTION_SCENE_MAX_POINTS,
                        ESP_ERR_INVALID_ARG, TAG, "too many control points");
    /* segment_count == 0 is valid for non-skeletal assets (e.g. face emote). */
    if (asset->segment_count > 0U) {
        ESP_RETURN_ON_FALSE(asset->segments != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "segments not NULL but pointer is NULL");
    }
    ESP_RETURN_ON_FALSE(asset->poses != NULL && asset->pose_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "poses empty");
    ESP_RETURN_ON_FALSE(asset->actions != NULL && asset->action_count > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "actions empty");
    ESP_RETURN_ON_FALSE(asset->layout != NULL, ESP_ERR_INVALID_ARG, TAG, "layout is NULL");
    if (asset->sequence_count > 0U) {
        ESP_RETURN_ON_FALSE(asset->sequence != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "sequence not NULL but pointer is NULL");
    }
    if (asset->resource_count > 0U) {
        ESP_RETURN_ON_FALSE(asset->resources != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "resources not NULL but pointer is NULL");
    }
    if (asset->color_palette_count > 0U) {
        ESP_RETURN_ON_FALSE(asset->color_palette != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "color palette not NULL but pointer is NULL");
    }

    for (uint8_t i = 0; i < asset->resource_count; i++) {
        ESP_RETURN_ON_ERROR(s_validate_resource_crop(&asset->resources[i], i),
                            TAG, "resource[%u] invalid", i);
    }
    for (uint8_t i = 0; i < asset->segment_count; i++) {
        ESP_RETURN_ON_ERROR(s_validate_segment(asset, i), TAG, "segment[%u] invalid", i);
    }
    for (uint16_t p = 0; p < asset->pose_count; p++) {
        ESP_RETURN_ON_FALSE(asset->poses[p].coords != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "pose[%u] coords is NULL", p);
    }
    /* Validate action step pose indices */
    for (uint16_t c = 0; c < asset->action_count; c++) {
        action = &asset->actions[c];
        ESP_RETURN_ON_FALSE(action->steps != NULL && action->step_count > 0U,
                            ESP_ERR_INVALID_ARG, TAG, "action[%u] has no steps", c);
        for (uint8_t s = 0; s < action->step_count; s++) {
            ESP_RETURN_ON_FALSE(action->steps[s].pose_index < asset->pose_count,
                                ESP_ERR_INVALID_ARG, TAG, "action[%u] step[%u] pose out of range", c, s);
            ESP_RETURN_ON_FALSE(s_interp_is_valid(action->steps[s].interp),
                                ESP_ERR_INVALID_ARG, TAG, "action[%u] step[%u] interp invalid", c, s);
        }
    }
    for (uint16_t i = 0; i < asset->sequence_count; i++) {
        ESP_RETURN_ON_FALSE(asset->sequence[i] < asset->action_count,
                            ESP_ERR_INVALID_ARG, TAG, "sequence[%u] action out of range", i);
    }

    memset(scene, 0, sizeof(*scene));
    scene->asset = asset;

    /* Bootstrap: start at action 0, step 0, snap immediately */
    action = &asset->actions[0];
    step = &action->steps[0];
    s_load_target(scene, step->pose_index, step->facing);
    s_snap_current_to_target(scene);

    gfx_motion_scene_log_active_step(scene, "init");

    return ESP_OK;
}

esp_err_t gfx_motion_scene_set_action(gfx_motion_scene_t *scene, uint16_t action_index, bool snap_now)
{
    const gfx_motion_action_step_t *step;

    ESP_RETURN_ON_FALSE(scene != NULL && scene->asset != NULL, ESP_ERR_INVALID_STATE, TAG, "scene not ready");
    ESP_RETURN_ON_FALSE(action_index < scene->asset->action_count,
                        ESP_ERR_INVALID_ARG, TAG, "action index out of range");

    scene->active_action = action_index;
    scene->active_step = 0;
    scene->step_ticks  = 0;

    step = &scene->asset->actions[action_index].steps[0];
    s_load_target(scene, step->pose_index, step->facing);

    if (snap_now || step->interp == GFX_MOTION_INTERP_HOLD) {
        s_snap_current_to_target(scene);
    }

    return ESP_OK;
}

esp_err_t gfx_motion_scene_set_action_loop(gfx_motion_scene_t *scene, bool loop)
{
    ESP_RETURN_ON_FALSE(scene != NULL && scene->asset != NULL, ESP_ERR_INVALID_STATE, TAG, "scene not ready");
    scene->action_loop_override_en = true;
    scene->action_loop_override = loop;
    return ESP_OK;
}

esp_err_t gfx_motion_scene_clear_action_loop_override(gfx_motion_scene_t *scene)
{
    ESP_RETURN_ON_FALSE(scene != NULL && scene->asset != NULL, ESP_ERR_INVALID_STATE, TAG, "scene not ready");
    scene->action_loop_override_en = false;
    return ESP_OK;
}

bool gfx_motion_scene_tick(gfx_motion_scene_t *scene)
{
    uint16_t n;
    int16_t  div;
    bool     changed = false;

    if (scene == NULL || scene->asset == NULL) {
        return false;
    }

    n   = scene->asset->joint_count;
    div = (scene->asset->layout->damping_div > 0) ?
          scene->asset->layout->damping_div :
          GFX_MOTION_DEFAULT_DAMPING_DIV;

    for (uint16_t i = 0; i < n; i++) {
        int16_t nx = gfx_motion_ease_i16(scene->pose_cur[i].x, scene->pose_tgt[i].x, div);
        int16_t ny = gfx_motion_ease_i16(scene->pose_cur[i].y, scene->pose_tgt[i].y, div);

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

void gfx_motion_scene_advance(gfx_motion_scene_t *scene)
{
    const gfx_motion_action_t      *action;
    const gfx_motion_action_step_t *step;
    uint8_t                         next_step;

    if (scene == NULL || scene->asset == NULL) {
        return;
    }

    action = &scene->asset->actions[scene->active_action];
    step = &action->steps[scene->active_step];

    scene->step_ticks++;
    if (scene->step_ticks < step->hold_ticks) {
        return;
    }

    scene->step_ticks = 0;
    next_step = (uint8_t)(scene->active_step + 1U);

    if (next_step >= action->step_count) {
        if (!s_action_loop_enabled(scene, action)) {
            return;
        }
        next_step = 0;
    }

    scene->active_step = next_step;
    step = &action->steps[next_step];
    s_load_target(scene, step->pose_index, step->facing);
    if (step->interp == GFX_MOTION_INTERP_HOLD) {
        s_snap_current_to_target(scene);
    }
}
