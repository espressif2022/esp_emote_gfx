/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "gfx_test_util.h"

static const char *const TAG = "gfx_test_util";
static TaskHandle_t s_mem_mon_task = NULL;

void test_app_wait_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

void test_app_wait_for_observe(uint32_t delay_ms)
{
    if (delay_ms > 0) {
        test_app_wait_ms(delay_ms);
    }
}

void test_app_log_case(const char *tag, const char *case_name)
{
    ESP_LOGI(tag, "=== %s ===", case_name ? case_name : "case");
}

void test_app_log_step(const char *tag, const char *step_name)
{
    ESP_LOGI(tag, "--- %s ---", step_name ? step_name : "step");
}

void test_app_mem_log_snapshot(const char *tag, const char *label)
{
    (void)tag;
    const char *lbl = label ? label : "?";
    const uint32_t cap_internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t cap_spiram   = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

    printf("[%s]\n", lbl);
    printf("\tDescription\tInternal\tSPIRAM\n");
    printf("Current Free Memory\t%zu\t\t%zu\n",
           heap_caps_get_free_size(cap_internal),
           heap_caps_get_free_size(cap_spiram));
    printf("Largest Free Block\t%zu\t\t%zu\n",
           heap_caps_get_largest_free_block(cap_internal),
           heap_caps_get_largest_free_block(cap_spiram));
    printf("Min. Ever Free Size\t%zu\t\t%zu\n",
           heap_caps_get_minimum_free_size(cap_internal),
           heap_caps_get_minimum_free_size(cap_spiram));
}

#if CONFIG_FREERTOS_USE_TRACE_FACILITY
static void test_app_mem_log_task_table(void)
{
    char *buf = (char *)malloc(3072);

    if (buf == NULL) {
        ESP_LOGW(TAG, "task list: malloc failed");
        return;
    }

    memset(buf, 0, 3072);
    vTaskList(buf);
    ESP_LOGI(TAG, "Task list (Name / State / Prio / Stack / Num):\n%s", buf);
    free(buf);
}
#else
static void test_app_mem_log_task_table(void)
{
}
#endif

static void test_app_mem_monitor_task(void *arg)
{
    uint32_t period_ms = *(uint32_t *)arg;

    if (period_ms == 0U) {
        s_mem_mon_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "mem_mon task started (period %" PRIu32 " ms)", period_ms);

    while (true) {
        test_app_mem_log_snapshot(TAG, "monitor");
        test_app_mem_log_task_table();
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

void test_app_mem_monitor_stop(void)
{
    if (s_mem_mon_task != NULL) {
        vTaskDelete(s_mem_mon_task);
        s_mem_mon_task = NULL;
    }
}

void test_app_mem_monitor_start(uint32_t period_ms)
{
    static uint32_t stored_period_ms;

    test_app_mem_monitor_stop();

    if (period_ms == 0U) {
        return;
    }

    stored_period_ms = period_ms;
    if (xTaskCreate(test_app_mem_monitor_task, "test_mem_mon", 4096,
                    &stored_period_ms, tskIDLE_PRIORITY + 1, &s_mem_mon_task) != pdPASS) {
        s_mem_mon_task = NULL;
        ESP_LOGW(TAG, "mem_mon task create failed");
    }
}
