/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "unity.h"

#include "render/gfx_render_priv.h"
#include "test_render_roundup_vectors.h"

TEST_CASE("render: roundup area table", "[unit][render][roundup]")
{
    for (size_t i = 0; i < sizeof(s_gfx_test_roundup_area_cases) / sizeof(s_gfx_test_roundup_area_cases[0]); i++) {
        const gfx_test_roundup_area_case_t *c = &s_gfx_test_roundup_area_cases[i];
        gfx_area_t got = gfx_render_roundup_area(&c->area, &c->alignment, &c->limit);

        TEST_ASSERT_EQUAL_MESSAGE(c->expected.x1, got.x1, c->name);
        TEST_ASSERT_EQUAL_MESSAGE(c->expected.y1, got.y1, c->name);
        TEST_ASSERT_EQUAL_MESSAGE(c->expected.x2, got.x2, c->name);
        TEST_ASSERT_EQUAL_MESSAGE(c->expected.y2, got.y2, c->name);
    }
}

TEST_CASE("render: roundup stride table", "[unit][render][roundup]")
{
    for (size_t i = 0; i < sizeof(s_gfx_test_roundup_stride_cases) / sizeof(s_gfx_test_roundup_stride_cases[0]); i++) {
        const gfx_test_roundup_stride_case_t *c = &s_gfx_test_roundup_stride_cases[i];
        uint32_t got = gfx_render_roundup_stride_bytes(c->stride_bytes, &c->alignment);

        TEST_ASSERT_EQUAL_UINT32_MESSAGE(c->expected, got, c->name);
    }
}

TEST_CASE("render: addr alignment helper", "[unit][render][roundup]")
{
    uint8_t buf[256] __attribute__((aligned(64)));

    memset(buf, 0, sizeof(buf));
    gfx_render_alignment_t align = {
        .width_px = 1,
        .height_px = 1,
        .stride_bytes = 64,
        .addr_bytes = 64,
    };

    TEST_ASSERT_TRUE(gfx_render_is_addr_aligned(buf, &align));
    TEST_ASSERT_FALSE(gfx_render_is_addr_aligned(buf + 1, &align));
}
