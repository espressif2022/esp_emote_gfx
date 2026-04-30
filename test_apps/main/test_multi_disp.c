/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "bsp/esp-bsp.h"
#include "common.h"

static const char *TAG = "test_multi_disp";

typedef struct {
    gfx_disp_t *disp_left;
    gfx_disp_t *disp_right;
    gfx_obj_t *anim_left;
    gfx_obj_t *anim_right;
} test_multi_disp_scene_t;

static int32_t s_drag_offset_x = 0;
static int32_t s_drag_offset_y = 0;
static bool s_drag_active = false;

static void test_multi_disp_flush_cb(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_disp_get_user_data(disp);

    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
    gfx_disp_flush_ready(disp, true);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void test_multi_disp_touch_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    gfx_coord_t obj_x = 0;
    gfx_coord_t obj_y = 0;

    (void)user_data;

    gfx_obj_get_pos(obj, &obj_x, &obj_y);
    if (event->type == GFX_TOUCH_EVENT_PRESS) {
        s_drag_offset_x = (int32_t)event->x - obj_x;
        s_drag_offset_y = (int32_t)event->y - obj_y;
        s_drag_active = true;
    }
    if (s_drag_active) {
        gfx_obj_set_pos(obj, (int32_t)event->x - s_drag_offset_x, (int32_t)event->y - s_drag_offset_y);
    }
    if (event->type == GFX_TOUCH_EVENT_RELEASE) {
        s_drag_active = false;
    }
}

static gfx_disp_t *test_multi_disp_add(void)
{
    gfx_disp_config_t disp_cfg = {
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .flush_cb = test_multi_disp_flush_cb,
        .update_cb = NULL,
        .user_data = (void *)panel_handle,
        .flags = {.swap = true},
        .buffers = {.buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16},
    };

    return gfx_disp_add(emote_handle, &disp_cfg);
}

static void test_multi_disp_scene_cleanup(test_multi_disp_scene_t *scene)
{
    if (scene == NULL) {
        return;
    }

    if (touch_default != NULL) {
        gfx_touch_set_disp(touch_default, disp_default);
    }
    if (scene->anim_left != NULL) {
        gfx_obj_delete(scene->anim_left);
        scene->anim_left = NULL;
    }
    if (scene->anim_right != NULL) {
        gfx_obj_delete(scene->anim_right);
        scene->anim_right = NULL;
    }
    if (scene->disp_left != NULL) {
        gfx_disp_del(scene->disp_left);
        scene->disp_left = NULL;
    }
    if (scene->disp_right != NULL) {
        gfx_disp_del(scene->disp_right);
        scene->disp_right = NULL;
    }
}

static void test_multi_disp_run(mmap_assets_handle_t assets_handle)
{
    test_multi_disp_scene_t scene = {0};
    const void *anim_data = NULL;
    size_t anim_size = 0;
    gfx_anim_src_t anim_src = {0};

    test_app_log_case(TAG, "Multi-display validation");

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    scene.disp_left = test_multi_disp_add();
    scene.disp_right = test_multi_disp_add();
    TEST_ASSERT_NOT_NULL(scene.disp_left);
    TEST_ASSERT_NOT_NULL(scene.disp_right);

    scene.anim_left = gfx_anim_create(scene.disp_left);
    scene.anim_right = gfx_anim_create(scene.disp_right);
    TEST_ASSERT_NOT_NULL(scene.anim_left);
    TEST_ASSERT_NOT_NULL(scene.anim_right);

    anim_data = mmap_assets_get_mem(assets_handle, MMAP_ASSETS_TEST_MI_2_EYE_8BIT_AAF);
    anim_size = mmap_assets_get_size(assets_handle, MMAP_ASSETS_TEST_MI_2_EYE_8BIT_AAF);
    anim_src.type = GFX_ANIM_SRC_TYPE_MEMORY;
    anim_src.data = anim_data;
    anim_src.data_len = anim_size;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src_desc(scene.anim_left, &anim_src));
    gfx_obj_align(scene.anim_left, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(scene.anim_left, 0, 0xFFFF, 15, true);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(scene.anim_left));

    anim_data = mmap_assets_get_mem(assets_handle, MMAP_ASSETS_TEST_TRANSPARENT_EAF);
    anim_size = mmap_assets_get_size(assets_handle, MMAP_ASSETS_TEST_TRANSPARENT_EAF);
    anim_src.data = anim_data;
    anim_src.data_len = anim_size;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src_desc(scene.anim_right, &anim_src));
    gfx_obj_align(scene.anim_right, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(scene.anim_right, 0, 0xFFFF, 15, true);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(scene.anim_right));

    TEST_ASSERT_EQUAL(ESP_OK, gfx_touch_set_disp(touch_default, scene.disp_right));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.anim_left, test_multi_disp_touch_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(scene.anim_right, test_multi_disp_touch_cb, NULL));
    test_app_unlock();

    test_app_wait_for_observe(5000);

    test_app_log_step(TAG, "Switch touch back to default display");
    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_EQUAL(ESP_OK, gfx_touch_set_disp(touch_default, disp_default));
    test_app_unlock();

    test_app_wait_for_observe(1200);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    test_multi_disp_scene_cleanup(&scene);
    test_app_unlock();
}

TEST_CASE("display: multi route map", "[display][multi]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, TEST_APP_ASSETS_PARTITION_DEFAULT));
    test_multi_disp_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
