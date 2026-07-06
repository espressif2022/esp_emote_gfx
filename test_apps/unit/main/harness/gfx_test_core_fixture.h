/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gfx_handle_t handle;
} gfx_test_core_ctx_t;

esp_err_t gfx_test_core_open(gfx_test_core_ctx_t *ctx);
void gfx_test_core_close(gfx_test_core_ctx_t *ctx);
esp_err_t gfx_test_core_lock(gfx_test_core_ctx_t *ctx);
void gfx_test_core_unlock(gfx_test_core_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
