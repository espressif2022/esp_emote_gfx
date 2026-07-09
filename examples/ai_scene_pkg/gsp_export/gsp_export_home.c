/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Host exporter for the demo home scene.
 *
 * This is intentionally small and boring: build an author-side scene, call the
 * same gsp_pack() compiler, then emit only the raw .gsp package. Formatting that
 * package as .inc, extracting previews, and writing a readable manifest are tool
 * pipeline jobs handled by gsp_package_tools.py.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsp_format.h"

#define HOME_SCREEN_W 480
#define HOME_SCREEN_H 480
#define HOME_OBJ_COUNT 12
#define HOME_ACTION_COUNT 3
#define HOME_IMG_W 96
#define HOME_IMG_H 72

typedef struct {
    uint8_t *data;
    uint16_t len;
} item_params_t;

static uint16_t rgb565(uint32_t rgb)
{
    uint8_t r = (uint8_t)((rgb >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((rgb >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(rgb & 0xFFu);

    return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                      ((uint16_t)(g & 0xFCu) << 3) |
                      ((uint16_t)b >> 3));
}

static void put_px(uint8_t *pixels, uint16_t x, uint16_t y, uint16_t color)
{
    size_t off = ((size_t)y * HOME_IMG_W + x) * 2u;

    pixels[off + 0] = (uint8_t)(color & 0xFFu);
    pixels[off + 1] = (uint8_t)((color >> 8) & 0xFFu);
}

static void build_preview_image(uint8_t *pixels)
{
    const uint16_t bg = rgb565(0x102033);
    const uint16_t cyan = rgb565(0x67D8F6);
    const uint16_t blue = rgb565(0x2F8CFF);
    const uint16_t green = rgb565(0x6FE0A8);
    const uint16_t amber = rgb565(0xFFD166);
    const uint16_t ink = rgb565(0x18202A);
    const uint16_t white = rgb565(0xF3F7FA);

    for (uint16_t y = 0; y < HOME_IMG_H; y++) {
        for (uint16_t x = 0; x < HOME_IMG_W; x++) {
            uint16_t color = bg;

            if (x < 2 || y < 2 || x >= HOME_IMG_W - 2 || y >= HOME_IMG_H - 2) {
                color = cyan;
            } else if (y < 18) {
                color = blue;
            } else if (x < 34 && y < 52) {
                color = green;
            } else if (x >= 62 && y >= 30) {
                color = amber;
            } else if (((x + y) % 17) < 3) {
                color = rgb565(0x1C3C55);
            }
            put_px(pixels, x, y, color);
        }
    }

    /* Tiny symbolic "UI card" marks; no font dependency in the baked image. */
    for (uint16_t y = 24; y < 30; y++) {
        for (uint16_t x = 42; x < 82; x++) {
            put_px(pixels, x, y, white);
        }
    }
    for (uint16_t y = 36; y < 42; y++) {
        for (uint16_t x = 42; x < 72; x++) {
            put_px(pixels, x, y, rgb565(0x8AA0B4));
        }
    }
    for (uint16_t y = 50; y < 60; y++) {
        for (uint16_t x = 12; x < 84; x++) {
            if (x < 14 || x >= 82 || y < 52 || y >= 58) {
                put_px(pixels, x, y, ink);
            }
        }
    }
}

static void free_item_params(item_params_t *p)
{
    if (p != NULL) {
        free(p->data);
        p->data = NULL;
        p->len = 0;
    }
}

static int build_item_params(const char *const *items, uint16_t item_count,
                             uint16_t selected, uint16_t item_height,
                             uint16_t rows_or_page, uint16_t flags,
                             item_params_t *out)
{
    uint32_t total = 12u;

    if (items == NULL || out == NULL) {
        return 1;
    }
    for (uint16_t i = 0; i < item_count; i++) {
        const char *item = items[i] != NULL ? items[i] : "";
        size_t len = strlen(item);
        if (len > 0xFFFFu || total + 2u + len > 0xFFFFu) {
            return 1;
        }
        total += 2u + (uint32_t)len;
    }

    uint8_t *buf = (uint8_t *)malloc(total);
    if (buf == NULL) {
        return 1;
    }

    gsp_wr_u16(buf + 0, item_count);
    gsp_wr_u16(buf + 2, selected);
    gsp_wr_u16(buf + 4, item_height);
    gsp_wr_u16(buf + 6, rows_or_page);
    gsp_wr_u16(buf + 8, flags);
    gsp_wr_u16(buf + 10, 0u);

    uint32_t cursor = 12u;
    for (uint16_t i = 0; i < item_count; i++) {
        const char *item = items[i] != NULL ? items[i] : "";
        uint16_t len = (uint16_t)strlen(item);
        gsp_wr_u16(buf + cursor, len);
        cursor += 2u;
        memcpy(buf + cursor, item, len);
        cursor += len;
    }

    out->data = buf;
    out->len = (uint16_t)total;
    return 0;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return 1;
    }

    if (fwrite(data, 1, size, fp) != size) {
        fprintf(stderr, "write %s failed: %s\n", path, strerror(errno));
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "examples/ai_scene_pkg/gsp_export";
    char gsp_path[512];

    snprintf(gsp_path, sizeof(gsp_path), "%s/home.gsp", out_dir);

    uint8_t *image_pixels = (uint8_t *)malloc((size_t)HOME_IMG_W * HOME_IMG_H * 2u);
    if (image_pixels == NULL) {
        return 1;
    }
    build_preview_image(image_pixels);

    const gfx_image_dsc_t preview_image = {
        .header = {
            .magic = GFX_IMAGE_HEADER_MAGIC,
            .cf = GFX_COLOR_FORMAT_RGB565,
            .w = HOME_IMG_W,
            .h = HOME_IMG_H,
            .stride = HOME_IMG_W * 2u,
        },
        .data_size = (uint32_t)HOME_IMG_W * HOME_IMG_H * 2u,
        .data = image_pixels,
    };

    static const char *const list_items[] = {
        "Layer model",
        "Wheel widget",
        "List widget",
        "Binary params",
    };
    static const char *const wheel_items[] = {
        "RGB565",
        "RGB888",
        "BGR888",
        "ARGB8888",
    };
    item_params_t list_params = {0};
    item_params_t wheel_params = {0};
    if (build_item_params(list_items, 4, 1, 34, 4, GSP_ITEM_PARAMS_F_SNAP_TO_ITEM,
                          &list_params) != 0 ||
        build_item_params(wheel_items, 4, 0, 30, 5, GSP_ITEM_PARAMS_F_CYCLIC,
                          &wheel_params) != 0) {
        free_item_params(&list_params);
        free_item_params(&wheel_params);
        free(image_pixels);
        return 1;
    }

    const gsp_desc_t descs[HOME_OBJ_COUNT] = {
        { .type = GSP_OBJ_CONTAINER, .parent_idx = GSP_NO_PARENT,
          .flags = GSP_F_BG_COLOR, .x = 0, .y = 0, .w = 480, .h = 480,
          .bg_color = 0x0E1116 },
        { .type = GSP_OBJ_LAYER, .parent_idx = 0,
          .flags = GSP_F_BG_COLOR | GSP_F_BORDER | GSP_F_RADIUS | GSP_F_NAME,
          .x = 36, .y = 68, .w = 408, .h = 344, .name = "homeLayer",
          .bg_color = 0x171C24, .border_color = 0x2E3A47, .border_width = 2,
          .radius = 18 },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT | GSP_F_NAME,
          .x = 30, .y = 30, .w = 340, .h = 32,
          .fg_color = 0xFFFFFF, .font_id = 0,
          .text = "AI Scene · u32-offset pkg", .name = "title" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT | GSP_F_NAME,
          .x = 30, .y = 72, .w = 340, .h = 26,
          .fg_color = 0x8AA0B4, .font_id = 1,
          .text = "page 1: image + callback + action", .name = "subtitle" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 1,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT,
          .x = 30, .y = 106, .w = 340, .h = 30,
          .fg_color = 0x6FE0A8, .font_id = 2,
          .text = "温度 23.5°C 数据绑定", .bind_id = 1 },
        { .type = GSP_OBJ_IMAGE, .parent_idx = 1,
          .flags = GSP_F_IMAGE,
          .x = 30, .y = 150, .w = HOME_IMG_W, .h = HOME_IMG_H,
          .image_src = &preview_image },
        { .type = GSP_OBJ_BUTTON, .parent_idx = 1,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER |
                   GSP_F_RADIUS | GSP_F_TEXT | GSP_F_CALLBACK | GSP_F_NAME,
          .x = 238, .y = 246, .w = 140, .h = 60,
          .bg_color = 0x2F8CFF, .fg_color = 0xFFFFFF, .border_color = 0x76B7E8,
          .border_width = 2, .radius = 12, .font_id = 3,
          .text = "Next", .callback = "on_ok", .name = "homeNext" },
        { .type = GSP_OBJ_LAYER, .parent_idx = 0,
          .flags = GSP_F_BG_COLOR | GSP_F_BORDER | GSP_F_RADIUS | GSP_F_NAME | GSP_F_HIDDEN,
          .x = 36, .y = 68, .w = 408, .h = 344,
          .bg_color = 0x171C24, .border_color = 0x2E3A47, .border_width = 2,
          .radius = 18, .name = "selectorLayer" },
        { .type = GSP_OBJ_LABEL, .parent_idx = 7,
          .flags = GSP_F_FG_COLOR | GSP_F_TEXT,
          .x = 30, .y = 28, .w = 340, .h = 32,
          .fg_color = 0xFFFFFF, .font_id = 0,
          .text = "Page 2 · List + Wheel" },
        { .type = GSP_OBJ_LIST, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER | GSP_F_NAME | GSP_F_PARAMS,
          .x = 30, .y = 78, .w = 168, .h = 170,
          .bg_color = 0x101923, .fg_color = 0xF3F7FA, .border_color = 0x2F8CFF,
          .border_width = 1, .font_id = 1, .name = "featureList",
          .params = list_params.data, .params_len = list_params.len },
        { .type = GSP_OBJ_WHEEL, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER | GSP_F_NAME | GSP_F_PARAMS,
          .x = 220, .y = 78, .w = 158, .h = 170,
          .bg_color = 0x101923, .fg_color = 0xF3F7FA, .border_color = 0x6FE0A8,
          .border_width = 1, .font_id = 1, .name = "formatWheel",
          .params = wheel_params.data, .params_len = wheel_params.len },
        { .type = GSP_OBJ_BUTTON, .parent_idx = 7,
          .flags = GSP_F_BG_COLOR | GSP_F_FG_COLOR | GSP_F_BORDER |
                   GSP_F_RADIUS | GSP_F_TEXT | GSP_F_NAME,
          .x = 238, .y = 246, .w = 140, .h = 60,
          .bg_color = 0x6FE0A8, .fg_color = 0x102033, .border_color = 0xA9F0C8,
          .border_width = 2, .radius = 12, .font_id = 3,
          .text = "Next", .name = "selectorNext" },
    };

    /*
     * v4 动作表（纯数据，无需改固件 C 逻辑）：
     *
     * 对象索引对照：
     *   obj1  = homeLayer      （第 1 页 layer）
     *   obj6  = homeNext       （第 1 页底部 Next 按钮，另有 GSP_F_CALLBACK "on_ok"）
     *   obj7  = selectorLayer  （第 2 页 layer，初始 GSP_F_HIDDEN）
     *   obj11 = selectorNext   （第 2 页底部 Next 按钮）
     *
     * 点击 homeNext (obj6)：
     *   1) SET_BG_COLOR → 把 homeLayer (obj1) 底色改成 0x1E3A5F（演示标量 arg）
     *   2) GOTO         → 显示 selectorLayer，并隐藏同 parent 下其它 layer
     *   同时 trampoline 仍会先跑 C 回调 on_ok（改标题文字），与动作表共存。
     *
     * 点击 selectorNext (obj11)：
     *   3) GOTO         → 显示 homeLayer，回到第 1 页
     */
    const gsp_action_desc_t actions[HOME_ACTION_COUNT] = {
        /* [0] homeNext click → homeLayer 换底色 */
        { .src_idx = 6, .event = GSP_EV_CLICK, .action = GSP_ACT_SET_BG_COLOR,
          .target_idx = 1, .arg = 0x1E3A5F },
        /* [1] homeNext click → 跳到 selectorLayer（第 2 页）*/
        { .src_idx = 6, .event = GSP_EV_CLICK, .action = GSP_ACT_GOTO,
          .target_name = "selectorLayer" },
        /* [2] selectorNext click → 跳回 homeLayer（第 1 页）*/
        { .src_idx = 11, .event = GSP_EV_CLICK, .action = GSP_ACT_GOTO,
          .target_name = "homeLayer" },
    };

    const gsp_scene_desc_t scene = {
        .screen_w = HOME_SCREEN_W,
        .screen_h = HOME_SCREEN_H,
        .screen_bg = 0x0E1116,
        .objs = descs,
        .obj_count = HOME_OBJ_COUNT,
        .actions = actions,
        .action_count = HOME_ACTION_COUNT,
    };

    size_t pkg_size = 0;
    uint8_t *pkg = gsp_pack(&scene, &pkg_size);
    if (pkg == NULL) {
        free_item_params(&list_params);
        free_item_params(&wheel_params);
        free(image_pixels);
        return 1;
    }

    int rc = write_file(gsp_path, pkg, pkg_size);

    printf("exported %s (%zu bytes)\n", gsp_path, pkg_size);

    free(pkg);
    free_item_params(&list_params);
    free_item_params(&wheel_params);
    free(image_pixels);
    return rc;
}
