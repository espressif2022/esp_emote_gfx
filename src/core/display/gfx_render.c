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

#include "esp_timer.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_RENDER
#include "common/gfx_log_priv.h"

#include "core/display/gfx_refr_priv.h"
#include "core/display/gfx_render_priv.h"
#include "core/draw/gfx_blend_priv.h"
#include "core/runtime/gfx_timer_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "render";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_render_sync_dirty_areas(gfx_disp_t *disp);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_render_sync_dirty_areas(gfx_disp_t *disp)
{
    if (!disp->flags.full_frame || disp->buf.buf2 == NULL || disp->sync_pending.count == 0) {
        return;
    }

    uint16_t *dst_screen_buf = disp->buf.buf_act;
    uint16_t *src_screen_buf = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
    uint32_t stride = disp->res.h_res;
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
        uint32_t w = (uint32_t)(a->x2 - a->x1 + 1);
        uint32_t h = (uint32_t)(a->y2 - a->y1 + 1);
        for (uint32_t y = 0; y < h; y++) {
            size_t offset = (size_t)(a->y1 + (gfx_coord_t)y) * stride + (size_t)a->x1;
            memcpy(dst_screen_buf + offset, src_screen_buf + offset, w * px_size);
        }
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

void gfx_render_draw_child_objects(gfx_disp_t *disp, const gfx_draw_ctx_t *ctx)
{
    if (disp == NULL || disp->child_list == NULL || ctx == NULL) {
        return;
    }

    gfx_obj_child_t *child_node = disp->child_list;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

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


void gfx_render_update_child_objects(gfx_disp_t *disp)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    gfx_obj_child_t *child_node = disp->child_list;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

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

uint32_t gfx_render_area_summary(gfx_disp_t *disp)
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

void gfx_render_part_area(gfx_disp_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area)
{
    if (disp == NULL || area == NULL) {
        return;
    }

    if (area->x2 < area->x1 || area->y2 < area->y1) {
        GFX_LOGE(TAG, "render area[%d]: invalid bounds (%d,%d)-(%d,%d)", area_idx,
                 area->x1, area->y1, area->x2, area->y2);
        return;
    }

    uint32_t area_w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t row_h = disp->buf.buf_pixels / area_w;
    if (row_h == 0) {
        GFX_LOGE(TAG, "render area[%d]: width %" PRIu32 " exceeds buffer, skipping", area_idx, area_w);
        return;
    }

    gfx_disp_flush_cb_t flush_cb = disp->cb.flush_cb;
    if (flush_cb != NULL && disp->sync.event_group == NULL) {
        GFX_LOGE(TAG, "render area[%d]: flush callback is set but event group is NULL", area_idx);
        return;
    }

    disp->render.flushing_last = false;
    gfx_coord_t cur_y = area->y1;

    while (cur_y <= area->y2) {
        int64_t render_start_us;
        int64_t flush_start_us;

        gfx_coord_t chunk_x1 = area->x1;
        gfx_coord_t chunk_y1 = cur_y;
        gfx_coord_t chunk_x2 = area->x2 + 1;
        gfx_coord_t chunk_y2 = cur_y + (gfx_coord_t)row_h;
        if (chunk_y2 > area->y2 + 1) {
            chunk_y2 = area->y2 + 1;
        }

        uint16_t *buf = disp->buf.buf_act;

        int dest_stride = disp->flags.full_frame ? disp->res.h_res : (chunk_x2 - chunk_x1);

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

        render_start_us = esp_timer_get_time();
        if (disp->style.bg_enable) {
            uint16_t bg = gfx_color_to_native_u16(disp->style.bg_color, disp->flags.swap);
            if (disp->flags.full_frame) {
                gfx_area_t fill_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 };  /* exclusive x2,y2 */
                gfx_sw_blend_fill_area(buf, (gfx_coord_t)disp->res.h_res, &fill_area, bg);
            } else {
                gfx_area_t fill_area = { 0, 0, chunk_x2 - chunk_x1, chunk_y2 - chunk_y1 };
                gfx_sw_blend_fill_area(buf, chunk_x2 - chunk_x1, &fill_area, bg);
            }
        }
        gfx_render_draw_child_objects(disp, &draw_ctx);
        disp->render.render_time_us += (uint64_t)(esp_timer_get_time() - render_start_us);

        if (flush_cb != NULL) {
            xEventGroupClearBits(disp->sync.event_group, WAIT_FLUSH_DONE);

            // uint32_t chunk_px = area_w * (uint32_t)(chunk_y2 - chunk_y1);

            bool is_last_chunk = (chunk_y2 >= area->y2 + 1);
            disp->render.flushing_last = is_last_chunk && is_last_area;

            // GFX_LOGD(TAG, "Flush: (%d,%d)-(%d,%d) %" PRIu32 " px%s",
            //          chunk_x1, chunk_y1, chunk_x2 - 1, chunk_y2 - 1, chunk_px,
            //          disp->render.flushing_last ? " (last)" : "");

            flush_start_us = esp_timer_get_time();
            flush_cb(disp, chunk_x1, chunk_y1, chunk_x2, chunk_y2, buf);

            xEventGroupWaitBits(disp->sync.event_group, WAIT_FLUSH_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
            disp->render.flush_time_us += (uint64_t)(esp_timer_get_time() - flush_start_us);
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
void gfx_render_dirty_areas(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return;
    }

    disp->render.render_time_us = 0;
    disp->render.flush_time_us = 0;
    disp->render.flush_count = 0;
    gfx_sw_blend_perf_reset(&disp->render.blend);
    gfx_sw_blend_perf_bind(&disp->render.blend);

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
void gfx_render_cleanup(gfx_disp_t *disp)
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
    static const uint32_t fps_sample_window = 100;
    static uint32_t fps_samples = 0;
    static uint32_t fps_elapsed_ms = 0;
    static uint32_t last_tick_ms = 0;

    uint32_t now_ms = gfx_timer_tick_get();
    if (last_tick_ms == 0) {
        last_tick_ms = now_ms;
    } else {
        uint32_t elapsed_ms = gfx_timer_tick_elaps(last_tick_ms);
        fps_samples++;
        fps_elapsed_ms += elapsed_ms;
        last_tick_ms = now_ms;

        if (fps_samples >= fps_sample_window) {
            gfx_timer_mgr_t *mgr = &ctx->timer_mgr;
            mgr->actual_fps = (fps_samples * 1000) / fps_elapsed_ms;
            fps_samples = 0;
            fps_elapsed_ms = 0;
        }
    }

    bool did_render = false;

    for (gfx_disp_t *disp = ctx->disp; disp != NULL; disp = disp->next) {
        int64_t frame_start_us = esp_timer_get_time();
        gfx_refr_update_layout_dirty(disp);

        if (disp->dirty.count > 1) {
            gfx_refr_merge_areas(disp);
        } else if (disp->dirty.count == 0) {
            continue;
        }

        gfx_render_update_child_objects(disp);

        uint32_t dirty_px = gfx_render_area_summary(disp);
        gfx_render_dirty_areas(disp);
        uint64_t frame_time_us = (uint64_t)(esp_timer_get_time() - frame_start_us);
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
