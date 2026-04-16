/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"

#define TEST_MEMORY_LEAK_THRESHOLD (500)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

extern void test_anim_run_case_matrix(void);
extern void test_anim_run_case_emote_gen(void);
extern void test_eye_organic_run_case(void);
extern void test_mesh_emote_run_case(void);
extern void test_mesh_drag_run_case(void);
extern void test_mesh_bulge_run_case(void);
extern void test_list_run_case(void);
extern void test_benchmark_run_case(void);
extern void test_lobster_expr_emote_run_case(void);
extern void test_stickman_emote_run_case(void);
extern void test_gfx_rig_stickman_host_run_case(void);
extern void test_gfx_sm_scene_run_case(void);

void app_main(void)
{
    // unity_run_menu();
    // test_anim_run_case_matrix();
    // test_anim_run_case_emote_gen();
    // test_eye_organic_run_case();
    // test_mesh_emote_run_case();
    // test_mesh_drag_run_case();
    // test_mesh_bulge_run_case();
    // test_list_run_case();
    // test_benchmark_run_case();
    // test_mesh_bulge_run_case();
    // test_lobster_expr_emote_run_case();
    // test_stickman_emote_run_case();
    // test_gfx_rig_stickman_host_run_case();
    test_gfx_sm_scene_run_case();
}
