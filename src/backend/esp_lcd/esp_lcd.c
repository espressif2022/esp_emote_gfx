/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/runtime/gfx_core_priv.h"
#include "gfx/backends/esp_lcd.h"
#include "platform/gfx_platform.h"

#include "backend/esp_lcd/esp_lcd_buf_pipeline.h"
#include "backend/esp_lcd/esp_lcd_priv.h"

#if CONFIG_SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

static const char *const TAG = "esp_lcd_backend";
static gfx_backend_esp_lcd_io_slot_t s_io_slots[GFX_BACKEND_ESP_LCD_IO_SLOTS];
static gfx_backend_esp_lcd_t *s_panel_backends[GFX_BACKEND_ESP_LCD_IO_SLOTS];

gfx_backend_esp_lcd_io_slot_t *gfx_backend_esp_lcd_io_slot_at(int slot_idx)
{
    if (slot_idx < 0 || slot_idx >= GFX_BACKEND_ESP_LCD_IO_SLOTS) {
        return NULL;
    }

    return &s_io_slots[slot_idx];
}

gfx_display_t *gfx_backend_esp_lcd_slot_pending_disp(int slot_idx)
{
    gfx_backend_esp_lcd_io_slot_t *slot = gfx_backend_esp_lcd_io_slot_at(slot_idx);

    return slot != NULL ? slot->pending_disp : NULL;
}

void gfx_backend_esp_lcd_slot_set_pending_disp(int slot_idx, gfx_display_t *disp)
{
    gfx_backend_esp_lcd_io_slot_t *slot = gfx_backend_esp_lcd_io_slot_at(slot_idx);

    if (slot != NULL) {
        slot->pending_disp = disp;
    }
}

int gfx_backend_esp_lcd_io_slot_acquire(esp_lcd_panel_io_handle_t io)
{
    int free_idx = -1;

    if (io == NULL) {
        return -1;
    }

    for (int i = 0; i < GFX_BACKEND_ESP_LCD_IO_SLOTS; i++) {
        if (s_io_slots[i].io == io) {
            s_io_slots[i].ref_count++;
            return i;
        }
        if (s_io_slots[i].ref_count == 0 && free_idx < 0) {
            free_idx = i;
        }
    }

    if (free_idx < 0) {
        return -1;
    }

    s_io_slots[free_idx].io = io;
    s_io_slots[free_idx].pending_disp = NULL;
    s_io_slots[free_idx].ref_count = 1;
    return free_idx;
}

void gfx_backend_esp_lcd_io_slot_release(int slot_idx)
{
    if (slot_idx < 0 || slot_idx >= GFX_BACKEND_ESP_LCD_IO_SLOTS) {
        return;
    }

    if (s_io_slots[slot_idx].ref_count <= 0) {
        return;
    }

    s_io_slots[slot_idx].ref_count--;
    s_io_slots[slot_idx].pending_disp = NULL;
    if (s_io_slots[slot_idx].ref_count == 0) {
        memset(&s_io_slots[slot_idx], 0, sizeof(s_io_slots[slot_idx]));
    }
}

static void gfx_backend_esp_lcd_panel_register(gfx_backend_esp_lcd_t *lcd)
{
    if (lcd == NULL || lcd->panel == NULL) {
        return;
    }

    for (int i = 0; i < GFX_BACKEND_ESP_LCD_IO_SLOTS; i++) {
        if (s_panel_backends[i] == lcd) {
            return;
        }
        if (s_panel_backends[i] == NULL) {
            s_panel_backends[i] = lcd;
            return;
        }
    }
}

static void gfx_backend_esp_lcd_panel_unregister(gfx_backend_esp_lcd_t *lcd)
{
    if (lcd == NULL) {
        return;
    }

    for (int i = 0; i < GFX_BACKEND_ESP_LCD_IO_SLOTS; i++) {
        if (s_panel_backends[i] == lcd) {
            s_panel_backends[i] = NULL;
            return;
        }
    }
}

static gfx_backend_esp_lcd_t *gfx_backend_esp_lcd_from_panel(esp_lcd_panel_handle_t panel)
{
    if (panel == NULL) {
        return NULL;
    }

    for (int i = 0; i < GFX_BACKEND_ESP_LCD_IO_SLOTS; i++) {
        if (s_panel_backends[i] != NULL && s_panel_backends[i]->panel == panel) {
            return s_panel_backends[i];
        }
    }

    return NULL;
}

static gfx_backend_esp_lcd_t *gfx_backend_esp_lcd_cast(gfx_backend_t *backend)
{
    return backend != NULL ? (gfx_backend_esp_lcd_t *)backend : NULL;
}

void gfx_backend_esp_lcd_signal_flush_done(gfx_backend_esp_lcd_t *lcd, gfx_display_t *disp)
{
    BaseType_t need_yield = pdFALSE;

    if (disp == NULL) {
        return;
    }

    if (lcd != NULL) {
        if (lcd->pipeline_enabled) {
            gfx_buf_pipeline_retire_isr(&lcd->pipeline);
        }
        if (lcd->pending_disp == disp) {
            lcd->pending_disp = NULL;
        }
        if (lcd->io_slot_idx >= 0) {
            gfx_backend_esp_lcd_io_slot_t *slot = gfx_backend_esp_lcd_io_slot_at(lcd->io_slot_idx);
            if (slot != NULL && slot->pending_disp == disp) {
                slot->pending_disp = NULL;
            }
        }
        if (lcd->notify_task != NULL) {
            if (gfx_platform_in_isr()) {
                vTaskNotifyGiveFromISR(lcd->notify_task, &need_yield);
            } else {
                xTaskNotifyGive(lcd->notify_task);
            }
        }
    }

    (void)gfx_display_flush_ready(disp);

    if (need_yield) {
        gfx_platform_yield_from_isr();
    }
}

static bool gfx_backend_esp_lcd_on_io_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)user_ctx;
    (void)edata;
    (void)gfx_backend_esp_lcd_notify_color_trans_done(panel_io);
    return false;
}

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
static bool gfx_backend_esp_lcd_on_dpi_color_trans_done(esp_lcd_panel_handle_t panel,
        esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    gfx_backend_esp_lcd_t *lcd = (gfx_backend_esp_lcd_t *)user_ctx;

    (void)panel;
    (void)edata;

    if (lcd != NULL && lcd->pending_disp != NULL) {
        gfx_backend_esp_lcd_signal_flush_done(lcd, lcd->pending_disp);
    }
    return false;
}
#endif

#if CONFIG_SOC_LCD_RGB_SUPPORTED
static bool gfx_backend_esp_lcd_on_rgb_frame_done(esp_lcd_panel_handle_t panel,
        const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    gfx_backend_esp_lcd_t *lcd = (gfx_backend_esp_lcd_t *)user_ctx;

    (void)panel;
    (void)edata;

    if (lcd != NULL && lcd->pending_disp != NULL) {
        gfx_backend_esp_lcd_signal_flush_done(lcd, lcd->pending_disp);
    }
    return false;
}
#endif

gfx_err_t gfx_backend_esp_lcd_register_callbacks(gfx_backend_esp_lcd_t *lcd)
{
    esp_err_t err;

    if (lcd == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    if (lcd->skip_panel_callbacks) {
        return GFX_OK;
    }

    switch (lcd->interface) {
    case GFX_BACKEND_ESP_LCD_IF_PANEL_IO:
        if (lcd->panel_io == NULL) {
            return GFX_ERR_INVALID_ARG;
        }
        lcd->io_slot_idx = gfx_backend_esp_lcd_io_slot_acquire(lcd->panel_io);
        if (lcd->io_slot_idx < 0) {
            return GFX_ERR_NO_MEM;
        }
        if (!s_io_slots[lcd->io_slot_idx].registered) {
            const esp_lcd_panel_io_callbacks_t cbs = {
                .on_color_trans_done = gfx_backend_esp_lcd_on_io_color_trans_done,
            };
            err = esp_lcd_panel_io_register_event_callbacks(lcd->panel_io, &cbs,
                    &s_io_slots[lcd->io_slot_idx]);
            if (err != ESP_OK) {
                gfx_backend_esp_lcd_io_slot_release(lcd->io_slot_idx);
                lcd->io_slot_idx = -1;
                return GFX_FAIL;
            }
            s_io_slots[lcd->io_slot_idx].registered = true;
        }
        return GFX_OK;

#if CONFIG_SOC_LCD_RGB_SUPPORTED
    case GFX_BACKEND_ESP_LCD_IF_RGB: {
        esp_lcd_rgb_panel_event_callbacks_t cbs = {
            .on_frame_buf_complete = gfx_backend_esp_lcd_on_rgb_frame_done,
        };
        err = esp_lcd_rgb_panel_register_event_callbacks(lcd->panel, &cbs, lcd);
        return err == ESP_OK ? GFX_OK : GFX_FAIL;
    }
#endif

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
    case GFX_BACKEND_ESP_LCD_IF_MIPI_DPI: {
        esp_lcd_dpi_panel_event_callbacks_t cbs = {
            .on_color_trans_done = gfx_backend_esp_lcd_on_dpi_color_trans_done,
        };
        err = esp_lcd_dpi_panel_register_event_callbacks(lcd->panel, &cbs, lcd);
        return err == ESP_OK ? GFX_OK : GFX_FAIL;
    }
#endif

    default:
        return GFX_ERR_NOT_SUPPORTED;
    }
}

static bool gfx_backend_esp_lcd_tear_mode_uses_partial_pipeline(gfx_tear_mode_t mode)
{
    return mode == GFX_TEAR_DOUBLE_PARTIAL || mode == GFX_TEAR_TRIPLE_PARTIAL;
}

static bool gfx_backend_esp_lcd_tear_mode_uses_full_pipeline(gfx_tear_mode_t mode)
{
    return mode == GFX_TEAR_DOUBLE_FULL || mode == GFX_TEAR_TRIPLE_FULL;
}

static gfx_err_t gfx_backend_esp_lcd_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    GFX_RETURN_ON_FALSE(lcd != NULL && disp != NULL && pixels != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "flush: invalid args");
    GFX_RETURN_ON_FALSE(disp->sync.event_group != NULL,
                        GFX_ERR_INVALID_STATE, TAG, "flush: event group is NULL");

    gfx_platform_event_clear(disp->sync.event_group, WAIT_FLUSH_DONE);

#if CONFIG_SOC_LCD_RGB_SUPPORTED
    if (lcd->interface == GFX_BACKEND_ESP_LCD_IF_RGB) {
        if (gfx_backend_esp_lcd_tear_mode_uses_partial_pipeline(lcd->tear_mode)) {
            return gfx_backend_esp_lcd_flush_bridge(lcd, disp, x1, y1, x2, y2, pixels, stride);
        }

        GFX_RETURN_ON_FALSE(gfx_display_has_full_frame_buf(disp),
                            GFX_ERR_NOT_SUPPORTED, TAG, "flush: RGB full mode requires full-screen render buffer");
        if (!gfx_display_is_flushing_last(disp)) {
            (void)gfx_backend_esp_lcd_signal_flush_done(lcd, disp);
            return GFX_OK;
        }

        lcd->pending_disp = disp;
        return gfx_backend_esp_lcd_flush_bridge(lcd, disp, 0, 0,
                                                (gfx_coord_t)gfx_display_get_h_res(disp),
                                                (gfx_coord_t)gfx_display_get_v_res(disp),
                                                pixels, stride);
    }
#else
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    if (lcd->interface == GFX_BACKEND_ESP_LCD_IF_RGB) {
        return GFX_ERR_NOT_SUPPORTED;
    }
#endif

    return gfx_backend_esp_lcd_flush_bridge(lcd, disp, x1, y1, x2, y2, pixels, stride);
}

static gfx_err_t gfx_backend_esp_lcd_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    GFX_RETURN_ON_FALSE(disp != NULL && disp->sync.event_group != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "wait: invalid display");
    gfx_platform_event_wait(disp->sync.event_group, WAIT_FLUSH_DONE, true, false, GFX_PLATFORM_WAIT_FOREVER);

    if (lcd != NULL) {
        gfx_backend_esp_lcd_post_flush_buf_update(lcd, disp);
    }

    return GFX_OK;
}

static void gfx_backend_esp_lcd_destroy(gfx_backend_t *backend)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    if (lcd == NULL) {
        return;
    }

    gfx_backend_esp_lcd_flush_bridge_deinit(lcd);
    gfx_backend_esp_lcd_panel_unregister(lcd);

    if (lcd->io_slot_idx >= 0) {
        s_io_slots[lcd->io_slot_idx].pending_disp = NULL;
        gfx_backend_esp_lcd_io_slot_release(lcd->io_slot_idx);
    }

    free(lcd);
}

static const gfx_backend_ops_t s_esp_lcd_backend_ops = {
    .flush = gfx_backend_esp_lcd_flush,
    .wait_flush = gfx_backend_esp_lcd_wait_flush,
    .destroy = gfx_backend_esp_lcd_destroy,
};

static gfx_tear_mode_t gfx_backend_esp_lcd_resolve_tear_mode(const gfx_backend_esp_lcd_config_t *cfg)
{
    if (cfg->tear_mode != GFX_TEAR_NONE) {
        return cfg->tear_mode;
    }

    if (cfg->flush_mode == GFX_BACKEND_ESP_LCD_FLUSH_STAGING) {
        if (cfg->panel_fb_count >= 3U) {
            return GFX_TEAR_TRIPLE_PARTIAL;
        }
        return GFX_TEAR_DOUBLE_PARTIAL;
    }

    if (cfg->panel_fb_count >= 3U) {
        return GFX_TEAR_TRIPLE_FULL;
    }

    if (cfg->panel_fb_count >= 2U) {
        if (cfg->interface == GFX_BACKEND_ESP_LCD_IF_RGB ||
                cfg->interface == GFX_BACKEND_ESP_LCD_IF_MIPI_DPI) {
            return GFX_TEAR_DOUBLE_FULL;
        }
        return GFX_TEAR_DOUBLE_DIRECT;
    }

    if (cfg->interface == GFX_BACKEND_ESP_LCD_IF_RGB ||
            cfg->interface == GFX_BACKEND_ESP_LCD_IF_MIPI_DPI) {
        return GFX_TEAR_DOUBLE_FULL;
    }

    return GFX_TEAR_NONE;
}

static void gfx_backend_esp_lcd_apply_config(gfx_backend_esp_lcd_t *lcd,
        const gfx_backend_esp_lcd_config_t *cfg)
{
    lcd->panel = cfg->panel;
    lcd->panel_io = cfg->panel_io;
    lcd->interface = cfg->interface;
    lcd->io_slot_idx = -1;
    lcd->flush_mode = cfg->flush_mode;
    lcd->tear_mode = gfx_backend_esp_lcd_resolve_tear_mode(cfg);
    lcd->rotation = cfg->rotation;
    lcd->staging_fb = cfg->staging_fb;
    lcd->staging_fb_bytes = cfg->staging_fb_bytes;
    lcd->staging_format = cfg->staging_format != 0 ? cfg->staging_format : GFX_COLOR_FORMAT_RGB565;
    lcd->panel_fb_count = cfg->panel_fb_count;
    for (uint8_t i = 0; i < 3U; i++) {
        lcd->panel_fb[i] = (i < cfg->panel_fb_count) ? cfg->panel_fb[i] : NULL;
    }
    lcd->pipeline_enabled = false;
    lcd->pending_pipeline_swap = false;
    lcd->disp_fb_valid = false;
    lcd->disp_fb = NULL;
    lcd->draw_fb = NULL;
    lcd->notify_task = cfg->notify_task;
    lcd->draw_bitmap_cb = cfg->draw_bitmap_cb;
    lcd->draw_bitmap_user_ctx = cfg->draw_bitmap_user_ctx;
    lcd->skip_panel_callbacks = cfg->skip_panel_callbacks;
}

static gfx_err_t gfx_backend_esp_lcd_init_pipeline(gfx_backend_esp_lcd_t *lcd)
{
    gfx_err_t err;

    if (lcd->panel_fb_count < 2U) {
        return GFX_OK;
    }

    if (gfx_backend_esp_lcd_tear_mode_uses_full_pipeline(lcd->tear_mode)) {
        err = gfx_buf_pipeline_init_full(&lcd->pipeline,
                                         lcd->panel_fb,
                                         lcd->panel_fb_count,
                                         &lcd->disp_fb,
                                         &lcd->draw_fb);
    } else if (gfx_backend_esp_lcd_tear_mode_uses_partial_pipeline(lcd->tear_mode)) {
        err = gfx_buf_pipeline_init_partial(&lcd->pipeline,
                                            lcd->panel_fb,
                                            lcd->panel_fb_count,
                                            &lcd->disp_fb,
                                            &lcd->draw_fb);
    } else {
        return GFX_OK;
    }

    if (err != GFX_OK) {
        return err;
    }

    lcd->pipeline_enabled = true;
    return GFX_OK;
}

gfx_backend_t *gfx_backend_esp_lcd_create(const gfx_backend_esp_lcd_config_t *cfg)
{
    gfx_backend_esp_lcd_t *lcd;
    gfx_err_t err;

    GFX_RETURN_ON_FALSE(cfg != NULL && cfg->panel != NULL,
                        NULL, TAG, "create: invalid config");

    if (cfg->flush_mode == GFX_BACKEND_ESP_LCD_FLUSH_STAGING &&
            cfg->panel_fb_count < 2U) {
        GFX_RETURN_ON_FALSE(cfg->staging_fb != NULL && cfg->staging_fb_bytes > 0U,
                            NULL, TAG, "create: staging flush requires staging_fb or panel_fb");
    }

    {
        const gfx_tear_mode_t tear_mode = gfx_backend_esp_lcd_resolve_tear_mode(cfg);

        if ((tear_mode == GFX_TEAR_TRIPLE_PARTIAL || tear_mode == GFX_TEAR_TRIPLE_FULL) &&
                cfg->panel_fb_count < 3U) {
            GFX_LOGE(TAG, "create: triple tear mode requires 3 panel framebuffers");
            return NULL;
        }

        if ((gfx_backend_esp_lcd_tear_mode_uses_partial_pipeline(tear_mode) ||
                gfx_backend_esp_lcd_tear_mode_uses_full_pipeline(tear_mode)) &&
                cfg->panel_fb_count < 2U) {
            GFX_LOGE(TAG, "create: tear mode requires at least 2 panel framebuffers");
            return NULL;
        }
    }

    lcd = calloc(1, sizeof(*lcd));
    GFX_RETURN_ON_FALSE(lcd != NULL, NULL, TAG, "create: no mem for backend");

    gfx_backend_esp_lcd_apply_config(lcd, cfg);

    if (gfx_backend_esp_lcd_init_pipeline(lcd) != GFX_OK) {
        GFX_LOGE(TAG, "create: pipeline init failed");
        free(lcd);
        return NULL;
    }

    err = gfx_backend_esp_lcd_register_callbacks(lcd);
    if (err != GFX_OK) {
        GFX_LOGE(TAG, "create: register callbacks failed: %d", (int)err);
        free(lcd);
        return NULL;
    }

    gfx_backend_esp_lcd_panel_register(lcd);

    lcd->base.ops = &s_esp_lcd_backend_ops;
    lcd->base.caps = GFX_BACKEND_CAP_FLUSH;
    lcd->base.alignment = gfx_backend_get_alignment(NULL);
    return &lcd->base;
}

void gfx_backend_esp_lcd_delete(gfx_backend_t *backend)
{
    gfx_backend_destroy(backend);
}

gfx_err_t gfx_backend_esp_lcd_set_notify_task(gfx_backend_t *backend, TaskHandle_t task)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    GFX_RETURN_ON_FALSE(lcd != NULL, GFX_ERR_INVALID_ARG, TAG, "set notify task: invalid backend");
    lcd->notify_task = task;
    return GFX_OK;
}

gfx_err_t gfx_backend_esp_lcd_set_draw_bitmap_callback(gfx_backend_t *backend,
        gfx_backend_esp_lcd_draw_bitmap_cb_t cb,
        void *user_ctx)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    GFX_RETURN_ON_FALSE(lcd != NULL, GFX_ERR_INVALID_ARG, TAG, "set draw hook: invalid backend");
    lcd->draw_bitmap_cb = cb;
    lcd->draw_bitmap_user_ctx = user_ctx;
    return GFX_OK;
}

bool gfx_backend_esp_lcd_notify_color_trans_done(esp_lcd_panel_io_handle_t panel_io)
{
    for (int i = 0; i < GFX_BACKEND_ESP_LCD_IO_SLOTS; i++) {
        if (s_io_slots[i].ref_count > 0 && s_io_slots[i].io == panel_io &&
                s_io_slots[i].pending_disp != NULL) {
            gfx_backend_esp_lcd_t *lcd = NULL;

            for (int j = 0; j < GFX_BACKEND_ESP_LCD_IO_SLOTS; j++) {
                if (s_panel_backends[j] != NULL && s_panel_backends[j]->io_slot_idx == i) {
                    lcd = s_panel_backends[j];
                    break;
                }
            }
            gfx_backend_esp_lcd_signal_flush_done(lcd, s_io_slots[i].pending_disp);
            return true;
        }
    }

    return false;
}

bool gfx_backend_esp_lcd_notify_panel_trans_done(esp_lcd_panel_handle_t panel)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_from_panel(panel);

    if (lcd == NULL || lcd->pending_disp == NULL) {
        return false;
    }

    gfx_backend_esp_lcd_signal_flush_done(lcd, lcd->pending_disp);
    return true;
}

bool gfx_backend_esp_lcd_notify_flush_done(gfx_backend_t *backend,
        gfx_display_t *disp)
{
    gfx_backend_esp_lcd_t *lcd = gfx_backend_esp_lcd_cast(backend);

    if (disp == NULL) {
        return false;
    }

    gfx_backend_esp_lcd_signal_flush_done(lcd, disp);
    return true;
}
