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
#include "gfx/widgets/wheel.h"

typedef struct {
    int32_t value_index;
    int32_t confirm_index;
    uint32_t value_calls;
    uint32_t confirm_calls;
} wheel_smoke_state_t;

static void expect_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "wheel smoke failed: %s\n", message);
        exit(1);
    }
}

static void value_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    wheel_smoke_state_t *state = (wheel_smoke_state_t *)user_data;
    (void)obj;
    state->value_index = selected_index;
    state->value_calls++;
}

static void confirm_cb(gfx_object_t *obj, int32_t selected_index, void *user_data)
{
    wheel_smoke_state_t *state = (wheel_smoke_state_t *)user_data;
    (void)obj;
    state->confirm_index = selected_index;
    state->confirm_calls++;
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
        "Low", "Medium", "High", "Turbo",
    };
    wheel_smoke_state_t state = {
        .value_index = -1,
        .confirm_index = -1,
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

    gfx_object_t *wheel = gfx_wheel_create(disp);
    expect_true(wheel != NULL, "wheel create");
    expect_true(gfx_object_set_pos(wheel, 20, 20) == GFX_OK, "wheel set pos");
    expect_true(gfx_object_set_size(wheel, 110, 100) == GFX_OK, "wheel set size");
    expect_true(gfx_wheel_set_item_height(wheel, 30) == GFX_OK, "item height");
    expect_true(gfx_wheel_set_items(wheel, items, (uint16_t)(sizeof(items) / sizeof(items[0]))) == GFX_OK,
                "set items");
    expect_true(gfx_wheel_set_value_cb(wheel, value_cb, &state) == GFX_OK, "value cb");
    expect_true(gfx_wheel_set_confirm_cb(wheel, confirm_cb, &state) == GFX_OK, "confirm cb");
    expect_true(gfx_wheel_set_selected(wheel, 1) == GFX_OK, "set selected");
    expect_true(state.value_index == 1, "value callback");

    expect_true(gfx_wheel_confirm(wheel) == GFX_OK, "programmatic confirm");
    expect_true(state.confirm_index == 1 && state.confirm_calls == 1, "programmatic confirm callback");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 70, 70, 10);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 70, 70, 20);
    expect_true(state.confirm_calls == 2, "tap confirm callback");

    inject_touch(disp, GFX_TOUCH_EVENT_PRESS, 70, 70, 30);
    inject_touch(disp, GFX_TOUCH_EVENT_MOVE, 70, 30, 50);
    inject_touch(disp, GFX_TOUCH_EVENT_RELEASE, 70, 30, 60);
    expect_true(state.confirm_calls == 2, "drag release does not confirm");

    gfx_core_deinit(gfx);
    return 0;
}
