/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gfx/asset.h"
#include "gfx/backends/sdl.h"
#include "gfx/base.h"
#include "gfx_host_font.h"
#include "gfx/widgets/anim.h"
#include "gfx/widgets/button.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/label.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/motion.h"

#include "../../test_apps/main/claw_motion.inc"

typedef struct {
    gfx_motion_player_t *motion;
    gfx_asset_store_t *asset_store;
    gfx_asset_view_t anim_view;
    gfx_object_t *anim;
    gfx_object_t *button;
    gfx_object_t *list;
    gfx_object_t *status;
    uint16_t action_idx;
} demo_state_t;

static const char *const s_action_names[] = {
    "Move",
    "Connecting",
    "Connect Fail",
    "Connect OK",
    "Disconnect",
    "Command Received",
    "Command Failed",
    "Work",
};

#define DEMO_LANDSCAPE_W 240U
#define DEMO_LANDSCAPE_H 160U

static uint8_t s_landscape_rgb888_map[DEMO_LANDSCAPE_W * DEMO_LANDSCAPE_H * 3U];

static const gfx_image_dsc_t s_landscape_rgb888 = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB888,
        .w = DEMO_LANDSCAPE_W,
        .h = DEMO_LANDSCAPE_H,
        .stride = DEMO_LANDSCAPE_W * 3U,
    },
    .data_size = sizeof(s_landscape_rgb888_map),
    .data = s_landscape_rgb888_map,
};

static void host_sleep_ms(unsigned ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0) {
    }
}

static void demo_landscape_put_px(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= DEMO_LANDSCAPE_W || y >= DEMO_LANDSCAPE_H) {
        return;
    }

    size_t offset = ((size_t)y * DEMO_LANDSCAPE_W + x) * 3U;
    s_landscape_rgb888_map[offset + 0U] = r;
    s_landscape_rgb888_map[offset + 1U] = g;
    s_landscape_rgb888_map[offset + 2U] = b;
}

static void demo_landscape_generate(void)
{
    for (uint32_t y = 0; y < DEMO_LANDSCAPE_H; y++) {
        for (uint32_t x = 0; x < DEMO_LANDSCAPE_W; x++) {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint32_t sky_h = 72U;
            uint32_t mountain_h = 36U;
            uint32_t lake_y0 = sky_h + mountain_h;
            int32_t xi = (int32_t)x;
            int32_t yi = (int32_t)y;

            if (y < sky_h) {
                uint32_t t = (y * 255U) / (sky_h - 1U);
                r = (uint8_t)((72U * (255U - t) + 165U * t) / 255U);
                g = (uint8_t)((152U * (255U - t) + 210U * t) / 255U);
                b = (uint8_t)((220U * (255U - t) + 238U * t) / 255U);

                int32_t dx = xi - 184;
                int32_t dy = yi - 34;
                int32_t dist2 = dx * dx + dy * dy;
                if (dist2 <= 19 * 19) {
                    r = 255U;
                    g = (uint8_t)(214U - (uint32_t)dist2 / 28U);
                    b = 96U;
                }
            } else if (y < lake_y0) {
                int32_t left_peak = 112 - abs(xi - 76) * 58 / 76;
                int32_t right_peak = 116 - abs(xi - 158) * 48 / 82;
                int32_t ridge = left_peak > right_peak ? left_peak : right_peak;

                if (yi <= ridge) {
                    uint32_t shade = (uint32_t)((yi - (int32_t)sky_h) * 120 / (int32_t)mountain_h);
                    r = (uint8_t)(64U + shade / 7U);
                    g = (uint8_t)(83U + shade / 5U);
                    b = (uint8_t)(104U + shade / 4U);

                    if ((xi > 54 && xi < 98 && yi < left_peak - 7) ||
                            (xi > 140 && xi < 178 && yi < right_peak - 7)) {
                        r = 224U;
                        g = 232U;
                        b = 228U;
                    }
                } else {
                    r = 138U;
                    g = 190U;
                    b = 194U;
                }
            } else if (y < 136U) {
                uint32_t t = ((y - lake_y0) * 255U) / (136U - lake_y0);
                r = (uint8_t)((54U * (255U - t) + 28U * t) / 255U);
                g = (uint8_t)((132U * (255U - t) + 98U * t) / 255U);
                b = (uint8_t)((176U * (255U - t) + 150U * t) / 255U);

                if (((x + y * 3U) % 23U) < 3U) {
                    r = (uint8_t)(r + 20U);
                    g = (uint8_t)(g + 25U);
                    b = (uint8_t)(b + 22U);
                }

                int32_t rx = abs(xi - 184);
                if (rx < (yi - (int32_t)lake_y0) / 2 + 8) {
                    r = (uint8_t)(r + 36U);
                    g = (uint8_t)(g + 30U);
                    b = (uint8_t)(b + 8U);
                }
            } else if (y < 142U) {
                r = 196U;
                g = 159U;
                b = 91U;
            } else {
                r = 58U;
                g = 124U;
                b = 72U;
                if (((x * 5U + y * 3U) % 31U) < 5U) {
                    r = 83U;
                    g = 151U;
                    b = 85U;
                }
            }

            demo_landscape_put_px(x, y, r, g, b);
        }
    }
}

static const char *demo_action_name(uint16_t action_idx)
{
    if (action_idx < (uint16_t)(sizeof(s_action_names) / sizeof(s_action_names[0]))) {
        return s_action_names[action_idx];
    }
    return "Unknown";
}

static void demo_apply_action(demo_state_t *state, bool snap)
{
    const char *name;

    if (state == NULL || state->motion == NULL) {
        return;
    }

    if (state->action_idx >= CLAW_MOTION_ACTION_COUNT) {
        state->action_idx = 0;
    }

    name = demo_action_name(state->action_idx);
    (void)gfx_motion_player_set_action(state->motion, state->action_idx, snap);
    (void)gfx_motion_player_set_action_loop(state->motion, true);
    (void)gfx_motion_player_sync(state->motion);

    if (state->button != NULL) {
        (void)gfx_button_set_text_fmt(state->button, "Next: %s", name);
    }
    if (state->list != NULL) {
        (void)gfx_list_set_focus(state->list, state->action_idx);
    }
    if (state->status != NULL) {
        (void)gfx_label_set_text_fmt(state->status, "Motion: %s   Image: RGB888 landscape", name);
    }

    printf("motion action: %u %s\n", (unsigned)state->action_idx, name);
    fflush(stdout);
}

static void demo_next_action(demo_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->action_idx = (uint16_t)((state->action_idx + 1U) % CLAW_MOTION_ACTION_COUNT);
    demo_apply_action(state, true);
}

static void demo_button_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || event == NULL || state == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    demo_next_action(state);
}

static void demo_list_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || state == NULL || focused_index < 0 || focused_index >= CLAW_MOTION_ACTION_COUNT) {
        return;
    }

    state->action_idx = (uint16_t)focused_index;
    demo_apply_action(state, true);
}

static void demo_create_label(gfx_display_t *display, gfx_font_t font, gfx_coord_t x, gfx_coord_t y,
                              uint16_t w, uint16_t h, const char *text, gfx_color_t color)
{
    gfx_object_t *label = gfx_label_create(display);

    if (label == NULL) {
        return;
    }

    (void)gfx_object_set_pos(label, x, y);
    (void)gfx_object_set_size(label, w, h);
    (void)gfx_label_set_text(label, text);
    (void)gfx_label_set_font(label, font);
    (void)gfx_label_set_color(label, color);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
}

static gfx_object_t *demo_create_image_preview(gfx_display_t *display)
{
    gfx_object_t *image = gfx_image_create(display);
    const gfx_image_src_t image_src = {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &s_landscape_rgb888,
    };

    if (image == NULL) {
        return NULL;
    }

    (void)gfx_object_set_pos(image, 48, 140);
    (void)gfx_image_set_source_desc(image, &image_src);
    return image;
}

static void demo_create_anim_panel(gfx_display_t *display, gfx_font_t font, demo_state_t *state)
{
    const char *asset_root = getenv("GFX_ASSET_ROOT");
    const char *anim_name = getenv("GFX_DEMO_ANIM");
    gfx_anim_src_t anim_src;

    if (state == NULL) {
        return;
    }

    if (asset_root == NULL || asset_root[0] == '\0') {
        asset_root = "test_apps/assets_test";
    }
    if (anim_name == NULL || anim_name[0] == '\0') {
        anim_name = "mi_1_eye_8bit.eaf";
    }

    demo_create_label(display, font, 40, 484, 240, 32, "File Anim", GFX_COLOR_HEX(0xA8B3BD));

    gfx_err_t err = gfx_asset_store_open_dir(asset_root, &state->asset_store);
    if (err != GFX_OK) {
        fprintf(stderr, "asset store open failed: %d root=%s\n", err, asset_root);
        return;
    }

    err = gfx_asset_open_by_name(state->asset_store, anim_name, &state->anim_view);
    if (err != GFX_OK) {
        fprintf(stderr, "anim asset open failed: %d file=%s\n", err, anim_name);
        return;
    }

    state->anim = gfx_anim_create(display);
    if (state->anim == NULL) {
        fprintf(stderr, "failed to create anim object\n");
        return;
    }

    anim_src.type = GFX_ANIM_SRC_TYPE_MEMORY;
    anim_src.data = state->anim_view.data;
    anim_src.data_len = state->anim_view.size;
    if (gfx_anim_set_src_desc(state->anim, &anim_src) != GFX_OK) {
        fprintf(stderr, "failed to set anim source: %s size=%zu\n", anim_name, state->anim_view.size);
        return;
    }

    (void)gfx_object_set_pos(state->anim, 40, 524);
    (void)gfx_object_set_size(state->anim, 240, 160);
    (void)gfx_anim_set_segment(state->anim, 0, 0xFFFFFFFF, 30, true);
    (void)gfx_anim_start(state->anim);

    printf("anim asset: root=%s file=%s size=%zu mapped=%s\n",
           asset_root,
           anim_name,
           state->anim_view.size,
           state->anim_view.is_mapped ? "yes" : "no");
    fflush(stdout);
}

int main(void)
{
    demo_state_t state = {0};
    gfx_core_config_t core_cfg = {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    };

    gfx_handle_t gfx = gfx_core_init(&core_cfg);
    if (gfx == NULL) {
        fprintf(stderr, "failed to init gfx core\n");
        return 1;
    }

    gfx_backend_t *backend = gfx_backend_sdl_create(&(gfx_backend_sdl_config_t) {
        .h_res = 720,
        .v_res = 720,
        .scale = 1,
        .title = "ESP Emote GFX SDL Playground",
    });
    if (backend == NULL) {
        fprintf(stderr, "failed to create SDL backend\n");
        gfx_core_deinit(gfx);
        return 1;
    }

    gfx_display_t *display = gfx_display_add(gfx, &(gfx_display_config_t) {
        .h_res = 720,
        .v_res = 720,
        .backend = backend,
        .flags = {
            .full_frame = 1,
            .double_buffer = 1,
        },
    });
    if (display == NULL) {
        fprintf(stderr, "failed to create display\n");
        gfx_backend_sdl_delete(backend);
        gfx_core_deinit(gfx);
        return 1;
    }

    (void)gfx_display_set_bg_color(display, GFX_COLOR_HEX(0x101418));
    gfx_font_t font = gfx_host_font_default();

    demo_create_label(display, font, 36, 24, 500, 44, "GFX Host SDL Playground", GFX_COLOR_HEX(0xF3F7FA));
    demo_create_label(display, font, 40, 88, 264, 36, "RGB888 Landscape", GFX_COLOR_HEX(0xA8B3BD));
    demo_create_label(display, font, 332, 88, 240, 36, "Motion Scene", GFX_COLOR_HEX(0xA8B3BD));

    demo_landscape_generate();
    (void)demo_create_image_preview(display);
    demo_create_anim_panel(display, font, &state);

    state.motion = gfx_motion_player_create(display, &claw_motion_scene_asset);
    if (state.motion != NULL) {
        (void)gfx_motion_player_set_canvas(state.motion, 292, 104, 316, 248);
        (void)gfx_motion_player_set_color(state.motion, GFX_COLOR_HEX(0xFF4D2B));
    } else {
        fprintf(stderr, "failed to create motion player\n");
    }

    state.button = gfx_button_create(display);
    if (state.button != NULL) {
        (void)gfx_object_set_pos(state.button, 40, 372);
        (void)gfx_object_set_size(state.button, 264, 68);
        (void)gfx_button_set_text(state.button, "Next: Move");
        (void)gfx_button_set_font(state.button, font);
        (void)gfx_button_set_bg_color(state.button, GFX_COLOR_HEX(0x245C8F));
        (void)gfx_button_set_bg_color_pressed(state.button, GFX_COLOR_HEX(0x2E7D32));
        (void)gfx_button_set_border_color(state.button, GFX_COLOR_HEX(0x76B7E8));
        (void)gfx_button_set_text_color(state.button, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_object_set_touch_cb(state.button, demo_button_touch_cb, &state);
    }

    state.list = gfx_list_create(display);
    if (state.list != NULL) {
        (void)gfx_object_set_pos(state.list, 332, 364);
        (void)gfx_object_set_size(state.list, 268, 84);
        (void)gfx_list_set_item_height(state.list, 42);
        (void)gfx_list_set_text_pad(state.list, 12, 4);
        for (uint16_t i = 0; i < (uint16_t)(sizeof(s_action_names) / sizeof(s_action_names[0])); i++) {
            (void)gfx_list_add_item(state.list, s_action_names[i]);
        }
        (void)gfx_list_set_font(state.list, font);
        (void)gfx_list_set_bg_color(state.list, GFX_COLOR_HEX(0x171D24));
        (void)gfx_list_set_focus_bg_color(state.list, GFX_COLOR_HEX(0xF1C40F));
        (void)gfx_list_set_text_color(state.list, GFX_COLOR_HEX(0xDCE4EC));
        (void)gfx_list_set_focus_text_color(state.list, GFX_COLOR_HEX(0x101418));
        (void)gfx_list_set_border_color(state.list, GFX_COLOR_HEX(0x3F5163));
        (void)gfx_list_set_focus_cb(state.list, demo_list_focus_cb, &state);
    }

    state.status = gfx_label_create(display);
    if (state.status != NULL) {
        (void)gfx_object_set_pos(state.status, 36, 444);
        (void)gfx_object_set_size(state.status, 564, 32);
        (void)gfx_label_set_font(state.status, font);
        (void)gfx_label_set_color(state.status, GFX_COLOR_HEX(0xA8B3BD));
        (void)gfx_label_set_long_mode(state.status, GFX_LABEL_LONG_CLIP);
    }

    demo_apply_action(&state, true);
    (void)gfx_core_refresh_now(gfx);

    while (!gfx_backend_sdl_poll(display)) {
        host_sleep_ms(16);
    }

    if (state.anim != NULL) {
        (void)gfx_anim_stop(state.anim);
    }
    gfx_asset_view_close(&state.anim_view);
    gfx_asset_store_close(state.asset_store);
    gfx_motion_player_delete(state.motion);
    gfx_core_deinit(gfx);
    return 0;
}
