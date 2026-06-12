/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_RENDER
#include "common/gfx_log_priv.h"

#include "core/display/gfx_refresh_priv.h"
#include "render/gfx_render_priv.h"
#include "render/sw/gfx_blend_priv.h"
#include "core/runtime/gfx_timer_priv.h"

/*********************
 *      DEFINES
 *********************/

#define GFX_RENDER_OPA_COVER 0xFFU

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "render";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_render_sync_dirty_areas(gfx_display_t *disp);
static bool gfx_render_backend_fill(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                    const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);
static void gfx_render_fill_area(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                 const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa);
static gfx_coord_t gfx_render_align_floor(gfx_coord_t value, uint16_t alignment);
static gfx_coord_t gfx_render_align_ceil(gfx_coord_t value, uint16_t alignment);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_render_sync_dirty_areas(gfx_display_t *disp)
{
    if (!disp->flags.full_frame || disp->buf.buf2 == NULL || disp->sync_pending.count == 0) {
        return;
    }

    uint16_t *dst_screen_buf = disp->buf.buf_act;
    uint16_t *src_screen_buf = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
    gfx_coord_t stride = (gfx_coord_t)disp->res.h_res;
    const size_t px_size = sizeof(uint16_t);

    for (uint8_t i = 0; i < disp->sync_pending.count; i++) {
        const gfx_area_t *a = &disp->sync_pending.areas[i];
        bool covered = false;
        for (uint8_t j = 0; j < disp->dirty.count && !covered; j++) {
            if (disp->dirty.merged[j]) {
                continue;
            }
            if (gfx_area_is_in(a, &disp->dirty.areas[j])) {
                covered = true;
            }
        }
        if (covered) {
            continue;
        }
        size_t w = (size_t)(a->x2 - a->x1 + 1);
        size_t h = (size_t)(a->y2 - a->y1 + 1);
        for (size_t y = 0; y < h; y++) {
            size_t offset = (size_t)(a->y1 + (gfx_coord_t)y) * stride + (size_t)a->x1;
            memcpy(dst_screen_buf + offset, src_screen_buf + offset, w * px_size);
        }
    }
}

static bool gfx_render_backend_fill(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                    const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    if (disp == NULL || ctx == NULL || area == NULL ||
            !gfx_backend_has_caps(disp->backend, GFX_BACKEND_CAP_FILL)) {
        return false;
    }

    const gfx_draw_ops_t *ops = gfx_backend_get_draw_ops(disp->backend);
    if (ops == NULL || ops->fill == NULL) {
        return false;
    }

    gfx_backend_surface_t dst = {
        .buf = ctx->buf,
        .area = ctx->buf_area,
        .stride = ctx->stride,
        .format = ctx->swap ? GFX_COLOR_FORMAT_RGB565_SWAPPED : GFX_COLOR_FORMAT_RGB565,
        .swap = ctx->swap,
    };

    return ops->fill(disp->backend, disp, &dst, area, color, opa) == ESP_OK;
}

static void gfx_render_fill_area(gfx_display_t *disp, const gfx_draw_ctx_t *ctx,
                                 const gfx_area_t *area, gfx_color_t color, gfx_opa_t opa)
{
    if (ctx == NULL || area == NULL || opa == 0U) {
        return;
    }

    if (opa == GFX_RENDER_OPA_COVER && gfx_render_backend_fill(disp, ctx, area, color, opa)) {
        return;
    }

    gfx_area_t local_area = {
        .x1 = (gfx_coord_t)(area->x1 - ctx->buf_area.x1),
        .y1 = (gfx_coord_t)(area->y1 - ctx->buf_area.y1),
        .x2 = (gfx_coord_t)(area->x2 - ctx->buf_area.x1),
        .y2 = (gfx_coord_t)(area->y2 - ctx->buf_area.y1),
    };

    if (opa == GFX_RENDER_OPA_COVER) {
        gfx_sw_blend_fill_area_color((gfx_color_t *)ctx->buf, ctx->stride, &local_area, color, ctx->swap);
    } else {
        gfx_sw_blend_draw((gfx_color_t *)ctx->buf, ctx->stride, NULL, 0, &local_area, color, opa, ctx->swap);
    }
}

static gfx_coord_t gfx_render_align_floor(gfx_coord_t value, uint16_t alignment)
{
    if (alignment <= 1U) {
        return value;
    }

    gfx_coord_t align = (gfx_coord_t)alignment;
    if (value >= 0) {
        return (gfx_coord_t)((value / align) * align);
    }

    return (gfx_coord_t)(-(((-value + align - 1) / align) * align));
}

static gfx_coord_t gfx_render_align_ceil(gfx_coord_t value, uint16_t alignment)
{
    if (alignment <= 1U) {
        return value;
    }

    gfx_coord_t align = (gfx_coord_t)alignment;
    if (value >= 0) {
        return (gfx_coord_t)(((value + align - 1) / align) * align);
    }

    return (gfx_coord_t)(-((-value / align) * align));
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_render_draw_child_objects(gfx_display_t *disp, const gfx_draw_ctx_t *ctx)
{
    if (disp == NULL || disp->child_list == NULL || ctx == NULL) {
        return;
    }

    gfx_object_child_t *child_node = disp->child_list;

    while (child_node != NULL) {
        gfx_object_t *obj = (gfx_object_t *)child_node->src;

        if (!obj->state.is_visible) {
            child_node = child_node->next;
            continue;
        }

        if (obj->vfunc.draw) {
            obj->vfunc.draw(obj, ctx);
        }

        child_node = child_node->next;
    }
}


void gfx_render_update_child_objects(gfx_display_t *disp)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    gfx_object_child_t *child_node = disp->child_list;

    while (child_node != NULL) {
        gfx_object_t *obj = (gfx_object_t *)child_node->src;

        if (!obj->state.is_visible) {
            child_node = child_node->next;
            continue;
        }

        if (obj->vfunc.update) {
            obj->vfunc.update(obj);
        }

        child_node = child_node->next;
    }
}

uint32_t gfx_render_area_summary(gfx_display_t *disp)
{
    uint32_t total_dirty_pixels = 0;

    if (disp == NULL) {
        return 0;
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }
        gfx_area_t *area = &disp->dirty.areas[i];
        uint32_t area_size = gfx_area_get_size(area);
        total_dirty_pixels += area_size;
        // GFX_LOGD(TAG, "Draw area [%d]: (%d,%d)->(%d,%d) %dx%d",
        //          i, area->x1, area->y1, area->x2, area->y2,
        //          area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    }

    return total_dirty_pixels;
}

gfx_area_t gfx_render_roundup_area(const gfx_area_t *area,
                                   const gfx_render_alignment_t *alignment,
                                   const gfx_area_t *limit)
{
    gfx_area_t result = {0};

    if (area == NULL) {
        return result;
    }

    uint16_t width_align = alignment != NULL && alignment->width_px > 0U ? alignment->width_px : 1U;
    uint16_t height_align = alignment != NULL && alignment->height_px > 0U ? alignment->height_px : 1U;

    result.x1 = gfx_render_align_floor(area->x1, width_align);
    result.y1 = gfx_render_align_floor(area->y1, height_align);
    result.x2 = gfx_render_align_ceil(area->x2, width_align);
    result.y2 = gfx_render_align_ceil(area->y2, height_align);

    if (limit != NULL) {
        if (result.x1 < limit->x1) {
            result.x1 = limit->x1;
        }
        if (result.y1 < limit->y1) {
            result.y1 = limit->y1;
        }
        if (result.x2 > limit->x2) {
            result.x2 = limit->x2;
        }
        if (result.y2 > limit->y2) {
            result.y2 = limit->y2;
        }
    }

    return result;
}

uint32_t gfx_render_roundup_stride_bytes(uint32_t stride_bytes,
        const gfx_render_alignment_t *alignment)
{
    uint16_t stride_align = alignment != NULL && alignment->stride_bytes > 0U ? alignment->stride_bytes : 1U;

    if (stride_align <= 1U || stride_bytes == 0U) {
        return stride_bytes;
    }

    return ((stride_bytes + stride_align - 1U) / stride_align) * stride_align;
}

void gfx_render_part_area(gfx_display_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area)
{
    if (disp == NULL || area == NULL) {
        return;
    }

    if (area->x2 < area->x1 || area->y2 < area->y1) {
        GFX_LOGE(TAG, "render area[%d]: invalid bounds (%d,%d)-(%d,%d)", area_idx,
                 area->x1, area->y1, area->x2, area->y2);
        return;
    }

    gfx_render_alignment_t alignment = gfx_backend_get_alignment(disp->backend);
    gfx_area_t display_limit = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (gfx_coord_t)disp->res.h_res,
        .y2 = (gfx_coord_t)disp->res.v_res,
    };
    gfx_area_t requested_area = {
        .x1 = area->x1,
        .y1 = area->y1,
        .x2 = (gfx_coord_t)(area->x2 + 1),
        .y2 = (gfx_coord_t)(area->y2 + 1),
    };
    gfx_area_t render_area = gfx_render_roundup_area(&requested_area, &alignment, &display_limit);
    if (render_area.x2 <= render_area.x1 || render_area.y2 <= render_area.y1) {
        GFX_LOGE(TAG, "render area[%d]: aligned area is empty", area_idx);
        return;
    }

    uint32_t render_w = (uint32_t)(render_area.x2 - render_area.x1);
    uint32_t render_h = (uint32_t)(render_area.y2 - render_area.y1);
    uint32_t row_h = disp->buf.buf_pixels / render_w;
    if (row_h == 0) {
        GFX_LOGE(TAG, "render area[%d]: width %" PRIu32 " exceeds buffer, skipping", area_idx, render_w);
        return;
    }
    if (row_h > render_h) {
        row_h = render_h;
    }

    disp->render.flushing_last = false;
    gfx_coord_t cur_y = render_area.y1;

    while (cur_y < render_area.y2) {
        int64_t render_start_us;
        int64_t flush_start_us;

        gfx_coord_t chunk_x1 = render_area.x1;
        gfx_coord_t chunk_y1 = cur_y;
        gfx_coord_t chunk_x2 = render_area.x2;
        uint32_t remaining_h = (uint32_t)(render_area.y2 - cur_y);
        uint32_t chunk_h = row_h < remaining_h ? row_h : remaining_h;
        gfx_coord_t chunk_y2 = (gfx_coord_t)(cur_y + (gfx_coord_t)chunk_h);
        if (chunk_y2 > render_area.y2) {
            chunk_y2 = render_area.y2;
        }

        uint16_t *buf = disp->buf.buf_act;

        gfx_coord_t dest_stride = disp->flags.full_frame ? (gfx_coord_t)disp->res.h_res : (chunk_x2 - chunk_x1);

        gfx_area_t buf_area;
        if (disp->flags.full_frame) {
            buf_area.x1 = 0;
            buf_area.y1 = 0;
            buf_area.x2 = (gfx_coord_t)disp->res.h_res;
            buf_area.y2 = (gfx_coord_t)disp->res.v_res;
        } else {
            buf_area.x1 = chunk_x1;
            buf_area.y1 = chunk_y1;
            buf_area.x2 = chunk_x2;
            buf_area.y2 = chunk_y2;
        }

        gfx_draw_ctx_t draw_ctx = {
            .buf = buf,
            .buf_area = buf_area,
            .clip_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 },
            .stride = dest_stride,
            .swap = disp->flags.swap,
        };

        render_start_us = gfx_platform_time_us();
        if (disp->style.bg_enable) {
            gfx_area_t fill_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 };
            gfx_render_fill_area(disp, &draw_ctx, &fill_area, disp->style.bg_color, GFX_RENDER_OPA_COVER);
        }
        gfx_render_draw_child_objects(disp, &draw_ctx);
        disp->render.render_time_us += (uint64_t)(gfx_platform_time_us() - render_start_us);

        if (disp->backend != NULL) {
            // uint32_t chunk_px = render_w * (uint32_t)(chunk_y2 - chunk_y1);

            bool is_last_chunk = (chunk_y2 >= render_area.y2);
            disp->render.flushing_last = is_last_chunk && is_last_area;

            // GFX_LOGD(TAG, "Flush: (%d,%d)-(%d,%d) %" PRIu32 " px%s",
            //          chunk_x1, chunk_y1, chunk_x2 - 1, chunk_y2 - 1, chunk_px,
            //          disp->render.flushing_last ? " (last)" : "");

            flush_start_us = gfx_platform_time_us();
            if (gfx_backend_flush(disp, chunk_x1, chunk_y1, chunk_x2, chunk_y2, buf) != ESP_OK) {
                GFX_LOGE(TAG, "render area[%d]: backend flush failed", area_idx);
                return;
            }
            if (gfx_backend_wait_flush(disp) != ESP_OK) {
                GFX_LOGE(TAG, "render area[%d]: backend wait failed", area_idx);
                return;
            }
            disp->render.flush_time_us += (uint64_t)(gfx_platform_time_us() - flush_start_us);
            disp->render.flush_count++;

            if (disp->buf.buf2 != NULL && (!disp->flags.full_frame || disp->render.flushing_last)) {
                disp->buf.buf_act = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
            }
        }

        cur_y = chunk_y2;
    }
}

/**
 * @brief Render all dirty areas
 * @param disp Display
 */
void gfx_render_dirty_areas(gfx_display_t *disp)
{
    if (disp == NULL) {
        return;
    }

    disp->render.render_time_us = 0;
    disp->render.flush_time_us = 0;
    disp->render.flush_count = 0;
    gfx_sw_blend_perf_reset(&disp->render.draw);
    gfx_sw_blend_perf_bind(&disp->render.draw);

    gfx_render_sync_dirty_areas(disp);

    uint8_t last_area_idx = 0;
    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (!disp->dirty.merged[i]) {
            last_area_idx = i;
        }
    }

    uint8_t sync_points = 0;
    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }
        gfx_area_t *area = &disp->dirty.areas[i];
        bool is_last_area = (i == last_area_idx);
        gfx_render_part_area(disp, area, i, is_last_area);
        sync_points++;
        gfx_area_copy(&disp->sync_pending.areas[sync_points], area);
    }
    gfx_sw_blend_perf_unbind();
    disp->sync_pending.count = sync_points;
}

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags
 * @param disp Display
 */
void gfx_render_cleanup(gfx_display_t *disp)
{
    if (disp == NULL) {
        return;
    }

    if (disp->dirty.count > 0) {
        gfx_invalidate_area_disp(disp, NULL);
    }
}

/**
 * @brief Handle rendering of all objects in the scene
 * @param ctx Player context
 * @return true if any display was rendered, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx)
{
    uint32_t now_ms = gfx_timer_tick_get();
    gfx_timer_mgr_t *mgr = &ctx->timer_mgr;
    const uint32_t fps_sample_window = 100;

    if (mgr->render_fps_last_tick == 0) {
        mgr->render_fps_last_tick = now_ms;
    } else {
        uint32_t elapsed_ms = gfx_timer_tick_elaps(mgr->render_fps_last_tick);
        mgr->render_fps_samples++;
        mgr->render_fps_elapsed_ms += elapsed_ms;
        mgr->render_fps_last_tick = now_ms;

        if (mgr->render_fps_samples >= fps_sample_window) {
            mgr->actual_fps = (mgr->render_fps_samples * 1000) / mgr->render_fps_elapsed_ms;
            mgr->render_fps_samples = 0;
            mgr->render_fps_elapsed_ms = 0;
        }
    }

    bool did_render = false;

    for (gfx_display_t *disp = ctx->disp; disp != NULL; disp = disp->next) {
        int64_t frame_start_us = gfx_platform_time_us();
        gfx_refresh_update_layout_dirty(disp);

        if (disp->dirty.count > 1) {
            gfx_refresh_merge_areas(disp);
        } else if (disp->dirty.count == 0) {
            continue;
        }

        gfx_render_update_child_objects(disp);

        uint32_t dirty_px = gfx_render_area_summary(disp);
        gfx_render_dirty_areas(disp);
        uint64_t frame_time_us = (uint64_t)(gfx_platform_time_us() - frame_start_us);
        disp->render.dirty_pixels = dirty_px;
        disp->render.frame_time_us = frame_time_us;

        if (dirty_px > 0) {
            did_render = true;
            uint32_t screen_px = disp->res.h_res * disp->res.v_res;
            float dirty_pct = (dirty_px * 100.0f) / (float)screen_px;
            GFX_LOGD(TAG,
                     "%.1f%% (%" PRIu64 "ms) (%" PRIu64 "|%" PRIu64 ")",
                     dirty_pct,
                     frame_time_us / 1000,
                     disp->render.render_time_us / 1000,
                     disp->render.flush_time_us / 1000);
        }

        gfx_render_cleanup(disp);
    }

    return did_render;
}
