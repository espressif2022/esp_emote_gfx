/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "gfx/types.h"

typedef struct gfx_display gfx_display_t;

uint8_t gfx_arena_scene_test_dirty_count(gfx_display_t *disp);
int gfx_arena_scene_test_dirty_area(gfx_display_t *disp, uint8_t index, gfx_area_t *out);
