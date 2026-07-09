/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "backend/esp_lcd/esp_lcd_copy_unrendered.h"
#include "backend/esp_lcd/esp_lcd_priv.h"
#include "core/display/gfx_display_priv.h"
#include "platform/esp_idf/esp_idf_flush_staging.h"

static const char *const TAG = "esp_lcd_bridge";

static bool gfx_backend_esp_lcd_rotation_active(int16_t rotation)
{
    int16_t normalized = (int16_t)(((rotation % 360) + 360) % 360);

    return normalized == 90 || normalized == 180 || normalized == 270;
}

static void *gfx_backend_esp_lcd_staging_target(gfx_backend_esp_lcd_t *lcd)
{
    if (lcd->pipeline_enabled && lcd->draw_fb != NULL) {
        return lcd->draw_fb;
    }

    return lcd->staging_fb;
}

static esp_err_t gfx_backend_esp_lcd_draw_bitmap(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels)
{
    if (lcd->draw_bitmap_cb != NULL) {
        return lcd->draw_bitmap_cb(&lcd->base, disp, lcd->panel, x1, y1, x2, y2,
                                   pixels, lcd->draw_bitmap_user_ctx);
    }

    return esp_lcd_panel_draw_bitmap(lcd->panel, x1, y1, x2, y2, pixels);
}

static bool gfx_backend_esp_lcd_staging_patch(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride,
        void *target_fb)
{
    gfx_platform_flush_staging_patch_t patch = {
        .staging_fb = target_fb,
        .hor_res = gfx_display_get_h_res(disp),
        .ver_res = gfx_display_get_v_res(disp),
        .format = lcd->staging_format,
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .src = pixels,
        .src_stride_px = stride,
    };

    if (gfx_backend_esp_lcd_rotation_active(lcd->rotation)) {
        return gfx_platform_flush_staging_rotate_patch(&patch, lcd->rotation);
    }

    return gfx_platform_flush_staging_copy_patch(&patch);
}

static gfx_err_t gfx_backend_esp_lcd_flush_staging(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    esp_err_t err;
    void *target_fb = gfx_backend_esp_lcd_staging_target(lcd);
    const bool is_last = gfx_display_is_flushing_last(disp);

    if (target_fb == NULL) {
        GFX_LOGE(TAG, "staging flush: target framebuffer is NULL");
        return GFX_ERR_INVALID_STATE;
    }

    if (!gfx_backend_esp_lcd_staging_patch(lcd, disp, x1, y1, x2, y2, pixels, stride, target_fb)) {
        GFX_LOGW(TAG, "staging flush: patch failed, falling back to direct blit");
        lcd->pending_disp = disp;
        if (lcd->io_slot_idx >= 0) {
            gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, disp);
        }
        err = gfx_backend_esp_lcd_draw_bitmap(lcd, disp, x1, y1, x2, y2, pixels);
        return err == ESP_OK ? GFX_OK : GFX_FAIL;
    }

    if (!is_last) {
        (void)gfx_backend_esp_lcd_signal_flush_done(lcd, disp);
        return GFX_OK;
    }

    if (lcd->pipeline_enabled) {
        /*
         * Skip copy_unrendered on the very first blit: disp_fb points to an
         * uninitialised panel FB at this point and would corrupt the draw FB
         * with garbage data in non-dirty regions.
         */
        if (lcd->disp_fb_valid) {
            gfx_copy_unrendered_areas(disp,
                                      lcd->disp_fb,
                                      lcd->draw_fb,
                                      gfx_display_get_h_res(disp),
                                      gfx_display_get_v_res(disp),
                                      disp->format.output_pixel_size);
        }
        /*
         * Commit the current front buffer to the pipeline inflight queue
         * BEFORE calling draw_bitmap.  The panel ISR fires after the blit
         * and calls retire_isr; if inflight is empty at that point the
         * semaphore is never given and acquire_sync blocks forever.
         */
        gfx_buf_pipeline_commit(&lcd->pipeline, lcd->disp_fb);
        lcd->disp_fb = target_fb;
        lcd->disp_fb_valid = true;
        lcd->pending_pipeline_swap = true;
    }

    lcd->pending_disp = disp;
    if (lcd->io_slot_idx >= 0) {
        gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, disp);
    }

    err = gfx_backend_esp_lcd_draw_bitmap(lcd, disp, 0, 0,
                                          (gfx_coord_t)gfx_display_get_h_res(disp),
                                          (gfx_coord_t)gfx_display_get_v_res(disp),
                                          target_fb);
    if (err != ESP_OK) {
        lcd->pending_disp = NULL;
        lcd->pending_pipeline_swap = false;
        if (lcd->io_slot_idx >= 0) {
            gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, NULL);
        }
        GFX_LOGE(TAG, "staging flush: full blit failed: %d", (int)err);
        return GFX_FAIL;
    }

    return GFX_OK;
}

static gfx_err_t gfx_backend_esp_lcd_flush_direct(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels)
{
    esp_err_t err;

    lcd->pending_disp = disp;
    if (lcd->io_slot_idx >= 0) {
        gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, disp);
    }

    err = gfx_backend_esp_lcd_draw_bitmap(lcd, disp, x1, y1, x2, y2, pixels);
    if (err != ESP_OK) {
        lcd->pending_disp = NULL;
        if (lcd->io_slot_idx >= 0) {
            gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, NULL);
        }
        GFX_LOGE(TAG, "direct flush: draw bitmap failed: %d", (int)err);
        return GFX_FAIL;
    }

    return GFX_OK;
}

static gfx_err_t gfx_backend_esp_lcd_flush_full(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        const void *pixels)
{
    esp_err_t err;
    const uint32_t h_res = gfx_display_get_h_res(disp);
    const uint32_t v_res = gfx_display_get_v_res(disp);

    if (!gfx_display_is_flushing_last(disp)) {
        (void)gfx_backend_esp_lcd_signal_flush_done(lcd, disp);
        return GFX_OK;
    }

    /*
     * Lazy sync-back: the newly rendered buffer still misses last frame's
     * content outside this frame's dirty areas. Copy only those regions from
     * the current front buffer; a full-screen dirty frame copies nothing.
     */
    if (lcd->pipeline_enabled && lcd->disp_fb_valid && lcd->draw_fb_stale) {
        gfx_copy_unrendered_areas(disp,
                                  lcd->disp_fb,
                                  (void *)pixels,
                                  h_res,
                                  v_res,
                                  disp->format.output_pixel_size);
    }
    lcd->draw_fb_stale = false;

    /*
     * Commit the current front buffer BEFORE draw_bitmap so that the panel
     * ISR (which fires after the blit and calls retire_isr) finds a buffer
     * in the inflight queue to retire and can give the semaphore.
     * disp_fb is updated to pixels (the buffer we are about to show).
     */
    if (lcd->pipeline_enabled) {
        gfx_buf_pipeline_commit(&lcd->pipeline, lcd->disp_fb);
        lcd->disp_fb = (void *)pixels;
        lcd->disp_fb_valid = true;
        lcd->pending_pipeline_swap = true;
    }

    lcd->pending_disp = disp;
    if (lcd->io_slot_idx >= 0) {
        gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, disp);
    }

    err = gfx_backend_esp_lcd_draw_bitmap(lcd, disp, 0, 0,
                                          (gfx_coord_t)h_res,
                                          (gfx_coord_t)v_res,
                                          pixels);
    if (err != ESP_OK) {
        lcd->pending_disp = NULL;
        lcd->pending_pipeline_swap = false;
        if (lcd->io_slot_idx >= 0) {
            gfx_backend_esp_lcd_slot_set_pending_disp(lcd->io_slot_idx, NULL);
        }
        GFX_LOGE(TAG, "full flush: blit failed: %d", (int)err);
        return GFX_FAIL;
    }

    return GFX_OK;
}

gfx_err_t gfx_backend_esp_lcd_flush_bridge(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    if (lcd == NULL || disp == NULL || pixels == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    switch (lcd->tear_mode) {
    case GFX_TEAR_DOUBLE_PARTIAL:
    case GFX_TEAR_TRIPLE_PARTIAL:
        return gfx_backend_esp_lcd_flush_staging(lcd, disp, x1, y1, x2, y2, pixels, stride);

    case GFX_TEAR_DOUBLE_FULL:
    case GFX_TEAR_TRIPLE_FULL:
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        (void)stride;
        return gfx_backend_esp_lcd_flush_full(lcd, disp, pixels);

    case GFX_TEAR_TE_SYNC:
        GFX_LOGW(TAG, "TE_SYNC not implemented, using direct flush");
    /* fall through */
    case GFX_TEAR_DOUBLE_DIRECT:
    case GFX_TEAR_NONE:
    default:
        (void)stride;
        return gfx_backend_esp_lcd_flush_direct(lcd, disp, x1, y1, x2, y2, pixels);
    }
}

void gfx_backend_esp_lcd_flush_bridge_deinit(gfx_backend_esp_lcd_t *lcd)
{
    if (lcd == NULL) {
        return;
    }

    gfx_buf_pipeline_destroy(&lcd->pipeline);
    lcd->pipeline_enabled = false;
    lcd->pending_pipeline_swap = false;
    lcd->disp_fb_valid = false;
    lcd->draw_fb_stale = false;
    lcd->disp_fb = NULL;
    lcd->draw_fb = NULL;
}

void gfx_backend_esp_lcd_post_flush_buf_update(gfx_backend_esp_lcd_t *lcd, gfx_display_t *disp)
{
    if (lcd == NULL || disp == NULL) {
        return;
    }

    if (lcd->pipeline_enabled && lcd->pending_pipeline_swap) {
        struct gfx_pipeline_buf *next;

        lcd->pending_pipeline_swap = false;

        /*
         * commit() and disp_fb update were already done in flush_full /
         * flush_staging BEFORE draw_bitmap so the ISR could retire the
         * inflight buffer and give the semaphore.  Here we only need to
         * acquire the next free buffer.
         */
        next = gfx_buf_pipeline_acquire_sync(&lcd->pipeline);
        if (next != NULL) {
            lcd->draw_fb = next->buffer;

            /*
             * Sync-back for FULL pipeline modes is deferred: the acquired
             * draw buffer misses the frame just shown, so mark it stale and
             * let the next flush copy only the regions that frame will not
             * redraw (copy_unrendered_areas). A full-screen redraw — the
             * common case for pager drags and full invalidations — then
             * skips the copy entirely.
             *
             * PARTIAL pipeline modes handle this via copy_unrendered_areas
             * inside flush_staging before each blit; no sync-back needed here.
             */
            if (lcd->tear_mode == GFX_TEAR_DOUBLE_FULL ||
                    lcd->tear_mode == GFX_TEAR_TRIPLE_FULL) {
                lcd->draw_fb_stale = true;
            }
        }

        if (gfx_display_has_full_frame_buf(disp)) {
            disp->buf.buf_act = lcd->draw_fb;
        }
        return;
    }

    if (!lcd->pipeline_enabled && disp->buf.buf2 != NULL &&
            gfx_display_has_full_frame_buf(disp) && disp->render.flushing_last) {
        disp->buf.buf_act = (disp->buf.buf_act == disp->buf.buf1) ? disp->buf.buf2 : disp->buf.buf1;
    }
}
