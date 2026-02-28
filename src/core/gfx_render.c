/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "core/gfx_render_priv.h"
#include "core/gfx_refr_priv.h"
#include "core/gfx_timer_priv.h"
#include "core/gfx_blend_priv.h"

static const char *TAG = "gfx_render";

/**
 * @brief Draw child objects in the specified clip region
 * @param ctx Draw context (buf, buf_area, clip_area, stride, swap); widgets compute dest from ctx
 */
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

/**
 * @brief Print summary of dirty areas
 * @param disp Display
 * @return Total dirty pixels
 */
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
        ESP_LOGD(TAG, "Draw area [%d]: (%d,%d)->(%d,%d) %dx%d",
                 i, area->x1, area->y1, area->x2, area->y2,
                 area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    }

    return total_dirty_pixels;
}

/**
 * @brief Render a single dirty area with dynamic height-based blocking
 * @param disp Display (non-NULL)
 * @param area Dirty area in screen coordinates (non-NULL, must have x2 >= x1, y2 >= y1)
 * @param area_idx Index of this area in the dirty list (for logging)
 * @param is_last_area true if this is the last dirty area (flushing_last = last chunk of area AND is_last_area)
 */
void gfx_render_part_area(gfx_disp_t *disp, gfx_area_t *area, uint8_t area_idx, bool is_last_area)
{
    if (disp == NULL || area == NULL) {
        return;
    }

    if (area->x2 < area->x1 || area->y2 < area->y1) {
        ESP_LOGE(TAG, "Area[%d] invalid bounds (%d,%d)-(%d,%d)", area_idx,
                 area->x1, area->y1, area->x2, area->y2);
        return;
    }

    uint32_t area_w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t row_h = disp->buf.buf_pixels / area_w;
    if (row_h == 0) {
        ESP_LOGE(TAG, "Area[%d] width %" PRIu32 " exceeds buffer, skip", area_idx, area_w);
        return;
    }

    gfx_disp_flush_cb_t flush_cb = disp->cb.flush_cb;
    if (flush_cb != NULL && disp->sync.event_group == NULL) {
        ESP_LOGE(TAG, "Area[%d] flush_cb set but event_group NULL", area_idx);
        return;
    }

    disp->render.flushing_last = false;
    gfx_coord_t cur_y = area->y1;

    while (cur_y <= area->y2) {

        gfx_coord_t chunk_x1 = area->x1;
        gfx_coord_t chunk_y1 = cur_y;
        gfx_coord_t chunk_x2 = area->x2 + 1;
        gfx_coord_t chunk_y2 = cur_y + (gfx_coord_t)row_h;
        if (chunk_y2 > area->y2 + 1) {
            chunk_y2 = area->y2 + 1;
        }

        uint16_t *buf = disp->buf.buf_act;

        int dest_stride = disp->flags.full_frame_buf ? disp->res.h_res : (chunk_x2 - chunk_x1);

        gfx_area_t buf_area;
        if (disp->flags.full_frame_buf) {
            buf_area.x1 = 0;
            buf_area.y1 = 0;
            buf_area.x2 = (gfx_coord_t)disp->res.h_res - 1;
            buf_area.y2 = (gfx_coord_t)disp->res.v_res - 1;
        } else {
            buf_area.x1 = chunk_x1;
            buf_area.y1 = chunk_y1;
            buf_area.x2 = chunk_x2 - 1;
            buf_area.y2 = chunk_y2 - 1;
        }

        gfx_draw_ctx_t draw_ctx = {
            .buf = buf,
            .buf_area = buf_area,
            .clip_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 },
            .stride = dest_stride,
            .swap = disp->flags.swap,
        };

        uint16_t bg = disp->flags.swap ? (uint16_t)__builtin_bswap16(disp->style.bg_color.full) : (uint16_t)disp->style.bg_color.full;
        if (disp->flags.full_frame_buf) {
            gfx_area_t fill_area = { chunk_x1, chunk_y1, chunk_x2, chunk_y2 };  /* exclusive x2,y2 */
            gfx_sw_blend_fill_area(buf, (gfx_coord_t)disp->res.h_res, &fill_area, bg);
        } else {
            gfx_area_t fill_area = { 0, 0, chunk_x2 - chunk_x1, chunk_y2 - chunk_y1 };
            gfx_sw_blend_fill_area(buf, chunk_x2 - chunk_x1, &fill_area, bg);
        }
        gfx_render_draw_child_objects(disp, &draw_ctx);

        if (flush_cb != NULL) {
            xEventGroupClearBits(disp->sync.event_group, WAIT_FLUSH_DONE);

            uint32_t chunk_px = area_w * (uint32_t)(chunk_y2 - chunk_y1);

            bool is_last_chunk = (chunk_y2 >= area->y2 + 1);
            disp->render.flushing_last = is_last_chunk && is_last_area;

            ESP_LOGD(TAG, "Flush: (%d,%d)-(%d,%d) %" PRIu32 " px%s",
                     chunk_x1, chunk_y1, chunk_x2 - 1, chunk_y2 - 1, chunk_px,
                     disp->render.flushing_last ? " (last)" : "");

            flush_cb(disp, chunk_x1, chunk_y1, chunk_x2, chunk_y2, buf);

            xEventGroupWaitBits(disp->sync.event_group, WAIT_FLUSH_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

            if (disp->buf.buf2 != NULL && (!disp->flags.full_frame_buf || disp->render.flushing_last)) {
                disp->buf.buf_act = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
            }
        }

        cur_y = chunk_y2;
    }
}

/**
 * @brief Copy from on-screen buffer to buf_act only for pending areas NOT covered by current dirty.
 * Called at start of render: if next frame redraws full screen (e.g. 480x480), skip sync entirely.
 */
#include "esp_timer.h"
static void gfx_render_sync_dirty_areas(gfx_disp_t *disp)
{
    if (!disp->flags.full_frame_buf || disp->buf.buf2 == NULL || disp->sync_pending.count == 0) {
        return;
    }

    uint16_t *buf_off_screen = disp->buf.buf_act;
    uint16_t *buf_on_screen = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
    uint32_t stride = disp->res.h_res;
    const size_t px_size = sizeof(uint16_t);

    uint8_t sync_count = 0;
    int64_t start_time = esp_timer_get_time();

    for (uint8_t i = 0; i < disp->sync_pending.count; i++) {
        if (disp->sync_pending.merged[i]) {
            continue;
        }
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
        ESP_LOGI(TAG, "sync area: (%d,%d)->(%d,%d)", a->x1, a->y1, a->x2, a->y2);
        uint32_t w = (uint32_t)(a->x2 - a->x1 + 1);
        uint32_t h = (uint32_t)(a->y2 - a->y1 + 1);
        for (uint32_t y = 0; y < h; y++) {
            size_t src_off = (size_t)(a->y1 + (gfx_coord_t)y) * stride + (size_t)a->x1;
            size_t dst_off = (size_t)(a->y1 + (gfx_coord_t)y) * stride + (size_t)a->x1;
            memcpy(buf_off_screen + dst_off, buf_on_screen + src_off, w * px_size);
        }
        sync_count++;
    }

    if (sync_count > 0) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "[sync] %d areas copied, %" PRId64 " ms",
                 sync_count, (end_time - start_time) / 1000);
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

    gfx_render_sync_dirty_areas(disp);

    /* Find index of last non-merged area so we can set flushing_last only on last chunk of last area */
    uint8_t last_area_idx = 0;
    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (!disp->dirty.merged[i]) {
            last_area_idx = i;
        }
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        if (disp->dirty.merged[i]) {
            continue;
        }

        gfx_area_t *area = &disp->dirty.areas[i];
        bool is_last_area = (i == last_area_idx);
        gfx_render_part_area(disp, area, i, is_last_area);
    }

    /* Record this frame's dirty areas for next frame's minimal sync */
    disp->sync_pending.count = disp->dirty.count;
    memcpy(disp->sync_pending.areas, disp->dirty.areas, sizeof(disp->dirty.areas));
    memcpy(disp->sync_pending.merged, disp->dirty.merged, sizeof(disp->dirty.merged));
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
        gfx_refr_update_layout_dirty(disp);

        if (disp->dirty.count > 1) {
            gfx_refr_merge_areas(disp);
        } else if (disp->dirty.count == 0) {
            continue;
        }

        gfx_render_update_child_objects(disp);

        uint32_t dirty_px = gfx_render_area_summary(disp);
        gfx_render_dirty_areas(disp);

        if (dirty_px > 0) {
            did_render = true;
            uint32_t screen_px = disp->res.h_res * disp->res.v_res;
            float dirty_pct = (dirty_px * 100.0f) / (float)screen_px;
            ESP_LOGD(TAG, "Rendered %" PRIu32 " px (%.1f%%)", dirty_px, dirty_pct);
        }

        gfx_render_cleanup(disp);
    }

    return did_render;
}
