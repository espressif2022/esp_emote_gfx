/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/button.h"
#include "gfx/widgets/container.h"
#include "core/display/gfx_display_priv.h"

static int s_failures;
static int s_button_events;

static void expect_true(bool condition, const char *msg)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", msg);
        s_failures++;
    }
}

static void button_touch_cb(gfx_object_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    (void)obj;
    (void)user_data;

    if (event != NULL) {
        s_button_events++;
    }
}

static void inject_touch(gfx_display_t *disp, gfx_touch_event_type_t type, uint16_t x, uint16_t y, uint32_t tick)
{
    expect_true(gfx_touch_inject(disp, &(gfx_touch_event_t) {
        .type = type,
        .x = x,
        .y = y,
        .track_id = 1,
        .timestamp_ms = tick,
    }) == GFX_OK, "touch inject succeeds");
}

static bool dirty_contains(gfx_display_t *disp, gfx_coord_t x1, gfx_coord_t y1, gfx_coord_t x2, gfx_coord_t y2)
{
    if (disp == NULL) {
        return false;
    }

    for (uint8_t i = 0; i < disp->dirty.count; i++) {
        const gfx_area_t *area = &disp->dirty.areas[i];
        if (area->x1 <= x1 && area->y1 <= y1 && area->x2 >= x2 && area->y2 >= y2) {
            return true;
        }
    }

    return false;
}

static void clear_dirty(gfx_display_t *disp)
{
    if (disp != NULL) {
        disp->dirty.count = 0;
    }
}

int main(void)
{
    gfx_handle_t handle = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    expect_true(handle != NULL, "core init");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 240,
        .v_res = 180,
    });
    expect_true(backend != NULL, "memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 240,
        .v_res = 180,
        .backend = backend,
        .flags = {
            .full_frame = true,
        },
    });
    expect_true(disp != NULL, "display create");

    gfx_object_t *container = gfx_container_create(disp);
    gfx_object_t *button = gfx_button_create(disp);
    gfx_object_t *aligned = gfx_button_create(disp);
    gfx_object_t *clipped = gfx_button_create(disp);
    expect_true(container != NULL, "container create");
    expect_true(button != NULL, "button create");
    expect_true(aligned != NULL, "aligned button create");
    expect_true(clipped != NULL, "clipped button create");

    expect_true(gfx_object_set_pos(container, 20, 20) == GFX_OK, "container pos");
    expect_true(gfx_object_set_size(container, 140, 96) == GFX_OK, "container size");
    expect_true(gfx_container_set_bg_color(container, GFX_COLOR_HEX(0x18202A)) == GFX_OK, "container bg");
    expect_true(gfx_container_set_border_width(container, 2) == GFX_OK, "container border");

    expect_true(gfx_object_set_pos(button, 24, 32) == GFX_OK, "button local pos");
    expect_true(gfx_object_set_size(button, 80, 34) == GFX_OK, "button size");
    expect_true(gfx_button_set_text(button, "Child") == GFX_OK, "button text");
    expect_true(gfx_object_set_touch_cb(button, button_touch_cb, NULL) == GFX_OK, "button touch cb");

    expect_true(gfx_object_add_child(container, button) == GFX_OK, "add child");
    expect_true(gfx_object_get_parent(button) == container, "parent set");

    expect_true(gfx_object_set_size(aligned, 40, 20) == GFX_OK, "aligned size");
    expect_true(gfx_button_set_text(aligned, "A") == GFX_OK, "aligned text");
    expect_true(gfx_object_add_child(container, aligned) == GFX_OK, "add aligned child");
    expect_true(gfx_object_align(aligned, GFX_ALIGN_CENTER, 0, 0) == GFX_OK, "align child to parent");
    gfx_core_refresh_now(handle);
    gfx_coord_t aligned_x = 0;
    gfx_coord_t aligned_y = 0;
    expect_true(gfx_object_get_pos(aligned, &aligned_x, &aligned_y) == GFX_OK, "aligned pos get");
    expect_true(aligned_x == 70 && aligned_y == 58, "align uses parent area");

    expect_true(gfx_object_set_size(clipped, 50, 30) == GFX_OK, "clipped size");
    expect_true(gfx_object_set_pos(clipped, 130, 80) == GFX_OK, "clipped local pos");
    expect_true(gfx_button_set_text(clipped, "Clip") == GFX_OK, "clipped text");
    expect_true(gfx_object_set_touch_cb(clipped, button_touch_cb, NULL) == GFX_OK, "clipped touch cb");
    expect_true(gfx_object_add_child(container, clipped) == GFX_OK, "add clipped child");
    expect_true(gfx_container_set_clip_children(container, true) == GFX_OK, "enable child clipping");

    clear_dirty(disp);
    expect_true(gfx_object_set_pos(clipped, 130, 80) == GFX_OK, "clipped pos invalidates");
    expect_true(dirty_contains(disp, 150, 100, 159, 115), "clipped dirty keeps visible intersection");
    expect_true(!dirty_contains(disp, 150, 100, 199, 129), "clipped dirty does not expose full child area");

    clear_dirty(disp);
    expect_true(gfx_object_set_visible(container, false) == GFX_OK, "hide container");
    expect_true(dirty_contains(disp, 44, 52, 123, 85), "hide parent invalidates child area");
    expect_true(gfx_object_set_visible(container, true) == GFX_OK, "show container");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 54, 62, 10);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 54, 62, 20);
    expect_true(s_button_events == 2, "child receives touch through container tree");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 154, 108, 30);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 154, 108, 40);
    expect_true(s_button_events == 4, "clipped child receives touch inside container");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 170, 110, 50);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 170, 110, 60);
    expect_true(s_button_events == 4, "clipped child ignores touch outside container");

    clear_dirty(disp);
    expect_true(gfx_object_set_pos(container, 24, 24) == GFX_OK, "move container");
    expect_true(dirty_contains(disp, 44, 52, 123, 85), "move parent invalidates child area");

    expect_true(gfx_object_remove_child(container, button) == GFX_OK, "remove child");
    expect_true(gfx_object_get_parent(button) == NULL, "parent cleared");
    expect_true(gfx_object_add_child(container, button) == GFX_OK, "re-add child");

    expect_true(gfx_object_delete(container) == GFX_OK, "delete parent cascades children");
    gfx_core_deinit(handle);

    if (s_failures != 0) {
        fprintf(stderr, "container smoke failed: %d failures\n", s_failures);
        return 1;
    }

    puts("container smoke passed");
    return 0;
}
