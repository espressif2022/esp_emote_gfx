/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "soc/soc_caps.h"

#include "core/display/gfx_disp_priv.h"
#include "core/display/gfx_refr_priv.h"
#include "core/runtime/gfx_core_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "disp";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_disp_init_default_state(gfx_disp_t *disp);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_disp_init_default_state(gfx_disp_t *disp)
{
    disp->child_list = NULL;
    disp->next = NULL;
    disp->buf.buf_act = disp->buf.buf1;
    disp->style.bg_color.full = 0x0000;
    disp->style.bg_enable = true;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_disp_buf_free(gfx_disp_t *disp)
{
    if (!disp) {
        return ESP_OK;
    }
    if (!disp->buf.ext_bufs) {
        if (disp->buf.buf1) {
            heap_caps_free(disp->buf.buf1);
            disp->buf.buf1 = NULL;
        }
        if (disp->buf.buf2) {
            heap_caps_free(disp->buf.buf2);
            disp->buf.buf2 = NULL;
        }
    }
    disp->buf.buf_pixels = 0;
    disp->buf.ext_bufs = false;
    return ESP_OK;
}

esp_err_t gfx_disp_buf_init(gfx_disp_t *disp, const gfx_disp_config_t *cfg)
{
    if (cfg->buffers.buf1 != NULL) {
        disp->buf.buf1 = (uint16_t *)cfg->buffers.buf1;
        disp->buf.buf2 = (uint16_t *)cfg->buffers.buf2;
        if (cfg->buffers.buf_pixels > 0) {
            disp->buf.buf_pixels = cfg->buffers.buf_pixels;
        } else {
            GFX_LOGW(TAG, "init display buffers: buf_pixels is zero, using screen size");
            disp->buf.buf_pixels = disp->res.h_res * disp->res.v_res;
        }
        disp->buf.ext_bufs = true;
    } else {
#if SOC_PSRAM_DMA_CAPABLE == 0
        if (cfg->flags.buff_dma && cfg->flags.buff_spiram) {
            GFX_LOGW(TAG, "init display buffers: dma with spiram is not supported");
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

        size_t buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : disp->res.h_res * disp->res.v_res;

        disp->buf.buf1 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
        if (!disp->buf.buf1) {
            GFX_LOGE(TAG, "init display buffers: allocate frame buffer 1 failed");
            return ESP_ERR_NO_MEM;
        }

        if (cfg->flags.double_buffer) {
            disp->buf.buf2 = (uint16_t *)heap_caps_malloc(buf_pixels * sizeof(uint16_t), buff_caps);
            if (!disp->buf.buf2) {
                GFX_LOGE(TAG, "init display buffers: allocate frame buffer 2 failed");
                heap_caps_free(disp->buf.buf1);
                disp->buf.buf1 = NULL;
                return ESP_ERR_NO_MEM;
            }
        } else {
            disp->buf.buf2 = NULL;
        }

        disp->buf.buf_pixels = buf_pixels;
        disp->buf.ext_bufs = false;
    }
    disp->buf.buf_act = disp->buf.buf1;
    disp->style.bg_color.full = 0x0000;
    return ESP_OK;
}

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

    if (disp->sync.event_group) {
        vEventGroupDelete(disp->sync.event_group);
        disp->sync.event_group = NULL;
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
        GFX_LOGE(TAG, "create display: handle or config is NULL");
        return NULL;
    }

    gfx_disp_t *new_disp = (gfx_disp_t *)malloc(sizeof(gfx_disp_t));
    if (new_disp == NULL) {
        GFX_LOGE(TAG, "create display: allocate display state failed");
        return NULL;
    }
    memset(new_disp, 0, sizeof(gfx_disp_t));
    new_disp->ctx = ctx;
    new_disp->res.h_res = cfg->h_res;
    new_disp->res.v_res = cfg->v_res;
    new_disp->flags.swap = cfg->flags.swap;
    new_disp->flags.full_frame = cfg->flags.full_frame;
    new_disp->cb.flush_cb = cfg->flush_cb;
    new_disp->cb.update_cb = cfg->update_cb;
    new_disp->cb.user_data = cfg->user_data;
    gfx_disp_init_default_state(new_disp);

    if (cfg->flags.full_frame && cfg->buffers.buf_pixels > 0) {
        uint32_t screen_px = new_disp->res.h_res * new_disp->res.v_res;
        if (cfg->buffers.buf_pixels != screen_px) {
            GFX_LOGE(TAG, "create display: full_frame requires buf_pixels (%u) == screen size (%u)",
                     (unsigned)cfg->buffers.buf_pixels, (unsigned)screen_px);
            free(new_disp);
            return NULL;
        }
    }

    new_disp->sync.event_group = xEventGroupCreate();
    if (new_disp->sync.event_group == NULL) {
        GFX_LOGE(TAG, "create display: create event group failed");
        free(new_disp);
        return NULL;
    }

    if (cfg->buffers.buf1 != NULL) {
        new_disp->buf.buf1 = (uint16_t *)cfg->buffers.buf1;
        new_disp->buf.buf2 = (uint16_t *)cfg->buffers.buf2;
        new_disp->buf.buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : new_disp->res.h_res * new_disp->res.v_res;
        new_disp->buf.ext_bufs = true;
        new_disp->buf.buf_act = new_disp->buf.buf1;
    } else {
        ret = gfx_disp_buf_init(new_disp, cfg);
        if (ret != ESP_OK) {
            vEventGroupDelete(new_disp->sync.event_group);
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
    GFX_LOGD(TAG, "create display: object created");
    return new_disp;
}

esp_err_t gfx_disp_add_child(gfx_disp_t *disp, void *src)
{
    if (disp == NULL || src == NULL) {
        GFX_LOGE(TAG, "add display child: display or source is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    gfx_core_context_t *ctx = disp->ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ((gfx_obj_t *)src)->disp = disp;

    gfx_obj_child_t *new_child = (gfx_obj_child_t *)malloc(sizeof(gfx_obj_child_t));
    if (new_child == NULL) {
        GFX_LOGE(TAG, "add display child: allocate child node failed");
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
        GFX_LOGE(TAG, "remove display child: display or source is NULL");
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

esp_err_t gfx_disp_delete_children(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (disp->child_list != NULL) {
        gfx_obj_t *obj = (gfx_obj_t *)disp->child_list->src;
        if (obj == NULL) {
            gfx_obj_child_t *node = disp->child_list;
            disp->child_list = node->next;
            free(node);
            continue;
        }

        esp_err_t ret = gfx_obj_delete(obj);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

/**********************
 *   REFRESH AND FLUSH
 **********************/

void gfx_disp_refresh_all(gfx_disp_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "refresh display: display is NULL");
        return;
    }
    gfx_area_t full_screen;
    full_screen.x1 = 0;
    full_screen.y1 = 0;
    full_screen.x2 = (int)disp->res.h_res - 1;
    full_screen.y2 = (int)disp->res.v_res - 1;
    gfx_invalidate_area_disp(disp, &full_screen);
}

bool gfx_disp_flush_ready(gfx_disp_t *disp, bool swap_act_buf)
{
    if (disp == NULL || disp->sync.event_group == NULL) {
        return false;
    }
    disp->render.swap_act_buf = swap_act_buf;
    if (xPortInIsrContext()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        bool result = xEventGroupSetBitsFromISR(disp->sync.event_group, WAIT_FLUSH_DONE, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
        return result;
    }
    return xEventGroupSetBits(disp->sync.event_group, WAIT_FLUSH_DONE);
}

/**********************
 *   CONFIG AND STATUS
 **********************/

void *gfx_disp_get_user_data(gfx_disp_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "get display user data: display is NULL");
        return NULL;
    }
    return disp->cb.user_data;
}

uint32_t gfx_disp_get_hor_res(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return DEFAULT_SCREEN_WIDTH;
    }
    return disp->res.h_res;
}

uint32_t gfx_disp_get_ver_res(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return DEFAULT_SCREEN_HEIGHT;
    }
    return disp->res.v_res;
}

esp_err_t gfx_disp_set_bg_color(gfx_disp_t *disp, gfx_color_t color)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "set display background color: display is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    disp->style.bg_color.full = color.full;
    GFX_LOGD(TAG, "set display background color: 0x%04X", color.full);
    return ESP_OK;
}

esp_err_t gfx_disp_set_bg_enable(gfx_disp_t *disp, bool enable)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "set display background enable: display is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    disp->style.bg_enable = enable;
    return ESP_OK;
}

bool gfx_disp_is_flushing_last(gfx_disp_t *disp)
{
    if (disp == NULL) {
        return false;
    }
    return disp->render.flushing_last;
}

esp_err_t gfx_disp_get_perf_stats(gfx_disp_t *disp, gfx_disp_perf_stats_t *out_stats)
{
    if (disp == NULL || out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    out_stats->dirty_pixels = disp->render.dirty_pixels;
    out_stats->frame_time_us = disp->render.frame_time_us;
    out_stats->render_time_us = disp->render.render_time_us;
    out_stats->flush_time_us = disp->render.flush_time_us;
    out_stats->flush_count = disp->render.flush_count;
    out_stats->blend = disp->render.blend;
    return ESP_OK;
}
