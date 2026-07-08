/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ai_scene_pkg_demo — 加载 uic 导出的 home.inc，并走当前 GSP 包格式渲染。
 *
 * 流程：
 *   1) #include "home.inc"：uic 从 home.json 导出的 GSP1 二进制包；
 *   2) gsp_load() 只读包字节，按 type 工厂建树；
 *   4) 断言树结构/父子/坐标正确（自检）；
 *   5) SDL 渲染真实 esp_emote_gfx 画面。
 *
 * 设 GSP_HEADLESS=1 则只做 1~4 步并按自检结果返回（可无窗口自动验证）。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/widgets/label.h"

#include "gfx_display_port.h"
#include "gfx_host_runner.h"

/* home.inc 自带 #include "gsp_format.h"，提供 home_fonts/home_scene_pkg。 */
#include "home.inc"

#define SCREEN_W HOME_SCREEN_W
#define SCREEN_H HOME_SCREEN_H

static int s_clicks;

static int load_file(const char *path, uint8_t **out_data, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint8_t *data;

    if (fp == NULL || out_data == NULL || out_size == NULL) {
        if (fp != NULL) {
            fclose(fp);
        }
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 1;
    }
    size = ftell(fp);
    if (size <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 1;
    }

    data = (uint8_t *)malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        return 1;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    *out_data = data;
    *out_size = (size_t)size;
    return 0;
}

static bool file_exists(const char *path)
{
    FILE *fp;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }
    fclose(fp);
    return true;
}

typedef struct {
    uint16_t id;
    gfx_font_t font;
    uint8_t *data;
} demo_font_slot_t;

static const char *resolve_host_font_path(const char *scene_path)
{
    static const char *const candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "examples/assets/format/DejaVuSans.ttf",
    };
    const char *env_path = getenv("GSP_FONT_PATH");

    if (env_path != NULL && env_path[0] != '\0') {
        return env_path;
    }

    if (file_exists(scene_path)) {
        return scene_path;
    }

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (file_exists(candidates[i])) {
            return candidates[i];
        }
    }

    return NULL;
}

static void free_demo_fonts(demo_font_slot_t *slots, size_t count)
{
    if (slots == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (slots[i].font != NULL) {
            (void)gfx_label_font_delete(slots[i].font);
        }
        free(slots[i].data);
    }
    free(slots);
}

static int create_demo_font(const gsp_font_desc_t *desc, demo_font_slot_t *slot)
{
    if (desc == NULL || slot == NULL) {
        return 1;
    }

    const char *font_path = resolve_host_font_path(desc->path);
    if (font_path == NULL) {
        fprintf(stderr, "load font failed: no usable TTF/TTC candidate\n");
        return 1;
    }

    uint8_t *font_data = NULL;
    size_t font_size = 0;
    if (load_file(font_path, &font_data, &font_size) != 0) {
        fprintf(stderr, "load font failed: %s\n", font_path);
        return 1;
    }

    gfx_label_cfg_t cfg = {
        .name = font_path,
        .mem = font_data,
        .mem_size = font_size,
        .font_size = desc->size_px,
    };
    gfx_font_t font = NULL;
    gfx_err_t err = gfx_label_font_create(&cfg, &font);
    if (err != GFX_OK || font == NULL) {
        fprintf(stderr, "create FreeType font failed: %s err=%d\n", font_path, err);
        free(font_data);
        return 1;
    }

    printf("FreeType font[%u]: %s size=%u (%zu bytes)\n",
           desc->id, font_path, desc->size_px, font_size);
    slot->id = desc->id;
    slot->font = font;
    slot->data = font_data;
    return 0;
}

static int create_demo_fonts(const gsp_font_desc_t *font_descs, uint16_t font_desc_count,
                             demo_font_slot_t **out_slots,
                             size_t *out_count, gsp_font_binding_t **out_bindings)
{
    if (font_descs == NULL || font_desc_count == 0 || out_slots == NULL ||
        out_count == NULL || out_bindings == NULL) {
        return 1;
    }

    demo_font_slot_t *slots = (demo_font_slot_t *)calloc(font_desc_count, sizeof(*slots));
    gsp_font_binding_t *bindings = (gsp_font_binding_t *)calloc(font_desc_count, sizeof(*bindings));
    if (slots == NULL || bindings == NULL) {
        free(slots);
        free(bindings);
        return 1;
    }

    for (uint16_t i = 0; i < font_desc_count; i++) {
        if (create_demo_font(&font_descs[i], &slots[i]) != 0) {
            free(bindings);
            free_demo_fonts(slots, font_desc_count);
            return 1;
        }
        bindings[i].id = slots[i].id;
        bindings[i].font = slots[i].font;
    }

    *out_slots = slots;
    *out_count = font_desc_count;
    *out_bindings = bindings;
    return 0;
}

static void on_ok_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    (void)obj;
    (void)user_data;
    if (event != NULL && event->type == GFX_TOUCH_EVENT_RELEASE) {
        s_clicks++;
        printf("[callback] on_ok clicked (#%d)\n", s_clicks);
    }
}

/* 自检：加载后的树结构必须与 home.inc 描述一致。返回 0 通过。*/
static int verify_tree(const gsp_scene_t *scene)
{
    int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); fails++; } } while (0)

    CHECK(scene->obj_count == HOME_OBJ_COUNT, "obj_count == HOME_OBJ_COUNT");
    CHECK(scene->root != NULL, "root != NULL");
    for (uint16_t i = 0; i < scene->obj_count; i++) {
        CHECK(scene->objs[i] != NULL, "object handle non-null");
    }
    if (scene->obj_count == 7) {
        CHECK(gfx_object_get_parent(scene->objs[0]) == NULL, "obj0 is root");
        CHECK(gfx_object_get_parent(scene->objs[1]) == scene->objs[0], "obj1 parent = obj0");
        for (uint16_t i = 2; i < 7; i++) {
            CHECK(gfx_object_get_parent(scene->objs[i]) == scene->objs[1], "obj[i] parent = obj1 (card)");
        }
        /* obj6 = button: panel(36,68) + local(238,158) = abs(274,226) */
        gfx_coord_t bx = 0, by = 0;
        (void)gfx_object_get_pos(scene->objs[6], &bx, &by);
        CHECK(bx == 274 && by == 226, "button abs pos = (274,226)");
        CHECK(strcmp(gfx_object_get_class_name(scene->objs[5]), "image") == 0, "obj5 is image");
        CHECK(scene->blob_count == 1, "scene has one image blob");
        CHECK(scene->img_dscs != NULL && scene->img_bufs != NULL, "image blob cache allocated");
        if (scene->img_dscs != NULL && scene->img_bufs != NULL) {
            CHECK(scene->img_bufs[0] != NULL, "image blob is decoded");
            CHECK(scene->img_dscs[0].header.w == 96 && scene->img_dscs[0].header.h == 72,
                  "image blob size = 96x72");
        }
    }
#undef CHECK
    return fails;
}

int main(void)
{
    const bool headless = (getenv("GSP_HEADLESS") != NULL && getenv("GSP_HEADLESS")[0] != '\0');
    demo_font_slot_t *font_slots = NULL;
    gsp_font_binding_t *font_bindings = NULL;
    size_t font_count = 0;

    if (create_demo_fonts(home_fonts, HOME_FONT_COUNT, &font_slots, &font_count, &font_bindings) != 0) {
        return 1;
    }

    /* 1) dump: home.inc already contains the compiled u32-offset package. */
    gsp_dump(home_scene_pkg, home_scene_pkg_len);

    /* 2) open display port（headless 也能用 SDL dummy 驱动创建 display） */
    gfx_display_port_t port = {0};
    gfx_err_t err = gfx_display_port_open(&(gfx_display_port_config_t) {
        .h_res = SCREEN_W,
        .v_res = SCREEN_H,
        .fps = 30,
        .color_format = GFX_COLOR_FORMAT_RGB565,
        .backend_type = GFX_DISPLAY_PORT_BACKEND_HOST_SDL,
        .sdl = {
            .scale = 1,
            .title = "AI Scene (home.inc)",
        },
        .runtime = {
            .manual_tick = true,
            .core = {
                .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
            },
        },
        .display = {
            .double_buffer = true,
        },
    }, &port);
    if (err != GFX_OK) {
        fprintf(stderr, "display port open failed: %d\n", err);
        free(font_bindings);
        free_demo_fonts(font_slots, font_count);
        return 1;
    }

    /* 3) load: 只读字节包 -> gfx object 树（工厂建树） */
    const gsp_cb_binding_t cbs[] = {
        { .name = "on_ok", .cb = on_ok_cb, .user_data = NULL },
    };
    gsp_scene_t scene = {0};
    int rc = gsp_load_with_fonts(home_scene_pkg, home_scene_pkg_len, port.disp,
                                 font_bindings, font_count,
                                 font_bindings[0].font,
                                 cbs, sizeof(cbs) / sizeof(cbs[0]), &scene);
    if (rc != GSP_OK) {
        fprintf(stderr, "gsp_load failed: %d\n", rc);
        gfx_display_port_close(&port);
        free(font_bindings);
        free_demo_fonts(font_slots, font_count);
        return 1;
    }

    int fails = verify_tree(&scene);
    printf(fails == 0 ? "SELF-CHECK: PASS (%u objects from home.inc)\n"
                      : "SELF-CHECK: FAIL (%u objects)\n",
           scene.obj_count);

    if (headless) {
        gsp_scene_free(&scene);
        gfx_display_port_close(&port);
        free(font_bindings);
        free_demo_fonts(font_slots, font_count);
        return fails == 0 ? 0 : 1;
    }

    (void)gfx_core_refresh_now(port.gfx);
    (void)gfx_host_runner_run(port.gfx, port.disp, &(gfx_host_runner_config_t) {
        .frame_delay_ms = 16,
    });

    gsp_scene_free(&scene);
    gfx_display_port_close(&port);
    free(font_bindings);
    free_demo_fonts(font_slots, font_count);
    return fails == 0 ? 0 : 1;
}
