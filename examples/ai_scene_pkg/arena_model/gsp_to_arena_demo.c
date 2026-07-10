/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_gsp_to_arena_demo — materialize GSP home.inc → ARN1, then formal arena path
 * ==========================================================================
 *   ./build-host-sdl/gfx_gsp_to_arena_demo              # headless
 *   ARENA_SDL=1 ./build-host-sdl/gfx_gsp_to_arena_demo  # visual
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/scene/arena.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/scene/gsp_to_arena.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx_display_port.h"
#include "gfx_host_runner.h"

#ifndef GFX_GSP_SCENE_INC
#define GFX_GSP_SCENE_INC "inc/home.inc"
#endif
#include GFX_GSP_SCENE_INC

extern const lv_font_t font_puhui_16_4;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        fails++; \
    } else { \
        printf("OK  %s\n", msg); \
    } \
} while (0)

static uint32_t s_ok_count;

static void on_ok(gfx_arena_t *arena, gfx_arena_node_t *node,
                  const gfx_touch_event_t *event, void *user_data)
{
    (void)arena;
    (void)event;
    (void)user_data;
    if (node != NULL) {
        node->bg_rgb = 0xff6644;
    }
    s_ok_count++;
}

static int run_headless(void)
{
    int fails = 0;
    s_ok_count = 0;

    printf("gsp_to_arena_demo: %s → ARN1\n", GFX_GSP_SCENE_INC);

    uint8_t *arn = NULL;
    size_t arn_size = 0;
    gfx_gsp_to_arena_info_t info = {0};
    int rc = gfx_gsp_to_arena(home_scene_pkg, sizeof(home_scene_pkg),
                          &arn, &arn_size, &info);
    CHECK(rc == GFX_GSP_TO_ARENA_OK, "gsp_to_arena");
    CHECK(arn != NULL && arn_size > 0, "arn package");
    CHECK(info.out_node_count == info.src_obj_count, "all GSP objs converted");
    CHECK(info.skipped == 0, "no skipped types");
    printf("    screen=%ux%u src_objs=%u out_nodes=%u skipped=%u arn=%zu B\n",
           info.screen_w, info.screen_h,
           info.src_obj_count, info.out_node_count, info.skipped, arn_size);

    gfx_arena_t arena = {0};
    CHECK(gfx_arena_load(arn, arn_size, &arena) == 0, "arena_load");
    free(arn);
    arn = NULL;

    {
        const gfx_arena_hdr_t *hdr0 = gfx_arena_hdr(&arena);
        int has_img = 0, has_list = 0, has_wheel = 0;
        for (uint16_t i = 0; i < hdr0->node_count; i++) {
            uint32_t off = hdr0->nodes_off + (uint32_t)i * (uint32_t)sizeof(gfx_arena_node_t);
            gfx_arena_node_t *n = gfx_arena_node(&arena, off);
            if (n == NULL) {
                continue;
            }
            if (n->type == GFX_ARENA_NODE_IMAGE && gfx_arena_img_pixels(&arena, n->reserved) != NULL) {
                has_img = 1;
            }
            if (n->type == GFX_ARENA_NODE_LIST && gfx_arena_items(&arena, n->reserved) != NULL) {
                has_list = 1;
            }
            if (n->type == GFX_ARENA_NODE_WHEEL && gfx_arena_items(&arena, n->reserved) != NULL) {
                has_wheel = 1;
            }
        }
        CHECK(has_img, "image RGB565 blob");
        CHECK(has_list, "list items blob");
        CHECK(has_wheel, "wheel items blob");
    }

    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    CHECK(handle != NULL, "core");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = info.screen_w ? info.screen_w : 480,
        .v_res = info.screen_h ? info.screen_h : 480,
    });
    CHECK(backend != NULL, "backend");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = info.screen_w ? info.screen_w : 480,
        .v_res = info.screen_h ? info.screen_h : 480,
        .backend = backend,
    });
    CHECK(disp != NULL, "display");

    gfx_arena_scene_t scene;
    CHECK(gfx_arena_scene_attach(disp, &arena, &scene) == 0, "attach");
    gfx_arena_scene_set_font(&scene, (gfx_font_t)&font_puhui_16_4);

    const gfx_arena_action_entry_t actions[] = {
        { .name = "on_ok", .cb = on_ok, .user_data = NULL },
    };
    gfx_arena_scene_set_actions(&scene, actions, 1);

    (void)gfx_arena_scene_mark_dirty_all(&scene);
    CHECK(gfx_core_refresh_now(handle) == GFX_OK, "refresh");

    /* Find a clickable button if any */
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(&scene.arena);
    uint32_t btn_off = GFX_ARENA_NO_NODE;
    for (uint16_t i = 0; i < hdr->node_count; i++) {
        uint32_t off = hdr->nodes_off + (uint32_t)i * (uint32_t)sizeof(gfx_arena_node_t);
        gfx_arena_node_t *n = gfx_arena_node(&scene.arena, off);
        if (n != NULL && n->type == GFX_ARENA_NODE_BUTTON &&
                (n->flags & GFX_ARENA_F_CLICKABLE) != 0) {
            btn_off = off;
            break;
        }
    }
    CHECK(btn_off != GFX_ARENA_NO_NODE, "has clickable button");

    int16_t x1, y1, x2, y2;
    CHECK(gfx_arena_node_abs_area(&scene.arena, btn_off, &x1, &y1, &x2, &y2) == 0,
          "button abs area");
    const uint16_t cx = (uint16_t)((x1 + x2) / 2);
    const uint16_t cy = (uint16_t)((y1 + y2) / 2);

    gfx_touch_event_t press = {
        .type = GFX_TOUCH_EVENT_PRESS, .x = cx, .y = cy, .track_id = 0
    };
    gfx_touch_event_t release = {
        .type = GFX_TOUCH_EVENT_RELEASE, .x = cx, .y = cy, .track_id = 0
    };
    CHECK(gfx_touch_inject(disp, &press) == GFX_OK, "press");
    CHECK(gfx_touch_inject(disp, &release) == GFX_OK, "release");
    CHECK(s_ok_count == 1, "on_ok action from GSP callback_off");

    gfx_arena_scene_detach(&scene);
    gfx_core_deinit(handle);

    return fails == 0 ? 0 : 1;
}

static int run_sdl(void)
{
    uint8_t *arn = NULL;
    size_t arn_size = 0;
    gfx_gsp_to_arena_info_t info = {0};
    if (gfx_gsp_to_arena(home_scene_pkg, sizeof(home_scene_pkg),
                     &arn, &arn_size, &info) != GFX_GSP_TO_ARENA_OK) {
        fprintf(stderr, "gsp_to_arena failed\n");
        return 1;
    }
    printf("GSP→ARN: %u nodes (skipped %u), %ux%u\n",
           info.out_node_count, info.skipped, info.screen_w, info.screen_h);

    gfx_display_port_t port = {0};
    if (gfx_display_port_open(&(gfx_display_port_config_t) {
    .h_res = info.screen_w,
    .v_res = info.screen_h,
    .fps = 30,
    .color_format = GFX_COLOR_FORMAT_RGB565,
    .backend_type = GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
    .sdl = { .scale = 1, .title = "gsp_to_arena (home.inc subset)" },
    .runtime = {
        .manual_tick = true,
        .core = { .task = GFX_CORE_TASK_DEFAULT_CONFIG() },
        },
        .display = { .double_buffer = true },
    }, &port) != GFX_OK) {
        free(arn);
        return 1;
    }

    gfx_arena_t arena = {0};
    if (gfx_arena_load(arn, arn_size, &arena) != 0) {
        gfx_display_port_close(&port);
        free(arn);
        return 1;
    }
    free(arn);
    arn = NULL;

    gfx_arena_scene_t scene;
    if (gfx_arena_scene_attach(port.disp, &arena, &scene) != 0) {
        gfx_arena_free(&arena);
        gfx_display_port_close(&port);
        return 1;
    }
    gfx_arena_scene_set_font(&scene, (gfx_font_t)&font_puhui_16_4);
    const gfx_arena_action_entry_t actions[] = {
        { .name = "on_ok", .cb = on_ok, .user_data = NULL },
    };
    gfx_arena_scene_set_actions(&scene, actions, 1);
    (void)gfx_arena_scene_mark_dirty_all(&scene);
    (void)gfx_core_refresh_now(port.gfx);

    printf("SDL: GSP home subset via arena. Close window to exit.\n");
    (void)gfx_host_runner_run(port.gfx, port.disp, &(gfx_host_runner_config_t) {
        .frame_delay_ms = 16,
    });

    gfx_arena_scene_detach(&scene);
    gfx_display_port_close(&port);
    return 0;
}

int main(void)
{
    const char *sdl = getenv("ARENA_SDL");
    if (sdl != NULL && sdl[0] == '1') {
        return run_sdl();
    }
    const int rc = run_headless();
    printf(rc == 0 ? "PASS\n" : "FAIL\n");
    return rc;
}
