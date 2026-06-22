/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "fonts/gfx_font_priv.h"
#include "gfx/widgets/label.h"

#ifdef __cplusplus
extern "C" {
#endif

bool gfx_host_font_is_builtin(const void *font);
gfx_font_t gfx_host_font_default(void);
void gfx_host_font_init_adapter(gfx_font_handle_t font_adapter, const void *font);

#ifdef __cplusplus
}
#endif
