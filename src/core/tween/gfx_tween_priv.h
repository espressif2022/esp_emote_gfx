/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gfx_tween_core_tick(void);
void gfx_tween_core_deinit(void);

#ifdef __cplusplus
}
#endif
