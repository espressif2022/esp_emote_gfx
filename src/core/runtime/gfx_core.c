/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"

#include "core/gfx_obj.h"
#include "core/display/gfx_refr_priv.h"
#include "core/display/gfx_render_priv.h"
#include "core/object/gfx_obj_priv.h"
#include "core/runtime/gfx_timer_priv.h"
#include "core/runtime/gfx_touch_priv.h"

#include "widget/img/gfx_img_dec_priv.h"
#include "widget/font/gfx_font_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "core";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_render_loop_task(void *arg);
static uint32_t gfx_cal_task_delay(uint32_t timer_delay);
static void gfx_do_refr_now_impl(gfx_core_context_t *ctx);
static inline TickType_t gfx_block_ticks(uint32_t ms);
static void gfx_wait_for_work(gfx_core_context_t *ctx, uint32_t next_sleep_ms, EventBits_t *out_triggered);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/** Convert ms to block time in ticks; minimum 1 tick. */
static inline TickType_t gfx_block_ticks(uint32_t ms)
{
    TickType_t t = pdMS_TO_TICKS(ms);
    return (t >= 1) ? t : 1;
}

/**
 * Wait for render work or lifecycle exit. Checks NEED_DELETE first (no block);
 * if set, signals DELETE_DONE and deletes the task (never returns).
 * Otherwise waits on render_events for up to next_sleep_ms and returns evt_invalidate bits.
 */
static void gfx_wait_for_work(gfx_core_context_t *ctx, uint32_t next_sleep_ms, EventBits_t *out_triggered)
{
    EventBits_t life = xEventGroupWaitBits(ctx->sync.lifecycle_events, NEED_DELETE,
                                           pdTRUE, pdFALSE, 0);
    if (life & NEED_DELETE) {
        xEventGroupSetBits(ctx->sync.lifecycle_events, DELETE_DONE);
        vTaskDeleteWithCaps(NULL);
        /* never returns */
    }

    if (ctx->sync.render_events != NULL) {
        *out_triggered = xEventGroupWaitBits(ctx->sync.render_events, GFX_EVENT_ALL,
                                             pdTRUE, pdFALSE, gfx_block_ticks(next_sleep_ms));
    } else {
        vTaskDelay(gfx_block_ticks(next_sleep_ms));
        *out_triggered = 0;
    }
}

static uint32_t gfx_cal_task_delay(uint32_t timer_delay)
{
    uint32_t min_delay_ms = (1000 / configTICK_RATE_HZ) + 1; // At least one tick + 1ms

    if (timer_delay == ANIM_NO_TIMER_READY) {
        return (min_delay_ms > 5) ? min_delay_ms : 5;
    } else {
        return (timer_delay < min_delay_ms) ? min_delay_ms : timer_delay;
    }
}

static void gfx_render_loop_task(void *arg)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)arg;
    SemaphoreHandle_t mutex = ctx->sync.render_mutex;
    uint32_t next_sleep_ms = GFX_RENDER_TASK_IDLE_SLEEP_MS;

    /**
     * If this task runs too early, add_disp() can fire the first frame before the
     * caller / external code is ready, which can lead to a deadlock. Delay so the
     * rest of the system can finish setup first.
     */
    vTaskDelay(pdMS_TO_TICKS(GFX_RENDER_TASK_IDLE_SLEEP_MS));

    for (;;) {
        EventBits_t evt_invalidate;
        gfx_wait_for_work(ctx, next_sleep_ms, &evt_invalidate);

        bool locked = (mutex != NULL && xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE);
        if (!locked) {
            next_sleep_ms = 1;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        bool evt_refr;
        uint32_t time_until_next = gfx_timer_handler(&ctx->timer_mgr, &evt_refr);
        bool need_refr = (evt_invalidate != 0) || evt_refr;

        if (need_refr) {
            gfx_do_refr_now_impl(ctx);
        }

        next_sleep_ms = gfx_cal_task_delay(time_until_next);
        xSemaphoreGiveRecursive(mutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void gfx_do_refr_now_impl(gfx_core_context_t *ctx)
{
    if (ctx->disp != NULL) {
        gfx_render_handler(ctx);
    }
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg)
{
    esp_err_t ret = ESP_OK;
    gfx_core_context_t *disp_ctx = NULL;
    BaseType_t task_ret = pdFAIL;
    bool lifecycle_events_created = false;
    bool mutex_created = false;
    bool decoder_inited = false;
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    bool font_lib_created = false;
#endif

    ESP_GOTO_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, err, TAG, "Invalid configuration");

    disp_ctx = malloc(sizeof(gfx_core_context_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate player context");

    memset(disp_ctx, 0, sizeof(gfx_core_context_t));

    disp_ctx->sync.lifecycle_events = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.lifecycle_events, ESP_ERR_NO_MEM, err, TAG, "Failed to create event group");
    lifecycle_events_created = true;

    disp_ctx->sync.render_events = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.render_events, ESP_ERR_NO_MEM, err, TAG, "Failed to create render event group");

    disp_ctx->sync.render_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.render_mutex, ESP_ERR_NO_MEM, err, TAG, "Failed to create recursive render mutex");
    mutex_created = true;

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    ret = gfx_ft_lib_create();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to create font library");
    font_lib_created = true;
#endif

    gfx_timer_mgr_init(&disp_ctx->timer_mgr, cfg->fps);

    ret = gfx_image_decoder_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to initialize image decoder");
    decoder_inited = true;

    const uint32_t stack_caps = cfg->task.task_stack_caps ? cfg->task.task_stack_caps : (MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);
    if (cfg->task.task_affinity < 0) {
        task_ret = xTaskCreateWithCaps(gfx_render_loop_task, "gfx_render", cfg->task.task_stack,
                                       disp_ctx, cfg->task.task_priority, NULL, stack_caps);
    } else {
        task_ret = xTaskCreatePinnedToCoreWithCaps(gfx_render_loop_task, "gfx_render", cfg->task.task_stack,
                   disp_ctx, cfg->task.task_priority, NULL, cfg->task.task_affinity, stack_caps);
    }
    ESP_GOTO_ON_FALSE(task_ret == pdPASS, ESP_ERR_NO_MEM, err, TAG, "Failed to create render task");

    return (gfx_handle_t)disp_ctx;

err:
    if (decoder_inited) {
        gfx_image_decoder_deinit();
    }
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    if (font_lib_created) {
        gfx_ft_lib_cleanup();
    }
#endif
    if (mutex_created) {
        vSemaphoreDelete(disp_ctx->sync.render_mutex);
    }
    if (disp_ctx->sync.render_events) {
        vEventGroupDelete(disp_ctx->sync.render_events);
        disp_ctx->sync.render_events = NULL;
    }
    if (lifecycle_events_created) {
        vEventGroupDelete(disp_ctx->sync.lifecycle_events);
    }
    free(disp_ctx);
    return NULL;
}

void gfx_emote_deinit(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        GFX_LOGE(TAG, "deinit graphics: context is NULL");
        return;
    }

    xEventGroupSetBits(ctx->sync.lifecycle_events, NEED_DELETE);
    xEventGroupWaitBits(ctx->sync.lifecycle_events, DELETE_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

    while (ctx->disp != NULL) {
        gfx_disp_t *d = ctx->disp;
        gfx_disp_delete_children(d);
        gfx_disp_del(d);
        free(d);
    }

    while (ctx->touch != NULL) {
        gfx_touch_t *t = ctx->touch;
        gfx_touch_del(t);
        free(t);
    }

    gfx_timer_mgr_deinit(&ctx->timer_mgr);

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_ft_lib_cleanup();
#endif

    if (ctx->sync.render_mutex) {
        vSemaphoreDelete(ctx->sync.render_mutex);
        ctx->sync.render_mutex = NULL;
    }

    if (ctx->sync.lifecycle_events) {
        vEventGroupDelete(ctx->sync.lifecycle_events);
        ctx->sync.lifecycle_events = NULL;
    }

    if (ctx->sync.render_events) {
        vEventGroupDelete(ctx->sync.render_events);
        ctx->sync.render_events = NULL;
    }

    gfx_image_decoder_deinit();
    free(ctx);
}

esp_err_t gfx_refr_now(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    SemaphoreHandle_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "refresh now: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mutex, portMAX_DELAY) != pdTRUE) {
        GFX_LOGE(TAG, "refresh now: acquire mutex failed");
        return ESP_ERR_TIMEOUT;
    }

    gfx_do_refr_now_impl(ctx);

    if (xSemaphoreGiveRecursive(mutex) != pdTRUE) {
        GFX_LOGE(TAG, "refresh now: release mutex failed");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t gfx_emote_lock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    SemaphoreHandle_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "lock graphics: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mutex, portMAX_DELAY) != pdTRUE) {
        GFX_LOGE(TAG, "lock graphics: acquire mutex failed");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t gfx_emote_unlock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    SemaphoreHandle_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "unlock graphics: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreGiveRecursive(mutex) != pdTRUE) {
        GFX_LOGE(TAG, "unlock graphics: release mutex failed");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
