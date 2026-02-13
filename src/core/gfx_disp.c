/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "soc/soc_caps.h"

#include "core/gfx_disp_priv.h"
#include "core/gfx_core_priv.h"
#include "core/gfx_refr_priv.h"

static const char *TAG = "gfx_disp";

/* ============================================================================
 * Buffer helpers (internal)
 * ============================================================================ */

esp_err_t gfx_disp_buf_free(gfx_disp_t *disp)
{
    if (!disp) {
        return ESP_OK;
    }
    if (!disp->ext_bufs) {
        if (disp->buf1) {
            heap_caps_free(disp->buf1);
            disp->buf1 = NULL;
        }
        if (disp->buf2) {
            heap_caps_free(disp->buf2);
            disp->buf2 = NULL;
        }
    }
    disp->buf_pixels = 0;
    disp->ext_bufs = false;
    return ESP_OK;
}

esp_err_t gfx_disp_buf_init(gfx_disp_t *disp, const gfx_disp_config_t *cfg)
{
    if (cfg->buffers.buf1 != NULL) {
        disp->buf1 = (uint16_t *)cfg->buffers.buf1;
        disp->buf2 = (uint16_t *)cfg->buffers.buf2;
        if (cfg->buffers.buf_pixels > 0) {
            disp->buf_pixels = cfg->buffers.buf_pixels;
        } else {
            ESP_LOGW(TAG, "buf_pixels=0, use default");
            disp->buf_pixels = disp->h_res * disp->v_res;
        }
        disp->ext_bufs = true;
    } else {
#if SOC_PSRAM_DMA_CAPABLE == 0
        if (cfg->flags.buff_dma && cfg->flags.buff_spiram) {
            ESP_LOGW(TAG, "DMA+SPIRAM not supported");
            return ESP_ERR_NOT_SUPPORTED;
        }
#endif
        uint32_t buff_caps = 0;
        if (cfg->flags.buff_dma) {
            buff_caps |= MALLOC_CAP_DMA;
        }
        if (cfg->flags.buff_spiram) {
            buff_caps |= MALLOC_CAP_SPIRAM;
        }
        if (buff_caps == 0) {
            buff_caps = MALLOC_CAP_DEFAULT;
        }

        size_t buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : disp->h_res * disp->v_res;

        disp->buf1 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
        if (!disp->buf1) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer 1");
            return ESP_ERR_NO_MEM;
        }

        if (cfg->flags.double_buffer) {
            disp->buf2 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
            if (!disp->buf2) {
                ESP_LOGE(TAG, "Failed to allocate frame buffer 2");
                heap_caps_free(disp->buf1);
                disp->buf1 = NULL;
                return ESP_ERR_NO_MEM;
            }
        } else {
            disp->buf2 = NULL;
        }

        disp->buf_pixels = buf_pixels;
        disp->ext_bufs = false;
    }
    disp->buf_act = disp->buf1;
    disp->bg_color.full = 0x0000;
    return ESP_OK;
}

/* ============================================================================
 * Display add / del / child
 * ============================================================================ */

void gfx_disp_del(gfx_disp_t *disp)
{
    if (!disp) {
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)disp->ctx;
    if (ctx != NULL) {
        if (ctx->disp == disp) {
            ctx->disp = disp->next;
        } else {
            gfx_disp_t *prev = ctx->disp;
            while (prev != NULL && prev->next != disp) {
                prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = disp->next;
            }
        }
    }

    gfx_obj_child_t *child_node = disp->child_list;
    while (child_node != NULL) {
        gfx_obj_child_t *next_child = child_node->next;
        free(child_node);
        child_node = next_child;
    }
    disp->child_list = NULL;

    if (disp->event_group) {
        vEventGroupDelete(disp->event_group);
        disp->event_group = NULL;
    }

    gfx_disp_buf_free(disp);
    disp->ctx = NULL;
    disp->next = NULL;
}

gfx_disp_t *gfx_disp_add(gfx_handle_t handle, const gfx_disp_config_t *cfg)
{
    esp_err_t ret;
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }

    gfx_disp_t *new_disp = (gfx_disp_t *)malloc(sizeof(gfx_disp_t));
    if (new_disp == NULL) {
        ESP_LOGE(TAG, "Failed to allocate display");
        return NULL;
    }
    memset(new_disp, 0, sizeof(gfx_disp_t));
    new_disp->ctx = ctx;
    new_disp->h_res = cfg->h_res;
    new_disp->v_res = cfg->v_res;
    new_disp->flags.swap = cfg->flags.swap;
    new_disp->flags.buff_dma = cfg->flags.buff_dma;
    new_disp->flags.buff_spiram = cfg->flags.buff_spiram;
    new_disp->flags.double_buffer = cfg->flags.double_buffer;
    new_disp->flush_cb = cfg->flush_cb;
    new_disp->update_cb = cfg->update_cb;
    new_disp->user_data = cfg->user_data;
    new_disp->child_list = NULL;
    new_disp->next = NULL;

    new_disp->event_group = xEventGroupCreate();
    if (new_disp->event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create disp event group");
        free(new_disp);
        return NULL;
    }

    if (cfg->buffers.buf1 != NULL) {
        new_disp->buf1 = (uint16_t *)cfg->buffers.buf1;
        new_disp->buf2 = (uint16_t *)cfg->buffers.buf2;
        new_disp->buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : new_disp->h_res * new_disp->v_res;
        new_disp->ext_bufs = true;
        new_disp->buf_act = new_disp->buf1;
        new_disp->bg_color.full = 0x0000;
    } else {
        ret = gfx_disp_buf_init(new_disp, cfg);
        if (ret != ESP_OK) {
            vEventGroupDelete(new_disp->event_group);
            free(new_disp);
            return NULL;
        }
    }

    if (ctx->disp == NULL) {
        ctx->disp = new_disp;
    } else {
        gfx_disp_t *tail = ctx->disp;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_disp;
    }
    gfx_disp_refresh_all(new_disp);
    return new_disp;
}

esp_err_t gfx_disp_add_child(gfx_disp_t *disp, void *src)
{
    if (disp == NULL || src == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    gfx_core_context_t *ctx = disp->ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ((gfx_obj_t *)src)->disp = disp;

    gfx_obj_child_t *new_child = (gfx_obj_child_t *)malloc(sizeof(gfx_obj_child_t));
    if (new_child == NULL) {
        ESP_LOGE(TAG, "Failed to allocate child node");
        return ESP_ERR_NO_MEM;
    }
    new_child->src = src;
    new_child->next = NULL;

    if (disp->child_list == NULL) {
        disp->child_list = new_child;
    } else {
        gfx_obj_child_t *current = disp->child_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_child;
    }
    return ESP_OK;
}

esp_err_t gfx_disp_remove_child(gfx_disp_t *disp, void *src)
{
    if (disp == NULL || src == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    gfx_obj_child_t *current = disp->child_list;
    gfx_obj_child_t *prev = NULL;

    while (current != NULL) {
        if (current->src == src) {
            if (prev == NULL) {
                disp->child_list = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}

/* ============================================================================
 * Refresh and flush
 * ============================================================================ */

void gfx_disp_refresh_all(gfx_disp_t *disp)
{
    if (disp == NULL) {
        ESP_LOGE(TAG, "disp is NULL");
        return;
    }
    gfx_area_t full_screen;
    full_screen.x1 = 0;
    full_screen.y1 = 0;
    full_screen.x2 = (int)disp->h_res - 1;
    full_screen.y2 = (int)disp->v_res - 1;
    gfx_invalidate_area_disp(disp, &full_screen);
}

bool gfx_disp_flush_ready(gfx_disp_t *disp, bool swap_act_buf)
{
    if (disp == NULL || disp->event_group == NULL) {
        return false;
    }
    disp->swap_act_buf = swap_act_buf;
    if (xPortInIsrContext()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        bool result = xEventGroupSetBitsFromISR(disp->event_group, WAIT_FLUSH_DONE, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return result;
    }
    return xEventGroupSetBits(disp->event_group, WAIT_FLUSH_DONE);
}

/* ============================================================================
 * Config and status
 * ============================================================================ */

void *gfx_disp_get_user_data(gfx_disp_t *disp)
{
    if (disp == NULL) {
        ESP_LOGE(TAG, "Invalid display");
        return NULL;
    }
    return disp->user_data;
}

esp_err_t gfx_disp_get_size(gfx_disp_t *disp, uint32_t *width, uint32_t *height)
{
    if (width == NULL || height == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    if (disp == NULL) {
        *width = DEFAULT_SCREEN_WIDTH;
        *height = DEFAULT_SCREEN_HEIGHT;
        ESP_LOGW(TAG, "disp is NULL, using default screen size");
        return ESP_OK;
    }
    *width = disp->h_res;
    *height = disp->v_res;
    return ESP_OK;
}

esp_err_t gfx_disp_set_bg_color(gfx_disp_t *disp, gfx_color_t color)
{
    if (disp == NULL) {
        ESP_LOGE(TAG, "disp is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    disp->bg_color.full = color.full;
    ESP_LOGD(TAG, "BG color: 0x%04X", color.full);
    return ESP_OK;
}

bool gfx_disp_is_flushing_last(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return false;
    }
    return disp->flushing_last;
}
