/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "platform/gfx_platform.h"

static TickType_t gfx_platform_ms_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == GFX_PLATFORM_WAIT_FOREVER) {
        return portMAX_DELAY;
    }

    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    return ticks > 0 ? ticks : 1;
}

static uint32_t gfx_platform_caps_to_idf(uint32_t caps)
{
    uint32_t idf_caps = 0;

    if (caps & GFX_PLATFORM_HEAP_DMA) {
        idf_caps |= MALLOC_CAP_DMA;
    }
    if (caps & GFX_PLATFORM_HEAP_SPIRAM) {
        idf_caps |= MALLOC_CAP_SPIRAM;
    }
    if (caps & GFX_PLATFORM_HEAP_INTERNAL) {
        idf_caps |= MALLOC_CAP_INTERNAL;
    }
    if (caps & GFX_PLATFORM_HEAP_8BIT) {
        idf_caps |= MALLOC_CAP_8BIT;
    }
    if (idf_caps == 0 || (caps & GFX_PLATFORM_HEAP_DEFAULT)) {
        idf_caps |= MALLOC_CAP_DEFAULT;
    }

    return idf_caps;
}

gfx_platform_event_t gfx_platform_event_create(void)
{
    return (gfx_platform_event_t)xEventGroupCreate();
}

void gfx_platform_event_delete(gfx_platform_event_t event)
{
    if (event != NULL) {
        vEventGroupDelete((EventGroupHandle_t)event);
    }
}

gfx_platform_event_bits_t gfx_platform_event_set(gfx_platform_event_t event, gfx_platform_event_bits_t bits)
{
    if (event == NULL) {
        return 0;
    }
    return (gfx_platform_event_bits_t)xEventGroupSetBits((EventGroupHandle_t)event, (EventBits_t)bits);
}

gfx_platform_event_bits_t gfx_platform_event_set_from_isr(gfx_platform_event_t event, gfx_platform_event_bits_t bits,
        bool *need_yield)
{
    if (event == NULL) {
        return 0;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t ok = xEventGroupSetBitsFromISR((EventGroupHandle_t)event, (EventBits_t)bits,
                    &higher_priority_task_woken);
    if (need_yield != NULL) {
        *need_yield = higher_priority_task_woken == pdTRUE;
    }
    return ok == pdTRUE ? bits : 0;
}

gfx_platform_event_bits_t gfx_platform_event_clear(gfx_platform_event_t event, gfx_platform_event_bits_t bits)
{
    if (event == NULL) {
        return 0;
    }
    return (gfx_platform_event_bits_t)xEventGroupClearBits((EventGroupHandle_t)event, (EventBits_t)bits);
}

gfx_platform_event_bits_t gfx_platform_event_wait(gfx_platform_event_t event, gfx_platform_event_bits_t bits,
        bool clear_on_exit, bool wait_all, uint32_t timeout_ms)
{
    if (event == NULL) {
        return 0;
    }

    return (gfx_platform_event_bits_t)xEventGroupWaitBits((EventGroupHandle_t)event,
            (EventBits_t)bits,
            clear_on_exit ? pdTRUE : pdFALSE,
            wait_all ? pdTRUE : pdFALSE,
            gfx_platform_ms_to_ticks(timeout_ms));
}

void gfx_platform_yield_from_isr(void)
{
    portYIELD_FROM_ISR();
}

bool gfx_platform_in_isr(void)
{
    return xPortInIsrContext();
}

gfx_platform_mutex_t gfx_platform_mutex_create_recursive(void)
{
    return (gfx_platform_mutex_t)xSemaphoreCreateRecursiveMutex();
}

void gfx_platform_mutex_delete(gfx_platform_mutex_t mutex)
{
    if (mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}

bool gfx_platform_mutex_lock(gfx_platform_mutex_t mutex, uint32_t timeout_ms)
{
    if (mutex == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive((SemaphoreHandle_t)mutex, gfx_platform_ms_to_ticks(timeout_ms)) == pdTRUE;
}

bool gfx_platform_mutex_unlock(gfx_platform_mutex_t mutex)
{
    if (mutex == NULL) {
        return false;
    }
    return xSemaphoreGiveRecursive((SemaphoreHandle_t)mutex) == pdTRUE;
}

esp_err_t gfx_platform_task_create(const gfx_platform_task_config_t *cfg, gfx_platform_task_fn_t fn,
                                   void *arg, gfx_platform_task_t *out_task)
{
    if (cfg == NULL || fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    TaskHandle_t task = NULL;
    uint32_t stack_caps = gfx_platform_caps_to_idf(cfg->stack_caps != 0 ? cfg->stack_caps : GFX_PLATFORM_HEAP_INTERNAL);
    BaseType_t ret;

    if (cfg->affinity < 0) {
        ret = xTaskCreateWithCaps(fn, cfg->name, cfg->stack_size, arg, cfg->priority, &task, stack_caps);
    } else {
        ret = xTaskCreatePinnedToCoreWithCaps(fn, cfg->name, cfg->stack_size, arg, cfg->priority, &task,
                                              cfg->affinity, stack_caps);
    }

    if (ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    if (out_task != NULL) {
        *out_task = (gfx_platform_task_t)task;
    }
    return ESP_OK;
}

void gfx_platform_task_delete_current(void)
{
    vTaskDeleteWithCaps(NULL);
}

void gfx_platform_delay_ms(uint32_t ms)
{
    vTaskDelay(gfx_platform_ms_to_ticks(ms));
}

uint32_t gfx_platform_min_delay_ms(void)
{
    return (1000U / configTICK_RATE_HZ) + 1U;
}

void *gfx_platform_malloc(size_t size, uint32_t caps)
{
    return heap_caps_malloc(size, gfx_platform_caps_to_idf(caps));
}

void *gfx_platform_calloc(size_t count, size_t size, uint32_t caps)
{
    return heap_caps_calloc(count, size, gfx_platform_caps_to_idf(caps));
}

void *gfx_platform_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
    return heap_caps_aligned_alloc(alignment, size, gfx_platform_caps_to_idf(caps));
}

void gfx_platform_free(void *ptr)
{
    heap_caps_free(ptr);
}

int64_t gfx_platform_time_us(void)
{
    return esp_timer_get_time();
}
