/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_arena_draw_demo — prove pixels come from arena without any gfx_object_t
 * =====================================================================
 *   pack arena → memcpy load → gfx_arena_draw() into memory-backend FB
 *
 * Explicitly does NOT call:
 *   gfx_core_init / gfx_display_add / gfx_*_create / gfx_arena_gfx_bind
 *
 *   cmake --build build-host-sdl --target gfx_arena_draw_demo
 *   ./build-host-sdl/gfx_arena_draw_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/backends/memory.h"
#include "gfx/scene/arena.h"
#include "gfx/scene/arena_draw.h"

#define SCREEN_W 320
#define SCREEN_H 240

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        fails++; \
    } else { \
        printf("OK  %s\n", msg); \
    } \
} while (0)

static uint16_t rgb888_to_rgb565(uint32_t rgb)
{
    const uint32_t r = (rgb >> 16) & 0xFFu;
    const uint32_t g = (rgb >> 8) & 0xFFu;
    const uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static uint16_t sample_px(const uint16_t *fb, int x, int y)
{
    return fb[(size_t)y * SCREEN_W + (size_t)x];
}

int main(void)
{
    int fails = 0;

    printf("arena_draw_demo: draw arena directly (no gfx_object_t)\n");

    const gfx_arena_desc_t descs[] = {
        {
            GFX_ARENA_NODE_CONTAINER, GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG, 0, 0, SCREEN_W, SCREEN_H,
            0x1c1f2e, "root", -1
        },
        /* label present in tree but must NOT be drawn by gfx_arena_draw */
        {
            GFX_ARENA_NODE_LABEL, GFX_ARENA_F_VISIBLE, 16, 16, 180, 28,
            0xffffff, "title", 0
        },
        {
            GFX_ARENA_NODE_BUTTON, GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG, 200, 180, 96, 40,
            0x2f8cff, "ok", 0
        },
    };

    size_t pkg_size = 0;
    uint8_t *pkg = gfx_arena_pack(descs, (uint16_t)(sizeof(descs) / sizeof(descs[0])), &pkg_size);
    CHECK(pkg != NULL, "pack");

    gfx_arena_t arena = {0};
    CHECK(gfx_arena_load(pkg, pkg_size, &arena) == 0, "load arena");

    /* Only use memory backend as an RGB565 framebuffer owner — no display/objects. */
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
    });
    CHECK(backend != NULL, "memory backend create");

    uint16_t *pixels = (uint16_t *)gfx_memory_backend_get_buffer_data(backend);
    CHECK(pixels != NULL, "framebuffer pointer");
    CHECK(gfx_memory_backend_get_buffer_pixels(backend) == (size_t)SCREEN_W * SCREEN_H,
          "framebuffer size");

    gfx_arena_fb_t fb = {
        .pixels = pixels,
        .width = SCREEN_W,
        .height = SCREEN_H,
        .clear_rgb565 = 0x0000,
    };
    CHECK(gfx_arena_draw(&arena, &fb) == 0, "arena_draw");

    const uint16_t root_c = rgb888_to_rgb565(0x1c1f2e);
    const uint16_t btn_c = rgb888_to_rgb565(0x2f8cff);

    CHECK(sample_px(pixels, 10, 10) == root_c, "root container bg pixel");
    CHECK(sample_px(pixels, 220, 190) == btn_c, "button bg pixel");
    /* Label area must still show parent/root bg — label is not drawn. */
    CHECK(sample_px(pixels, 20, 20) == root_c, "label region still root bg (label skipped)");

    /* In-place mutate arena, redraw, prove FB tracks arena (not object state). */
    gfx_arena_node_t *ok = gfx_arena_find_by_name(&arena, "ok");
    CHECK(ok != NULL, "find ok node");
    if (ok != NULL) {
        ok->bg_rgb = 0xff6644;
    }
    CHECK(gfx_arena_draw(&arena, &fb) == 0, "redraw after in-place mutate");
    CHECK(sample_px(pixels, 220, 190) == rgb888_to_rgb565(0xff6644),
          "button pixel follows arena mutate (no gfx_object_t)");

    printf("\nProof checklist:\n");
    printf("  [x] no gfx_core / gfx_display / gfx_*_create\n");
    printf("  [x] pixels filled by gfx_arena_draw from arena nodes\n");
    printf("  [x] label skipped\n");
    printf("  [x] in-place arena mutate reflected on next draw\n");

    gfx_memory_backend_delete(backend);
    gfx_arena_free(&arena);
    free(pkg);

    printf(fails == 0 ? "PASS\n" : "FAIL\n");
    return fails == 0 ? 0 : 1;
}
