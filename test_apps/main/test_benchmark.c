/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "unity.h"
#include "common.h"
#include "widget/gfx_button.h"
#include "widget/gfx_img.h"
#include "widget/gfx_mesh_img.h"
#include "widget/gfx_label.h"
#include "widget/gfx_anim.h"
#include "widget/gfx_list.h"

static const char *TAG = "test_benchmark";

typedef struct {
    const char *name;
    void (*setup_cb)(gfx_disp_t *disp, void *user_data);
    void (*update_cb)(gfx_disp_t *disp, void *user_data);
    uint32_t duration_ms;
} test_benchmark_case_t;

#define TEST_BENCHMARK_MAX_OBJS         64U
#define TEST_BENCHMARK_MAX_MESH_POINTS  128U
#define TEST_BENCHMARK_LABEL_COUNT      12
#define TEST_BENCHMARK_LIST_COUNT       12
#define TEST_BENCHMARK_VERBOSE_STATS    0
#define TEST_BENCHMARK_SUPPRESS_REFR_WARN 1

typedef struct {
    gfx_obj_t *objs[TEST_BENCHMARK_MAX_OBJS];
    size_t obj_count;
    gfx_mesh_img_point_t mesh_base_points[TEST_BENCHMARK_MAX_MESH_POINTS];
    uint32_t mesh_point_count;
    uint32_t wave_step;
    uint32_t list_item_count;
    mmap_assets_handle_t assets_handle;
} test_benchmark_scene_t;

static double test_benchmark_mpix_per_s(uint64_t pixels, uint64_t time_us)
{
    if (time_us == 0U) {
        return 0.0;
    }
    return ((double)pixels) / (double)time_us;
}

static void test_benchmark_log_result(const char *case_name, float render_fps, float loop_fps, float avg_update_us,
                                      const gfx_disp_perf_stats_t *perf)
{
    uint64_t total_blend_px;
    uint64_t total_blend_time_us;
    double blend_total_mpix_s;

    if (case_name == NULL) {
        return;
    }

    if (perf == NULL) {
        ESP_LOGI(TAG, "[%s] fps(r/l)=%.1f/%.1f upd=%.0fus", case_name, render_fps, loop_fps, avg_update_us);
        return;
    }

    total_blend_px = perf->blend.fill.pixels + perf->blend.color_draw.pixels + perf->blend.image_draw.pixels +
                     perf->blend.triangle_draw.pixels;
    total_blend_time_us = perf->blend.fill.time_us + perf->blend.color_draw.time_us + perf->blend.image_draw.time_us +
                          perf->blend.triangle_draw.time_us;
    blend_total_mpix_s = test_benchmark_mpix_per_s(total_blend_px, total_blend_time_us);

    ESP_LOGI(TAG,
             "[%s] fps(r/l)=%.1f/%.1f upd=%.0fus frame=%.2fms render=%.2fms flush=%.2fms blend=%.2fMPix/s",
             case_name,
             render_fps,
             loop_fps,
             avg_update_us,
             (double)perf->frame_time_us / 1000.0,
             (double)perf->render_time_us / 1000.0,
             (double)perf->flush_time_us / 1000.0,
             blend_total_mpix_s);

#if TEST_BENCHMARK_VERBOSE_STATS
    double fill_mpix_s = test_benchmark_mpix_per_s(perf->blend.fill.pixels, perf->blend.fill.time_us);
    double color_mpix_s = test_benchmark_mpix_per_s(perf->blend.color_draw.pixels, perf->blend.color_draw.time_us);
    double img_mpix_s = test_benchmark_mpix_per_s(perf->blend.image_draw.pixels, perf->blend.image_draw.time_us);
    double tri_mpix_s = test_benchmark_mpix_per_s(perf->blend.triangle_draw.pixels, perf->blend.triangle_draw.time_us);
    ESP_LOGI(TAG, "[%s] blend(MPix/s): fill=%.2f color=%.2f img=%.2f tri=%.2f",
             case_name, fill_mpix_s, color_mpix_s, img_mpix_s, tri_mpix_s);
    ESP_LOGI(TAG, "[%s] fill: calls=%" PRIu64 " px=%" PRIu64 " time=%" PRIu64 "us",
             case_name, perf->blend.fill.calls, perf->blend.fill.pixels, perf->blend.fill.time_us);
    ESP_LOGI(TAG, "[%s] color: calls=%" PRIu64 " px=%" PRIu64 " time=%" PRIu64 "us",
             case_name, perf->blend.color_draw.calls, perf->blend.color_draw.pixels, perf->blend.color_draw.time_us);
    ESP_LOGI(TAG, "[%s] img: calls=%" PRIu64 " px=%" PRIu64 " time=%" PRIu64 "us",
             case_name, perf->blend.image_draw.calls, perf->blend.image_draw.pixels, perf->blend.image_draw.time_us);
    ESP_LOGI(TAG, "[%s] tri: calls=%" PRIu64 " raster_px=%" PRIu64 " time=%" PRIu64 "us",
             case_name,
             perf->blend.triangle_draw.calls,
             perf->blend.triangle_draw.pixels,
             perf->blend.triangle_draw.time_us);
#endif
}

static void test_benchmark_clear_scene(test_benchmark_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    for (size_t i = 0; i < scene->obj_count; i++) {
        if (scene->objs[i]) {
            gfx_obj_delete(scene->objs[i]);
            scene->objs[i] = NULL;
        }
    }
    scene->obj_count = 0;
}

/* --- Bench Scenarios --- */

/* 1. Fill Benchmark: Fullscreen rectangle */
static void setup_fill(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *rect = gfx_button_create(disp);
    TEST_ASSERT_NOT_NULL(rect);

    gfx_obj_set_size(rect, gfx_disp_get_hor_res(disp), gfx_disp_get_ver_res(disp));
    gfx_button_set_bg_color(rect, GFX_COLOR_HEX(0xFF0000));
    scene->objs[scene->obj_count++] = rect;
}

/* 2. Image Benchmark: Fullscreen Image */
static void setup_img(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *img_obj = gfx_img_create(disp);
    TEST_ASSERT_NOT_NULL(img_obj);

    gfx_img_set_src(img_obj, (void *)&simple_face);
    gfx_obj_set_size(img_obj, gfx_disp_get_hor_res(disp), gfx_disp_get_ver_res(disp));
    gfx_obj_align(img_obj, GFX_ALIGN_CENTER, 0, 0);
    scene->objs[scene->obj_count++] = img_obj;
}

/* 3. Mesh Benchmark: Various grid densities */
static void setup_mesh_4x4(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *mesh = gfx_mesh_img_create(disp);
    TEST_ASSERT_NOT_NULL(mesh);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(mesh, (void *)&simple_face));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(mesh, 4, 4));
    gfx_obj_set_size(mesh, gfx_disp_get_hor_res(disp), gfx_disp_get_ver_res(disp));
    gfx_obj_align(mesh, GFX_ALIGN_CENTER, 0, 0);

    scene->mesh_point_count = gfx_mesh_img_get_point_count(mesh);
    TEST_ASSERT_TRUE(scene->mesh_point_count <= TEST_BENCHMARK_MAX_MESH_POINTS);
    for (uint32_t i = 0; i < scene->mesh_point_count; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(mesh, i, &scene->mesh_base_points[i]));
    }

    scene->objs[scene->obj_count++] = mesh;
}

static void setup_mesh_8x8(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *mesh = gfx_mesh_img_create(disp);
    TEST_ASSERT_NOT_NULL(mesh);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_src(mesh, (void *)&simple_face));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_grid(mesh, 8, 8));
    gfx_obj_set_size(mesh, gfx_disp_get_hor_res(disp), gfx_disp_get_ver_res(disp));
    gfx_obj_align(mesh, GFX_ALIGN_CENTER, 0, 0);

    scene->mesh_point_count = gfx_mesh_img_get_point_count(mesh);
    TEST_ASSERT_TRUE(scene->mesh_point_count <= TEST_BENCHMARK_MAX_MESH_POINTS);
    for (uint32_t i = 0; i < scene->mesh_point_count; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_get_point(mesh, i, &scene->mesh_base_points[i]));
    }

    scene->objs[scene->obj_count++] = mesh;
}

static void update_mesh_anim(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *mesh;
    int16_t amplitude;

    (void)disp;

    if (scene == NULL || scene->obj_count == 0 || scene->mesh_point_count == 0) {
        return;
    }

    mesh = scene->objs[0];
    if (mesh == NULL) {
        return;
    }

    scene->wave_step = (scene->wave_step + 1U) % 20U;
    amplitude = (scene->wave_step < 10U) ? (int16_t)scene->wave_step : (int16_t)(19U - scene->wave_step);

    for (uint32_t i = 0; i < scene->mesh_point_count; i++) {
        int16_t delta = (i % 2U == 0U) ? amplitude : -amplitude;
        TEST_ASSERT_EQUAL(ESP_OK, gfx_mesh_img_set_point(mesh, i,
                                                         scene->mesh_base_points[i].x + delta,
                                                         scene->mesh_base_points[i].y));
    }
}

/* 4. Labels Benchmark: static labels */
static void setup_labels(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    char text_buf[24];

    for (int i = 0; i < TEST_BENCHMARK_LABEL_COUNT && scene->obj_count < TEST_BENCHMARK_MAX_OBJS; i++) {
        gfx_obj_t *label = gfx_label_create(disp);
        TEST_ASSERT_NOT_NULL(label);
        snprintf(text_buf, sizeof(text_buf), "L%02d FPS", i);
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_text(label, text_buf));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_font(label, (gfx_font_t)&font_puhui_16_4));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_color(label, GFX_COLOR_HEX(0xF5F5F5)));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_enable(label, true));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_bg_color(label, GFX_COLOR_HEX(0x262626)));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(label, 60, 24));
        TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_pos(label, (i % 4) * 74 + 6, (i / 4) * 30 + 10));
        scene->objs[scene->obj_count++] = label;
    }
}

/* 5. List Benchmark: static list + selected row switching */
static void setup_list(gfx_disp_t *disp, void *user_data)
{
    static const char *items[TEST_BENCHMARK_LIST_COUNT] = {
        "Face Tracking",
        "Expression Blend",
        "Audio Sync",
        "Motion Preset A",
        "Motion Preset B",
        "Diagnostics",
        "System Info",
        "Lighting",
        "Volume",
        "Calibration",
        "Restore Defaults",
        "About",
    };
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *list;

    list = gfx_list_create(disp);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_size(list, 228, 170));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(list, GFX_ALIGN_CENTER, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_font(list, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_bg_color(list, GFX_COLOR_HEX(0x171A22)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_border_color(list, GFX_COLOR_HEX(0x425069)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected_bg_color(list, GFX_COLOR_HEX(0x2450B8)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_text_color(list, GFX_COLOR_HEX(0xECF1FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_row_height(list, 22));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_padding(list, 8, 6));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_items(list, items, TEST_BENCHMARK_LIST_COUNT));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(list, 0));

    scene->list_item_count = TEST_BENCHMARK_LIST_COUNT;
    scene->objs[scene->obj_count++] = list;
}

static void update_list_select(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;

    (void)disp;

    if (scene == NULL || scene->obj_count == 0 || scene->list_item_count == 0) {
        return;
    }

    scene->wave_step = (scene->wave_step + 1U) % scene->list_item_count;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_list_set_selected(scene->objs[0], (int32_t)scene->wave_step));
}

static void setup_anim_single(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *anim_obj;
    const void *anim_data;
    size_t anim_size;

    TEST_ASSERT_NOT_NULL(scene);
    anim_data = mmap_assets_get_mem(scene->assets_handle, MMAP_TEST_ASSETS_A1_017_041_075_EAF);
    anim_size = mmap_assets_get_size(scene->assets_handle, MMAP_TEST_ASSETS_A1_017_041_075_EAF);
    TEST_ASSERT_NOT_NULL(anim_data);
    TEST_ASSERT_TRUE(anim_size > 0U);

    anim_obj = gfx_anim_create(disp);
    TEST_ASSERT_NOT_NULL(anim_obj);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_obj, anim_data, anim_size));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_obj, 0, 0xFFFFFFFF, 50, true));
    gfx_obj_set_size(anim_obj, 200, 150);
    gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    scene->objs[scene->obj_count++] = anim_obj;
}

static void setup_anim_dual(gfx_disp_t *disp, void *user_data)
{
    test_benchmark_scene_t *scene = (test_benchmark_scene_t *)user_data;
    gfx_obj_t *anim_left;
    gfx_obj_t *anim_right;
    const void *anim_data;
    size_t anim_size;

    TEST_ASSERT_NOT_NULL(scene);
    anim_data = mmap_assets_get_mem(scene->assets_handle, MMAP_TEST_ASSETS_A2_018_039_063_EAF);
    anim_size = mmap_assets_get_size(scene->assets_handle, MMAP_TEST_ASSETS_A2_018_039_063_EAF);
    TEST_ASSERT_NOT_NULL(anim_data);
    TEST_ASSERT_TRUE(anim_size > 0U);

    anim_left = gfx_anim_create(disp);
    anim_right = gfx_anim_create(disp);
    TEST_ASSERT_NOT_NULL(anim_left);
    TEST_ASSERT_NOT_NULL(anim_right);

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_left, anim_data, anim_size));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_right, anim_data, anim_size));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_left, 0, 0xFFFFFFFF, 45, true));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_right, 0, 0xFFFFFFFF, 45, true));
    gfx_obj_set_size(anim_left, 160, 120);
    gfx_obj_set_size(anim_right, 160, 120);
    gfx_obj_align(anim_left, GFX_ALIGN_CENTER, -88, 0);
    gfx_obj_align(anim_right, GFX_ALIGN_CENTER, 88, 0);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_left));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_right));

    scene->objs[scene->obj_count++] = anim_left;
    scene->objs[scene->obj_count++] = anim_right;
}

static void run_benchmark_case(const test_benchmark_case_t *bcase, mmap_assets_handle_t assets_handle)
{
    test_benchmark_scene_t scene = {0};
    gfx_disp_perf_stats_t perf = {0};
    uint32_t frame_count = 0;
    uint32_t update_calls = 0;
    int64_t bench_start_us;
    int64_t bench_elapsed_us;
    int64_t total_update_us = 0;
    uint32_t start_tick;
    float loop_fps = 0.0f;
    float avg_update_us = 0.0f;
    float avg_fps = 0;

    ESP_LOGI(TAG, "Running Benchmark: %s", bcase->name);
    scene.assets_handle = assets_handle;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x000000));
    if (bcase->setup_cb) {
        bcase->setup_cb(disp_default, &scene);
    }
    test_app_unlock();

    test_app_wait_ms(300);

    bench_start_us = esp_timer_get_time();
    start_tick = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(bcase->duration_ms)) {
        if (bcase->update_cb) {
            int64_t update_start_us = esp_timer_get_time();
            TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
            bcase->update_cb(disp_default, &scene);
            test_app_unlock();
            total_update_us += (esp_timer_get_time() - update_start_us);
            update_calls++;
        }

        /* Yield to let the render task run. */
        vTaskDelay(pdMS_TO_TICKS(10));
        frame_count++;
    }

    bench_elapsed_us = esp_timer_get_time() - bench_start_us;
    avg_fps = (float)gfx_timer_get_actual_fps(emote_handle);
    if (bench_elapsed_us > 0) {
        loop_fps = (float)((double)frame_count * 1000000.0 / (double)bench_elapsed_us);
    }
    if (update_calls > 0U) {
        avg_update_us = (float)((double)total_update_us / (double)update_calls);
    }

    if (gfx_disp_get_perf_stats(disp_default, &perf) == ESP_OK) {
        test_benchmark_log_result(bcase->name, avg_fps, loop_fps, avg_update_us, &perf);
    } else {
        test_benchmark_log_result(bcase->name, avg_fps, loop_fps, avg_update_us, NULL);
    }

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_benchmark_clear_scene(&scene);
    test_app_unlock();
}

void test_benchmark_run_case(void)
{
#if TEST_BENCHMARK_SUPPRESS_REFR_WARN
    esp_log_level_t refr_prev_level = esp_log_level_get("refr");
#endif
    test_app_runtime_t runtime;
    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));

#if TEST_BENCHMARK_SUPPRESS_REFR_WARN
    esp_log_level_set("refr", ESP_LOG_ERROR);
#endif

    static const test_benchmark_case_t cases[] = {
        {"Solid Fill (Full)", setup_fill, NULL, 3000},
        {"Static Image (Full)", setup_img, NULL, 3000},
        {"Anim Playback (Single)", setup_anim_single, NULL, 6000},
        {"Anim Playback (Dual)", setup_anim_dual, NULL, 7000},
        {"Mesh Grid 4x4 (Animated)", setup_mesh_4x4, update_mesh_anim, 5000},
        {"Mesh Grid 8x8 (Animated)", setup_mesh_8x8, update_mesh_anim, 5000},
        {"List (12 + Select)", setup_list, update_list_select, 4000},
        {"Labels (12 Static)", setup_labels, NULL, 3000},
    };

    for (size_t i = 0; i < TEST_APP_ARRAY_SIZE(cases); i++) {
        run_benchmark_case(&cases[i], runtime.assets_handle);
    }

    test_app_log_step(TAG, "Benchmark suite finished");
    test_app_wait_for_observe(8000);

#if TEST_BENCHMARK_SUPPRESS_REFR_WARN
    esp_log_level_set("refr", refr_prev_level);
#endif

    test_app_runtime_close(&runtime);
}
