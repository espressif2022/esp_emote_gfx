/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arena_demo_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t *ok_count;
    uint32_t  title_off;
    gfx_arena_scene_t *scene;
} arena_demo_cb_ctx_t;

static arena_demo_cb_ctx_t s_cb_ctx;
static arena_action_entry_t s_actions[1];

static void on_ok_action(arena_t *arena, arena_node_t *node,
                         const gfx_touch_event_t *event, void *user_data)
{
    (void)event;
    arena_demo_cb_ctx_t *ctx = (arena_demo_cb_ctx_t *)user_data;
    if (node == NULL || ctx == NULL) {
        return;
    }

    node->bg_rgb = 0xff6644;
    if (ctx->ok_count != NULL) {
        (*ctx->ok_count)++;
    }

    arena_node_t *title = arena_node(arena, ctx->title_off);
    if (title != NULL && ctx->ok_count != NULL) {
        char *s = (char *)arena_str(arena, title->name_off);
        if (s != NULL) {
            char buf[40];
            (void)snprintf(buf, sizeof(buf), "Arena OK #%lu", (unsigned long)*ctx->ok_count);
            size_t cap = strlen(s);
            size_t n = strlen(buf);
            if (n > cap) {
                n = cap;
            }
            memcpy(s, buf, n);
            s[n] = '\0';
            if (ctx->scene != NULL) {
                (void)arena_scene_mark_dirty(ctx->scene, ctx->title_off);
            }
        }
    }

    if (ctx->scene != NULL) {
        uint32_t off = arena_node_offset(arena, node);
        if (off != ARENA_NO_NODE) {
            (void)arena_scene_mark_dirty(ctx->scene, off);
        }
    }
}

uint8_t *arena_demo_pack(size_t *out_size)
{
    /* Long enough string slot for in-place title rewrite after OK. */
    static const char title_slot[] = "Arena Demo                ";

    /* 48x48 RGB565 checker icon (packed into package blob). */
    enum { ICON_W = 48, ICON_H = 48 };
    static uint16_t icon_px[ICON_W * ICON_H];
    static int icon_ready;
    if (!icon_ready) {
        for (int y = 0; y < ICON_H; y++) {
            for (int x = 0; x < ICON_W; x++) {
                const int cell = ((x / 8) ^ (y / 8)) & 1;
                icon_px[y * ICON_W + x] = cell ? (uint16_t)0xF800 : (uint16_t)0x07E0;
            }
        }
        icon_ready = 1;
    }

    const arena_desc_t descs[] = {
        {
            .type = ARENA_NODE_CONTAINER,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG,
            .x = 0, .y = 0,
            .w = ARENA_DEMO_SCREEN_W, .h = ARENA_DEMO_SCREEN_H,
            .bg_rgb = 0x1c1f2e, .name = "root", .parent = -1, .action = NULL
        },
        {
            .type = ARENA_NODE_CONTAINER,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG,
            .x = 24, .y = 24, .w = ARENA_DEMO_SCREEN_W - 48, .h = 72,
            .bg_rgb = 0x2a3142, .name = "header", .parent = 0, .action = NULL
        },
        {
            .type = ARENA_NODE_LABEL,
            .flags = ARENA_F_VISIBLE,
            .x = 20, .y = 22, .w = 480, .h = 32,
            .bg_rgb = 0xF3F7FA, .name = title_slot, .parent = 1, .action = NULL
        },
        {
            .type = ARENA_NODE_LABEL,
            .flags = ARENA_F_VISIBLE,
            .x = 24, .y = 120, .w = 560, .h = 28,
            .bg_rgb = 0xA8B3C7,
            .name = "Tap OK — arena path (no gfx_object)",
            .parent = 0,
            .action = NULL
        },
        {
            .type = ARENA_NODE_IMAGE,
            .flags = ARENA_F_VISIBLE,
            .x = 24, .y = 170, .w = ICON_W, .h = ICON_H,
            .bg_rgb = 0, .name = "icon", .parent = 0, .action = NULL,
            .img_rgb565 = icon_px, .img_w = ICON_W, .img_h = ICON_H
        },
        {
            .type = ARENA_NODE_BUTTON,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG | ARENA_F_CLICKABLE,
            .x = ARENA_DEMO_SCREEN_W - 160, .y = ARENA_DEMO_SCREEN_H - 80,
            .w = 120, .h = 48,
            .bg_rgb = 0x2f8cff, .name = "OK", .parent = 0, .action = "on_ok"
        },
        {
            .type = ARENA_NODE_BUTTON,
            .flags = ARENA_F_VISIBLE | ARENA_F_BG | ARENA_F_CLICKABLE,
            .x = ARENA_DEMO_SCREEN_W - 300, .y = ARENA_DEMO_SCREEN_H - 80,
            .w = 120, .h = 48,
            .bg_rgb = 0x3d4a63, .name = "Idle", .parent = 0, .action = NULL
        },
    };

    return arena_pack(descs, (uint16_t)(sizeof(descs) / sizeof(descs[0])), out_size);
}

int arena_demo_bind(gfx_display_t *disp, const uint8_t *pkg, size_t pkg_size,
                    gfx_font_t font, gfx_arena_scene_t *out_scene,
                    uint32_t *ok_count_inout)
{
    if (disp == NULL || pkg == NULL || out_scene == NULL) {
        return -1;
    }

    arena_t arena = {0};
    if (arena_load(pkg, pkg_size, &arena) != 0) {
        return -2;
    }
    if (arena_scene_attach(disp, &arena, out_scene) != 0) {
        arena_free(&arena);
        return -3;
    }

    arena_scene_set_font(out_scene, font);

    /* Title is node index 2 (root, header, title, ...). */
    const arena_hdr_t *hdr = arena_hdr(&out_scene->arena);
    uint32_t title_off = hdr->nodes_off + 2u * (uint32_t)sizeof(arena_node_t);

    s_cb_ctx.ok_count = ok_count_inout;
    s_cb_ctx.title_off = title_off;
    s_cb_ctx.scene = out_scene;

    s_actions[0] = (arena_action_entry_t) {
        .name = "on_ok",
        .cb = on_ok_action,
        .user_data = &s_cb_ctx,
    };
    arena_scene_set_actions(out_scene, s_actions, 1);

    (void)arena_scene_mark_dirty_all(out_scene);
    return 0;
}
