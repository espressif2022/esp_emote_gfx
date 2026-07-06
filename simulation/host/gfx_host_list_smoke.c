/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/widgets/list.h"

typedef struct {
    int32_t focus_index;
    int32_t select_index;
    bool confirmed;
    uint16_t page_index;
    uint16_t items_per_page;
    uint32_t focus_calls;
    uint32_t select_calls;
    uint32_t page_calls;
} list_smoke_state_t;

static void expect_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "list smoke failed: %s\n", message);
        exit(1);
    }
}

static void focus_cb(gfx_object_t *obj, int32_t focused_index, void *user_data)
{
    list_smoke_state_t *state = (list_smoke_state_t *)user_data;
    (void)obj;
    state->focus_index = focused_index;
    state->focus_calls++;
}

static void select_cb(gfx_object_t *obj, int32_t selected_index, bool confirmed, void *user_data)
{
    list_smoke_state_t *state = (list_smoke_state_t *)user_data;
    (void)obj;
    state->select_index = selected_index;
    state->confirmed = confirmed;
    state->select_calls++;
}

static void page_load_cb(gfx_object_t *obj, uint16_t page_index, uint16_t items_per_page, void *user_data)
{
    list_smoke_state_t *state = (list_smoke_state_t *)user_data;
    (void)obj;
    state->page_index = page_index;
    state->items_per_page = items_per_page;
    state->page_calls++;
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

int main(void)
{
    static const char *const items[] = {
        "One", "Two", "Three", "Four", "Five", "Six", "Seven",
    };
    list_smoke_state_t state = {
        .focus_index = -1,
        .select_index = -1,
    };

    gfx_handle_t gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    expect_true(gfx != NULL, "core init");

    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = 160,
        .v_res = 160,
    });
    expect_true(backend != NULL, "memory backend create");

    gfx_display_t *disp = gfx_display_add(gfx, &(gfx_display_config_t) {
        .h_res = 160,
        .v_res = 160,
        .backend = backend,
        .flags = {
        },
    });
    expect_true(disp != NULL, "display add");

    gfx_object_t *list = gfx_list_create(disp);
    expect_true(list != NULL, "list create");
    expect_true(gfx_object_set_pos(list, 10, 10) == GFX_OK, "list set pos");
    expect_true(gfx_object_set_size(list, 100, 90) == GFX_OK, "list set size");
    expect_true(gfx_list_set_item_height(list, 30) == GFX_OK, "item height");
    expect_true(gfx_list_set_items(list, items, (uint16_t)(sizeof(items) / sizeof(items[0]))) == GFX_OK,
                "set items");
    expect_true(gfx_list_set_items_per_page(list, 3) == GFX_OK, "items per page");
    expect_true(gfx_list_set_focus_cb(list, focus_cb, &state) == GFX_OK, "focus cb");
    expect_true(gfx_list_set_select_cb(list, select_cb, &state) == GFX_OK, "select cb");
    expect_true(gfx_list_set_page_load_cb(list, page_load_cb, &state) == GFX_OK, "page load cb");

    expect_true(gfx_list_get_page_count(list) == 3, "page count");
    expect_true(gfx_list_get_items_per_page(list) == 3, "effective items per page");

    expect_true(gfx_list_next_page(list) == GFX_OK, "next page");
    expect_true(gfx_list_get_page(list) == 1, "page index after next");
    expect_true(gfx_list_get_top_index(list) == 3, "top index after next page");
    expect_true(state.page_calls == 1 && state.page_index == 1 && state.items_per_page == 3,
                "page callback after next");

    expect_true(gfx_list_prev_page(list) == GFX_OK, "prev page");
    expect_true(gfx_list_get_page(list) == 0, "page index after prev");
    expect_true(gfx_list_get_top_index(list) == 0, "top index after prev page");

    expect_true(gfx_list_set_selected(list, 2) == GFX_OK, "programmatic select");
    expect_true(state.select_index == 2 && !state.confirmed, "programmatic select callback");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 20, 45, 10);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 20, 45, 20);
    expect_true(state.focus_index == 1, "touch focus item 1");
    expect_true(state.select_index == 1 && state.confirmed, "touch confirm item 1");
    expect_true(gfx_list_get_selected(list) == 1, "selected getter after touch");

    expect_true(gfx_list_confirm(list) == GFX_OK, "confirm selected");
    expect_true(state.select_index == 1 && state.confirmed, "confirm callback");

    expect_true(gfx_list_set_page(list, 0) == GFX_OK, "reset to page 0 before drag");
    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 20, 85, 30);
    inject_touch(disp, GFX_TOUCH_EVENT_MOVE, 20, 35, 50);
    expect_true(gfx_list_get_scroll_offset(list) > 0, "drag scrolls down");
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 20, 35, 60);
    int32_t release_scroll = gfx_list_get_scroll_offset(list);
    for (int i = 0; i < 12; i++) {
        expect_true(gfx_core_tick(gfx) == GFX_OK, "tick list inertia");
    }
    int32_t inertial_scroll = gfx_list_get_scroll_offset(list);
    expect_true(inertial_scroll >= release_scroll, "inertia keeps scrolling after release");
    for (int i = 0; i < 80; i++) {
        expect_true(gfx_core_tick(gfx) == GFX_OK, "tick list settle");
    }
    int32_t settled_scroll = gfx_list_get_scroll_offset(list);
    expect_true(settled_scroll >= 0 && settled_scroll <= 120, "list settles inside valid scroll range");
    expect_true((settled_scroll % 30) == 0, "list settles snapped to item");

    expect_true(gfx_list_set_scroll_offset(list, 0) == GFX_OK, "reset scroll before overscroll");
    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 20, 35, 2000);
    inject_touch(disp, GFX_TOUCH_EVENT_MOVE, 20, 95, 2020);
    expect_true(gfx_list_get_scroll_offset(list) < 0, "drag allows top overscroll");
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 20, 95, 2030);
    for (int i = 0; i < 80; i++) {
        expect_true(gfx_core_tick(gfx) == GFX_OK, "tick list top bounce");
    }
    expect_true(gfx_list_get_scroll_offset(list) == 0, "top overscroll bounces back");

    gfx_core_deinit(gfx);
    return 0;
}
