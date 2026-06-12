/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "unity.h"
#include "gfx.h"

TEST_CASE("backend: memory framebuffer lifecycle", "[backend]")
{
    uint16_t ext_buf[4] = {0};
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 2,
        .v_res = 2,
        .buffer = ext_buf,
        .buffer_pixels = 4,
    });
    TEST_ASSERT_NOT_NULL(backend);

    TEST_ASSERT_EQUAL(4, gfx_memory_backend_get_buffer_pixels(backend));
    TEST_ASSERT_EQUAL_PTR(ext_buf, gfx_memory_backend_get_buffer(backend));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_memory_backend_clear(backend, GFX_COLOR_HEX(0x00ff00)));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, ext_buf[0]);
    TEST_ASSERT_EQUAL_HEX16(0x07e0, ext_buf[3]);

    gfx_memory_backend_delete(backend);
}
