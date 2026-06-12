/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL               -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_INVALID_SIZE    0x104
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_NOT_SUPPORTED   0x106
#define ESP_ERR_TIMEOUT         0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC     0x109

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#ifndef BIT0
#define BIT0 BIT(0)
#endif
#ifndef BIT1
#define BIT1 BIT(1)
#endif
#ifndef BIT2
#define BIT2 BIT(2)
#endif

#ifdef __cplusplus
}
#endif
