/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "gfx/backends/esp_lcd.h"
#include "backend/esp_lcd/esp_lcd_buf_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GFX_BACKEND_ESP_LCD_IO_SLOTS
#define GFX_BACKEND_ESP_LCD_IO_SLOTS 4
#endif

typedef struct gfx_backend_esp_lcd gfx_backend_esp_lcd_t;

typedef struct {
    esp_lcd_panel_io_handle_t io;
    gfx_display_t *pending_disp;
    bool registered;
    int ref_count;
} gfx_backend_esp_lcd_io_slot_t;

struct gfx_backend_esp_lcd {
    gfx_backend_t base;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
    gfx_backend_esp_lcd_interface_t interface;
    gfx_display_t *pending_disp;
    int io_slot_idx;

    gfx_backend_esp_lcd_flush_mode_t flush_mode;
    gfx_tear_mode_t tear_mode;
    int16_t rotation;
    void *staging_fb;
    size_t staging_fb_bytes;
    gfx_color_format_t staging_format;

    void *panel_fb[3];
    uint8_t panel_fb_count;
    bool pipeline_enabled;
    bool pending_pipeline_swap;
    bool disp_fb_valid;      ///< false until the first blit; skip copy_unrendered before that
    bool draw_fb_stale;      ///< FULL modes: draw_fb misses the last frame; sync lazily at next flush
    void *disp_fb;
    void *draw_fb;
    gfx_buf_pipeline_t pipeline;

    TaskHandle_t notify_task;
    gfx_backend_esp_lcd_draw_bitmap_cb_t draw_bitmap_cb;
    void *draw_bitmap_user_ctx;
    bool skip_panel_callbacks;
};

gfx_display_t *gfx_backend_esp_lcd_slot_pending_disp(int slot_idx);
void gfx_backend_esp_lcd_slot_set_pending_disp(int slot_idx, gfx_display_t *disp);
int gfx_backend_esp_lcd_io_slot_acquire(esp_lcd_panel_io_handle_t io);
void gfx_backend_esp_lcd_io_slot_release(int slot_idx);
gfx_backend_esp_lcd_io_slot_t *gfx_backend_esp_lcd_io_slot_at(int slot_idx);

gfx_err_t gfx_backend_esp_lcd_register_callbacks(gfx_backend_esp_lcd_t *lcd);
void gfx_backend_esp_lcd_signal_flush_done(gfx_backend_esp_lcd_t *lcd, gfx_display_t *disp);

gfx_err_t gfx_backend_esp_lcd_flush_bridge(gfx_backend_esp_lcd_t *lcd,
        gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride);

void gfx_backend_esp_lcd_flush_bridge_deinit(gfx_backend_esp_lcd_t *lcd);

void gfx_backend_esp_lcd_post_flush_buf_update(gfx_backend_esp_lcd_t *lcd, gfx_display_t *disp);

#ifdef __cplusplus
}
#endif
