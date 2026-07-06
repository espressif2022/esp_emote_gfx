/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/gfx_render_priv.h"
#include "../../test_apps/unit/main/test_render_roundup_vectors.h"

static void expect_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "roundup smoke failed: %s\n", message);
        exit(1);
    }
}

int main(void)
{
    for (size_t i = 0; i < sizeof(s_gfx_test_roundup_area_cases) / sizeof(s_gfx_test_roundup_area_cases[0]); i++) {
        const gfx_test_roundup_area_case_t *c = &s_gfx_test_roundup_area_cases[i];
        gfx_area_t got = gfx_render_roundup_area(&c->area, &c->alignment, &c->limit);

        expect_true(got.x1 == c->expected.x1, c->name);
        expect_true(got.y1 == c->expected.y1, c->name);
        expect_true(got.x2 == c->expected.x2, c->name);
        expect_true(got.y2 == c->expected.y2, c->name);
    }

    for (size_t i = 0; i < sizeof(s_gfx_test_roundup_stride_cases) / sizeof(s_gfx_test_roundup_stride_cases[0]); i++) {
        const gfx_test_roundup_stride_case_t *c = &s_gfx_test_roundup_stride_cases[i];
        uint32_t got = gfx_render_roundup_stride_bytes(c->stride_bytes, &c->alignment);

        expect_true(got == c->expected, c->name);
    }

    return 0;
}
