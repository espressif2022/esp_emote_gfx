/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      TYPEDEFS
 *********************/
struct gfx_core_context;

/*********************
 *   INTERNAL API
 *********************/
void gfx_touch_delete_all(struct gfx_core_context *ctx);

#ifdef __cplusplus
}
#endif
