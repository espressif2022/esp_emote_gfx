/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "core/gfx_render_priv.h"
#include "core/gfx_refr_priv.h"
#include "core/gfx_timer_priv.h"

static const char *TAG = "gfx_render";

/**
 * @brief Fast fill buffer with background color
 * @param buf Pointer to uint16_t buffer
 * @param color 16-bit color value
 * @param pixels Number of pixels to fill
 */
static inline void gfx_fill_color(uint16_t *buf, uint16_t color, size_t pixels)
{
    if ((color & 0xFF) == (color >> 8)) {
        memset(buf, color & 0xFF, pixels * sizeof(uint16_t));
    } else {
        uint32_t color32 = (color << 16) | color;
        uint32_t *buf32 = (uint32_t *)buf;
        size_t pixels_half = pixels / 2;

        for (size_t i = 0; i < pixels_half; i++) {
            buf32[i] = color32;
        }

        if (pixels & 1) {
            buf[pixels - 1] = color;
        }
    }
}

/**
 * @brief Draw child objects in the specified area
 * @param ctx Graphics context
 * @param x1 Left coordinate
 * @param y1 Top coordinate
 * @param x2 Right coordinate
 * @param y2 Bottom coordinate
 * @param dest_buf Destination buffer
 */
void gfx_render_child_objects(gfx_core_context_t *ctx, int x1, int y1, int x2, int y2, const void *dest_buf)
{
    if (ctx->disp.child_list == NULL) {
        return;
    }

    gfx_core_child_t *child_node = ctx->disp.child_list;
    bool swap = ctx->display.flags.swap;

    while (child_node != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)child_node->src;

        // Skip rendering if object is not visible
        if (!obj->state.is_visible) {
            child_node = child_node->next;
            continue;
        }

        /* Call object's draw function if available */
        if (obj->vfunc.draw) {
            obj->vfunc.draw(obj, x1, y1, x2, y2, dest_buf, swap);
        }

        child_node = child_node->next;
    }
}

/**
 * @brief Print summary of dirty areas
 * @param ctx Graphics context
 * @return Total dirty pixels
 */
uint32_t gfx_render_area_summary(gfx_core_context_t *ctx)
{
    uint32_t total_dirty_pixels = 0;

    for (uint8_t i = 0; i < ctx->disp.dirty_count; i++) {
        if (ctx->disp.area_merged[i]) {
            continue;    /* Skip merged areas */
        }
        gfx_area_t *area = &ctx->disp.dirty_areas[i];
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
 * @param ctx Graphics context
 * @param area Area to render
 * @param area_idx Area index for logging
 * @param start_block_count Starting block count for logging
 * @return Number of flush operations performed
 */
uint32_t gfx_render_part_area(gfx_core_context_t *ctx, gfx_area_t *area,
                              uint8_t area_idx, uint32_t start_block_count)
{
    uint32_t area_width = area->x2 - area->x1 + 1;

    uint32_t per_flush = ctx->disp.buf_pixels / area_width;
    if (per_flush == 0) {
        ESP_LOGE(TAG, "Area[%d] width %lu exceeds buffer width, skipping", area_idx, area_width);
        return 0;
    }

    int current_y = area->y1;
    uint32_t flush_idx = 0;
    uint32_t flushes_done = 0;

    while (current_y <= area->y2) {
        flush_idx++;

        int x1 = area->x1;
        int y1 = current_y;
        int x2 = area->x2 + 1;
        int y2 = current_y + per_flush;
        if (y2 > area->y2 + 1) {
            y2 = area->y2 + 1;
        }

        uint16_t *buf_act = ctx->disp.buf_act;

        gfx_fill_color(buf_act, ctx->disp.bg_color.full, ctx->disp.buf_pixels);
        gfx_render_child_objects(ctx, x1, y1, x2, y2, buf_act);

        if (ctx->callbacks.flush_cb) {
            xEventGroupClearBits(ctx->sync.event_group, WAIT_FLUSH_DONE);

            uint32_t chunk_pixels = area_width * (y2 - y1);
            ESP_LOGD(TAG, "Flush[%lu]: (%d,%d)->(%d,%d) %lupx",
                     start_block_count + flush_idx, x1, y1, x2 - 1, y2 - 1, chunk_pixels);

            ctx->callbacks.flush_cb(ctx, x1, y1, x2, y2, buf_act);
            xEventGroupWaitBits(ctx->sync.event_group, WAIT_FLUSH_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));
        }

        current_y = y2;
        flushes_done++;
    }

    return flushes_done;
}

/**
 * @brief Render all dirty areas
 * @param ctx Graphics context
 * @return Total number of flush operations
 */
uint32_t gfx_render_dirty_areas(gfx_core_context_t *ctx)
{
    uint32_t rendered_blocks = 0;

    for (uint8_t i = 0; i < ctx->disp.dirty_count; i++) {
        if (ctx->disp.area_merged[i]) {
            continue;
        }

        gfx_area_t *area = &ctx->disp.dirty_areas[i];
        rendered_blocks += gfx_render_part_area(ctx, area, i, rendered_blocks);
    }

    return rendered_blocks;
}

/**
 * @brief Cleanup after rendering - swap buffers and clear dirty flags
 * @param ctx Graphics context
 */
void gfx_render_cleanup(gfx_core_context_t *ctx)
{
    ctx->disp.flushing_last = true;
    if (ctx->disp.buf2 != NULL) {
        if (ctx->disp.buf_act == ctx->disp.buf1) {
            ctx->disp.buf_act = ctx->disp.buf2;
        } else {
            ctx->disp.buf_act = ctx->disp.buf1;
        }
    }

    if (ctx->disp.dirty_count > 0) {
        gfx_invalidate_area(ctx, NULL);
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

    // FPS statistics - count every render call (even if no dirty areas)
    uint32_t current_tick = gfx_timer_tick_get();
    if (last_render_tick == 0) {
        last_render_tick = current_tick;
    } else {
        uint32_t render_elapsed = gfx_timer_tick_elaps(last_render_tick);
        fps_sample_count++;
        fps_total_time += render_elapsed;
        last_render_tick = current_tick;

        if (fps_sample_count >= 100) {
            gfx_timer_mgr_t *timer_mgr = &ctx->timer.timer_mgr;
            timer_mgr->actual_fps = (fps_sample_count * 1000) / fps_total_time;
            ESP_LOGI(TAG, "average fps: %"PRIu32"(%"PRIu32")", timer_mgr->actual_fps, timer_mgr->fps);
            fps_sample_count = 0;
            fps_total_time = 0;
        }
    }

    // Update layout for objects marked as dirty before rendering
    gfx_refr_update_layout_dirty(ctx);

    if (ctx->disp.dirty_count > 1) {
        gfx_refr_merge_areas(ctx);
    }

    if (ctx->disp.dirty_count == 0) {
        return false;
    }

    uint32_t total_dirty_pixels = gfx_render_area_summary(ctx);
    uint32_t screen_pixels = ctx->display.h_res * ctx->display.v_res;

    uint32_t rendered_blocks = gfx_render_dirty_areas(ctx);

    float dirty_percentage = (total_dirty_pixels * 100.0f) / screen_pixels;
    ESP_LOGD(TAG, "Rendered %lu blocks, %lupx (%.1f%%)",
             rendered_blocks, total_dirty_pixels, dirty_percentage);

    gfx_render_cleanup(ctx);

    return true;
}
