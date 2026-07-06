/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/display/gfx_display_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

void gfx_copy_unrendered_areas(const gfx_display_t *disp,
                               const void *src_fb,
                               void *dst_fb,
                               uint32_t hor_res,
                               uint32_t ver_res,
                               size_t pixel_size);

#ifdef __cplusplus
}
#endif
