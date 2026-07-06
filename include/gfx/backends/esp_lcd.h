/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /** SPI / I80 / 8080 and other panel-IO buses (partial flush). */
    GFX_BACKEND_ESP_LCD_IF_PANEL_IO = 0,
    /** ESP32 RGB LCD peripheral (full-frame scan-out). */
    GFX_BACKEND_ESP_LCD_IF_RGB,
    /** MIPI-DSI DPI panel (partial or full flush). */
    GFX_BACKEND_ESP_LCD_IF_MIPI_DPI,
} gfx_backend_esp_lcd_interface_t;

/**
 * @brief How flush chunks are delivered to esp_lcd_panel_draw_bitmap().
 *
 * Orthogonal to render buffer size (buf_pixels >= h_res * v_res selects full-screen
 * render buffer layout (partition vs full-screen stride). See esp_lcd.h pairing
 * table in the backend README.
 */
typedef enum {
    /** Each render flush chunk is sent to the panel immediately (typical SPI). */
    GFX_BACKEND_ESP_LCD_FLUSH_DIRECT = 0,
    /**
     * Patch each chunk into an app-owned full-screen staging_fb, then send the
     * assembled frame on the last chunk of the frame. Use with partition render
     * buffers when the panel path needs one full-frame transfer and/or PPA
     * rotation before output.
     */
    GFX_BACKEND_ESP_LCD_FLUSH_STAGING,
} gfx_backend_esp_lcd_flush_mode_t;

typedef enum {
    GFX_BACKEND_ESP_LCD_ROTATE_0 = 0,
    GFX_BACKEND_ESP_LCD_ROTATE_90 = 90,
    GFX_BACKEND_ESP_LCD_ROTATE_180 = 180,
    GFX_BACKEND_ESP_LCD_ROTATE_270 = 270,
} gfx_backend_esp_lcd_rotation_t;

typedef enum {
    GFX_TEAR_NONE = 0,
    GFX_TEAR_DOUBLE_PARTIAL = 1,
    GFX_TEAR_TRIPLE_PARTIAL = 2,
    GFX_TEAR_DOUBLE_FULL = 3,
    GFX_TEAR_TRIPLE_FULL = 4,
    GFX_TEAR_DOUBLE_DIRECT = 5,
    GFX_TEAR_TE_SYNC = 6,
} gfx_tear_mode_t;

typedef esp_err_t (*gfx_backend_esp_lcd_draw_bitmap_cb_t)(gfx_backend_t *backend,
        gfx_display_t *disp,
        esp_lcd_panel_handle_t panel,
        int x_start,
        int y_start,
        int x_end,
        int y_end,
        const void *pixels,
        void *user_ctx);

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io; /**< Required for PANEL_IO; ignored for RGB. */
    gfx_backend_esp_lcd_interface_t interface;

    /** @see gfx_backend_esp_lcd_flush_mode_t. Legacy; prefer tear_mode. */
    gfx_backend_esp_lcd_flush_mode_t flush_mode;
    /**
     * Optional explicit tear-avoidance mode. When zero, inferred from flush_mode
     * and panel_fb_count during backend create.
     */
    gfx_tear_mode_t tear_mode;
    /**
     * Clockwise panel rotation for FLUSH_STAGING. Requires staging_fb and PPA on
     * targets that support hardware rotation.
     */
    int16_t rotation;
    /**
     * Full-screen flush staging buffer for FLUSH_STAGING (app allocated).
     * Not required when panel_fb_count >= 2 (draw_fb from pipeline is used).
     */
    void *staging_fb;
    /** Size of staging_fb in bytes. Required when flush_mode is FLUSH_STAGING and no panel_fb. */
    size_t staging_fb_bytes;
    /** Pixel format of staging_fb. Defaults to RGB565 when zero. */
    gfx_color_format_t staging_format;

    /** Panel framebuffers for tear-avoidance pipeline (2 or 3 entries). */
    void *panel_fb[3];
    /** Number of valid entries in panel_fb (0 = no pipeline). */
    uint8_t panel_fb_count;

    /**
     * Optional task handle woken from panel completion ISRs for double/triple
     * buffering pipelines (adapter-style task notify, not TE sync).
     */
    TaskHandle_t notify_task;

    /** Optional draw hook replacing esp_lcd_panel_draw_bitmap(). */
    gfx_backend_esp_lcd_draw_bitmap_cb_t draw_bitmap_cb;
    void *draw_bitmap_user_ctx;

    /**
     * When true, the backend does not register esp_lcd callbacks automatically.
     * The application must call gfx_backend_esp_lcd_notify_* instead.
     */
    bool skip_panel_callbacks;
} gfx_backend_esp_lcd_config_t;

/**
 * @brief Create a GFX backend for ESP-IDF LCD panels.
 *
 * Registers the appropriate esp_lcd completion callback for the interface when
 * skip_panel_callbacks is false (default):
 * - PANEL_IO: esp_lcd_panel_io.on_color_trans_done
 * - RGB: esp_lcd_rgb_panel.on_frame_buf_complete
 * - MIPI_DPI: esp_lcd_dpi_panel.on_color_trans_done
 *
 * When the returned backend is passed to gfx_display_add() and display creation
 * succeeds, the display owns it and deletes it from gfx_display_delete().
 *
 * @param cfg Backend configuration.
 * @return Backend pointer on success, or NULL on failure.
 */
gfx_backend_t *gfx_backend_esp_lcd_create(const gfx_backend_esp_lcd_config_t *cfg);

/**
 * @brief Delete an ESP LCD backend that is not owned by a display.
 */
void gfx_backend_esp_lcd_delete(gfx_backend_t *backend);

/**
 * @brief Set or update the adapter-style notify task for a backend instance.
 */
gfx_err_t gfx_backend_esp_lcd_set_notify_task(gfx_backend_t *backend, TaskHandle_t task);

/**
 * @brief Install or replace the custom draw_bitmap hook on an existing backend.
 */
gfx_err_t gfx_backend_esp_lcd_set_draw_bitmap_callback(gfx_backend_t *backend,
        gfx_backend_esp_lcd_draw_bitmap_cb_t cb,
        void *user_ctx);

/**
 * @brief Notify flush completion from a panel-IO color-trans-done ISR.
 *
 * Safe to call from ISR context. Equivalent to chaining the backend's internal
 * callback when multiple displays share one panel IO handle.
 */
bool gfx_backend_esp_lcd_notify_color_trans_done(esp_lcd_panel_io_handle_t panel_io);

/**
 * @brief Notify flush completion from an RGB or MIPI-DPI panel ISR.
 */
bool gfx_backend_esp_lcd_notify_panel_trans_done(esp_lcd_panel_handle_t panel);

/**
 * @brief Notify flush completion for a known backend/display pair.
 */
bool gfx_backend_esp_lcd_notify_flush_done(gfx_backend_t *backend,
        gfx_display_t *disp);

#ifdef __cplusplus
}
#endif
