/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"

#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "core/gfx_backend_memory.h"
#include "core/gfx_types_priv.h"
#include "core/display/gfx_backend_priv.h"

struct gfx_memory_backend {
    gfx_disp_backend_t base;
    uint16_t *buffer;
    uint32_t h_res;
    uint32_t v_res;
    bool swap;
    bool ext_buffer;
};

static const char *const TAG = "memory_backend";

static esp_err_t gfx_memory_backend_flush_impl(gfx_disp_backend_t *backend, gfx_disp_t *disp,
        gfx_coord_t x1, gfx_coord_t y1,
        gfx_coord_t x2, gfx_coord_t y2,
        const void *pixels)
{
    (void)disp;
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    const uint16_t *src = (const uint16_t *)pixels;

    ESP_RETURN_ON_FALSE(mem != NULL && src != NULL, ESP_ERR_INVALID_ARG, TAG, "flush: invalid args");
    ESP_RETURN_ON_FALSE(x1 >= 0 && y1 >= 0 && x2 >= x1 && y2 >= y1,
                        ESP_ERR_INVALID_ARG, TAG, "flush: invalid area");
    ESP_RETURN_ON_FALSE((uint32_t)x2 <= mem->h_res && (uint32_t)y2 <= mem->v_res,
                        ESP_ERR_INVALID_ARG, TAG, "flush: area out of range");

    uint32_t w = (uint32_t)(x2 - x1);
    uint32_t h = (uint32_t)(y2 - y1);
    for (uint32_t y = 0; y < h; y++) {
        uint16_t *dst_row = mem->buffer + ((uint32_t)y1 + y) * mem->h_res + (uint32_t)x1;
        const uint16_t *src_row = src + y * w;
        if (mem->swap) {
            for (uint32_t x = 0; x < w; x++) {
                dst_row[x] = gfx_color_maybe_swap_u16(src_row[x], true);
            }
        } else {
            memcpy(dst_row, src_row, (size_t)w * sizeof(uint16_t));
        }
    }
    return ESP_OK;
}

static esp_err_t gfx_memory_backend_wait_flush_impl(gfx_disp_backend_t *backend, gfx_disp_t *disp)
{
    (void)backend;
    (void)disp;
    return ESP_OK;
}

static void gfx_memory_backend_destroy_impl(gfx_disp_backend_t *backend)
{
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    if (mem == NULL) {
        return;
    }
    if (!mem->ext_buffer) {
        heap_caps_free(mem->buffer);
    }
    free(mem);
}

static const gfx_disp_backend_vtable_t s_memory_backend_vtable = {
    .flush = gfx_memory_backend_flush_impl,
    .wait_flush = gfx_memory_backend_wait_flush_impl,
    .destroy = gfx_memory_backend_destroy_impl,
};

gfx_disp_backend_t *gfx_memory_backend_create(const gfx_memory_backend_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg != NULL && cfg->h_res > 0 && cfg->v_res > 0,
                        NULL, TAG, "create: invalid config");

    size_t pixels = (size_t)cfg->h_res * cfg->v_res;
    gfx_memory_backend_t *mem = calloc(1, sizeof(*mem));
    ESP_RETURN_ON_FALSE(mem != NULL, NULL, TAG, "create: no mem for backend");

    mem->base.vtable = &s_memory_backend_vtable;
    mem->h_res = cfg->h_res;
    mem->v_res = cfg->v_res;
    mem->swap = cfg->swap;

    if (cfg->buffer != NULL) {
        ESP_GOTO_ON_FALSE(cfg->buffer_pixels >= pixels, ESP_ERR_INVALID_SIZE,
                          err, TAG, "create: external buffer too small");
        mem->buffer = (uint16_t *)cfg->buffer;
        mem->ext_buffer = true;
    } else {
        mem->buffer = heap_caps_calloc(pixels, sizeof(uint16_t), MALLOC_CAP_DEFAULT);
        ESP_GOTO_ON_FALSE(mem->buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "create: no mem for framebuffer");
        mem->ext_buffer = false;
    }

    return &mem->base;

err:
    free(mem);
    return NULL;
}

void gfx_memory_backend_delete(gfx_disp_backend_t *backend)
{
    gfx_disp_backend_destroy(backend);
}

const uint16_t *gfx_memory_backend_get_buffer(const gfx_disp_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? mem->buffer : NULL;
}

size_t gfx_memory_backend_get_buffer_pixels(const gfx_disp_backend_t *backend)
{
    const gfx_memory_backend_t *mem = (const gfx_memory_backend_t *)backend;
    return mem != NULL ? (size_t)mem->h_res * mem->v_res : 0;
}

esp_err_t gfx_memory_backend_clear(gfx_disp_backend_t *backend, gfx_color_t color)
{
    gfx_memory_backend_t *mem = (gfx_memory_backend_t *)backend;
    ESP_RETURN_ON_FALSE(mem != NULL && mem->buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "clear: invalid backend");

    size_t pixels = (size_t)mem->h_res * mem->v_res;
    uint16_t value = color.full;
    for (size_t i = 0; i < pixels; i++) {
        mem->buffer[i] = value;
    }
    return ESP_OK;
}
