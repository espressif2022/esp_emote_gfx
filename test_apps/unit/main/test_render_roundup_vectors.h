/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>

#include "common/gfx_types_priv.h"
#include "core/display/gfx_backend_priv.h"

typedef struct {
    const char *name;
    gfx_area_t area;
    gfx_render_alignment_t alignment;
    gfx_area_t limit;
    gfx_area_t expected;
} gfx_test_roundup_area_case_t;

typedef struct {
    const char *name;
    uint32_t stride_bytes;
    gfx_render_alignment_t alignment;
    uint32_t expected;
} gfx_test_roundup_stride_case_t;

static const gfx_test_roundup_area_case_t s_gfx_test_roundup_area_cases[] = {
    {
        .name = "identity",
        .area = { .x1 = 2, .y1 = 3, .x2 = 10, .y2 = 11 },
        .alignment = { .width_px = 1, .height_px = 1, .stride_bytes = 1, .addr_bytes = 1 },
        .limit = { .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 },
        .expected = { .x1 = 2, .y1 = 3, .x2 = 10, .y2 = 11 },
    },
    {
        .name = "round width outward",
        .area = { .x1 = 1, .y1 = 0, .x2 = 9, .y2 = 8 },
        .alignment = { .width_px = 4, .height_px = 1, .stride_bytes = 1, .addr_bytes = 1 },
        .limit = { .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 },
        .expected = { .x1 = 0, .y1 = 0, .x2 = 12, .y2 = 8 },
    },
    {
        .name = "clip to limit",
        .area = { .x1 = 28, .y1 = 0, .x2 = 34, .y2 = 4 },
        .alignment = { .width_px = 4, .height_px = 1, .stride_bytes = 1, .addr_bytes = 1 },
        .limit = { .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 },
        .expected = { .x1 = 28, .y1 = 0, .x2 = 32, .y2 = 4 },
    },
};

static const gfx_test_roundup_stride_case_t s_gfx_test_roundup_stride_cases[] = {
    {
        .name = "identity",
        .stride_bytes = 16,
        .alignment = { .width_px = 1, .height_px = 1, .stride_bytes = 1, .addr_bytes = 1 },
        .expected = 16,
    },
    {
        .name = "round to 64",
        .stride_bytes = 100,
        .alignment = { .width_px = 1, .height_px = 1, .stride_bytes = 64, .addr_bytes = 64 },
        .expected = 128,
    },
    {
        .name = "already aligned",
        .stride_bytes = 128,
        .alignment = { .width_px = 1, .height_px = 1, .stride_bytes = 64, .addr_bytes = 64 },
        .expected = 128,
    },
};
