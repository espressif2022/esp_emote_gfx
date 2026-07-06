/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/tween.h"
#include "gfx/widgets/button.h"

static int s_failures;
static int s_last_value;
static int s_value_calls;
static bool s_done_called;

static void expect_true(bool condition, const char *msg)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", msg);
        s_failures++;
    }
}

static void tween_value_cb(gfx_tween_t *tween, gfx_object_t *obj, int32_t value, void *user_data)
{
    (void)tween;
    (void)obj;
    (void)user_data;

    s_last_value = value;
    s_value_calls++;
}

static void tween_done_cb(gfx_tween_t *tween, gfx_object_t *obj, void *user_data)
{
    (void)tween;
    (void)obj;
    (void)user_data;

    s_done_called = true;
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
        .h_res = 160,
        .v_res = 120,
    });
    expect_true(backend != NULL, "memory backend create");

    gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
        .h_res = 160,
        .v_res = 120,
        .backend = backend,
        .flags = {
        },
    });
    expect_true(disp != NULL, "display create");

    gfx_object_t *obj = gfx_button_create(disp);
    expect_true(obj != NULL, "object create");

    gfx_tween_t *tween = gfx_tween_create(obj);
    expect_true(tween != NULL, "tween create");
    expect_true(gfx_tween_start_i32(tween, 100, 0, 40, GFX_TWEEN_EASE_OUT_QUAD,
                                    tween_value_cb, tween_done_cb, NULL) == GFX_OK,
                "start tween");
    expect_true(gfx_tween_is_active(tween), "tween active after start");
    expect_true(s_last_value == 100, "initial callback value");

    usleep(20000);
    expect_true(gfx_core_tick(handle) == GFX_OK, "tick midway");
    expect_true(s_last_value < 100 && s_last_value > 0, "midway value moved");
    expect_true(!s_done_called, "not done midway");

    usleep(30000);
    expect_true(gfx_core_tick(handle) == GFX_OK, "tick complete");
    expect_true(!gfx_tween_is_active(tween), "tween inactive after complete");
    expect_true(s_last_value == 0, "final callback value");
    expect_true(s_done_called, "done callback called");
    expect_true(s_value_calls >= 3, "value callback count");

    gfx_tween_delete(tween);
    gfx_core_deinit(handle);

    if (s_failures != 0) {
        fprintf(stderr, "tween smoke failed: %d failures\n", s_failures);
        return 1;
    }

    puts("tween smoke passed");
    return 0;
}
