/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GFX_LOG_MODULE GFX_LOG_MODULE_CORE
#include "common/gfx_log_priv.h"
#include "expression_emote.h"
#include "emote_defs.h"
#include "gfx/backends/sdl.h"

#define HOST_LCD_H_RES 320
#define HOST_LCD_V_RES 240

typedef struct {
    emote_handle_t emote;
    gfx_image_dsc_t custom_img_dsc;
    bool custom_label_created;
    bool custom_img_created;
    bool custom_anim_created;
} host_expression_demo_t;

typedef void (*host_expression_step_fn_t)(host_expression_demo_t *demo);

typedef struct {
    const char *name;
    uint32_t duration_ms;
    host_expression_step_fn_t start;
} host_expression_step_t;

static const char *TAG = "host_expression";

static void host_sleep_ms(unsigned ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0) {
    }
}

static uint64_t host_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void step_dialog_angry(host_expression_demo_t *demo)
{
    GFX_LOGI(TAG, "Insert anim: angry");
    (void)emote_insert_anim_dialog(demo->emote, "angry", 5000);
}

static void step_listen(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_LISTEN, NULL);
}

static void step_speak_zh(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_SPEAK,
                              "你好，我是 esp_emote_expression，我是 Brookesia！");
}

static void step_speak_en(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_SPEAK,
                              "Hello, I'm esp_emote_expression, I'm Brookesia!");
}

static void step_happy(host_expression_demo_t *demo)
{
    (void)emote_set_anim_emoji(demo->emote, "happy");
}

static void step_stop_dialog(host_expression_demo_t *demo)
{
    GFX_LOGI(TAG, "Stop anim");
    (void)emote_stop_anim_dialog(demo->emote);
}

static void step_qrcode(host_expression_demo_t *demo)
{
    (void)emote_set_qrcode_data(demo->emote, "https://www.esp32.com");
}

static void step_bat_50(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_BAT, "0,50");
}

static void step_idle(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_IDLE, NULL);
}

static void step_bat_100(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_BAT, "1,100");
}

static void step_off(host_expression_demo_t *demo)
{
    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_OFF, NULL);
}

static void step_face_hide(host_expression_demo_t *demo)
{
    GFX_LOGI(TAG, "Set face visible: false");
    (void)emote_set_anim_visible(demo->emote, false);
}

static void step_face_show(host_expression_demo_t *demo)
{
    GFX_LOGI(TAG, "Set face visible: true");
    (void)emote_set_anim_visible(demo->emote, true);
}

static void step_listen_hide(host_expression_demo_t *demo)
{
    GFX_LOGI(TAG, "Set listen anim visible: false");
    (void)emote_set_obj_visible(demo->emote, EMT_DEF_ELEM_LISTEN_ANIM, false);
}

static void step_custom_label(host_expression_demo_t *demo)
{
    gfx_object_t *custom_label = emote_get_obj_by_name(demo->emote, "custom_label");

    if (custom_label == NULL) {
        custom_label = emote_create_obj_by_type(demo->emote, EMOTE_OBJ_TYPE_LABEL, "custom_label");
    }
    if (custom_label != NULL) {
        (void)emote_lock(demo->emote);
        (void)gfx_label_set_text(custom_label, "Custom Label");
        (void)gfx_label_set_color(custom_label, GFX_COLOR_HEX(0xFF0000));
        (void)gfx_object_set_size(custom_label, 200, 30);
        (void)gfx_object_align(custom_label, GFX_ALIGN_CENTER, 0, 0);
        (void)gfx_object_set_visible(custom_label, true);
        (void)emote_unlock(demo->emote);
    }
    demo->custom_label_created = custom_label != NULL;
}

static void step_toast_update(host_expression_demo_t *demo)
{
    gfx_object_t *toast_label;

    (void)emote_set_event_msg(demo->emote, EMOTE_MGR_EVT_SPEAK, "");
    toast_label = emote_get_obj_by_name(demo->emote, "toast_label");
    if (toast_label != NULL) {
        (void)emote_lock(demo->emote);
        (void)gfx_label_set_text(toast_label, "Toast Label Updated");
        (void)emote_unlock(demo->emote);
    }
}

static void step_custom_image(host_expression_demo_t *demo)
{
    icon_data_t *icon_data = NULL;
    gfx_object_t *custom_img;

    if (emote_get_icon_data_by_name(demo->emote, "icon_tips", &icon_data) != GFX_OK ||
            icon_data == NULL || icon_data->data == NULL ||
            icon_data->size <= sizeof(gfx_image_header_t)) {
        GFX_LOGW(TAG, "icon_tips unavailable for custom image");
        return;
    }

    memcpy(&demo->custom_img_dsc.header, icon_data->data, sizeof(gfx_image_header_t));
    demo->custom_img_dsc.data = (const uint8_t *)icon_data->data + sizeof(gfx_image_header_t);
    demo->custom_img_dsc.data_size = icon_data->size - sizeof(gfx_image_header_t);

    custom_img = emote_get_obj_by_name(demo->emote, "custom_image");
    if (custom_img == NULL) {
        custom_img = emote_create_obj_by_type(demo->emote, EMOTE_OBJ_TYPE_IMAGE, "custom_image");
    }
    if (custom_img != NULL) {
        (void)emote_lock(demo->emote);
        (void)emote_gfx_image_set_dsc(custom_img, &demo->custom_img_dsc);
        (void)gfx_object_set_visible(custom_img, true);
        (void)gfx_object_align(custom_img, GFX_ALIGN_CENTER, 0, 50);
        (void)emote_unlock(demo->emote);
    }
    demo->custom_img_created = custom_img != NULL;
}

static void step_custom_anim(host_expression_demo_t *demo)
{
    emoji_data_t *emoji_data = NULL;
    gfx_object_t *custom_anim;

    if (emote_get_emoji_data_by_name(demo->emote, "happy", &emoji_data) != GFX_OK ||
            emoji_data == NULL || emoji_data->data == NULL) {
        GFX_LOGW(TAG, "happy unavailable for custom animation");
        return;
    }

    custom_anim = emote_get_obj_by_name(demo->emote, "custom_anim");
    if (custom_anim == NULL) {
        custom_anim = emote_create_obj_by_type(demo->emote, EMOTE_OBJ_TYPE_ANIM, "custom_anim");
    }
    if (custom_anim != NULL) {
        (void)emote_lock(demo->emote);
        (void)emote_gfx_anim_set_memory(custom_anim, emoji_data->data, emoji_data->size);
        (void)gfx_anim_set_segment(custom_anim, 0, 0xFFFF, emoji_data->fps, emoji_data->loop);
        (void)gfx_anim_start(custom_anim);
        (void)gfx_object_set_visible(custom_anim, true);
        (void)gfx_object_align(custom_anim, GFX_ALIGN_CENTER, 0, 0);
        (void)emote_unlock(demo->emote);
    }
    demo->custom_anim_created = custom_anim != NULL;
}

static const host_expression_step_t s_steps[] = {
    { "dialog angry", 5000, step_dialog_angry },
    { "listen", 3000, step_listen },
    { "speak zh", 2000, step_speak_zh },
    { "speak en", 2000, step_speak_en },
    { "happy", 3000, step_happy },
    { "dialog angry short", 3000, step_dialog_angry },
    { "stop dialog", 1000, step_stop_dialog },
    { "qrcode", 3000, step_qrcode },
    { "battery 50", 2000, step_bat_50 },
    { "idle", 2000, step_idle },
    { "battery 100 charging", 2000, step_bat_100 },
    { "off", 3000, step_off },
    { "listen again", 3000, step_listen },
    { "face hide", 3000, step_face_hide },
    { "face show", 3000, step_face_show },
    { "listen hide", 3000, step_listen_hide },
    { "custom label", 2000, step_custom_label },
    { "toast update", 2000, step_toast_update },
    { "custom image", 2000, step_custom_image },
    { "custom anim", 3000, step_custom_anim },
};

static void start_step(host_expression_demo_t *demo, size_t index)
{
    GFX_LOGI(TAG, "test_emote step %u/%u: %s",
             (unsigned)(index + 1U), (unsigned)(sizeof(s_steps) / sizeof(s_steps[0])),
             s_steps[index].name);
    s_steps[index].start(demo);
    (void)emote_notify_all_refresh(demo->emote);
}

int main(void)
{
    const char *asset_root = getenv("GFX_EXPRESSION_FS_ROOT");
    gfx_backend_t *backend;
    host_expression_demo_t demo = {0};
    size_t step_index = 0;
    uint64_t step_start_ms;
    bool script_done = false;

    if (asset_root == NULL || asset_root[0] == '\0') {
        asset_root = "examples/expression/assets";
    }

    backend = gfx_backend_sdl_create(&(gfx_backend_sdl_config_t) {
        .h_res = HOST_LCD_H_RES,
        .v_res = HOST_LCD_V_RES,
        .scale = 1,
        .title = "GFX Expression SDL",
    });
    if (backend == NULL) {
        fprintf(stderr, "failed to create SDL backend\n");
        return 1;
    }

    demo.emote = emote_init(&(emote_config_t) {
        .backend = backend,
        .flags = {
            .double_buffer = true,
        },
        .gfx_emote = {
            .h_res = HOST_LCD_H_RES,
            .v_res = HOST_LCD_V_RES,
            .fps = 30,
        },
        .buffers = {
            .buf_pixels = HOST_LCD_H_RES * HOST_LCD_V_RES,
        },
        .task = {
            .task_priority = 4,
            .task_stack = 8192,
            .task_affinity = -1,
            .task_stack_in_ext = false,
        },
    });
    if (demo.emote == NULL) {
        fprintf(stderr, "failed to init expression emote\n");
        gfx_backend_sdl_delete(backend);
        return 1;
    }

    if (emote_mount_and_load_assets(demo.emote, &(emote_data_t) {
    .type = EMOTE_SOURCE_PATH,
    .source = {
        .path = asset_root,
    },
}) != GFX_OK) {
        fprintf(stderr, "failed to mount expression assets: %s\n", asset_root);
        emote_deinit(demo.emote);
        return 1;
    }

    start_step(&demo, step_index);
    step_start_ms = host_now_ms();

    while (!gfx_backend_sdl_poll(demo.emote->gfx_disp)) {
        uint64_t now_ms = host_now_ms();

        if (!script_done && now_ms - step_start_ms >= s_steps[step_index].duration_ms) {
            step_index++;
            if (step_index < sizeof(s_steps) / sizeof(s_steps[0])) {
                start_step(&demo, step_index);
                step_start_ms = now_ms;
            } else {
                GFX_LOGI(TAG, "test_emote script complete; close window to exit");
                script_done = true;
            }
        }
        host_sleep_ms(1);
    }

    emote_deinit(demo.emote);
    return 0;
}
