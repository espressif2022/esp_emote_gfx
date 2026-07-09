/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ai_scene_pkg_demo — 加载 uic 导出的 GSP .inc，并走当前 GSP 包格式渲染。
 *
 * 流程：
 *   1) #include GSP_SCENE_INC：导出的 GSP1 v4 二进制包（含动作表）；
 *   2) gsp_load() 只读包字节，按 type 工厂建树 + 解析动作表；
 *   3) 断言树结构/父子/坐标/动作表正确（自检）；
 *   4) SDL 渲染真实 esp_emote_gfx 画面。
 *
 * 交互（点击 OK 按钮）：
 *   - on_ok C 回调（GSP_F_CALLBACK）：计数并改标题文字；
 *   - 动作表（v4，纯数据，无需 C 代码）：给面板换底色 + 改副标题文字。
 *   二者由 loader 的统一 trampoline 依次分发，在同一控件上共存。
 *
 * 设 GSP_HEADLESS=1 则只做 1~3 步并按自检结果返回（可无窗口自动验证）。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/input.h"
#include "gfx/object.h"
#include "gfx/widgets/label.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/wheel.h"

#include "gfx_display_port.h"
#include "gfx_host_runner.h"

#ifndef GSP_SCENE_INC
#define GSP_SCENE_INC "inc/home.inc"
#endif

#include GSP_SCENE_INC

#if defined(HOME_CONTROL_SCREEN_W)
#define GSP_DEMO_SCREEN_W       HOME_CONTROL_SCREEN_W
#define GSP_DEMO_SCREEN_H       HOME_CONTROL_SCREEN_H
#define GSP_DEMO_OBJ_COUNT      HOME_CONTROL_OBJ_COUNT
#define GSP_DEMO_FONT_COUNT     HOME_CONTROL_FONT_COUNT
#define GSP_DEMO_ACTION_COUNT   HOME_CONTROL_ACTION_COUNT
#define GSP_DEMO_FONTS          home_control_fonts
#define GSP_DEMO_SCENE_PKG      home_control_scene_pkg
#define GSP_DEMO_SCENE_PKG_LEN  home_control_scene_pkg_len
#define GSP_DEMO_TITLE          "AI Scene (" GSP_SCENE_INC ")"
#elif defined(HOME_SCREEN_W)
#define GSP_DEMO_SCREEN_W       HOME_SCREEN_W
#define GSP_DEMO_SCREEN_H       HOME_SCREEN_H
#define GSP_DEMO_OBJ_COUNT      HOME_OBJ_COUNT
#define GSP_DEMO_FONT_COUNT     HOME_FONT_COUNT
#define GSP_DEMO_ACTION_COUNT   HOME_ACTION_COUNT
#define GSP_DEMO_FONTS          home_fonts
#define GSP_DEMO_SCENE_PKG      home_scene_pkg
#define GSP_DEMO_SCENE_PKG_LEN  home_scene_pkg_len
#define GSP_DEMO_TITLE          "AI Scene (" GSP_SCENE_INC ")"
#else
#error "Unsupported GSP scene include: missing HOME_* or HOME_CONTROL_* symbols"
#endif

#define SCREEN_W GSP_DEMO_SCREEN_W
#define SCREEN_H GSP_DEMO_SCREEN_H

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
    gsp_scene_t *scene = (gsp_scene_t *)user_data;

    (void)obj;
    if (event != NULL && event->type == GFX_TOUCH_EVENT_RELEASE) {
        s_clicks++;
        gfx_object_t *title = gsp_scene_find_by_name(scene, "title");
        if (title != NULL) {
            (void)gfx_label_set_text_fmt(title, "OK clicked #%d", s_clicks);
        }
        printf("[callback] on_ok clicked (#%d)\n", s_clicks);
    }
}

static void inject_click(gfx_display_t *disp, gfx_object_t *obj)
{
    gfx_coord_t x = 0, y = 0;
    uint16_t w = 0, h = 0;

    if (disp == NULL || obj == NULL) {
        return;
    }
    (void)gfx_object_get_pos(obj, &x, &y);
    (void)gfx_object_get_size(obj, &w, &h);
    const gfx_touch_event_t press = {
        .type = GFX_TOUCH_EVENT_PRESS,
        .x = (uint16_t)(x + w / 2), .y = (uint16_t)(y + h / 2),
    };
    gfx_touch_event_t release = press;
    release.type = GFX_TOUCH_EVENT_RELEASE;
    (void)gfx_touch_inject(disp, &press);
    (void)gfx_touch_inject(disp, &release);
}

/* 自检：默认做通用 package 检查；若加载的是当前 home demo，再做强结构检查。*/
static int verify_tree(const gsp_scene_t *scene)
{
    int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); fails++; } } while (0)

    CHECK(scene->obj_count == GSP_DEMO_OBJ_COUNT, "obj_count == package obj count");
    CHECK(scene->root != NULL, "root != NULL");
    CHECK(scene->action_count == GSP_DEMO_ACTION_COUNT, "action_count == package action count");
    for (uint16_t i = 0; i < scene->obj_count; i++) {
        CHECK(scene->objs[i] != NULL, "object handle non-null");
    }
    if (scene->obj_count == GSP_DEMO_OBJ_COUNT) {
        CHECK(gfx_object_get_parent(scene->objs[0]) == NULL, "obj0 is root");
    }
    if (scene->blob_count > 0) {
        CHECK(scene->img_dscs != NULL && scene->img_bufs != NULL, "image blob cache allocated when blobs exist");
        if (scene->img_dscs != NULL && scene->img_bufs != NULL) {
            CHECK(scene->img_bufs[0] != NULL, "image blob is decoded");
        }
    }

    if (scene->obj_count == 12 && gsp_scene_find_by_name(scene, "homeLayer") != NULL) {
        CHECK(gsp_scene_find_by_name(scene, "title") == gsp_scene_get_obj(scene, 2),
              "name lookup: title -> obj2");
        CHECK(gsp_scene_find_by_name(scene, "subtitle") == gsp_scene_get_obj(scene, 3),
              "name lookup: subtitle -> obj3");
        CHECK(gsp_scene_find_by_name(scene, "homeLayer") == gsp_scene_get_obj(scene, 1),
              "name lookup: homeLayer -> obj1");
        CHECK(gsp_scene_find_by_name(scene, "homeNext") == gsp_scene_get_obj(scene, 6),
              "name lookup: homeNext -> obj6");
        CHECK(gsp_scene_find_by_name(scene, "selectorLayer") == gsp_scene_get_obj(scene, 7),
              "name lookup: selectorLayer -> obj7");
        CHECK(gsp_scene_find_by_name(scene, "featureList") == gsp_scene_get_obj(scene, 9),
              "name lookup: featureList -> obj9");
        CHECK(gsp_scene_find_by_name(scene, "formatWheel") == gsp_scene_get_obj(scene, 10),
              "name lookup: formatWheel -> obj10");
        CHECK(gsp_scene_find_by_name(scene, "selectorNext") == gsp_scene_get_obj(scene, 11),
              "name lookup: selectorNext -> obj11");
        CHECK(gsp_scene_find_by_bind_id(scene, 1) == gsp_scene_get_obj(scene, 4),
              "bind lookup: bind_id 1 -> obj4");
        CHECK(gfx_object_get_visible(gsp_scene_get_obj(scene, 1)), "homeLayer initially visible");
        CHECK(!gfx_object_get_visible(gsp_scene_get_obj(scene, 7)), "selectorLayer initially hidden");
        CHECK(gfx_list_get_item_count(gsp_scene_get_obj(scene, 9)) == 4,
              "featureList has 4 params-v1 items");
        CHECK(gfx_wheel_get_item_count(gsp_scene_get_obj(scene, 10)) == 4,
              "formatWheel has 4 params-v1 items");
        CHECK(strcmp(gfx_wheel_get_item_text(gsp_scene_get_obj(scene, 10), 0), "RGB565") == 0,
              "formatWheel item0 = RGB565");
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

    if (create_demo_fonts(GSP_DEMO_FONTS, GSP_DEMO_FONT_COUNT,
                          &font_slots, &font_count, &font_bindings) != 0) {
        return 1;
    }

    /* 1) dump: the selected .inc already contains the compiled u32-offset package. */
    gsp_dump(GSP_DEMO_SCENE_PKG, GSP_DEMO_SCENE_PKG_LEN);

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
            .title = GSP_DEMO_TITLE,
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
    gsp_scene_t scene = {0};
    const gsp_cb_binding_t cbs[] = {
        { .name = "on_ok", .cb = on_ok_cb, .user_data = &scene },
    };
    int rc = gsp_load_with_fonts(GSP_DEMO_SCENE_PKG, GSP_DEMO_SCENE_PKG_LEN, port.disp,
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
    printf(fails == 0 ? "SELF-CHECK: PASS (%u objects from %s)\n"
                      : "SELF-CHECK: FAIL (%u objects)\n",
           scene.obj_count, GSP_SCENE_INC);

    if (headless) {
        (void)gfx_core_refresh_now(port.gfx);
        gfx_object_t *home_next = gsp_scene_find_by_name(&scene, "homeNext");
        gfx_object_t *selector_next = gsp_scene_find_by_name(&scene, "selectorNext");
        gfx_object_t *home_layer = gsp_scene_find_by_name(&scene, "homeLayer");
        gfx_object_t *selector_layer = gsp_scene_find_by_name(&scene, "selectorLayer");
        gfx_object_t *feature_list = gsp_scene_find_by_name(&scene, "featureList");
        gfx_object_t *format_wheel = gsp_scene_find_by_name(&scene, "formatWheel");

        if (home_next != NULL && home_layer != NULL && selector_layer != NULL) {
            const int before = s_clicks;
            inject_click(port.disp, home_next);
            const bool to_selector = (s_clicks == before + 1) &&
                                     !gfx_object_get_visible(home_layer) &&
                                     gfx_object_get_visible(selector_layer);
            printf("LAYER-GOTO-1: %s (homeNext -> selectorLayer)\n",
                   to_selector ? "PASS" : "FAIL");
            if (!to_selector) {
                fails++;
            }
        }
        if (feature_list != NULL && gfx_list_get_item_count(feature_list) > 2) {
            (void)gfx_list_set_focus(feature_list, 2);
            (void)gfx_list_confirm(feature_list);
        }
        if (format_wheel != NULL && gfx_wheel_get_item_count(format_wheel) > 2) {
            (void)gfx_wheel_set_selected(format_wheel, 2);
            (void)gfx_wheel_confirm(format_wheel);
        }
        if (selector_next != NULL && home_layer != NULL && selector_layer != NULL) {
            inject_click(port.disp, selector_next);
            const bool to_home = gfx_object_get_visible(home_layer) &&
                                 !gfx_object_get_visible(selector_layer);
            printf("LAYER-GOTO-2: %s (selectorNext -> homeLayer)\n",
                   to_home ? "PASS" : "FAIL");
            if (!to_home) {
                fails++;
            }
        }

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
