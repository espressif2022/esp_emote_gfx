/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOGE(tag, format, ...) fprintf(stderr, "E/%s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) fprintf(stderr, "W/%s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) fprintf(stdout, "I/%s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) fprintf(stdout, "D/%s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) fprintf(stdout, "V/%s: " format "\n", tag, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
