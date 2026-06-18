/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/container.h"
#include "gfx/widgets/coverflow.h"

static uint8_t s_img0_pixels[24 * 16 * 3];
static uint8_t s_img1_pixels[24 * 16 * 3];
static uint8_t s_img2_pixels[24 * 16 * 3];

static const gfx_image_dsc_t s_img0 = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB888,
        .w = 24,
        .h = 16,
        .stride = 24 * 3,
    },
    .data_size = sizeof(s_img0_pixels),
    .data = s_img0_pixels,
};

static const gfx_image_dsc_t s_img1 = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB888,
        .w = 24,
        .h = 16,
        .stride = 24 * 3,
    },
    .data_size = sizeof(s_img1_pixels),
    .data = s_img1_pixels,
};

static const gfx_image_dsc_t s_img2 = {
    .header = {
        .magic = GFX_IMAGE_HEADER_MAGIC,
        .cf = GFX_COLOR_FORMAT_RGB888,
        .w = 24,
        .h = 16,
        .stride = 24 * 3,
    },
    .data_size = sizeof(s_img2_pixels),
    .data = s_img2_pixels,
};

static const gfx_image_dsc_t *const s_images[] = {
    &s_img0,
    &s_img1,
    &s_img2,
};

static void fill_image(uint8_t *pixels, uint8_t r, uint8_t g, uint8_t b)
{
    for (size_t i = 0; i < 24U * 16U; i++) {
        pixels[i * 3U + 0U] = r;
        pixels[i * 3U + 1U] = g;
        pixels[i * 3U + 2U] = b;
    }
}

static void expect_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "coverflow smoke failed: %s\n", message);
        exit(1);
    }
}

typedef struct {
    int count;
    int last_index;
} changed_trace_t;

static void changed_cb(gfx_object_t *obj, int32_t index, void *user_data)
{
    changed_trace_t *trace = (changed_trace_t *)user_data;
    (void)obj;

    if (trace == NULL) {
        return;
    }
    trace->count++;
    trace->last_index = (int)index;
}

static void inject_touch(gfx_display_t *disp, gfx_touch_event_type_t type, uint16_t x, uint16_t y, uint32_t tick)
{
    expect_true(gfx_touch_inject(disp, &(gfx_touch_event_t) {
        .type = type,
        .x = x,
        .y = y,
        .strength = type == GFX_TOUCH_EVENT_RELEASE ? 0 : 1,
        .timestamp_ms = tick,
    }) == GFX_OK, "touch inject");
}

static void tick_many(gfx_handle_t gfx, int count)
{
    for (int i = 0; i < count; i++) {
        expect_true(gfx_core_tick(gfx) == GFX_OK, "core tick");
    }
}

static void sleep_ms(unsigned ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };

    while (nanosleep(&req, &req) != 0) {
    }
}

int main(void)
{
    changed_trace_t trace = {
        .count = 0,
        .last_index = -1,
    };

    fill_image(s_img0_pixels, 0x40, 0x70, 0xC0);
    fill_image(s_img1_pixels, 0x90, 0xD0, 0x60);
    fill_image(s_img2_pixels, 0xD0, 0x60, 0x80);

    gfx_handle_t gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    expect_true(gfx != NULL, "core init");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 180,
        .v_res = 120,
    });
    expect_true(backend != NULL, "memory backend create");

    gfx_display_t *disp = gfx_display_add(gfx, &(gfx_display_config_t) {
        .h_res = 180,
        .v_res = 120,
        .backend = backend,
        .flags = {
            .full_frame = 1,
        },
    });
    expect_true(disp != NULL, "display add");

    gfx_object_t *cover = gfx_coverflow_create(disp);
    expect_true(cover != NULL, "coverflow create");
    expect_true(gfx_object_set_pos(cover, 10, 10) == GFX_OK, "set pos");
    expect_true(gfx_object_set_size(cover, 160, 90) == GFX_OK, "set size");
    expect_true(gfx_coverflow_set_image_items(cover, s_images, (uint16_t)(sizeof(s_images) / sizeof(s_images[0]))) == GFX_OK,
                "set image items");
    expect_true(gfx_coverflow_set_zoom(cover, 112, 58) == GFX_OK, "set zoom");
    expect_true(gfx_coverflow_set_spacing(cover, 38) == GFX_OK, "set spacing");
    expect_true(gfx_coverflow_set_side_dim(cover, 92) == GFX_OK, "set side dim");
    expect_true(gfx_coverflow_set_selected(cover, 1) == GFX_OK, "set selected");
    expect_true(gfx_coverflow_set_changed_cb(cover, changed_cb, &trace) == GFX_OK, "set changed cb");

    gfx_core_refresh_now(gfx);
    expect_true(gfx_coverflow_get_selected(cover) == 1, "selected getter");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 90, 55, 10);
    inject_touch(disp, GFX_TOUCH_EVENT_MOVE, 20, 55, 20);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 20, 55, 30);
    expect_true(gfx_coverflow_get_selected(cover) == 1, "release keeps current selection until tween end");
    expect_true(trace.count == 0, "release does not emit changed callback immediately");
    sleep_ms(220);
    tick_many(gfx, 16);
    expect_true(gfx_coverflow_get_selected(cover) == 2, "drag left selects next");
    expect_true(trace.count == 1 && trace.last_index == 2, "tween completion emits changed callback");

    gfx_object_t *card0 = gfx_container_create(disp);
    gfx_object_t *card1 = gfx_container_create(disp);
    gfx_object_t *card2 = gfx_container_create(disp);
    gfx_object_t *card1_title = gfx_container_create(disp);
    gfx_object_t *card1_image = gfx_container_create(disp);
    gfx_object_t *cards[] = {
        card0,
        card1,
        card2,
    };
    gfx_coverflow_card_dsc_t card_dsc[] = {
        {
            .card = card0,
        },
        {
            .card = card1,
            .image_slot = card1_image,
            .title_slot = card1_title,
        },
        {
            .card = card2,
        },
    };
    expect_true(card0 != NULL && card1 != NULL && card2 != NULL &&
                card1_title != NULL && card1_image != NULL, "card containers create");
    expect_true(gfx_object_add_child(card1, card1_title) == GFX_OK, "add reversed title child");
    expect_true(gfx_object_add_child(card1, card1_image) == GFX_OK, "add reversed image child");
    expect_true(gfx_coverflow_set_card_items(cover, cards, (uint16_t)(sizeof(cards) / sizeof(cards[0]))) == GFX_OK,
                "set card items");
    expect_true(gfx_coverflow_set_selected(cover, 1) == GFX_OK, "set selected card mode");
    gfx_core_refresh_now(gfx);
    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 90, 55, 40);
    inject_touch(disp, GFX_TOUCH_EVENT_MOVE, 20, 55, 50);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 20, 55, 60);
    sleep_ms(220);
    tick_many(gfx, 16);
    expect_true(gfx_coverflow_get_selected(cover) == 2, "card mode drag left selects next");

    expect_true(gfx_coverflow_set_card_descriptors(cover, card_dsc,
                (uint16_t)(sizeof(card_dsc) / sizeof(card_dsc[0]))) == GFX_OK,
                "set card descriptors");
    expect_true(gfx_coverflow_set_selected(cover, 1) == GFX_OK, "set selected descriptor mode");
    gfx_core_refresh_now(gfx);
    gfx_coord_t image_x = 0;
    gfx_coord_t image_y = 0;
    gfx_coord_t title_x = 0;
    gfx_coord_t title_y = 0;
    uint16_t image_w = 0;
    uint16_t image_h = 0;
    uint16_t title_w = 0;
    uint16_t title_h = 0;
    expect_true(gfx_object_get_pos(card1_image, &image_x, &image_y) == GFX_OK, "get explicit image pos");
    expect_true(gfx_object_get_pos(card1_title, &title_x, &title_y) == GFX_OK, "get explicit title pos");
    expect_true(gfx_object_get_size(card1_image, &image_w, &image_h) == GFX_OK, "get explicit image size");
    expect_true(gfx_object_get_size(card1_title, &title_w, &title_h) == GFX_OK, "get explicit title size");
    expect_true(image_h > 0 && title_h > 0, "explicit slots get non-empty areas");
    expect_true(image_y < title_y, "explicit image slot stays above title slot");
    expect_true(image_w == title_w, "explicit slots share card content width");

    gfx_core_deinit(gfx);
    return 0;
}
