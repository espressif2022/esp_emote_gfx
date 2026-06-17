/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"
#include "soc/soc_caps.h"

#include "core/display/gfx_display_priv.h"
#include "core/display/gfx_refresh_priv.h"
#include "core/runtime/gfx_core_priv.h"
#include "gfx/display.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "disp";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_display_init_default_state(gfx_display_t *disp);
static void gfx_display_cleanup(gfx_display_t *disp);
static size_t gfx_display_normalize_alloc_alignment(size_t alignment);
static void *gfx_display_alloc_buffer(size_t size, size_t alignment, uint32_t caps);
static gfx_color_format_t gfx_display_resolve_output_format(const gfx_display_config_t *cfg);
static gfx_color_format_t gfx_display_resolve_render_format(const gfx_display_config_t *cfg);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gfx_display_init_default_state(gfx_display_t *disp)
{
    disp->child_list = NULL;
    disp->next = NULL;
    disp->buf.buf_act = disp->buf.buf1;
    disp->format.render_format = GFX_COLOR_FORMAT_RGB565;
    disp->format.output_format = GFX_COLOR_FORMAT_RGB565;
    disp->format.render_pixel_size = GFX_PIXEL_SIZE_16BPP;
    disp->format.output_pixel_size = GFX_PIXEL_SIZE_16BPP;
    disp->style.bg_color.full = 0x0000;
    disp->style.bg_enable = true;
}

static gfx_color_format_t gfx_display_resolve_output_format(const gfx_display_config_t *cfg)
{
    if (cfg == NULL || cfg->color_format == GFX_COLOR_FORMAT_UNKNOWN) {
        return GFX_COLOR_FORMAT_RGB565;
    }

    return cfg->color_format;
}

static gfx_color_format_t gfx_display_resolve_render_format(const gfx_display_config_t *cfg)
{
    gfx_color_format_t output_format = gfx_display_resolve_output_format(cfg);

    if (output_format == GFX_COLOR_FORMAT_RGB565 ||
            output_format == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
            output_format == GFX_COLOR_FORMAT_RGB888 ||
            output_format == GFX_COLOR_FORMAT_BGR888 ||
            output_format == GFX_COLOR_FORMAT_XRGB8888) {
        return output_format;
    }

    return GFX_COLOR_FORMAT_RGB565;
}

static size_t gfx_display_normalize_alloc_alignment(size_t alignment)
{
    size_t min_alignment = sizeof(void *);

    if (alignment < min_alignment) {
        alignment = min_alignment;
    }

    if ((alignment & (alignment - 1U)) != 0U) {
        size_t rounded = min_alignment;
        while (rounded < alignment) {
            rounded <<= 1;
        }
        alignment = rounded;
    }

    return alignment;
}

static void *gfx_display_alloc_buffer(size_t size, size_t alignment, uint32_t caps)
{
    if (size == 0U) {
        return NULL;
    }

    alignment = gfx_display_normalize_alloc_alignment(alignment);
    return gfx_platform_aligned_alloc(alignment, size, caps);
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

esp_err_t gfx_display_buf_free(gfx_display_t *disp)
{
    if (!disp) {
        return ESP_OK;
    }
    if (!disp->buf.ext_bufs) {
        if (disp->buf.buf1) {
            gfx_platform_free(disp->buf.buf1);
            disp->buf.buf1 = NULL;
        }
        if (disp->buf.buf2) {
            gfx_platform_free(disp->buf.buf2);
            disp->buf.buf2 = NULL;
        }
    }
    if (disp->buf.flush_buf) {
        gfx_platform_free(disp->buf.flush_buf);
        disp->buf.flush_buf = NULL;
    }
    disp->buf.buf_pixels = 0;
    disp->buf.flush_buf_bytes = 0;
    disp->buf.ext_bufs = false;
    return ESP_OK;
}

esp_err_t gfx_display_buf_init(gfx_display_t *disp, const gfx_display_config_t *cfg)
{
    uint8_t render_pixel_size;
    uint8_t output_pixel_size;

    ESP_RETURN_ON_FALSE(disp != NULL && cfg != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "init display buffers: invalid args");

    disp->format.render_format = gfx_display_resolve_render_format(cfg);
    disp->format.output_format = gfx_display_resolve_output_format(cfg);
    render_pixel_size = gfx_color_format_get_size(disp->format.render_format);
    output_pixel_size = gfx_color_format_get_size(disp->format.output_format);
    ESP_RETURN_ON_FALSE(render_pixel_size > 0U && output_pixel_size > 0U,
                        ESP_ERR_NOT_SUPPORTED, TAG, "init display buffers: unsupported color format");
    ESP_RETURN_ON_FALSE(disp->format.output_format == GFX_COLOR_FORMAT_RGB565 ||
                        disp->format.output_format == GFX_COLOR_FORMAT_RGB565_SWAPPED ||
                        disp->format.output_format == GFX_COLOR_FORMAT_RGB888 ||
                        disp->format.output_format == GFX_COLOR_FORMAT_BGR888 ||
                        disp->format.output_format == GFX_COLOR_FORMAT_XRGB8888,
                        ESP_ERR_NOT_SUPPORTED, TAG,
                        "init display buffers: output format %u is not supported",
                        (unsigned)disp->format.output_format);
    disp->format.render_pixel_size = render_pixel_size;
    disp->format.output_pixel_size = output_pixel_size;

    if (cfg->buffers.buf1 != NULL) {
        disp->buf.buf1 = cfg->buffers.buf1;
        disp->buf.buf2 = cfg->buffers.buf2;
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
            buff_caps |= GFX_PLATFORM_HEAP_DMA;
        }
        if (cfg->flags.buff_spiram) {
            buff_caps |= GFX_PLATFORM_HEAP_SPIRAM;
        }
        if (buff_caps == 0) {
            buff_caps = GFX_PLATFORM_HEAP_DEFAULT;
        }

        size_t buf_pixels = cfg->buffers.buf_pixels > 0 ? cfg->buffers.buf_pixels : disp->res.h_res * disp->res.v_res;
        gfx_render_alignment_t alignment = gfx_backend_get_alignment(disp->backend);
        size_t addr_alignment = alignment.addr_bytes;

        disp->buf.buf1 = gfx_display_alloc_buffer(buf_pixels * render_pixel_size, addr_alignment, buff_caps);
        if (!disp->buf.buf1) {
            GFX_LOGE(TAG, "init display buffers: allocate frame buffer 1 failed");
            return ESP_ERR_NO_MEM;
        }

        if (cfg->flags.double_buffer) {
            disp->buf.buf2 = gfx_display_alloc_buffer(buf_pixels * render_pixel_size, addr_alignment, buff_caps);
            if (!disp->buf.buf2) {
                GFX_LOGE(TAG, "init display buffers: allocate frame buffer 2 failed");
                gfx_platform_free(disp->buf.buf1);
                disp->buf.buf1 = NULL;
                return ESP_ERR_NO_MEM;
            }
        } else {
            disp->buf.buf2 = NULL;
        }

        disp->buf.buf_pixels = buf_pixels;
        disp->buf.ext_bufs = false;
    }

    if (disp->format.output_format != disp->format.render_format) {
        disp->buf.flush_buf_bytes = disp->buf.buf_pixels * disp->format.output_pixel_size;
        disp->buf.flush_buf = gfx_display_alloc_buffer(disp->buf.flush_buf_bytes,
                              gfx_backend_get_alignment(disp->backend).addr_bytes,
                              GFX_PLATFORM_HEAP_DEFAULT);
        if (disp->buf.flush_buf == NULL) {
            GFX_LOGE(TAG, "init display buffers: allocate flush conversion buffer failed");
            gfx_display_buf_free(disp);
            return ESP_ERR_NO_MEM;
        }
    }

    disp->buf.buf_act = disp->buf.buf1;
    disp->style.bg_color.full = 0x0000;
    return ESP_OK;
}

static void gfx_display_cleanup(gfx_display_t *disp)
{
    if (!disp) {
        return;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)disp->ctx;
    if (ctx != NULL) {
        if (ctx->disp == disp) {
            ctx->disp = disp->next;
        } else {
            gfx_display_t *prev = ctx->disp;
            while (prev != NULL && prev->next != disp) {
                prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = disp->next;
            }
        }
    }

    gfx_object_child_list_free_nodes(&disp->child_list);

    if (disp->sync.event_group) {
        gfx_platform_event_delete(disp->sync.event_group);
        disp->sync.event_group = NULL;
    }

    gfx_backend_destroy(disp->backend);
    disp->backend = NULL;

    gfx_display_buf_free(disp);
    disp->ctx = NULL;
    disp->next = NULL;
}

void gfx_display_delete(gfx_display_t *disp)
{
    if (!disp) {
        return;
    }

    (void)gfx_display_delete_children(disp);
    gfx_display_cleanup(disp);
    free(disp);
}

gfx_display_t *gfx_display_add(gfx_handle_t handle, const gfx_display_config_t *cfg)
{
    esp_err_t ret;
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    bool backend_from_cfg = false;
    if (ctx == NULL || cfg == NULL) {
        GFX_LOGE(TAG, "create display: handle or config is NULL");
        return NULL;
    }

    gfx_display_t *new_disp = (gfx_display_t *)malloc(sizeof(gfx_display_t));
    if (new_disp == NULL) {
        GFX_LOGE(TAG, "create display: allocate display state failed");
        return NULL;
    }
    memset(new_disp, 0, sizeof(gfx_display_t));
    new_disp->ctx = ctx;
    new_disp->res.h_res = cfg->h_res;
    new_disp->res.v_res = cfg->v_res;
    new_disp->flags.full_frame = cfg->flags.full_frame;
    new_disp->cb.update_cb = cfg->update_cb;
    new_disp->cb.user_data = cfg->user_data;
    if (cfg->backend != NULL) {
        backend_from_cfg = true;
        new_disp->backend = cfg->backend;
    } else if (cfg->flush_cb != NULL) {
        new_disp->backend = gfx_callback_backend_create(cfg->flush_cb, cfg->user_data);
        if (new_disp->backend == NULL) {
            free(new_disp);
            return NULL;
        }
    }

    gfx_display_init_default_state(new_disp);

    if (cfg->flags.full_frame && cfg->buffers.buf_pixels > 0) {
        uint32_t screen_px = new_disp->res.h_res * new_disp->res.v_res;
        if (cfg->buffers.buf_pixels != screen_px) {
            GFX_LOGE(TAG, "create display: full_frame requires buf_pixels (%u) == screen size (%u)",
                     (unsigned)cfg->buffers.buf_pixels, (unsigned)screen_px);
            if (!backend_from_cfg) {
                gfx_backend_destroy(new_disp->backend);
            }
            free(new_disp);
            return NULL;
        }
    }

    new_disp->sync.event_group = gfx_platform_event_create();
    if (new_disp->sync.event_group == NULL) {
        GFX_LOGE(TAG, "create display: create event group failed");
        if (!backend_from_cfg) {
            gfx_backend_destroy(new_disp->backend);
        }
        free(new_disp);
        return NULL;
    }

    ret = gfx_display_buf_init(new_disp, cfg);
    if (ret != ESP_OK) {
        gfx_platform_event_delete(new_disp->sync.event_group);
        if (!backend_from_cfg) {
            gfx_backend_destroy(new_disp->backend);
        }
        free(new_disp);
        return NULL;
    }

    if (backend_from_cfg && new_disp->backend != NULL) {
        new_disp->backend->user_data = cfg->user_data;
    }

    if (ctx->disp == NULL) {
        ctx->disp = new_disp;
    } else {
        gfx_display_t *tail = ctx->disp;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_disp;
    }
    gfx_display_refresh_all(new_disp);
    GFX_LOGD(TAG, "create display: object created");
    return new_disp;
}

esp_err_t gfx_display_add_child(gfx_display_t *disp, void *src)
{
    gfx_object_t *obj = (gfx_object_t *)src;

    if (disp == NULL || src == NULL) {
        GFX_LOGE(TAG, "add display child: display or source is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    gfx_core_context_t *ctx = disp->ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    obj->disp = disp;
    obj->parent = NULL;

    esp_err_t ret = gfx_object_child_list_add(&disp->child_list, obj);
    if (ret != ESP_OK) {
        GFX_LOGE(TAG, "add display child: allocate child node failed");
    }
    return ret;
}

esp_err_t gfx_display_remove_child(gfx_display_t *disp, void *src)
{
    if (disp == NULL || src == NULL) {
        GFX_LOGE(TAG, "remove display child: display or source is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    return gfx_object_child_list_remove(&disp->child_list, (gfx_object_t *)src);
}

esp_err_t gfx_display_delete_children(gfx_display_t *disp)
{
    if (disp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (disp->child_list != NULL) {
        gfx_object_t *obj = (gfx_object_t *)disp->child_list->src;
        if (obj == NULL) {
            gfx_object_child_t *node = disp->child_list;
            disp->child_list = node->next;
            free(node);
            continue;
        }

        esp_err_t ret = gfx_object_delete(obj);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

/**********************
 *   REFRESH AND FLUSH
 **********************/

void gfx_display_refresh_all(gfx_display_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "refresh display: display is NULL");
        return;
    }
    gfx_area_t full_screen;
    full_screen.x1 = 0;
    full_screen.y1 = 0;
    full_screen.x2 = (gfx_coord_t)disp->res.h_res - 1;
    full_screen.y2 = (gfx_coord_t)disp->res.v_res - 1;
    gfx_invalidate_area_disp(disp, &full_screen);
}

bool gfx_display_flush_ready(gfx_display_t *disp, bool swap_act_buf)
{
    if (disp == NULL || disp->sync.event_group == NULL) {
        return false;
    }
    disp->render.swap_act_buf = swap_act_buf;
    if (gfx_platform_in_isr()) {
        bool need_yield = false;
        bool result = gfx_platform_event_set_from_isr(disp->sync.event_group, WAIT_FLUSH_DONE, &need_yield) != 0;
        if (need_yield) {
            gfx_platform_yield_from_isr();
        }
        return result;
    }
    return gfx_platform_event_set(disp->sync.event_group, WAIT_FLUSH_DONE) != 0;
}

/**********************
 *   CONFIG AND STATUS
 **********************/

void *gfx_display_get_user_data(gfx_display_t *disp)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "get display user data: display is NULL");
        return NULL;
    }
    return disp->backend != NULL ? disp->backend->user_data : disp->cb.user_data;
}

uint32_t gfx_display_get_h_res(gfx_display_t *disp)
{
    if (disp == NULL) {
        return DEFAULT_SCREEN_WIDTH;
    }
    return disp->res.h_res;
}

uint32_t gfx_display_get_v_res(gfx_display_t *disp)
{
    if (disp == NULL) {
        return DEFAULT_SCREEN_HEIGHT;
    }
    return disp->res.v_res;
}

gfx_color_format_t gfx_display_get_color_format(gfx_display_t *disp)
{
    if (disp == NULL) {
        return GFX_COLOR_FORMAT_RGB565;
    }
    return disp->format.output_format;
}

gfx_err_t gfx_display_set_bg_color(gfx_display_t *disp, gfx_color_t color)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "set display background color: display is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    disp->style.bg_color.full = color.full;
    GFX_LOGD(TAG, "set display background color: 0x%04X", color.full);
    return ESP_OK;
}

gfx_err_t gfx_display_set_bg_enable(gfx_display_t *disp, bool enable)
{
    if (disp == NULL) {
        GFX_LOGE(TAG, "set display background enable: display is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    disp->style.bg_enable = enable;
    return ESP_OK;
}

bool gfx_display_is_flushing_last(gfx_display_t *disp)
{
    if (disp == NULL) {
        return false;
    }
    return disp->render.flushing_last;
}

gfx_err_t gfx_display_get_perf_stats(gfx_display_t *disp, gfx_display_perf_stats_t *out_stats)
{
    if (disp == NULL || out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    out_stats->dirty_pixels = disp->render.dirty_pixels;
    out_stats->frame_time_us = disp->render.frame_time_us;
    out_stats->render_time_us = disp->render.render_time_us;
    out_stats->flush_time_us = disp->render.flush_time_us;
    out_stats->flush_count = disp->render.flush_count;
    out_stats->draw = disp->render.draw;
    return ESP_OK;
}
