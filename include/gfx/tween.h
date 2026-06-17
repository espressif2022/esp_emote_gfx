/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/base.h"
#include "gfx/object.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_tween gfx_tween_t;

typedef enum {
    GFX_TWEEN_EASE_LINEAR = 0,
    GFX_TWEEN_EASE_OUT_QUAD,
    GFX_TWEEN_EASE_OUT_CUBIC,
    GFX_TWEEN_EASE_IN_OUT_CUBIC,
} gfx_tween_ease_t;

typedef void (*gfx_tween_i32_cb_t)(gfx_tween_t *tween, gfx_object_t *obj, int32_t value, void *user_data);
typedef void (*gfx_tween_done_cb_t)(gfx_tween_t *tween, gfx_object_t *obj, void *user_data);

gfx_tween_t *gfx_tween_create(gfx_object_t *obj);
void gfx_tween_delete(gfx_tween_t *tween);

gfx_err_t gfx_tween_start_i32(gfx_tween_t *tween,
                              int32_t from,
                              int32_t to,
                              uint32_t duration_ms,
                              gfx_tween_ease_t ease,
                              gfx_tween_i32_cb_t value_cb,
                              gfx_tween_done_cb_t done_cb,
                              void *user_data);
void gfx_tween_stop(gfx_tween_t *tween, bool complete);
bool gfx_tween_is_active(const gfx_tween_t *tween);

#ifdef __cplusplus
}
#endif
