/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#define GFX_LOG_MODULE GFX_LOG_MODULE_TIMER
#include "common/gfx_log_priv.h"

#include "core/runtime/gfx_core_priv.h"

/*********************
 *      DEFINES
 *********************/

#define GFX_NO_TIMER_READY 0xFFFFFFFF

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "timer";

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   PUBLIC FUNCTIONS
 **********************/

uint32_t gfx_timer_tick_get(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000); // Convert microseconds to milliseconds
}

uint32_t gfx_timer_tick_elaps(uint32_t prev_tick)
{
    uint32_t act_time = gfx_timer_tick_get();

    /*If there is no overflow in sys_time simple subtract*/
    if (act_time >= prev_tick) {
        prev_tick = act_time - prev_tick;
    } else {
        prev_tick = UINT32_MAX - prev_tick + 1;
        prev_tick += act_time;
    }

    return prev_tick;
}

bool gfx_timer_exec(gfx_timer_t *timer)
{
    if (timer == NULL || timer->paused) {
        GFX_LOGD(TAG, "run timer callback: timer is NULL or paused");
        return false;
    }

    // Don't execute if repeat_count is 0 (timer completed)
    if (timer->repeat_count == 0) {
        return false;
    }

    uint32_t time_elapsed = gfx_timer_tick_elaps(timer->last_run);

    if (time_elapsed >= timer->period) {
        timer->last_run = gfx_timer_tick_get() - (time_elapsed % timer->period);

        if (timer->timer_cb) {
            timer->timer_cb(timer->user_data);
        }

        if (timer->repeat_count > 0) {
            timer->repeat_count--;
        }

        return true;
    }

    return false;
}

uint32_t gfx_timer_handler(gfx_timer_mgr_t *timer_mgr, bool *out_should_render)
{
    /* Step 1: execute timers and find the minimum remaining time */
    uint32_t min_timer_remaining_ms = GFX_NO_TIMER_READY;
    gfx_timer_t *timer_node = timer_mgr->timer_list;
    gfx_timer_t *next_timer = NULL;

    while (timer_node != NULL) {
        next_timer = timer_node->next;

        gfx_timer_exec(timer_node);

        if (!timer_node->paused && timer_node->repeat_count != 0) {
            uint32_t timer_elapsed_ms = gfx_timer_tick_elaps(timer_node->last_run);
            uint32_t timer_remaining_ms = (timer_elapsed_ms >= timer_node->period)
                                          ? 0
                                          : (timer_node->period - timer_elapsed_ms);

            if (timer_remaining_ms < min_timer_remaining_ms) {
                min_timer_remaining_ms = timer_remaining_ms;
            }
        }

        timer_node = next_timer;
    }

    /* Step 2: update FPS period and render due state */
    uint32_t fps_period_ms = (timer_mgr->fps > 0) ? (1000 / timer_mgr->fps) : 30;
    uint32_t fps_elapsed_ms;
    uint32_t fps_remaining_ms;
    bool render_due;

    if (timer_mgr->last_tick == 0) {
        render_due = true;
        timer_mgr->last_tick = gfx_timer_tick_get();
        fps_remaining_ms = fps_period_ms;
    } else {
        fps_elapsed_ms = gfx_timer_tick_elaps(timer_mgr->last_tick);
        fps_remaining_ms = (fps_elapsed_ms >= fps_period_ms) ? 0 : (fps_period_ms - fps_elapsed_ms);
        render_due = (fps_remaining_ms == 0);
        if (render_due) {
            timer_mgr->last_tick = gfx_timer_tick_get();
        }
    }

    /* Step 3: calculate the next task delay */
    uint32_t task_delay_ms;
    if (min_timer_remaining_ms == GFX_NO_TIMER_READY) {
        task_delay_ms = (fps_remaining_ms > 0) ? fps_remaining_ms : 1;
    } else {
        task_delay_ms = (min_timer_remaining_ms < fps_remaining_ms)
                        ? min_timer_remaining_ms
                        : fps_remaining_ms;
        if (task_delay_ms == 0) {
            task_delay_ms = 1;
        }
    }

    timer_mgr->time_until_next = task_delay_ms;

    if (out_should_render != NULL) {
        *out_should_render = render_due;
    }

    return task_delay_ms;
}

gfx_timer_handle_t gfx_timer_create(void *handle, gfx_timer_cb_t timer_cb, uint32_t period, void *user_data)
{
    if (handle == NULL || timer_cb == NULL) {
        return NULL;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_timer_mgr_t *timer_mgr = &ctx->timer_mgr;

    gfx_timer_t *new_timer = (gfx_timer_t *)malloc(sizeof(gfx_timer_t));
    if (new_timer == NULL) {
        GFX_LOGE(TAG, "create timer: allocate timer failed");
        return NULL;
    }

    new_timer->period = period;
    new_timer->timer_cb = timer_cb;
    new_timer->user_data = user_data;
    new_timer->repeat_count = -1;
    new_timer->paused = false;
    new_timer->last_run = gfx_timer_tick_get();
    new_timer->next = NULL;

    if (timer_mgr->timer_list == NULL) {
        timer_mgr->timer_list = new_timer;
    } else {
        gfx_timer_t *current_timer = timer_mgr->timer_list;
        while (current_timer->next != NULL) {
            current_timer = current_timer->next;
        }
        current_timer->next = new_timer;
    }

    return (gfx_timer_handle_t)new_timer;
}

void gfx_timer_delete(void *handle, gfx_timer_handle_t timer_handle)
{
    if (handle == NULL || timer_handle == NULL) {
        return;
    }

    gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_timer_mgr_t *timer_mgr = &ctx->timer_mgr;

    gfx_timer_t *current_timer = timer_mgr->timer_list;
    gfx_timer_t *prev_timer = NULL;

    while (current_timer != NULL && current_timer != timer) {
        prev_timer = current_timer;
        current_timer = current_timer->next;
    }

    if (current_timer == timer) {
        if (prev_timer == NULL) {
            timer_mgr->timer_list = timer->next;
        } else {
            prev_timer->next = timer->next;
        }

        free(timer);
        GFX_LOGD(TAG, "delete timer: timer deleted");
    }
}

void gfx_timer_pause(gfx_timer_handle_t timer_handle)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        timer->paused = true;
    }
}

void gfx_timer_resume(gfx_timer_handle_t timer_handle)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        timer->paused = false;
        timer->last_run = gfx_timer_tick_get();

        if (timer->repeat_count == 0) {
            timer->repeat_count = -1;
        }
    }
}

void gfx_timer_set_repeat_count(gfx_timer_handle_t timer_handle, int32_t repeat_count)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        timer->repeat_count = repeat_count;
    }
}

void gfx_timer_set_period(gfx_timer_handle_t timer_handle, uint32_t period)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        timer->period = period;
    }
}

void gfx_timer_reset(gfx_timer_handle_t timer_handle)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        timer->last_run = gfx_timer_tick_get();
    }
}

bool gfx_timer_is_running(gfx_timer_handle_t timer_handle)
{
    if (timer_handle != NULL) {
        gfx_timer_t *timer = (gfx_timer_t *)timer_handle;
        return !timer->paused;
    }
    return false;
}

void gfx_timer_mgr_init(gfx_timer_mgr_t *timer_mgr, uint32_t fps)
{
    if (timer_mgr != NULL) {
        timer_mgr->timer_list = NULL;
        timer_mgr->time_until_next = GFX_NO_TIMER_READY;
        timer_mgr->last_tick = 0;
        timer_mgr->fps = fps;
        timer_mgr->actual_fps = 0;
        GFX_LOGI(TAG, "init timer manager: fps=%"PRIu32" period=%"PRIu32" ms", fps, (fps > 0) ? (1000 / fps) : 30);
    }
}

void gfx_timer_mgr_deinit(gfx_timer_mgr_t *timer_mgr)
{
    if (timer_mgr == NULL) {
        return;
    }

    gfx_timer_t *timer_node = timer_mgr->timer_list;
    while (timer_node != NULL) {
        gfx_timer_t *next_timer = timer_node->next;
        free(timer_node);
        timer_node = next_timer;
    }

    timer_mgr->timer_list = NULL;
}

uint32_t gfx_timer_get_actual_fps(void *handle)
{
    if (handle == NULL) {
        return 0;
    }

    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    gfx_timer_mgr_t *timer_mgr = &ctx->timer_mgr;

    return timer_mgr->actual_fps;
}
