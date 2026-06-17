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
#include "gfx/widgets/container.h"
#include "gfx/widgets/coverflow.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/label.h"
#include "gfx/widgets/list.h"
#include "gfx/widgets/mesh_image.h"
#include "gfx/widgets/motion.h"
#include "gfx/widgets/pageflow.h"
#include "gfx/widgets/wheel.h"

#include "../../test_apps/main/claw_motion.inc"
#include "gfx_host_demo_images.inc"

typedef struct {
    gfx_motion_player_t *motion;
    gfx_asset_store_t *asset_store;
    gfx_asset_view_t anim_view;
    gfx_object_t *anim;
    gfx_object_t *image;
    gfx_object_t *button_demo;
    gfx_object_t *motion_button;
    gfx_object_t *widget_list;
    gfx_object_t *list_demo;
    gfx_object_t *action_list;
    gfx_object_t *wheel;
    gfx_object_t *pageflow;
    gfx_object_t *coverflow;
    gfx_object_t *cover_cards[4];
    gfx_coverflow_card_dsc_t cover_card_dsc[4];
    gfx_object_t *preview_title;
    gfx_object_t *preview_note;
    gfx_object_t *label_demo;
    gfx_object_t *status;
    uint16_t widget_idx;
    uint16_t action_idx;
} demo_state_t;

typedef enum {
    DEMO_WIDGET_LABEL = 0,
    DEMO_WIDGET_BUTTON,
    DEMO_WIDGET_LIST,
    DEMO_WIDGET_IMAGE,
    DEMO_WIDGET_ANIM,
    DEMO_WIDGET_MOTION,
    DEMO_WIDGET_WHEEL,
    DEMO_WIDGET_PAGEFLOW,
    DEMO_WIDGET_COVERFLOW,
} demo_widget_id_t;

static const char *const s_widget_names[] = {
    "Label",
    "Button",
    "List",
    "Image",
    "Anim",
    "Motion",
    "Wheel",
    "Pageflow",
    "Coverflow",
};

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

static const char *const s_cover_card_titles[] = {
    "Misty Ridge",
    "Warm Harbor",
    "Quiet Trail",
    "Night Lake",
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

static const char *demo_widget_name(uint16_t widget_idx)
{
    if (widget_idx < (uint16_t)(sizeof(s_widget_names) / sizeof(s_widget_names[0]))) {
        return s_widget_names[widget_idx];
    }
    return "Unknown";
}

static void demo_set_object_visible(gfx_object_t *obj, bool visible)
{
    if (obj != NULL) {
        (void)gfx_object_set_visible(obj, visible);
    }
}

static void demo_hide_preview_objects(demo_state_t *state)
{
    if (state == NULL) {
        return;
    }

    demo_set_object_visible(state->label_demo, false);
    demo_set_object_visible(state->button_demo, false);
    demo_set_object_visible(state->motion_button, false);
    demo_set_object_visible(state->list_demo, false);
    demo_set_object_visible(state->action_list, false);
    demo_set_object_visible(state->wheel, false);
    demo_set_object_visible(state->pageflow, false);
    demo_set_object_visible(state->coverflow, false);
    demo_set_object_visible(state->image, false);
    demo_set_object_visible(state->anim, false);
    demo_set_object_visible(state->preview_note, false);
    if (state->motion != NULL) {
        (void)gfx_motion_player_set_visible(state->motion, false);
    }
}

static void demo_update_status(demo_state_t *state)
{
    if (state == NULL || state->status == NULL) {
        return;
    }

    (void)gfx_label_set_text_fmt(state->status,
                                 "Widget: %s   Motion: %s   Image: RGB888",
                                 demo_widget_name(state->widget_idx),
                                 demo_action_name(state->action_idx));
}

static void demo_apply_widget_focus(demo_state_t *state, uint16_t widget_idx)
{
    if (state == NULL) {
        return;
    }

    if (widget_idx >= (uint16_t)(sizeof(s_widget_names) / sizeof(s_widget_names[0]))) {
        widget_idx = 0;
    }

    state->widget_idx = widget_idx;
    demo_hide_preview_objects(state);

    if (state->preview_title != NULL) {
        (void)gfx_label_set_text_fmt(state->preview_title, "%s Preview", demo_widget_name(widget_idx));
    }

    switch ((demo_widget_id_t)widget_idx) {
    case DEMO_WIDGET_LABEL:
        demo_set_object_visible(state->label_demo, true);
        break;
    case DEMO_WIDGET_BUTTON:
        demo_set_object_visible(state->button_demo, true);
        break;
    case DEMO_WIDGET_LIST:
        demo_set_object_visible(state->list_demo, true);
        break;
    case DEMO_WIDGET_IMAGE:
        demo_set_object_visible(state->image, true);
        break;
    case DEMO_WIDGET_ANIM:
        demo_set_object_visible(state->anim, true);
        break;
    case DEMO_WIDGET_MOTION:
        if (state->motion != NULL) {
            (void)gfx_motion_player_set_visible(state->motion, true);
        }
        demo_set_object_visible(state->motion_button, true);
        demo_set_object_visible(state->action_list, true);
        break;
    case DEMO_WIDGET_WHEEL:
        demo_set_object_visible(state->wheel, true);
        break;
    case DEMO_WIDGET_PAGEFLOW:
        demo_set_object_visible(state->pageflow, true);
        break;
    case DEMO_WIDGET_COVERFLOW:
        demo_set_object_visible(state->coverflow, true);
        break;
    default:
        if (state->preview_note != NULL) {
            (void)gfx_label_set_text_fmt(state->preview_note, "%s is planned in TODO", demo_widget_name(widget_idx));
            demo_set_object_visible(state->preview_note, true);
        }
        break;
    }

    demo_update_status(state);
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

    if (state->motion_button != NULL) {
        (void)gfx_button_set_text_fmt(state->motion_button, "Next: %s", name);
    }
    if (state->action_list != NULL) {
        (void)gfx_list_set_focus(state->action_list, state->action_idx);
    }
    demo_update_status(state);

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

static void demo_motion_button_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || event == NULL || state == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    demo_next_action(state);
}

static void demo_button_preview_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || event == NULL || state == NULL || event->type != GFX_TOUCH_EVENT_RELEASE) {
        return;
    }

    if (state->status != NULL) {
        (void)gfx_label_set_text(state->status, "Button: released");
    }
}

static void demo_widget_list_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || state == NULL || focused_index < 0 ||
            focused_index >= (int32_t)(sizeof(s_widget_names) / sizeof(s_widget_names[0]))) {
        return;
    }

    state->widget_idx = (uint16_t)focused_index;
    (void)gfx_list_set_selected(obj, focused_index);
    demo_apply_widget_focus(state, state->widget_idx);
}

static void demo_action_list_focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    if (obj == NULL || state == NULL || focused_index < 0 || focused_index >= CLAW_MOTION_ACTION_COUNT) {
        return;
    }

    state->action_idx = (uint16_t)focused_index;
    (void)gfx_list_set_selected(obj, focused_index);
    demo_apply_action(state, true);
}

static void demo_wheel_value_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;
    const char *text;

    if (obj == NULL || state == NULL || selected_index < 0) {
        return;
    }

    text = gfx_wheel_get_item_text(obj, (uint16_t)selected_index);
    if (state->status != NULL) {
        (void)gfx_label_set_text_fmt(state->status, "Wheel: %s   index=%ld",
                                     text ? text : "Unknown", (long)selected_index);
    }
}

static void demo_pageflow_changed_cb(gfx_object_t *obj, int32_t page_index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    (void)obj;
    if (state != NULL && state->status != NULL) {
        (void)gfx_label_set_text_fmt(state->status, "Pageflow: page %ld", (long)page_index);
    }
}

static void demo_coverflow_changed_cb(gfx_object_t *obj, int32_t index, void *user_data)
{
    demo_state_t *state = (demo_state_t *)user_data;

    (void)obj;
    if (state != NULL && state->status != NULL) {
        (void)gfx_label_set_text_fmt(state->status, "Coverflow: item %ld", (long)index);
    }
}

static gfx_object_t *demo_create_label(gfx_display_t *display, gfx_font_t font, gfx_coord_t x, gfx_coord_t y,
                                       uint16_t w, uint16_t h, const char *text, gfx_color_t color)
{
    gfx_object_t *label = gfx_label_create(display);

    if (label == NULL) {
        return NULL;
    }

    (void)gfx_object_set_pos(label, x, y);
    (void)gfx_object_set_size(label, w, h);
    (void)gfx_label_set_text(label, text);
    (void)gfx_label_set_font(label, font);
    (void)gfx_label_set_color(label, color);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
    return label;
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

    (void)gfx_object_set_pos(image, 300, 148);
    (void)gfx_image_set_source_desc(image, &image_src);
    return image;
}

static gfx_object_t *demo_create_cover_card(gfx_display_t *display, gfx_font_t font,
                                            const gfx_image_dsc_t *image_dsc, const char *title,
                                            gfx_object_t **out_image, gfx_object_t **out_label)
{
    gfx_object_t *card = gfx_container_create(display);
    gfx_object_t *image = gfx_mesh_img_create(display);
    gfx_object_t *label = gfx_label_create(display);

    if (card == NULL || image == NULL || label == NULL) {
        return card;
    }

    (void)gfx_container_set_bg_color(card, GFX_COLOR_HEX(0x17212B));
    (void)gfx_container_set_border_color(card, GFX_COLOR_HEX(0x7EC8E3));
    (void)gfx_container_set_border_width(card, 2);
    (void)gfx_mesh_img_set_image_rect(image, image_dsc, 120, 78);
    (void)gfx_label_set_font(label, font);
    (void)gfx_label_set_text(label, title);
    (void)gfx_label_set_color(label, GFX_COLOR_HEX(0xF3F7FA));
    (void)gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
    (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);

    (void)gfx_object_add_child(card, image);
    (void)gfx_object_add_child(card, label);
    (void)gfx_object_set_visible(card, false);
    if (out_image != NULL) {
        *out_image = image;
    }
    if (out_label != NULL) {
        *out_label = label;
    }
    return card;
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

    (void)gfx_object_set_pos(state->anim, 300, 180);
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
        .color_format = GFX_COLOR_FORMAT_RGB888,
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

    (void)demo_create_label(display, font, 32, 24, 640, 44, "GFX SDL Playground", GFX_COLOR_HEX(0xF3F7FA));
    (void)demo_create_label(display, font, 32, 86, 220, 36, "Widgets", GFX_COLOR_HEX(0xA8B3BD));
    state.preview_title = demo_create_label(display, font, 284, 86, 380, 36, "Label Preview", GFX_COLOR_HEX(0xA8B3BD));
    state.preview_note = demo_create_label(display, font, 284, 154, 380, 48, "", GFX_COLOR_HEX(0xDCE4EC));
    state.label_demo = demo_create_label(display, font, 300, 170, 360, 64, "Hello from gfx_label", GFX_COLOR_HEX(0xF3F7FA));

    demo_landscape_generate();
    state.image = demo_create_image_preview(display);
    demo_create_anim_panel(display, font, &state);

    state.motion = gfx_motion_player_create(display, &claw_motion_scene_asset);
    if (state.motion != NULL) {
        (void)gfx_motion_player_set_canvas(state.motion, 300, 126, 340, 270);
        (void)gfx_motion_player_set_color(state.motion, GFX_COLOR_HEX(0xFF4D2B));
    } else {
        fprintf(stderr, "failed to create motion player\n");
    }

    state.widget_list = gfx_list_create(display);
    if (state.widget_list != NULL) {
        (void)gfx_object_set_pos(state.widget_list, 28, 128);
        (void)gfx_object_set_size(state.widget_list, 224, 552);
        (void)gfx_list_set_item_height(state.widget_list, 56);
        (void)gfx_list_set_text_pad(state.widget_list, 16, 9);
        for (uint16_t i = 0; i < (uint16_t)(sizeof(s_widget_names) / sizeof(s_widget_names[0])); i++) {
            (void)gfx_list_add_item(state.widget_list, s_widget_names[i]);
        }
        (void)gfx_list_set_font(state.widget_list, font);
        (void)gfx_list_set_bg_color(state.widget_list, GFX_COLOR_HEX(0x161C22));
        (void)gfx_list_set_focus_bg_color(state.widget_list, GFX_COLOR_HEX(0x2E7D32));
        (void)gfx_list_set_selected_bg_color(state.widget_list, GFX_COLOR_HEX(0x24476A));
        (void)gfx_list_set_pressed_bg_color(state.widget_list, GFX_COLOR_HEX(0x375D7D));
        (void)gfx_list_set_text_color(state.widget_list, GFX_COLOR_HEX(0xDCE4EC));
        (void)gfx_list_set_focus_text_color(state.widget_list, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_selected_text_color(state.widget_list, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_pressed_text_color(state.widget_list, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_border_color(state.widget_list, GFX_COLOR_HEX(0x40515F));
        (void)gfx_list_set_drag_threshold(state.widget_list, 8);
        (void)gfx_list_set_snap_to_item(state.widget_list, true);
        (void)gfx_list_set_focus_cb(state.widget_list, demo_widget_list_focus_cb, &state);
    }

    state.button_demo = gfx_button_create(display);
    if (state.button_demo != NULL) {
        (void)gfx_object_set_pos(state.button_demo, 330, 198);
        (void)gfx_object_set_size(state.button_demo, 260, 78);
        (void)gfx_button_set_text(state.button_demo, "Press Button");
        (void)gfx_button_set_font(state.button_demo, font);
        (void)gfx_button_set_bg_color(state.button_demo, GFX_COLOR_HEX(0x245C8F));
        (void)gfx_button_set_bg_color_pressed(state.button_demo, GFX_COLOR_HEX(0x2E7D32));
        (void)gfx_button_set_border_color(state.button_demo, GFX_COLOR_HEX(0x76B7E8));
        (void)gfx_button_set_text_color(state.button_demo, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_object_set_touch_cb(state.button_demo, demo_button_preview_touch_cb, &state);
    }

    state.motion_button = gfx_button_create(display);
    if (state.motion_button != NULL) {
        (void)gfx_object_set_pos(state.motion_button, 300, 420);
        (void)gfx_object_set_size(state.motion_button, 218, 66);
        (void)gfx_button_set_text(state.motion_button, "Next: Move");
        (void)gfx_button_set_font(state.motion_button, font);
        (void)gfx_button_set_bg_color(state.motion_button, GFX_COLOR_HEX(0xB64A2E));
        (void)gfx_button_set_bg_color_pressed(state.motion_button, GFX_COLOR_HEX(0x2E7D32));
        (void)gfx_button_set_border_color(state.motion_button, GFX_COLOR_HEX(0xF6B48C));
        (void)gfx_button_set_text_color(state.motion_button, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_object_set_touch_cb(state.motion_button, demo_motion_button_touch_cb, &state);
    }

    state.list_demo = gfx_list_create(display);
    if (state.list_demo != NULL) {
        static const char *const list_items[] = {
            "Status card", "Quick action", "Network", "Display", "Storage", "About",
            "Audio", "Battery", "Weather", "Schedule", "Messages", "System",
        };

        (void)gfx_object_set_pos(state.list_demo, 312, 152);
        (void)gfx_object_set_size(state.list_demo, 300, 300);
        (void)gfx_list_set_items(state.list_demo, list_items, (uint16_t)(sizeof(list_items) / sizeof(list_items[0])));
        (void)gfx_list_set_font(state.list_demo, font);
        (void)gfx_list_set_item_height(state.list_demo, 54);
        (void)gfx_list_set_text_pad(state.list_demo, 16, 9);
        (void)gfx_list_set_bg_color(state.list_demo, GFX_COLOR_HEX(0x171D24));
        (void)gfx_list_set_focus_bg_color(state.list_demo, GFX_COLOR_HEX(0xF1C40F));
        (void)gfx_list_set_selected_bg_color(state.list_demo, GFX_COLOR_HEX(0x245C8F));
        (void)gfx_list_set_pressed_bg_color(state.list_demo, GFX_COLOR_HEX(0x3F6FA3));
        (void)gfx_list_set_text_color(state.list_demo, GFX_COLOR_HEX(0xDCE4EC));
        (void)gfx_list_set_focus_text_color(state.list_demo, GFX_COLOR_HEX(0x101418));
        (void)gfx_list_set_selected_text_color(state.list_demo, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_pressed_text_color(state.list_demo, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_border_color(state.list_demo, GFX_COLOR_HEX(0x3F5163));
        (void)gfx_list_set_drag_threshold(state.list_demo, 8);
        (void)gfx_list_set_snap_to_item(state.list_demo, true);
        (void)gfx_list_set_selected(state.list_demo, 1);
    }

    state.action_list = gfx_list_create(display);
    if (state.action_list != NULL) {
        (void)gfx_object_set_pos(state.action_list, 536, 420);
        (void)gfx_object_set_size(state.action_list, 154, 232);
        (void)gfx_list_set_item_height(state.action_list, 46);
        (void)gfx_list_set_text_pad(state.action_list, 10, 7);
        for (uint16_t i = 0; i < (uint16_t)(sizeof(s_action_names) / sizeof(s_action_names[0])); i++) {
            (void)gfx_list_add_item(state.action_list, s_action_names[i]);
        }
        (void)gfx_list_set_font(state.action_list, font);
        (void)gfx_list_set_bg_color(state.action_list, GFX_COLOR_HEX(0x171D24));
        (void)gfx_list_set_focus_bg_color(state.action_list, GFX_COLOR_HEX(0xF1C40F));
        (void)gfx_list_set_selected_bg_color(state.action_list, GFX_COLOR_HEX(0x245C8F));
        (void)gfx_list_set_pressed_bg_color(state.action_list, GFX_COLOR_HEX(0x3F6FA3));
        (void)gfx_list_set_text_color(state.action_list, GFX_COLOR_HEX(0xDCE4EC));
        (void)gfx_list_set_focus_text_color(state.action_list, GFX_COLOR_HEX(0x101418));
        (void)gfx_list_set_selected_text_color(state.action_list, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_pressed_text_color(state.action_list, GFX_COLOR_HEX(0xFFFFFF));
        (void)gfx_list_set_border_color(state.action_list, GFX_COLOR_HEX(0x3F5163));
        (void)gfx_list_set_drag_threshold(state.action_list, 8);
        (void)gfx_list_set_snap_to_item(state.action_list, true);
        (void)gfx_list_set_focus_cb(state.action_list, demo_action_list_focus_cb, &state);
    }

    state.wheel = gfx_wheel_create(display);
    if (state.wheel != NULL) {
        static const char *const wheel_items[] = {
            "Low", "Medium", "High", "Turbo", "Sleep", "Focus", "Play",
        };

        (void)gfx_object_set_pos(state.wheel, 320, 154);
        (void)gfx_object_set_size(state.wheel, 300, 272);
        (void)gfx_wheel_set_items(state.wheel, wheel_items, (uint16_t)(sizeof(wheel_items) / sizeof(wheel_items[0])));
        (void)gfx_wheel_set_font(state.wheel, font);
        (void)gfx_wheel_set_item_height(state.wheel, 52);
        (void)gfx_wheel_set_visible_rows(state.wheel, 5);
        (void)gfx_wheel_set_cyclic(state.wheel, true);
        (void)gfx_wheel_set_drag_threshold(state.wheel, 8);
        (void)gfx_wheel_set_bg_color(state.wheel, GFX_COLOR_HEX(0x171D24));
        (void)gfx_wheel_set_text_color(state.wheel, GFX_COLOR_HEX(0x9AA7B2));
        (void)gfx_wheel_set_center_bg_color(state.wheel, GFX_COLOR_HEX(0xF1C40F));
        (void)gfx_wheel_set_center_text_color(state.wheel, GFX_COLOR_HEX(0x101418));
        (void)gfx_wheel_set_border_color(state.wheel, GFX_COLOR_HEX(0x3F5163));
        (void)gfx_wheel_set_value_cb(state.wheel, demo_wheel_value_cb, &state);
        (void)gfx_wheel_set_selected(state.wheel, 1);
    }

    state.pageflow = gfx_pageflow_create(display);
    if (state.pageflow != NULL) {
        (void)gfx_object_set_pos(state.pageflow, 300, 148);
        (void)gfx_object_set_size(state.pageflow, 340, 220);
        (void)gfx_pageflow_set_image_pages(state.pageflow, s_demo_flow_images,
                                           (uint16_t)(sizeof(s_demo_flow_images) / sizeof(s_demo_flow_images[0])));
        (void)gfx_pageflow_set_font(state.pageflow, font);
        (void)gfx_pageflow_set_drag_threshold(state.pageflow, 8);
        (void)gfx_pageflow_set_page_threshold(state.pageflow, 56);
        (void)gfx_pageflow_set_bg_color(state.pageflow, GFX_COLOR_HEX(0x101418));
        (void)gfx_pageflow_set_page_color(state.pageflow, GFX_COLOR_HEX(0x1F2A35));
        (void)gfx_pageflow_set_text_color(state.pageflow, GFX_COLOR_HEX(0xF3F7FA));
        (void)gfx_pageflow_set_border_color(state.pageflow, GFX_COLOR_HEX(0x76B7E8));
        (void)gfx_pageflow_set_changed_cb(state.pageflow, demo_pageflow_changed_cb, &state);
    }

    state.coverflow = gfx_coverflow_create(display);
    if (state.coverflow != NULL) {
        uint16_t cover_count = (uint16_t)(sizeof(s_demo_flow_images) / sizeof(s_demo_flow_images[0]));
        if (cover_count > (uint16_t)(sizeof(state.cover_cards) / sizeof(state.cover_cards[0]))) {
            cover_count = (uint16_t)(sizeof(state.cover_cards) / sizeof(state.cover_cards[0]));
        }

        (void)gfx_object_set_pos(state.coverflow, 284, 148);
        (void)gfx_object_set_size(state.coverflow, 390, 246);
        (void)gfx_coverflow_set_font(state.coverflow, font);
        (void)gfx_coverflow_set_drag_threshold(state.coverflow, 8);
        (void)gfx_coverflow_set_page_threshold(state.coverflow, 50);
        (void)gfx_coverflow_set_zoom(state.coverflow, 112, 58);
        (void)gfx_coverflow_set_spacing(state.coverflow, 38);
        (void)gfx_coverflow_set_side_dim(state.coverflow, 92);
        (void)gfx_coverflow_set_bg_color(state.coverflow, GFX_COLOR_HEX(0x101418));
        (void)gfx_coverflow_set_center_color(state.coverflow, GFX_COLOR_HEX(0x245C8F));
        (void)gfx_coverflow_set_side_color(state.coverflow, GFX_COLOR_HEX(0x1F2A35));
        (void)gfx_coverflow_set_text_color(state.coverflow, GFX_COLOR_HEX(0xF3F7FA));
        (void)gfx_coverflow_set_border_color(state.coverflow, GFX_COLOR_HEX(0x76B7E8));
        (void)gfx_coverflow_set_changed_cb(state.coverflow, demo_coverflow_changed_cb, &state);
        for (uint16_t i = 0; i < cover_count; i++) {
            gfx_object_t *image = NULL;
            gfx_object_t *label = NULL;
            state.cover_cards[i] = demo_create_cover_card(display, font, s_demo_flow_images[i],
                                  s_cover_card_titles[i], &image, &label);
            state.cover_card_dsc[i] = (gfx_coverflow_card_dsc_t) {
                .card = state.cover_cards[i],
                .image_slot = image,
                .title_slot = label,
            };
        }
        (void)gfx_coverflow_set_card_descriptors(state.coverflow, state.cover_card_dsc, cover_count);
        (void)gfx_coverflow_set_selected(state.coverflow, 1);
    }

    state.status = gfx_label_create(display);
    if (state.status != NULL) {
        (void)gfx_object_set_pos(state.status, 284, 650);
        (void)gfx_object_set_size(state.status, 390, 34);
        (void)gfx_label_set_font(state.status, font);
        (void)gfx_label_set_color(state.status, GFX_COLOR_HEX(0xA8B3BD));
        (void)gfx_label_set_long_mode(state.status, GFX_LABEL_LONG_CLIP);
    }

    demo_apply_action(&state, true);
    demo_apply_widget_focus(&state, 0);
    if (state.widget_list != NULL) {
        (void)gfx_list_set_focus(state.widget_list, state.widget_idx);
        (void)gfx_list_set_selected(state.widget_list, state.widget_idx);
    }
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
