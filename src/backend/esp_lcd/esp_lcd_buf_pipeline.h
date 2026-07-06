/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sys/queue.h"

#include "gfx/error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gfx_pipeline_buf {
    void *buffer;
    STAILQ_ENTRY(gfx_pipeline_buf) entry;
};

typedef struct {
    portMUX_TYPE lock;
    struct gfx_pipeline_buf *elems;
    uint8_t elem_count;
    SemaphoreHandle_t avail_sem;
    STAILQ_HEAD(, gfx_pipeline_buf) inflight_queue;
    STAILQ_HEAD(, gfx_pipeline_buf) free_queue;
} gfx_buf_pipeline_t;

/**
 * @brief Initialize a partial-mode panel FB pipeline (draw=fb[0], disp=fb[1], free=fb[2+]).
 */
gfx_err_t gfx_buf_pipeline_init_partial(gfx_buf_pipeline_t *pipeline,
                                        void **panel_fbs,
                                        uint8_t panel_fb_count,
                                        void **out_disp_fb,
                                        void **out_draw_fb);

/**
 * @brief Initialize a full-frame panel FB pipeline (draw=fb[0], disp=fb[1], free=fb[2+]).
 */
gfx_err_t gfx_buf_pipeline_init_full(gfx_buf_pipeline_t *pipeline,
                                     void **panel_fbs,
                                     uint8_t panel_fb_count,
                                     void **out_disp_fb,
                                     void **out_draw_fb);

void gfx_buf_pipeline_destroy(gfx_buf_pipeline_t *pipeline);

void gfx_buf_pipeline_commit(gfx_buf_pipeline_t *pipeline, void *buf);

struct gfx_pipeline_buf *gfx_buf_pipeline_acquire_sync(gfx_buf_pipeline_t *pipeline);

void gfx_buf_pipeline_retire_isr(gfx_buf_pipeline_t *pipeline);

bool gfx_buf_pipeline_is_active(const gfx_buf_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif
