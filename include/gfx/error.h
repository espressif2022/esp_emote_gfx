/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int gfx_err_t;

#define GFX_OK                  0
#define GFX_FAIL               -1
#define GFX_ERR_NO_MEM          0x101
#define GFX_ERR_INVALID_ARG     0x102
#define GFX_ERR_INVALID_STATE   0x103
#define GFX_ERR_INVALID_SIZE    0x104
#define GFX_ERR_NOT_FOUND       0x105
#define GFX_ERR_NOT_SUPPORTED   0x106
#define GFX_ERR_TIMEOUT         0x107
#define GFX_ERR_INVALID_RESPONSE 0x108
#define GFX_ERR_INVALID_CRC     0x109

#ifdef __cplusplus
}
#endif
