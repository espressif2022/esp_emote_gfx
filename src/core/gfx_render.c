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
 * @brief Draw child objects in the specified area
 * @param disp Display
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
 * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 */
void gfx_render_draw_child_objects(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *dest_buf)
{
    if (disp == NULL || disp->child_list == NULL) {
        return;
    }

    gfx_obj_child_t *child_node = disp->child_list;
    bool swap = disp->flags.swap;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

        if (!obj->state.is_visible) {
            child_node = child_node->next;
            continue;
        }

        if (obj->vfunc.draw) {
            obj->vfunc.draw(obj, x1, y1, x2, y2, dest_buf, swap);
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

    for (uint8_t i = 0; i < disp->dirty_count; i++) {
        if (disp->area_merged[i]) {
            continue;
        }
        gfx_area_t *area = &disp->dirty_areas[i];
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
 */
uint32_t gfx_render_part_area(gfx_disp_t *disp, gfx_area_t *area,
                              uint8_t area_idx, uint32_t start_block_count)
{
    uint32_t area_width = area->x2 - area->x1 + 1;

    uint32_t per_flush = disp->buf_pixels / area_width;
    if (per_flush == 0) {
        ESP_LOGE(TAG, "Area[%d] width %" PRIu32 " exceeds buffer width, skipping", area_idx, area_width);
        return 0;
    }

    int current_y = area->y1;
    uint32_t flush_idx = 0;
    uint32_t flushes_done = 0;

    gfx_disp_flush_cb_t flush_cb = disp->flush_cb;

    while (current_y <= area->y2) {
        flush_idx++;

        int x1 = area->x1;
        int y1 = current_y;
        int x2 = area->x2 + 1;
        int y2 = current_y + per_flush;
        if (y2 > area->y2 + 1) {
            y2 = area->y2 + 1;
        }

        uint16_t *buf_act = disp->buf_act;

        gfx_sw_blend_fill(buf_act, \
                          disp->flags.swap ? __builtin_bswap16(disp->bg_color.full) : disp->bg_color.full, \
                          disp->buf_pixels);
        gfx_render_draw_child_objects(disp, x1, y1, x2, y2, buf_act);

        if (flush_cb) {
            xEventGroupClearBits(disp->event_group, WAIT_FLUSH_DONE);

            uint32_t chunk_pixels = area_width * (y2 - y1);
            ESP_LOGD(TAG, "Flush[%" PRIu32 "]: (%d,%d)->(%d,%d) %" PRIu32 "px",
                     start_block_count + flush_idx, x1, y1, x2 - 1, y2 - 1, chunk_pixels);

            flush_cb(disp, x1, y1, x2, y2, buf_act);
            xEventGroupWaitBits(disp->event_group, WAIT_FLUSH_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

            if (disp->buf2 != NULL) {
                if (disp->buf_act == disp->buf1) {
                    disp->buf_act = disp->buf2;
                } else {
                    disp->buf_act = disp->buf1;
                }
            }
        }

        current_y = y2;
        flushes_done++;
    }

    return flushes_done;
}

/**
 * @brief Render all dirty areas
 * @param disp Display
 * @return Total number of flush operations
 */
uint32_t gfx_render_dirty_areas(gfx_disp_t *disp)
{
    uint32_t rendered_blocks = 0;

    if (disp == NULL) {
        return 0;
    }

    for (uint8_t i = 0; i < disp->dirty_count; i++) {
        if (disp->area_merged[i]) {
            continue;
        }

        gfx_area_t *area = &disp->dirty_areas[i];
        rendered_blocks += gfx_render_part_area(disp, area, i, rendered_blocks);
    }

    return rendered_blocks;
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

    disp->flushing_last = true;
    if (disp->dirty_count > 0) {
        gfx_invalidate_area_disp(disp, NULL);
    }
}

/**
 * @brief Handle rendering of all objects in the scene
 * @param ctx Player context
 * @return true if rendering was performed, false otherwise
 */
bool gfx_render_handler(gfx_core_context_t *ctx)
{
    static uint32_t fps_sample_count = 0;
    static uint32_t fps_total_time = 0;
    static uint32_t last_render_tick = 0;

    uint32_t current_tick = gfx_timer_tick_get();
    if (last_render_tick == 0) {
        last_render_tick = current_tick;
    } else {
        uint32_t render_elapsed = gfx_timer_tick_elaps(last_render_tick);
        fps_sample_count++;
        fps_total_time += render_elapsed;
        last_render_tick = current_tick;

        if (fps_sample_count >= 100) {
            gfx_timer_mgr_t *timer_mgr = &ctx->timer_mgr;
            timer_mgr->actual_fps = (fps_sample_count * 1000) / fps_total_time;
            // ESP_LOGI(TAG, "average fps: %"PRIu32"(%"PRIu32")", timer_mgr->actual_fps, timer_mgr->fps);
            fps_sample_count = 0;
            fps_total_time = 0;
        }
    }

    bool any_rendered = false;

    for (gfx_disp_t *disp = ctx->disp; disp != NULL; disp = disp->next) {
        gfx_refr_update_layout_dirty(disp);

        if (disp->dirty_count > 1) {
            gfx_refr_merge_areas(disp);
        }

        if (disp->dirty_count == 0) {
            continue;
        }

        gfx_render_update_child_objects(disp);

        uint32_t total_dirty_pixels = gfx_render_area_summary(disp);
        uint32_t screen_pixels = disp->h_res * disp->v_res;

        uint32_t rendered_blocks = gfx_render_dirty_areas(disp);

        if (rendered_blocks > 0) {
            any_rendered = true;
            float dirty_percentage = (total_dirty_pixels * 100.0f) / screen_pixels;
            ESP_LOGD(TAG, "Rendered %" PRIu32 " blocks, %" PRIu32 "px (%.1f%%)",
                     rendered_blocks, total_dirty_pixels, dirty_percentage);
        }

        gfx_render_cleanup(disp);
    }

    return any_rendered;
}
