/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"

#include "common/gfx_subsystem_init_priv.h"
#include "core/gfx_obj.h"
#include "core/display/gfx_refresh_priv.h"
#include "render/gfx_render_priv.h"
#include "core/object/gfx_object_priv.h"
#include "core/runtime/gfx_timer_priv.h"
#include "core/runtime/gfx_touch_priv.h"
#include "core/tween/gfx_tween_priv.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *const TAG = "core";

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gfx_render_loop_task(void *arg);
static uint32_t gfx_cal_task_delay(uint32_t timer_delay);
static void gfx_do_refr_now_impl(gfx_core_context_t *ctx);
static gfx_err_t gfx_core_tick_locked(gfx_core_context_t *ctx, bool force_refresh);
static void gfx_wait_for_work(gfx_core_context_t *ctx, uint32_t next_sleep_ms, gfx_platform_event_bits_t *out_triggered);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Wait for render work or lifecycle exit. Checks NEED_DELETE first (no block);
 * if set, signals DELETE_DONE and deletes the task (never returns).
 * Otherwise waits on render_events for up to next_sleep_ms and returns evt_invalidate bits.
 */
static void gfx_wait_for_work(gfx_core_context_t *ctx, uint32_t next_sleep_ms, gfx_platform_event_bits_t *out_triggered)
{
    gfx_platform_event_bits_t life = gfx_platform_event_wait(ctx->sync.lifecycle_events, NEED_DELETE,
                                     true, false, 0);
    if (life & NEED_DELETE) {
        gfx_platform_event_set(ctx->sync.lifecycle_events, DELETE_DONE);
        gfx_platform_task_delete_current();
        /* never returns */
    }

    if (ctx->sync.render_events != NULL) {
        *out_triggered = gfx_platform_event_wait(ctx->sync.render_events, GFX_EVENT_ALL,
                         true, false, next_sleep_ms);
    } else {
        gfx_platform_delay_ms(next_sleep_ms);
        *out_triggered = 0;
    }
}

static uint32_t gfx_cal_task_delay(uint32_t timer_delay)
{
    uint32_t min_delay_ms = gfx_platform_min_delay_ms();

    if (timer_delay == ANIM_NO_TIMER_READY) {
        return (min_delay_ms > 5) ? min_delay_ms : 5;
    } else {
        return (timer_delay < min_delay_ms) ? min_delay_ms : timer_delay;
    }
}

static void gfx_render_loop_task(void *arg)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)arg;
    gfx_platform_mutex_t mutex = ctx->sync.render_mutex;
    uint32_t next_sleep_ms = GFX_RENDER_TASK_IDLE_SLEEP_MS;

    /**
     * If this task runs too early, add_disp() can fire the first frame before the
     * caller / external code is ready, which can lead to a deadlock. Delay so the
     * rest of the system can finish setup first.
     */
    gfx_platform_delay_ms(GFX_RENDER_TASK_IDLE_SLEEP_MS);

    for (;;) {
        gfx_platform_event_bits_t evt_invalidate;
        gfx_wait_for_work(ctx, next_sleep_ms, &evt_invalidate);

        bool locked = gfx_platform_mutex_lock(mutex, GFX_PLATFORM_WAIT_FOREVER);
        if (!locked) {
            next_sleep_ms = 1;
            gfx_platform_delay_ms(1);
            continue;
        }

        (void)gfx_core_tick_locked(ctx, evt_invalidate != 0);
        next_sleep_ms = gfx_cal_task_delay(ctx->timer_mgr.time_until_next);
        gfx_platform_mutex_unlock(mutex);
        gfx_platform_delay_ms(1);
    }
}

static void gfx_do_refr_now_impl(gfx_core_context_t *ctx)
{
    if (ctx->disp != NULL) {
        gfx_render_handler(ctx);
    }
}

static gfx_err_t gfx_core_tick_locked(gfx_core_context_t *ctx, bool force_refresh)
{
    bool timer_refresh = false;
    bool tween_refresh = false;

    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)gfx_timer_handler(&ctx->timer_mgr, &timer_refresh);
    tween_refresh = gfx_tween_core_tick();
    if (force_refresh || timer_refresh || tween_refresh) {
        gfx_do_refr_now_impl(ctx);
    }

    return ESP_OK;
}

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

gfx_handle_t gfx_core_init(const gfx_core_config_t *cfg)
{
    esp_err_t ret = ESP_OK;
    gfx_core_context_t *disp_ctx = NULL;
    bool lifecycle_events_created = false;
    bool mutex_created = false;
    bool decoder_inited = false;
    bool font_lib_created = false;

    ESP_GOTO_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, err, TAG, "Invalid configuration");

    disp_ctx = malloc(sizeof(gfx_core_context_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate player context");

    memset(disp_ctx, 0, sizeof(gfx_core_context_t));
    disp_ctx->manual_tick = cfg->manual_tick;

    disp_ctx->sync.lifecycle_events = gfx_platform_event_create();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.lifecycle_events, ESP_ERR_NO_MEM, err, TAG, "Failed to create event group");
    lifecycle_events_created = true;

    disp_ctx->sync.render_events = gfx_platform_event_create();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.render_events, ESP_ERR_NO_MEM, err, TAG, "Failed to create render event group");

    disp_ctx->sync.render_mutex = gfx_platform_mutex_create_recursive();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.render_mutex, ESP_ERR_NO_MEM, err, TAG, "Failed to create recursive render mutex");
    mutex_created = true;

    ret = gfx_subsystem_font_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to create font library");
    font_lib_created = true;

    gfx_timer_mgr_init(&disp_ctx->timer_mgr, cfg->fps);

    ret = gfx_subsystem_image_decoder_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to initialize image decoder");
    decoder_inited = true;

    ret = gfx_subsystem_accel_init();
    if (ret != ESP_OK) {
        GFX_LOGW(TAG, "platform acceleration disabled (%d), falling back to software", ret);
    }

    if (!disp_ctx->manual_tick) {
        ret = gfx_platform_task_create(&(gfx_platform_task_config_t) {
            .name = "gfx_render",
            .stack_size = cfg->task.task_stack,
            .priority = cfg->task.task_priority,
            .affinity = cfg->task.task_affinity,
            .stack_caps = cfg->task.task_stack_caps,
        }, gfx_render_loop_task, disp_ctx, NULL);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to create render task");
    }

    return (gfx_handle_t)disp_ctx;

err:
    gfx_subsystem_accel_deinit();
    if (decoder_inited) {
        gfx_subsystem_image_decoder_deinit();
    }
    if (font_lib_created) {
        gfx_subsystem_font_deinit();
    }
    if (mutex_created) {
        gfx_platform_mutex_delete(disp_ctx->sync.render_mutex);
    }
    if (disp_ctx->sync.render_events) {
        gfx_platform_event_delete(disp_ctx->sync.render_events);
        disp_ctx->sync.render_events = NULL;
    }
    if (lifecycle_events_created) {
        gfx_platform_event_delete(disp_ctx->sync.lifecycle_events);
    }
    free(disp_ctx);
    return NULL;
}

void gfx_core_deinit(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL) {
        GFX_LOGE(TAG, "deinit graphics: context is NULL");
        return;
    }

    if (!ctx->manual_tick) {
        gfx_platform_event_set(ctx->sync.lifecycle_events, NEED_DELETE);
        gfx_platform_event_wait(ctx->sync.lifecycle_events, DELETE_DONE, true, false, GFX_PLATFORM_WAIT_FOREVER);
    }

    while (ctx->disp != NULL) {
        gfx_display_delete(ctx->disp);
    }

    gfx_tween_core_deinit();

    gfx_touch_delete_all(ctx);

    gfx_timer_mgr_deinit(&ctx->timer_mgr);

    gfx_subsystem_font_deinit();

    if (ctx->sync.render_mutex) {
        gfx_platform_mutex_delete(ctx->sync.render_mutex);
        ctx->sync.render_mutex = NULL;
    }

    if (ctx->sync.lifecycle_events) {
        gfx_platform_event_delete(ctx->sync.lifecycle_events);
        ctx->sync.lifecycle_events = NULL;
    }

    if (ctx->sync.render_events) {
        gfx_platform_event_delete(ctx->sync.render_events);
        ctx->sync.render_events = NULL;
    }

    gfx_subsystem_accel_deinit();
    gfx_subsystem_image_decoder_deinit();
    free(ctx);
}

gfx_err_t gfx_core_refresh_now(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_platform_mutex_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "refresh now: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!gfx_platform_mutex_lock(mutex, GFX_PLATFORM_WAIT_FOREVER)) {
        GFX_LOGE(TAG, "refresh now: acquire mutex failed");
        return ESP_ERR_TIMEOUT;
    }

    gfx_do_refr_now_impl(ctx);

    if (!gfx_platform_mutex_unlock(mutex)) {
        GFX_LOGE(TAG, "refresh now: release mutex failed");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

gfx_err_t gfx_core_tick(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_platform_mutex_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    gfx_platform_event_bits_t pending_events = 0;
    gfx_err_t ret;

    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "tick graphics: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->sync.lifecycle_events != NULL) {
        gfx_platform_event_bits_t life = gfx_platform_event_wait(ctx->sync.lifecycle_events, NEED_DELETE,
                                         false, false, 0);
        if (life & NEED_DELETE) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    if (ctx->sync.render_events != NULL) {
        pending_events = gfx_platform_event_wait(ctx->sync.render_events, GFX_EVENT_ALL,
                         true, false, 0);
    }

    if (!gfx_platform_mutex_lock(mutex, GFX_PLATFORM_WAIT_FOREVER)) {
        GFX_LOGE(TAG, "tick graphics: acquire mutex failed");
        return ESP_ERR_TIMEOUT;
    }

    ret = gfx_core_tick_locked(ctx, pending_events != 0);

    if (!gfx_platform_mutex_unlock(mutex)) {
        GFX_LOGE(TAG, "tick graphics: release mutex failed");
        return ESP_ERR_INVALID_STATE;
    }

    return ret;
}

gfx_err_t gfx_core_lock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_platform_mutex_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "lock graphics: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!gfx_platform_mutex_lock(mutex, GFX_PLATFORM_WAIT_FOREVER)) {
        GFX_LOGE(TAG, "lock graphics: acquire mutex failed");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

gfx_err_t gfx_core_unlock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_platform_mutex_t mutex = ctx ? ctx->sync.render_mutex : NULL;
    if (ctx == NULL || mutex == NULL) {
        GFX_LOGE(TAG, "unlock graphics: context or mutex is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!gfx_platform_mutex_unlock(mutex)) {
        GFX_LOGE(TAG, "unlock graphics: release mutex failed");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
