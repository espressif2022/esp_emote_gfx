/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "esp_err.h"
#include "esp_lcd_panel_rgb.h"

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "gfx/backends/esp_lcd_rgb.h"
#include "core/display/gfx_backend_priv.h"
#include "core/display/gfx_display_priv.h"
#include "core/runtime/gfx_core_priv.h"

typedef struct {
    gfx_backend_t base;
    esp_lcd_panel_handle_t panel;
    gfx_display_t *pending_disp;
} gfx_backend_esp_lcd_rgb_t;

static const char *const TAG = "esp_lcd_rgb_backend";

static bool gfx_backend_esp_lcd_rgb_frame_done(esp_lcd_panel_handle_t panel,
        const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    gfx_backend_esp_lcd_rgb_t *lcd = (gfx_backend_esp_lcd_rgb_t *)user_ctx;
    gfx_display_t *disp;

    (void)panel;
    (void)edata;

    if (lcd == NULL) {
        return false;
    }

    disp = lcd->pending_disp;
    lcd->pending_disp = NULL;
    if (disp != NULL) {
        (void)gfx_display_flush_ready(disp, true);
    }
    return false;
}

static gfx_err_t gfx_backend_esp_lcd_rgb_flush(gfx_backend_t *backend, gfx_display_t *disp,
        gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels, gfx_coord_t stride)
{
    gfx_backend_esp_lcd_rgb_t *lcd = (gfx_backend_esp_lcd_rgb_t *)backend;
    esp_err_t err;

    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)stride;

    GFX_RETURN_ON_FALSE(lcd != NULL && disp != NULL && pixels != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "flush: invalid args");
    GFX_RETURN_ON_FALSE(disp->flags.full_frame,
                        GFX_ERR_NOT_SUPPORTED, TAG, "flush: RGB panel backend requires full_frame display");
    GFX_RETURN_ON_FALSE(disp->sync.event_group != NULL,
                        GFX_ERR_INVALID_STATE, TAG, "flush: event group is NULL");

    gfx_platform_event_clear(disp->sync.event_group, WAIT_FLUSH_DONE);

    if (!gfx_display_is_flushing_last(disp)) {
        (void)gfx_display_flush_ready(disp, false);
        return GFX_OK;
    }

    lcd->pending_disp = disp;
    err = esp_lcd_panel_draw_bitmap(lcd->panel, 0, 0,
                                    gfx_display_get_h_res(disp),
                                    gfx_display_get_v_res(disp),
                                    pixels);
    if (err != ESP_OK) {
        lcd->pending_disp = NULL;
        GFX_LOGE(TAG, "flush: draw bitmap failed: %d", (int)err);
        return GFX_FAIL;
    }

    return GFX_OK;
}

static gfx_err_t gfx_backend_esp_lcd_rgb_wait_flush(gfx_backend_t *backend, gfx_display_t *disp)
{
    (void)backend;

    GFX_RETURN_ON_FALSE(disp != NULL && disp->sync.event_group != NULL,
                        GFX_ERR_INVALID_ARG, TAG, "wait: invalid display");
    gfx_platform_event_wait(disp->sync.event_group, WAIT_FLUSH_DONE, true, false, GFX_PLATFORM_WAIT_FOREVER);
    return GFX_OK;
}

static void gfx_backend_esp_lcd_rgb_destroy(gfx_backend_t *backend)
{
    free(backend);
}

static const gfx_backend_ops_t s_esp_lcd_rgb_backend_ops = {
    .flush = gfx_backend_esp_lcd_rgb_flush,
    .wait_flush = gfx_backend_esp_lcd_rgb_wait_flush,
    .destroy = gfx_backend_esp_lcd_rgb_destroy,
};

gfx_backend_t *gfx_backend_esp_lcd_rgb_create(const gfx_backend_esp_lcd_rgb_config_t *cfg)
{
    gfx_backend_esp_lcd_rgb_t *lcd;
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = gfx_backend_esp_lcd_rgb_frame_done,
    };

    GFX_RETURN_ON_FALSE(cfg != NULL && cfg->panel != NULL,
                        NULL, TAG, "create: invalid config");

    lcd = calloc(1, sizeof(*lcd));
    GFX_RETURN_ON_FALSE(lcd != NULL, NULL, TAG, "create: no mem for backend");

    lcd->panel = cfg->panel;
    if (esp_lcd_rgb_panel_register_event_callbacks(cfg->panel, &cbs, lcd) != ESP_OK) {
        GFX_LOGE(TAG, "create: register frame callback failed");
        free(lcd);
        return NULL;
    }

    lcd->base.ops = &s_esp_lcd_rgb_backend_ops;
    lcd->base.caps = GFX_BACKEND_CAP_FLUSH;
    lcd->base.alignment = gfx_backend_get_alignment(NULL);
    return &lcd->base;
}

void gfx_backend_esp_lcd_rgb_delete(gfx_backend_t *backend)
{
    gfx_backend_destroy(backend);
}
