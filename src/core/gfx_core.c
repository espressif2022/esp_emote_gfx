/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"

#include "core/gfx_obj.h"
#include "core/gfx_obj_priv.h"
#include "core/gfx_refr_priv.h"
#include "core/gfx_render_priv.h"
#include "core/gfx_timer_priv.h"
#include "core/gfx_touch_priv.h"

#include "widget/gfx_font_priv.h"
#include "decoder/gfx_img_dec_priv.h"

static const char *TAG = "gfx_core";

static void gfx_render_loop_task(void *arg);
static bool gfx_event_handler(gfx_core_context_t *ctx);
static uint32_t gfx_cal_task_delay(uint32_t timer_delay);
/* ============================================================================
 * Initialization and Cleanup Functions
 * ============================================================================ */

gfx_handle_t gfx_emote_init(const gfx_core_config_t *cfg)
{
    esp_err_t ret = ESP_OK;
    gfx_core_context_t *disp_ctx = NULL;
    bool lifecycle_events_created = false;
    bool mutex_created = false;
    bool decoder_inited = false;
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    bool font_lib_created = false;
#endif

    ESP_GOTO_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, err, TAG, "Invalid configuration");

    disp_ctx = malloc(sizeof(gfx_core_context_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate player context");

    // Initialize all fields to zero/NULL
    memset(disp_ctx, 0, sizeof(gfx_core_context_t));

    disp_ctx->sync.lifecycle_events = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.lifecycle_events, ESP_ERR_NO_MEM, err, TAG, "Failed to create event group");
    lifecycle_events_created = true;

    // Create recursive render mutex for protecting rendering operations
    disp_ctx->sync.render_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(disp_ctx->sync.render_mutex, ESP_ERR_NO_MEM, err, TAG, "Failed to create recursive render mutex");
    mutex_created = true;

#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    ret = gfx_ft_lib_create();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to create font library");
    font_lib_created = true;
#endif

    // Initialize timer manager
    gfx_timer_mgr_init(&disp_ctx->timer_mgr, cfg->fps);

    // Initialize image decoder system
    ret = gfx_image_decoder_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to initialize image decoder");
    decoder_inited = true;

    // Create render task
    const uint32_t stack_caps = cfg->task.task_stack_caps ? cfg->task.task_stack_caps : (MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);
    if (cfg->task.task_affinity < 0) {
        xTaskCreateWithCaps(gfx_render_loop_task, "gfx_render", cfg->task.task_stack,
                            disp_ctx, cfg->task.task_priority, NULL, stack_caps);
    } else {
        xTaskCreatePinnedToCoreWithCaps(gfx_render_loop_task, "gfx_render", cfg->task.task_stack,
                                        disp_ctx, cfg->task.task_priority, NULL, cfg->task.task_affinity, stack_caps);
    }

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
        ESP_LOGE(TAG, "Invalid graphics context");
        return;
    }

    xEventGroupSetBits(ctx->sync.lifecycle_events, NEED_DELETE);
    xEventGroupWaitBits(ctx->sync.lifecycle_events, DELETE_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

    // Free all displays and their child nodes
    while (ctx->disp != NULL) {
        gfx_disp_t *d = ctx->disp;
        gfx_disp_del(d);
        free(d);
    }

    // Free all touch nodes
    while (ctx->touch != NULL) {
        gfx_touch_t *t = ctx->touch;
        gfx_touch_del(t);
        free(t);
    }

    gfx_timer_mgr_deinit(&ctx->timer_mgr);

    // Delete font library
#ifdef CONFIG_GFX_FONT_FREETYPE_SUPPORT
    gfx_ft_lib_cleanup();
#endif

    // Delete mutex
    if (ctx->sync.render_mutex) {
        vSemaphoreDelete(ctx->sync.render_mutex);
        ctx->sync.render_mutex = NULL;
    }

    // Delete event group
    if (ctx->sync.lifecycle_events) {
        vEventGroupDelete(ctx->sync.lifecycle_events);
        ctx->sync.lifecycle_events = NULL;
    }

    // Deinitialize image decoder system
    gfx_image_decoder_deinit();

    // Free context
    free(ctx);
}

/* ============================================================================
 * Task and Event Handling Functions
 * ============================================================================ */

static uint32_t gfx_cal_task_delay(uint32_t timer_delay)
{
    uint32_t min_delay_ms = (1000 / configTICK_RATE_HZ) + 1; // At least one tick + 1ms

    if (timer_delay == ANIM_NO_TIMER_READY) {
        return (min_delay_ms > 5) ? min_delay_ms : 5;
    } else {
        return (timer_delay < min_delay_ms) ? min_delay_ms : timer_delay;
    }
}

static bool gfx_event_handler(gfx_core_context_t *ctx)
{
    EventBits_t event_bits = xEventGroupWaitBits(ctx->sync.lifecycle_events,
                             NEED_DELETE, pdTRUE, pdFALSE, pdMS_TO_TICKS(0));

    if (event_bits & NEED_DELETE) {
        xEventGroupSetBits(ctx->sync.lifecycle_events, DELETE_DONE);
        vTaskDeleteWithCaps(NULL);
        return true;
    }

    return false;
}

static void gfx_render_loop_task(void *arg)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)arg;
    uint32_t timer_delay = 1; // Default delay

    while (1) {
        if (ctx->sync.render_mutex && xSemaphoreTakeRecursive(ctx->sync.render_mutex, portMAX_DELAY) == pdTRUE) {
            if (gfx_event_handler(ctx)) {
                xSemaphoreGiveRecursive(ctx->sync.render_mutex);
                break;
            }

            timer_delay = gfx_timer_handler(&ctx->timer_mgr);

            // Only render when FPS period has elapsed (controlled by timer_mgr->should_render)
            if (ctx->timer_mgr.should_render && ctx->disp != NULL) {
                gfx_render_handler(ctx);
            }

            uint32_t task_delay = gfx_cal_task_delay(timer_delay);

            xSemaphoreGiveRecursive(ctx->sync.render_mutex);
            vTaskDelay(pdMS_TO_TICKS(task_delay));
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

/* ============================================================================
 * Synchronization and Locking Functions
 * ============================================================================ */

esp_err_t gfx_emote_lock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || ctx->sync.render_mutex == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context or mutex");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(ctx->sync.render_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire graphics lock");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t gfx_emote_unlock(gfx_handle_t handle)
{
    gfx_core_context_t *ctx = (gfx_core_context_t *)handle;
    if (ctx == NULL || ctx->sync.render_mutex == NULL) {
        ESP_LOGE(TAG, "Invalid graphics context or mutex");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreGiveRecursive(ctx->sync.render_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to release graphics lock");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
