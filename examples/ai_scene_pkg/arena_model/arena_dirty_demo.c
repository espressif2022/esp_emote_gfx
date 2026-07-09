/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * arena_dirty_demo — Phase2/3/4 closed-loop test (keep as CI interface)
 * =====================================================================
 *   pack → load → arena_scene_attach + set_font
 *   → mark_dirty(node)  (must NOT fullscreen by default)
 *   → gfx_core_refresh_now (render arena: bg + label/button text)
 *   → touch inject → action callback
 *
 *   cmake --build build-host-sdl --target gfx_arena_dirty_demo
 *   ./build-host-sdl/gfx_arena_dirty_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/scene/arena.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/widgets/font_lvgl.h"

extern const lv_font_t font_puhui_16_4;

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

static int s_action_hits;

static void on_ok_action(arena_t *arena, arena_node_t *node,
                         const gfx_touch_event_t *event, void *user_data)
{
    (void)arena;
    (void)event;
    (void)user_data;
    if (node != NULL) {
        node->bg_rgb = 0xff6644;
    }
    s_action_hits++;
}

static uint16_t sample_px(const uint16_t *fb, int x, int y)
{
    return fb[(size_t)y * SCREEN_W + (size_t)x];
}

static uint16_t rgb888_to_rgb565(uint32_t rgb)
{
    const uint32_t r = (rgb >> 16) & 0xFFu;
    const uint32_t g = (rgb >> 8) & 0xFFu;
    const uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static int count_non_bg_in_rect(const uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t bg)
{
    int n = 0;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (sample_px(fb, x, y) != bg) {
                n++;
            }
        }
    }
    return n;
}

int main(void)
{
    int fails = 0;
    s_action_hits = 0;

    printf("arena_dirty_demo: dirty + label/button text + touch/action\n");

    enum { ICON_W = 32, ICON_H = 32 };
    uint16_t icon_px[ICON_W * ICON_H];
    for (int y = 0; y < ICON_H; y++) {
        for (int x = 0; x < ICON_W; x++) {
            icon_px[y * ICON_W + x] = ((x ^ y) & 4) ? (uint16_t)0xF800 : (uint16_t)0x001F;
        }
    }

    const arena_desc_t descs[] = {
        {
            .type = ARENA_NODE_CONTAINER,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG,
            .x = 0, .y = 0, .w = SCREEN_W, .h = SCREEN_H,
            .bg_rgb = 0x1c1f2e, .name = "root", .parent = -1, .action = NULL
        },
        {
            .type = ARENA_NODE_LABEL,
            .flags = ARENA_F_VISIBLE,
            .x = 16, .y = 16, .w = 160, .h = 28,
            .bg_rgb = 0xF3F7FA, .name = "Hello", .parent = 0, .action = NULL
        },
        {
            .type = ARENA_NODE_IMAGE,
            .flags = ARENA_F_VISIBLE,
            .x = 16, .y = 56, .w = ICON_W, .h = ICON_H,
            .bg_rgb = 0, .name = "icon", .parent = 0, .action = NULL,
            .img_rgb565 = icon_px, .img_w = ICON_W, .img_h = ICON_H
        },
        {
            .type = ARENA_NODE_BUTTON,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG | ARENA_F_CLICKABLE,
            .x = 200, .y = 180, .w = 96, .h = 40,
            .bg_rgb = 0x2f8cff, .name = "OK", .parent = 0, .action = "on_ok"
        },
    };

    size_t pkg_size = 0;
    uint8_t *pkg = arena_pack(descs, 4, &pkg_size);
    CHECK(pkg != NULL, "pack");

    arena_t arena = {0};
    CHECK(arena_load(pkg, pkg_size, &arena) == 0, "load RAM copy");
    CHECK(arena.base != pkg, "writable copy != ROM pkg");

    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    CHECK(handle != NULL, "gfx_core_init");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
    });
    CHECK(backend != NULL, "memory backend");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
        .backend = backend,
    });
    CHECK(disp != NULL, "display add");

    gfx_arena_scene_t scene;
    CHECK(arena_scene_attach(disp, &arena, &scene) == 0, "arena_scene_attach");
    arena_scene_set_font(&scene, (gfx_font_t)&font_puhui_16_4);
    CHECK(scene.font_adapter != NULL, "font adapter ready");

    const arena_action_entry_t actions[] = {
        { .name = "on_ok", .cb = on_ok_action, .user_data = NULL },
    };
    arena_scene_set_actions(&scene, actions, 1);

    arena_node_t *ok = arena_find_by_name(&scene.arena, "OK");
    arena_node_t *title = arena_find_by_name(&scene.arena, "Hello");
    CHECK(ok != NULL && title != NULL, "find ok + Hello");
    uint32_t ok_off = arena_node_offset(&scene.arena, ok);
    uint32_t title_off = arena_node_offset(&scene.arena, title);
    CHECK(ok_off != ARENA_NO_NODE && title_off != ARENA_NO_NODE, "offsets");

    gfx_display_refresh_all(disp);
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "initial refresh");

    uint16_t *pixels = (uint16_t *)gfx_memory_backend_get_buffer_data(backend);
    CHECK(pixels != NULL, "fb");

    const uint16_t root_bg = rgb888_to_rgb565(0x1c1f2e);
    CHECK(sample_px(pixels, 10, 10) == root_bg, "root bg");

    /* Image checker: sample a red-ish and blue-ish cell. */
    uint16_t img_a = sample_px(pixels, 18, 58);
    uint16_t img_b = sample_px(pixels, 22, 58);
    CHECK(img_a != root_bg && img_b != root_bg, "image pixels drawn");
    CHECK(img_a != img_b, "image checker contrast");

    /* Label "Hello" should paint some non-background pixels in its box. */
    int label_ink = count_non_bg_in_rect(pixels, 16, 16, 16 + 80, 16 + 24, root_bg);
    CHECK(label_ink > 20, "label text ink pixels");
    printf("    label ink=%d\n", label_ink);

    /* Button fill: sample left-middle (avoid centered caption + round corners). */
    const uint16_t btn_c = rgb888_to_rgb565(0x2f8cff);
    uint16_t btn_px = sample_px(pixels, 205, 198);
    CHECK(btn_px == btn_c, "button fill pixel");

    CHECK(arena_scene_mark_dirty(&scene, ok_off) == 0, "mark_dirty button");
    CHECK(arena_scene_test_dirty_count(disp) >= 1, "dirty count >= 1");

    gfx_area_t dirty0;
    CHECK(arena_scene_test_dirty_area(disp, 0, &dirty0) == 0, "read dirty[0]");
    const int dirty_w = (int)dirty0.x2 - (int)dirty0.x1 + 1;
    const int dirty_h = (int)dirty0.y2 - (int)dirty0.y1 + 1;
    CHECK(dirty_w <= 120 && dirty_h <= 60, "dirty is local (not fullscreen)");
    printf("    dirty[0]=[%d,%d,%d,%d] size=%dx%d\n",
           dirty0.x1, dirty0.y1, dirty0.x2, dirty0.y2, dirty_w, dirty_h);

    ok->bg_rgb = 0x44cc88;
    CHECK(arena_scene_mark_dirty(&scene, ok_off) == 0, "mark after color change");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh dirty chunk");

    const uint16_t expect = rgb888_to_rgb565(0x44cc88);
    btn_px = sample_px(pixels, 205, 198);
    CHECK(btn_px == expect, "button fill after dirty refresh");

    gfx_touch_event_t press = {
        .type = GFX_TOUCH_EVENT_PRESS,
        .x = 220, .y = 190, .track_id = 0, .timestamp_ms = 1,
    };
    gfx_touch_event_t release = {
        .type = GFX_TOUCH_EVENT_RELEASE,
        .x = 220, .y = 190, .track_id = 0, .timestamp_ms = 2,
    };
    CHECK(gfx_touch_inject(disp, &press) == GFX_OK, "inject press");
    CHECK(gfx_touch_inject(disp, &release) == GFX_OK, "inject release");
    CHECK(s_action_hits == 1, "action on_ok fired");
    CHECK(ok->bg_rgb == 0xff6644, "action mutated bg in-place");

    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh after action");
    CHECK(sample_px(pixels, 205, 198) == rgb888_to_rgb565(0xff6644),
          "pixel after action");

    CHECK(arena_scene_hit_test(&scene, 10, 10) == ARENA_NO_NODE, "root not clickable");
    CHECK(arena_scene_hit_test(&scene, 220, 190) == ok_off, "hit button");

    /* Label color mutate + dirty */
    title->bg_rgb = 0xffcc00;
    CHECK(arena_scene_mark_dirty(&scene, title_off) == 0, "mark label dirty");
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh label");
    int label_ink2 = count_non_bg_in_rect(pixels, 16, 16, 16 + 80, 16 + 24, root_bg);
    CHECK(label_ink2 > 20, "label still has ink after recolor");

    arena_scene_detach(&scene);
    gfx_core_deinit(handle);
    free(pkg);

    if (fails == 0) {
        printf("PASS arena_dirty_demo\n");
        return 0;
    }
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
}
