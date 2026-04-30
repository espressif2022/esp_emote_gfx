/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layer 3 — motion scene player
 *
 * This file owns runtime orchestration only: one gfx_mesh_img per segment,
 * motion callbacks, object lifetime, canvas mapping, and segment dispatch.
 * Primitive geometry lives in gfx_motion_primitives.c; style/resource binding
 * lives in gfx_motion_style.c.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_MOTION
#include "common/gfx_log_priv.h"

#include "core/gfx_disp.h"
#include "core/display/gfx_disp_priv.h"
#include "widget/motion/gfx_motion_player_priv.h"

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "gfx_motion_player";

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_motion_player_to_screen(const gfx_motion_asset_t *asset,
                                        const gfx_motion_point_t *dp,
                                        gfx_coord_t ox, gfx_coord_t oy,
                                        uint16_t ow, uint16_t oh,
                                        gfx_motion_player_screen_point_t *out)
{
    const gfx_motion_meta_t *m = asset->meta;
    float scale = fminf((float)ow / (float)m->viewbox_w,
                        (float)oh / (float)m->viewbox_h);
    float rw = (float)m->viewbox_w * scale;
    float rh = (float)m->viewbox_h * scale;
    float offx = (float)ox + floorf(((float)ow - rw) * 0.5f) - (float)m->viewbox_x * scale;
    float offy = (float)oy + floorf(((float)oh - rh) * 0.5f) - (float)m->viewbox_y * scale;

    out->x = (int32_t)lroundf(offx + (float)dp->x * scale);
    out->y = (int32_t)lroundf(offy + (float)dp->y * scale);
}

static int32_t gfx_motion_player_scalar_px(const gfx_motion_asset_t *asset,
        uint16_t ow, uint16_t oh, float v)
{
    const gfx_motion_meta_t *m = asset->meta;
    float scale = fminf((float)ow / (float)m->viewbox_w,
                        (float)oh / (float)m->viewbox_h);
    int32_t px = (int32_t)lroundf(v * scale);
    return (px < 1) ? 1 : px;
}

static esp_err_t gfx_motion_player_set_grid_internal(gfx_motion_player_t *player, uint8_t seg_idx,
        gfx_obj_t *obj, uint8_t cols, uint8_t rows)
{
    ESP_RETURN_ON_FALSE(player != NULL && obj != NULL, ESP_ERR_INVALID_ARG, TAG, "grid target is NULL");

    if (seg_idx < GFX_MOTION_PLAYER_MAX_SEGMENTS &&
            player->seg_grid_cols[seg_idx] == cols &&
            player->seg_grid_rows[seg_idx] == rows) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gfx_mesh_img_set_grid(obj, cols, rows), TAG, "set grid seg[%u]", seg_idx);
    if (seg_idx < GFX_MOTION_PLAYER_MAX_SEGMENTS) {
        player->seg_grid_cols[seg_idx] = cols;
        player->seg_grid_rows[seg_idx] = rows;
    }
    return gfx_motion_player_apply_resource_uv(player, seg_idx, obj, cols, rows);
}

static esp_err_t gfx_motion_player_configure_segment_mesh(gfx_motion_player_t *player,
        uint8_t seg_idx,
        gfx_obj_t *obj)
{
    const gfx_motion_segment_t *seg = &player->scene.asset->segments[seg_idx];

    switch (seg->kind) {
    case GFX_MOTION_SEG_RING: {
        uint8_t segs = gfx_motion_player_ring_segs((float)(seg->radius_hint > 0 ? seg->radius_hint : 20));
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, segs, 1U),
                            TAG, "set ring grid seg[%u]", seg_idx);
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_wrap_cols(obj, true), TAG, "set ring wrap seg[%u]", seg_idx);
        break;
    }
    case GFX_MOTION_SEG_CAPSULE:
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, 1U, 1U),
                            TAG, "set capsule grid seg[%u]", seg_idx);
        break;
    case GFX_MOTION_SEG_BEZIER_LOOP: {
        uint16_t n = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
        uint16_t k = (n - 1U) / 3U;
        uint16_t tcols = k * (uint16_t)MOTION_BEZIER_SEGS_PER_SEG;
        uint8_t gcols = (tcols > 255U) ? 255U : (uint8_t)tcols;
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, gcols, 1U),
                            TAG, "set bezier loop grid seg[%u]", seg_idx);
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_wrap_cols(obj, true),
                            TAG, "set bezier loop wrap seg[%u]", seg_idx);
        break;
    }
    case GFX_MOTION_SEG_BEZIER_FILL: {
        uint16_t nj = seg->joint_count >= 4U ? seg->joint_count : 4U;
        uint16_t kk = (nj - 1U) / 3U;

        if (nj == 7U || nj == 13U) {
            ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj,
                                (uint8_t)MOTION_BEZIER_FILL_SEGS, 1U),
                                TAG, "set bezier fill grid seg[%u]", seg_idx);
            ESP_RETURN_ON_ERROR(gfx_mesh_img_set_wrap_cols(obj, false),
                                TAG, "set bezier fill wrap seg[%u]", seg_idx);
        } else if (((nj - 1U) % 3U) == 0U) {
            uint16_t tcols = kk * (uint16_t)MOTION_BEZIER_FILL_LOOP_SEGS_PER_SEG;
            uint8_t gcols = (tcols > 255U) ? 255U : (uint8_t)tcols;
            ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, gcols, 1U),
                                TAG, "set bezier fill grid seg[%u]", seg_idx);
            ESP_RETURN_ON_ERROR(gfx_mesh_img_set_wrap_cols(obj, true),
                                TAG, "set bezier fill wrap seg[%u]", seg_idx);
        } else {
            ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, 1U, 1U),
                                TAG, "set bezier fill fallback grid seg[%u]", seg_idx);
            ESP_RETURN_ON_ERROR(gfx_mesh_img_set_wrap_cols(obj, false),
                                TAG, "set bezier fill fallback wrap seg[%u]", seg_idx);
        }
        break;
    }
    case GFX_MOTION_SEG_BEZIER_STRIP: {
        uint16_t n = (seg->joint_count >= 4U) ? seg->joint_count : 4U;
        uint16_t k = (n - 1U) / 3U;
        uint16_t tcols = k * (uint16_t)MOTION_BEZIER_SEGS_PER_SEG;
        uint8_t gcols = (tcols > 255U) ? 255U : (uint8_t)tcols;
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, gcols, 1U),
                            TAG, "set bezier strip grid seg[%u]", seg_idx);
        break;
    }
    default:
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, 1U, 1U),
                            TAG, "set default grid seg[%u]", seg_idx);
        break;
    }

    if (seg->kind != GFX_MOTION_SEG_BEZIER_FILL || !MOTION_BEZIER_FILL_USE_SCANLINE) {
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_aa_inward(obj, true), TAG, "set aa inward seg[%u]", seg_idx);
    }

    return ESP_OK;
}

static void gfx_motion_player_init_palette(gfx_motion_player_t *player, bool swap)
{
    const gfx_motion_asset_t *asset = player->scene.asset;

    for (uint8_t pi = 0U; pi < GFX_MOTION_PALETTE_MAX; pi++) {
        player->palette_imgs[pi] = player->solid_img;
        player->palette_pixels[pi] = player->solid_pixel;
        player->palette_imgs[pi].data = (const uint8_t *)&player->palette_pixels[pi];
    }

    if (asset->color_palette != NULL) {
        uint8_t n = (asset->color_palette_count < GFX_MOTION_PALETTE_MAX)
                    ? asset->color_palette_count : GFX_MOTION_PALETTE_MAX;
        for (uint8_t pi = 0U; pi < n; pi++) {
            gfx_color_t pc = GFX_COLOR_HEX(asset->color_palette[pi]);
            player->palette_pixels[pi] = gfx_color_to_native_u16(pc, swap);
        }
    }
}

static bool gfx_motion_player_tick_cb(gfx_motion_t *motion, void *user_data)
{
    gfx_motion_player_t *player = (gfx_motion_player_t *)user_data;
    bool changed;

    (void)motion;
    if (player == NULL) {
        return false;
    }
    gfx_motion_scene_advance(&player->scene);
    changed = gfx_motion_scene_tick(&player->scene);
    return changed || player->scene.dirty || player->mesh_dirty;
}

static esp_err_t gfx_motion_player_apply_segment(gfx_motion_player_t *player,
        gfx_motion_player_runtime_scratch_t *scratch,
        uint8_t seg_idx,
        int32_t def_stroke_px)
{
    const gfx_motion_scene_t *scene = &player->scene;
    const gfx_motion_asset_t *asset = scene->asset;
    const gfx_motion_segment_t *seg = &asset->segments[seg_idx];
    gfx_obj_t *obj = player->seg_objs[seg_idx];

    if (obj == NULL) {
        return ESP_OK;
    }
    if (!gfx_motion_player_segment_layer_visible(player, seg)) {
        gfx_obj_set_visible(obj, false);
        return ESP_OK;
    }

    int32_t stroke_px = (seg->stroke_width > 0)
                        ? gfx_motion_player_scalar_px(asset, player->canvas_w, player->canvas_h, (float)seg->stroke_width)
                        : def_stroke_px;

    switch (seg->kind) {
    case GFX_MOTION_SEG_CAPSULE: {
        gfx_motion_player_screen_point_t pa, pb;
        gfx_motion_player_to_screen(asset, &scene->pose_cur[seg->joint_a],
                                    player->canvas_x, player->canvas_y,
                                    player->canvas_w, player->canvas_h, &pa);
        gfx_motion_player_to_screen(asset, &scene->pose_cur[seg->joint_b],
                                    player->canvas_x, player->canvas_y,
                                    player->canvas_w, player->canvas_h, &pb);
        ESP_RETURN_ON_ERROR(gfx_motion_player_apply_capsule(obj, &pa, &pb, stroke_px),
                            TAG, "capsule seg[%u]", seg_idx);
        break;
    }

    case GFX_MOTION_SEG_RING: {
        gfx_motion_player_screen_point_t pc;
        gfx_motion_player_to_screen(asset, &scene->pose_cur[seg->joint_a],
                                    player->canvas_x, player->canvas_y,
                                    player->canvas_w, player->canvas_h, &pc);
        int32_t radius_px = (seg->radius_hint > 0)
                            ? gfx_motion_player_scalar_px(asset, player->canvas_w, player->canvas_h, (float)seg->radius_hint)
                            : stroke_px * 4;
        uint8_t segs = gfx_motion_player_ring_segs((float)radius_px);
        ESP_RETURN_ON_ERROR(gfx_motion_player_set_grid_internal(player, seg_idx, obj, segs, 1U),
                            TAG, "ring grid seg[%u]", seg_idx);
        ESP_RETURN_ON_ERROR(gfx_motion_player_apply_ring(obj, scratch, &pc, radius_px, stroke_px, segs),
                            TAG, "ring seg[%u]", seg_idx);
        break;
    }

    case GFX_MOTION_SEG_BEZIER_STRIP:
    case GFX_MOTION_SEG_BEZIER_LOOP: {
        uint16_t n = seg->joint_count;
        if (n < 2U || n > MOTION_BEZIER_MAX_PTS ||
                (uint32_t)seg->joint_a + n > asset->joint_count) {
            return ESP_OK;
        }
        for (uint16_t j = 0; j < n; j++) {
            gfx_motion_player_to_screen(asset, &scene->pose_cur[seg->joint_a + j],
                                        player->canvas_x, player->canvas_y,
                                        player->canvas_w, player->canvas_h,
                                        &scratch->ctrl_pts[j]);
        }
        bool loop = (seg->kind == GFX_MOTION_SEG_BEZIER_LOOP);
        ESP_RETURN_ON_ERROR(gfx_motion_player_apply_bezier(obj, scratch, scratch->ctrl_pts,
                            (uint8_t)n, stroke_px, loop), TAG, "bezier seg[%u]", seg_idx);
        break;
    }

    case GFX_MOTION_SEG_BEZIER_FILL: {
        uint16_t n = seg->joint_count;
        if (n < 3U || n > MOTION_BEZIER_MAX_PTS ||
                (uint32_t)seg->joint_a + n > asset->joint_count) {
            return ESP_OK;
        }
        for (uint16_t j = 0; j < n; j++) {
            gfx_motion_player_to_screen(asset, &scene->pose_cur[seg->joint_a + j],
                                        player->canvas_x, player->canvas_y,
                                        player->canvas_w, player->canvas_h,
                                        &scratch->ctrl_pts[j]);
        }
        ESP_RETURN_ON_ERROR(gfx_motion_player_apply_bezier_fill(obj, scratch, scratch->ctrl_pts, (uint8_t)n),
                            TAG, "bezier_fill seg[%u]", seg_idx);
        break;
    }

    default:
        break;
    }

    gfx_obj_set_visible(obj, true);
    return ESP_OK;
}

static esp_err_t gfx_motion_player_apply_cb(gfx_motion_t *motion, void *user_data, bool force)
{
    gfx_motion_player_t *player = (gfx_motion_player_t *)user_data;
    gfx_motion_player_runtime_scratch_t *scratch;
    const gfx_motion_scene_t *scene;
    const gfx_motion_asset_t *asset;
    int32_t def_stroke_px;

    (void)motion;
    if (player == NULL) {
        return ESP_OK;
    }
    scene = &player->scene;
    if (scene->asset == NULL) {
        return ESP_OK;
    }
    if (!force && !scene->dirty && !player->mesh_dirty) {
        return ESP_OK;
    }

    asset = scene->asset;
    scratch = (gfx_motion_player_runtime_scratch_t *)player->scratch;
    ESP_RETURN_ON_FALSE(scratch != NULL, ESP_ERR_INVALID_STATE, TAG, "runtime scratch is NULL");

    def_stroke_px = gfx_motion_player_scalar_px(asset, player->canvas_w, player->canvas_h,
                    (float)asset->layout->stroke_width);

    for (uint8_t i = 0; i < player->seg_obj_count && i < asset->segment_count; i++) {
        ESP_RETURN_ON_ERROR(gfx_motion_player_apply_segment(player, scratch, i, def_stroke_px),
                            TAG, "apply segment[%u]", i);
    }

    player->mesh_dirty = false;
    ((gfx_motion_scene_t *)scene)->dirty = false;
    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_motion_player_init(gfx_motion_player_t *player,
                                 gfx_disp_t *disp,
                                 const gfx_motion_asset_t *asset)
{
    esp_err_t ret = ESP_OK;
    gfx_img_src_t solid_src;
    gfx_motion_cfg_t motion_cfg;
    bool swap;

    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, TAG, "disp is NULL");
    ESP_RETURN_ON_FALSE(asset != NULL, ESP_ERR_INVALID_ARG, TAG, "asset is NULL");
    ESP_RETURN_ON_FALSE(asset->segment_count <= GFX_MOTION_PLAYER_MAX_SEGMENTS,
                        ESP_ERR_INVALID_ARG, TAG, "too many segments");

    memset(player, 0, sizeof(*player));

    player->scratch = calloc(1, sizeof(gfx_motion_player_runtime_scratch_t));
    ESP_GOTO_ON_FALSE(player->scratch != NULL, ESP_ERR_NO_MEM, err, TAG, "alloc runtime scratch failed");

    ESP_GOTO_ON_ERROR(gfx_motion_scene_init(&player->scene, asset), err, TAG, "scene init failed");

    player->stroke_color = GFX_COLOR_HEX(GFX_MOTION_DEFAULT_STROKE_COLOR);
    player->layer_mask = UINT32_MAX;
    swap = disp->flags.swap;
    player->solid_pixel = gfx_color_to_native_u16(player->stroke_color, swap);
    player->solid_img.header.magic = 0x19;
    player->solid_img.header.cf = GFX_COLOR_FORMAT_RGB565;
    player->solid_img.header.w = 1;
    player->solid_img.header.h = 1;
    player->solid_img.header.stride = 2;
    player->solid_img.data_size = 2;
    player->solid_img.data = (const uint8_t *)&player->solid_pixel;
    gfx_motion_player_init_palette(player, swap);

    solid_src.type = GFX_IMG_SRC_TYPE_IMAGE_DSC;
    solid_src.data = &player->solid_img;

    player->canvas_x = 0;
    player->canvas_y = 0;
    player->canvas_w = (uint16_t)gfx_disp_get_hor_res(disp);
    player->canvas_h = (uint16_t)gfx_disp_get_ver_res(disp);
    player->mesh_dirty = true;

    for (uint8_t i = 0; i < asset->segment_count; i++) {
        gfx_obj_t *obj = gfx_mesh_img_create(disp);
        ESP_GOTO_ON_FALSE(obj != NULL, ESP_ERR_NO_MEM, err, TAG, "mesh_img[%u] failed", i);
        gfx_obj_set_visible(obj, false);

        ESP_GOTO_ON_ERROR(gfx_motion_player_configure_segment_mesh(player, i, obj),
                          err, TAG, "configure segment mesh[%u]", i);
        ESP_GOTO_ON_ERROR(gfx_motion_player_bind_segment_style(player, i, obj, &solid_src),
                          err, TAG, "bind segment style[%u]", i);

        gfx_obj_set_visible(obj, false);
        player->seg_objs[i] = obj;
    }
    player->seg_obj_count = asset->segment_count;

    if (asset->segment_count == 0U) {
        return ESP_OK;
    }

    gfx_motion_cfg_init(&motion_cfg,
                        gfx_motion_player_layout_timer_period_ms(asset->layout),
                        gfx_motion_player_layout_damping_div(asset->layout));
    ESP_GOTO_ON_ERROR(
        gfx_motion_init(&player->motion, disp, player->seg_objs[0], &motion_cfg,
                        gfx_motion_player_tick_cb, gfx_motion_player_apply_cb, player),
        err, TAG, "motion init failed");

    return ESP_OK;

err:
    gfx_motion_player_deinit(player);
    return ret;
}

void gfx_motion_player_deinit(gfx_motion_player_t *player)
{
    if (player == NULL) {
        return;
    }
    gfx_motion_deinit(&player->motion);
    for (uint8_t i = 0; i < player->seg_obj_count; i++) {
        if (player->seg_objs[i] != NULL) {
            gfx_obj_delete(player->seg_objs[i]);
            player->seg_objs[i] = NULL;
        }
    }
    if (player->scratch != NULL) {
        free(player->scratch);
        player->scratch = NULL;
    }

    memset(player, 0, sizeof(*player));
}

esp_err_t gfx_motion_player_set_color(gfx_motion_player_t *player, gfx_color_t color)
{
    gfx_img_src_t solid_src;
    bool swap;

    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    if (player->seg_obj_count == 0U) {
        player->stroke_color = color;
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(player->seg_objs[0] != NULL && player->seg_objs[0]->disp != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display not ready");

    swap = player->seg_objs[0]->disp->flags.swap;
    player->stroke_color = color;
    player->solid_pixel = gfx_color_to_native_u16(color, swap);

    solid_src.type = GFX_IMG_SRC_TYPE_IMAGE_DSC;
    solid_src.data = &player->solid_img;

    for (uint8_t i = 0; i < player->seg_obj_count; i++) {
        if (player->scene.asset != NULL && i < player->scene.asset->segment_count) {
            const gfx_motion_segment_t *seg = &player->scene.asset->segments[i];
            if (seg->resource_idx != 0U || seg->color_idx != 0U) {
                continue;
            }
            if (MOTION_BEZIER_FILL_USE_SCANLINE &&
                    seg->kind == GFX_MOTION_SEG_BEZIER_FILL && seg->resource_idx == 0U) {
                ESP_RETURN_ON_ERROR(gfx_mesh_img_set_scanline_fill(player->seg_objs[i], true, color),
                                    TAG, "set fill color seg[%u]", i);
            }
        }
        ESP_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(player->seg_objs[i], &solid_src),
                            TAG, "set color seg[%u]", i);
    }
    return ESP_OK;
}

esp_err_t gfx_motion_player_set_canvas(gfx_motion_player_t *player,
                                       gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    ESP_RETURN_ON_FALSE(w > 0U && h > 0U, ESP_ERR_INVALID_ARG, TAG, "size must be > 0");
    player->canvas_x = x;
    player->canvas_y = y;
    player->canvas_w = w;
    player->canvas_h = h;
    player->mesh_dirty = true;
    return ESP_OK;
}

esp_err_t gfx_motion_player_set_layer_mask(gfx_motion_player_t *player, uint32_t layer_mask)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    player->layer_mask = layer_mask;
    player->mesh_dirty = true;
    return ESP_OK;
}

esp_err_t gfx_motion_player_sync(gfx_motion_player_t *player)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");

    if (player->seg_obj_count == 0U) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(player->motion.apply_cb != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "player apply callback is NULL");
    return player->motion.apply_cb(&player->motion, player->motion.user_data, true);
}

esp_err_t gfx_motion_player_set_action(gfx_motion_player_t *player, uint16_t action_idx, bool snap)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    return gfx_motion_scene_set_action(&player->scene, action_idx, snap);
}

esp_err_t gfx_motion_player_set_action_loop(gfx_motion_player_t *player, bool loop)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    return gfx_motion_scene_set_action_loop(&player->scene, loop);
}

esp_err_t gfx_motion_player_clear_action_loop_override(gfx_motion_player_t *player)
{
    ESP_RETURN_ON_FALSE(player != NULL, ESP_ERR_INVALID_ARG, TAG, "player is NULL");
    return gfx_motion_scene_clear_action_loop_override(&player->scene);
}
