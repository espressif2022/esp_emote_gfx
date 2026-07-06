/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_APP_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#ifndef TEST_APP_MEM_MONITOR_PERIOD_MS
#define TEST_APP_MEM_MONITOR_PERIOD_MS 10000
#endif

void test_app_wait_ms(uint32_t delay_ms);
void test_app_wait_for_observe(uint32_t delay_ms);
void test_app_log_case(const char *tag, const char *case_name);
void test_app_log_step(const char *tag, const char *step_name);
void test_app_mem_log_snapshot(const char *tag, const char *label);
void test_app_mem_monitor_start(uint32_t period_ms);
void test_app_mem_monitor_stop(void);

#ifdef __cplusplus
}
#endif
