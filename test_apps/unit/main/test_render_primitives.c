/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "gfx.h"
#include "unity.h"

#include "render/gfx_render_priv.h"

#define TEST_SURFACE_W 48
#define TEST_SURFACE_H 32
#define TEST_BENCH_ITERS 1000

static uint16_t s_surface[TEST_SURFACE_W * TEST_SURFACE_H];

static gfx_render_surface_t test_surface_make(void)
{
    return (gfx_render_surface_t) {
        .buf = s_surface,
        .buf_area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = TEST_SURFACE_W,
            .y2 = TEST_SURFACE_H,
        },
        .clip_area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = TEST_SURFACE_W,
            .y2 = TEST_SURFACE_H,
        },
        .stride = TEST_SURFACE_W,
        .format = GFX_COLOR_FORMAT_RGB565,
    };
}

static uint16_t test_pixel(gfx_coord_t x, gfx_coord_t y)
{
    return s_surface[(size_t)y * TEST_SURFACE_W + (size_t)x];
}

TEST_CASE("render: round rect fill covers center and clips corners", "[unit][render][round_rect]")
{
    gfx_render_surface_t surface = test_surface_make();
    gfx_area_t area = {
        .x1 = 4,
        .y1 = 3,
        .x2 = 28,
        .y2 = 19,
    };
    gfx_color_t color = GFX_COLOR_HEX(0xF81C1C);
    gfx_round_rect_fill_dsc_t dsc = {
        .color = color,
        .opa = 0xFFU,
        .radius = 6,
    };

    memset(s_surface, 0, sizeof(s_surface));
    gfx_render_surface_round_rect_fill(NULL, &surface, &area, &dsc);

    TEST_ASSERT_EQUAL_HEX16(0, test_pixel(area.x1, area.y1));
    TEST_ASSERT_EQUAL_HEX16(0, test_pixel((gfx_coord_t)(area.x2 - 1), area.y1));
    TEST_ASSERT_EQUAL_HEX16(0, test_pixel(area.x1, (gfx_coord_t)(area.y2 - 1)));
    TEST_ASSERT_EQUAL_HEX16(0, test_pixel((gfx_coord_t)(area.x2 - 1), (gfx_coord_t)(area.y2 - 1)));

    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 12), (gfx_coord_t)(area.y1 + 8)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 6), area.y1));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel(area.x1, (gfx_coord_t)(area.y1 + 6)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x2 - 1), (gfx_coord_t)(area.y1 + 6)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 6), (gfx_coord_t)(area.y2 - 1)));
}

TEST_CASE("render: round rect stroke keeps inner area clear", "[unit][render][round_rect]")
{
    gfx_render_surface_t surface = test_surface_make();
    gfx_area_t area = {
        .x1 = 4,
        .y1 = 3,
        .x2 = 28,
        .y2 = 19,
    };
    gfx_color_t color = GFX_COLOR_HEX(0x22CC44);
    gfx_round_rect_stroke_dsc_t dsc = {
        .color = color,
        .opa = 0xFFU,
        .radius = 6,
        .width = 2,
    };

    memset(s_surface, 0, sizeof(s_surface));
    gfx_render_surface_round_rect_stroke(NULL, &surface, &area, &dsc);

    TEST_ASSERT_EQUAL_HEX16(0, test_pixel(area.x1, area.y1));
    TEST_ASSERT_EQUAL_HEX16(0, test_pixel((gfx_coord_t)(area.x1 + 12), (gfx_coord_t)(area.y1 + 8)));

    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 12), area.y1));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 12), (gfx_coord_t)(area.y1 + 1)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel(area.x1, (gfx_coord_t)(area.y1 + 8)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x2 - 1), (gfx_coord_t)(area.y1 + 8)));
    TEST_ASSERT_EQUAL_HEX16(color.full, test_pixel((gfx_coord_t)(area.x1 + 12), (gfx_coord_t)(area.y2 - 1)));
}

TEST_CASE("render: round rect fill benchmark", "[bench][render][round_rect]")
{
    gfx_render_surface_t surface = test_surface_make();
    gfx_area_t area = {
        .x1 = 2,
        .y1 = 2,
        .x2 = 42,
        .y2 = 26,
    };
    gfx_round_rect_fill_dsc_t dsc = {
        .color = GFX_COLOR_HEX(0x3366CC),
        .opa = 0xFFU,
        .radius = 10,
    };
    int64_t t0;
    int64_t elapsed;

    memset(s_surface, 0, sizeof(s_surface));

    t0 = esp_timer_get_time();
    for (int i = 0; i < TEST_BENCH_ITERS; i++) {
        gfx_render_surface_round_rect_fill(NULL, &surface, &area, &dsc);
    }
    elapsed = esp_timer_get_time() - t0;

    printf("round_rect_fill %dx%d r%u: total=%lld us avg=%lld us iters=%d\n",
           area.x2 - area.x1, area.y2 - area.y1, dsc.radius,
           (long long)elapsed, (long long)(elapsed / TEST_BENCH_ITERS), TEST_BENCH_ITERS);

    TEST_ASSERT_GREATER_THAN_INT64(0, elapsed);
}
