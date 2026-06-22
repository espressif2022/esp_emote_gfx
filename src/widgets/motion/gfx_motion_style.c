/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_MOTION
#define GFX_LOG_TAG    "gfx_motion_style"
#include "common/gfx_log_priv.h"
#include "widgets/motion/gfx_motion_player_priv.h"

gfx_err_t gfx_motion_player_bind_style_common(gfx_motion_player_t *player,
        const gfx_motion_segment_t *seg, gfx_object_t *obj, const gfx_image_src_t *solid_src)
{
    GFX_RETURN_ON_FALSE(player != NULL && seg != NULL && obj != NULL && solid_src != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "style target is NULL");

    GFX_RETURN_ON_ERROR(
        gfx_mesh_img_set_scanline_fill(obj, false, gfx_motion_player_resolve_fill_color(player, seg)),
        TAG, "clear scanline fill");

    if (seg->resource_idx > 0U &&
            player->scene.asset != NULL &&
            player->scene.asset->resources != NULL &&
            (uint8_t)(seg->resource_idx - 1U) < player->scene.asset->resource_count &&
            player->scene.asset->resources[seg->resource_idx - 1U].image != NULL) {
        gfx_image_src_t res_src = {
            .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
            .data = (const void *)player->scene.asset->resources[seg->resource_idx - 1U].image,
        };
        GFX_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(obj, &res_src), TAG, "set resource src");
    } else if (seg->color_idx > 0U && seg->color_idx <= GFX_MOTION_PALETTE_MAX) {
        gfx_image_src_t pal_src = {
            .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
            .data = &player->palette_imgs[seg->color_idx - 1U],
        };
        GFX_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(obj, &pal_src), TAG, "set palette src");
    } else {
        GFX_RETURN_ON_ERROR(gfx_mesh_img_set_src_desc(obj, solid_src), TAG, "set solid src");
    }

    GFX_RETURN_ON_ERROR(gfx_mesh_img_set_opa(obj, gfx_motion_player_segment_opacity(seg)),
                        TAG, "set opacity");
    if (MOTION_BEZIER_FILL_USE_SCANLINE &&
            seg->kind == GFX_MOTION_SEG_BEZIER_FILL && seg->resource_idx == 0U) {
        GFX_RETURN_ON_ERROR(
            gfx_mesh_img_set_scanline_fill(obj, true, gfx_motion_player_resolve_fill_color(player, seg)),
            TAG, "set scanline fill");
    }

    return GFX_OK;
}

uint16_t gfx_motion_player_layout_timer_period_ms(const gfx_motion_layout_t *layout)
{
    return (layout != NULL && layout->timer_period_ms > 0U) ?
           layout->timer_period_ms :
           GFX_MOTION_DEFAULT_TIMER_PERIOD_MS;
}

int16_t gfx_motion_player_layout_damping_div(const gfx_motion_layout_t *layout)
{
    return (layout != NULL && layout->damping_div > 0) ?
           layout->damping_div :
           GFX_MOTION_DEFAULT_DAMPING_DIV;
}

gfx_opa_t gfx_motion_player_segment_opacity(const gfx_motion_segment_t *seg)
{
    if (seg == NULL || seg->opacity == 0U) {
        return GFX_MOTION_DEFAULT_SEG_OPACITY;
    }

    return seg->opacity;
}

gfx_color_t gfx_motion_player_resolve_fill_color(const gfx_motion_player_t *rt,
        const gfx_motion_segment_t *seg)
{
    if (rt != NULL && seg != NULL && rt->scene.asset != NULL &&
            seg->color_idx > 0U &&
            rt->scene.asset->color_palette != NULL &&
            seg->color_idx <= rt->scene.asset->color_palette_count) {
        return GFX_COLOR_HEX(rt->scene.asset->color_palette[seg->color_idx - 1U]);
    }
    return (rt != NULL) ? rt->stroke_color : GFX_COLOR_HEX(GFX_MOTION_DEFAULT_STROKE_COLOR);
}

bool gfx_motion_player_segment_layer_visible(const gfx_motion_player_t *rt,
        const gfx_motion_segment_t *seg)
{
    if (rt == NULL || seg == NULL || seg->layer_bit == 0U) {
        return true;
    }

    return (rt->layer_mask & (1UL << (seg->layer_bit - 1U))) != 0U;
}

gfx_err_t gfx_motion_player_apply_resource_uv(const gfx_motion_player_t *rt, uint8_t seg_idx,
        gfx_object_t *obj, uint8_t cols, uint8_t rows)
{
    const gfx_motion_segment_t *seg;
    const gfx_motion_resource_t *res;
    gfx_mesh_img_point_q8_t *points;
    uint32_t img_w;
    uint32_t img_h;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t crop_w;
    uint32_t crop_h;
    uint32_t right_q8;
    uint32_t bottom_q8;
    size_t point_count;
    size_t idx = 0U;
    gfx_err_t ret;

    if (rt == NULL || rt->scene.asset == NULL || seg_idx >= rt->scene.asset->segment_count) {
        return GFX_OK;
    }

    seg = &rt->scene.asset->segments[seg_idx];
    if (seg->resource_idx == 0U) {
        return GFX_OK;
    }

    res = &rt->scene.asset->resources[seg->resource_idx - 1U];
    img_w = res->image->header.w;
    img_h = res->image->header.h;
    crop_x = res->uv_x;
    crop_y = res->uv_y;
    crop_w = (res->uv_w > 0U) ? res->uv_w : (img_w - crop_x);
    crop_h = (res->uv_h > 0U) ? res->uv_h : (img_h - crop_y);
    right_q8 = (crop_x + crop_w - 1U) << GFX_MESH_FRAC_SHIFT;
    bottom_q8 = (crop_y + crop_h - 1U) << GFX_MESH_FRAC_SHIFT;
    point_count = (size_t)(cols + 1U) * (size_t)(rows + 1U);

    points = calloc(point_count, sizeof(*points));
    GFX_RETURN_ON_FALSE(points != NULL, GFX_ERR_NO_MEM, TAG, "alloc resource uv points failed");

    for (uint8_t row = 0U; row <= rows; row++) {
        uint32_t y_q8 = ((crop_y << GFX_MESH_FRAC_SHIFT) * (uint32_t)(rows - row) +
                         bottom_q8 * (uint32_t)row) / rows;
        for (uint8_t col = 0U; col <= cols; col++) {
            uint32_t x_q8 = ((crop_x << GFX_MESH_FRAC_SHIFT) * (uint32_t)(cols - col) +
                             right_q8 * (uint32_t)col) / cols;
            points[idx].x_q8 = (int32_t)x_q8;
            points[idx].y_q8 = (int32_t)y_q8;
            idx++;
        }
    }

    ret = gfx_mesh_img_set_rest_points_q8(obj, points, point_count);
    free(points);
    return ret;
}

gfx_err_t gfx_motion_player_bind_segment_style(gfx_motion_player_t *player, uint8_t seg_idx,
        gfx_object_t *obj, const gfx_image_src_t *solid_src)
{
    const gfx_motion_asset_t *asset;
    const gfx_motion_segment_t *seg;

    GFX_RETURN_ON_FALSE(player != NULL && obj != NULL && solid_src != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "style target is NULL");
    asset = player->scene.asset;
    GFX_RETURN_ON_FALSE(asset != NULL && seg_idx < asset->segment_count,
                        GFX_ERR_INVALID_ARG, TAG, "style segment index out of range");

    seg = &asset->segments[seg_idx];
    GFX_RETURN_ON_ERROR(gfx_motion_player_bind_style_common(player, seg, obj, solid_src),
                        TAG, "bind common style seg[%u]", seg_idx);
    if (seg->resource_idx > 0U &&
            asset->resources != NULL &&
            (uint8_t)(seg->resource_idx - 1U) < asset->resource_count &&
            asset->resources[seg->resource_idx - 1U].image != NULL) {
        GFX_RETURN_ON_ERROR(
            gfx_motion_player_apply_resource_uv(player, seg_idx, obj,
                                                player->seg_grid_cols[seg_idx],
                                                player->seg_grid_rows[seg_idx]),
            TAG, "set resource uv seg[%u]", seg_idx);
    }

    return GFX_OK;
}
