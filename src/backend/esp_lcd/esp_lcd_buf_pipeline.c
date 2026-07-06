/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "common/gfx_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_DISP
#include "common/gfx_log_priv.h"

#include "backend/esp_lcd/esp_lcd_buf_pipeline.h"

static const char *const TAG = "esp_lcd_pipeline";

static bool gfx_buf_pipeline_elem_managed(gfx_buf_pipeline_t *pipeline, void *buf)
{
    struct gfx_pipeline_buf *elem;

    if (pipeline == NULL || buf == NULL) {
        return false;
    }

    STAILQ_FOREACH(elem, &pipeline->inflight_queue, entry) {
        if (elem->buffer == buf) {
            return true;
        }
    }
    STAILQ_FOREACH(elem, &pipeline->free_queue, entry) {
        if (elem->buffer == buf) {
            return true;
        }
    }

    return false;
}

bool gfx_buf_pipeline_is_active(const gfx_buf_pipeline_t *pipeline)
{
    return pipeline != NULL && pipeline->elem_count >= 2U && pipeline->elems != NULL;
}

static gfx_err_t gfx_buf_pipeline_init_layout(gfx_buf_pipeline_t *pipeline,
        void **panel_fbs,
        uint8_t panel_fb_count,
        void **out_disp_fb,
        void **out_draw_fb)
{
    if (pipeline == NULL || panel_fbs == NULL || out_disp_fb == NULL || out_draw_fb == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    if (panel_fb_count < 2U || panel_fbs[0] == NULL || panel_fbs[1] == NULL) {
        return GFX_ERR_INVALID_ARG;
    }

    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    STAILQ_INIT(&pipeline->inflight_queue);
    STAILQ_INIT(&pipeline->free_queue);

    pipeline->avail_sem = xSemaphoreCreateCounting(panel_fb_count, 0);
    if (pipeline->avail_sem == NULL) {
        return GFX_ERR_NO_MEM;
    }

    pipeline->elem_count = panel_fb_count;
    pipeline->elems = calloc(panel_fb_count, sizeof(struct gfx_pipeline_buf));
    if (pipeline->elems == NULL) {
        vSemaphoreDelete(pipeline->avail_sem);
        pipeline->avail_sem = NULL;
        return GFX_ERR_NO_MEM;
    }

    for (uint8_t i = 0; i < panel_fb_count; i++) {
        pipeline->elems[i].buffer = panel_fbs[i];
    }

    /* Match esp_lvgl_adapter: draw=fb[0], disp=fb[1], free pool=fb[2..]. */
    *out_draw_fb = panel_fbs[0];
    *out_disp_fb = panel_fbs[1];

    for (uint8_t i = 2; i < panel_fb_count; i++) {
        STAILQ_INSERT_TAIL(&pipeline->free_queue, &pipeline->elems[i], entry);
        (void)xSemaphoreGive(pipeline->avail_sem);
    }

    return GFX_OK;
}

gfx_err_t gfx_buf_pipeline_init_partial(gfx_buf_pipeline_t *pipeline,
                                        void **panel_fbs,
                                        uint8_t panel_fb_count,
                                        void **out_disp_fb,
                                        void **out_draw_fb)
{
    return gfx_buf_pipeline_init_layout(pipeline, panel_fbs, panel_fb_count, out_disp_fb, out_draw_fb);
}

gfx_err_t gfx_buf_pipeline_init_full(gfx_buf_pipeline_t *pipeline,
                                     void **panel_fbs,
                                     uint8_t panel_fb_count,
                                     void **out_disp_fb,
                                     void **out_draw_fb)
{
    return gfx_buf_pipeline_init_layout(pipeline, panel_fbs, panel_fb_count, out_disp_fb, out_draw_fb);
}

void gfx_buf_pipeline_destroy(gfx_buf_pipeline_t *pipeline)
{
    if (pipeline == NULL) {
        return;
    }

    free(pipeline->elems);
    pipeline->elems = NULL;
    pipeline->elem_count = 0;

    if (pipeline->avail_sem != NULL) {
        vSemaphoreDelete(pipeline->avail_sem);
        pipeline->avail_sem = NULL;
    }

    STAILQ_INIT(&pipeline->inflight_queue);
    STAILQ_INIT(&pipeline->free_queue);
}

void gfx_buf_pipeline_commit(gfx_buf_pipeline_t *pipeline, void *buf)
{
    if (pipeline == NULL || buf == NULL || pipeline->elems == NULL) {
        return;
    }

    portENTER_CRITICAL(&pipeline->lock);
    if (gfx_buf_pipeline_elem_managed(pipeline, buf)) {
        portEXIT_CRITICAL(&pipeline->lock);
        return;
    }

    for (uint8_t i = 0; i < pipeline->elem_count; i++) {
        if (pipeline->elems[i].buffer == buf) {
            STAILQ_INSERT_TAIL(&pipeline->inflight_queue, &pipeline->elems[i], entry);
            break;
        }
    }
    portEXIT_CRITICAL(&pipeline->lock);
}

struct gfx_pipeline_buf *gfx_buf_pipeline_acquire_sync(gfx_buf_pipeline_t *pipeline)
{
    struct gfx_pipeline_buf *next = NULL;

    if (pipeline == NULL || pipeline->avail_sem == NULL) {
        return NULL;
    }

    if (xSemaphoreTake(pipeline->avail_sem, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }

    portENTER_CRITICAL(&pipeline->lock);
    next = STAILQ_FIRST(&pipeline->free_queue);
    if (next != NULL) {
        STAILQ_REMOVE_HEAD(&pipeline->free_queue, entry);
    }
    portEXIT_CRITICAL(&pipeline->lock);

    if (next == NULL) {
        GFX_LOGW(TAG, "pipeline acquire: semaphore signaled but free queue empty");
    }

    return next;
}

void IRAM_ATTR gfx_buf_pipeline_retire_isr(gfx_buf_pipeline_t *pipeline)
{
    struct gfx_pipeline_buf *elem;
    BaseType_t wake = pdFALSE;

    if (pipeline == NULL || pipeline->elems == NULL) {
        return;
    }

    portENTER_CRITICAL_ISR(&pipeline->lock);
    elem = STAILQ_FIRST(&pipeline->inflight_queue);
    if (elem != NULL) {
        STAILQ_REMOVE_HEAD(&pipeline->inflight_queue, entry);
        STAILQ_INSERT_TAIL(&pipeline->free_queue, elem, entry);
        if (pipeline->avail_sem != NULL) {
            xSemaphoreGiveFromISR(pipeline->avail_sem, &wake);
        }
    }
    portEXIT_CRITICAL_ISR(&pipeline->lock);

    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
