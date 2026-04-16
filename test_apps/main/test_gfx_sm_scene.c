/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * Test: unified gfx_sm_runtime — stickman + face 同屏同时运行
 *
 * 左 1/3：stickman fireman   (CAPSULE / RING segments)
 * 右 2/3：face expressions   (BEZIER_FILL / BEZIER_LOOP / BEZIER_STRIP segments)
 *
 * 两者使用完全相同的 gfx_sm_runtime_t + gfx_sm_runtime_init()，
 * 通过 gfx_sm_runtime_set_canvas() 限定各自的画布区域；
 * 各自独立的 clip timer 按各自 hold_ticks 驱动切换。
 */

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "esp_check.h"
#include "unity.h"
#include "core/gfx_disp.h"
#include "widget/gfx_sm_scene.h"
#include "common.h"

static const char *TAG = "test_sm_scene";

#include "stickman_fireman_scene.inc"
#include "face_emote_scene.inc"

/* ------------------------------------------------------------------ */
/*  Shared helpers                                                      */
/* ------------------------------------------------------------------ */

static uint32_t s_clip_hold_ms(const gfx_sm_asset_t *asset, uint16_t clip_idx)
{
    const gfx_sm_clip_t      *clip = &asset->clips[clip_idx];
    const gfx_sm_clip_step_t *step;
    uint32_t period_ms = (asset->layout->timer_period_ms > 0U)
                         ? asset->layout->timer_period_ms : 33U;
    uint32_t max_ticks = 0;

    for (uint8_t i = 0; i < clip->step_count; i++) {
        step = &clip->steps[i];
        if (step->hold_ticks > max_ticks) {
            max_ticks = step->hold_ticks;
        }
    }
    return (max_ticks > 0U ? max_ticks : 1U) * period_ms;
}

/* ------------------------------------------------------------------ */
/*  Per-runtime context                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    gfx_sm_runtime_t     rt;
    gfx_timer_handle_t   clip_timer;
    uint16_t             seq_index;
    const gfx_sm_asset_t *asset;
} test_sm_slot_t;

static void s_clip_timer_cb(void *user_data)
{
    test_sm_slot_t       *slot  = (test_sm_slot_t *)user_data;
    const gfx_sm_asset_t *asset = slot->asset;

    slot->seq_index = (uint16_t)((slot->seq_index + 1U) % asset->sequence_count);

    uint16_t ci = asset->sequence[slot->seq_index];
    gfx_sm_runtime_set_clip(&slot->rt, ci, false);
    ESP_LOGI(TAG, "[%s] → %s", asset->clips[0].name_cn ? "face" : "skel",
             asset->clips[ci].name);

    gfx_timer_set_period(slot->clip_timer, s_clip_hold_ms(asset, ci));
    gfx_timer_reset(slot->clip_timer);
}

/* ------------------------------------------------------------------ */
/*  Dual test: both runtimes live simultaneously                       */
/* ------------------------------------------------------------------ */

static test_sm_slot_t s_slot_skel;   /* stickman — left half  */
static test_sm_slot_t s_slot_face;   /* face     — right half */

static void test_sm_dual_run(void)
{
    uint16_t disp_w, disp_h;
    uint16_t half_w;

    test_app_log_case(TAG,
        "gfx_sm_runtime dual: stickman(left) + face(right) — one API, two assets");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);

    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x0A0A0A));

    disp_w = (uint16_t)gfx_disp_get_hor_res(disp_default);
    disp_h = (uint16_t)gfx_disp_get_ver_res(disp_default);
    half_w = disp_w / 2U;   /* stickman: left 1/2 */

    /* ── Slot 0: stickman fireman, left 1/3 ── */
    memset(&s_slot_skel, 0, sizeof(s_slot_skel));
    s_slot_skel.asset = &s_fm_scene_asset;

    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_init(&s_slot_skel.rt, disp_default, &s_fm_scene_asset));
    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_set_canvas(&s_slot_skel.rt, 0, 0, half_w, disp_h));
    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_set_color(&s_slot_skel.rt, GFX_COLOR_HEX(0xFFFFFF)));

    s_slot_skel.seq_index = 0;
    uint16_t ci_skel0 = s_fm_scene_asset.sequence[0];
    gfx_sm_runtime_set_clip(&s_slot_skel.rt, ci_skel0, true);

    s_slot_skel.clip_timer = gfx_timer_create(emote_handle, s_clip_timer_cb,
                                               s_clip_hold_ms(&s_fm_scene_asset, ci_skel0),
                                               &s_slot_skel);
    TEST_ASSERT_NOT_NULL(s_slot_skel.clip_timer);

    /* ── Slot 1: face expressions, right 2/3 ── */
    memset(&s_slot_face, 0, sizeof(s_slot_face));
    s_slot_face.asset = &s_fe_scene_asset;

    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_init(&s_slot_face.rt, disp_default, &s_fe_scene_asset));
    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_set_canvas(&s_slot_face.rt, (gfx_coord_t)half_w - 30, 0,
                                   (uint16_t)(disp_w - half_w) + 60, disp_h));
    TEST_ASSERT_EQUAL(ESP_OK,
        gfx_sm_runtime_set_color(&s_slot_face.rt, GFX_COLOR_HEX(0x40B0FF)));

    s_slot_face.seq_index = 0;
    uint16_t ci_face0 = s_fe_scene_asset.sequence[0];
    gfx_sm_runtime_set_clip(&s_slot_face.rt, ci_face0, true);

    s_slot_face.clip_timer = gfx_timer_create(emote_handle, s_clip_timer_cb,
                                               s_clip_hold_ms(&s_fe_scene_asset, ci_face0),
                                               &s_slot_face);
    TEST_ASSERT_NOT_NULL(s_slot_face.clip_timer);

    test_app_unlock();

    test_app_log_step(TAG, "Left 1/3: fireman  Right 2/3: 49 face expressions — observe");
    test_app_wait_for_observe(1000 * 10000);

    /* ── Teardown ── */
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_timer_delete(emote_handle, s_slot_skel.clip_timer);
    gfx_timer_delete(emote_handle, s_slot_face.clip_timer);
    gfx_sm_runtime_deinit(&s_slot_skel.rt);
    gfx_sm_runtime_deinit(&s_slot_face.rt);
    test_app_unlock();
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */

void test_gfx_sm_scene_run_case(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_sm_dual_run();
    test_app_runtime_close(&runtime);
}
