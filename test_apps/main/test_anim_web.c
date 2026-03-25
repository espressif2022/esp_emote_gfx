/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdbool.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mmap_generate_test_assets.h"
#include "unity.h"
#include "common.h"

static const char *TAG = "test_anim_web";

enum {
    TEST_ANIM_EVENT_NEXT = BIT0,
};

static EventGroupHandle_t s_anim_events;

static void test_anim_next_btn_cb(gfx_obj_t *obj, const gfx_touch_event_t *event, void *user_data)
{
    (void)obj;
    (void)user_data;
    if (event != NULL && event->type == GFX_TOUCH_EVENT_RELEASE && s_anim_events != NULL) {
        xEventGroupSetBits(s_anim_events, TEST_ANIM_EVENT_NEXT);
        ESP_LOGI(TAG, "Next");
    }
}

#define TEST_ANIM_INDEX_JSON_NAME "index.json"
#define TEST_ANIM_INDEX_MAX         64
#define TEST_ANIM_INDEX_STR_LEN     96

typedef struct {
    char name[TEST_ANIM_INDEX_STR_LEN];
    char file[TEST_ANIM_INDEX_STR_LEN];
    int x;
    int y;
    int loop_start;
    int loop_end;
    bool has_loop;
    int asset_id;
} test_anim_index_item_t;

static test_anim_index_item_t s_index_items[TEST_ANIM_INDEX_MAX];
static size_t s_index_count;

static int test_anim_mmap_find_asset_id_by_name(mmap_assets_handle_t assets_handle, const char *filename)
{
    if (filename == NULL || filename[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < mmap_assets_get_stored_files(assets_handle); i++) {
        const char *n = mmap_assets_get_name(assets_handle, i);
        if (n != NULL && strcmp(n, filename) == 0) {
            return i;
        }
    }
    return -1;
}

static int test_anim_mmap_find_index_json_id(mmap_assets_handle_t assets_handle)
{
    return test_anim_mmap_find_asset_id_by_name(assets_handle, TEST_ANIM_INDEX_JSON_NAME);
}

/**
 * Parse index.json from mmap and fill s_index_items in array order (round-robin uses this order).
 */
static void test_anim_index_load(mmap_assets_handle_t assets_handle)
{
    s_index_count = 0;

    int idx = test_anim_mmap_find_index_json_id(assets_handle);
    if (idx < 0) {
        ESP_LOGW(TAG, "%s not found in mmap", TEST_ANIM_INDEX_JSON_NAME);
        return;
    }

    const void *mem = mmap_assets_get_mem(assets_handle, idx);
    size_t len = (size_t)mmap_assets_get_size(assets_handle, idx);
    if (mem == NULL || len == 0) {
        ESP_LOGW(TAG, "%s has no data", TEST_ANIM_INDEX_JSON_NAME);
        return;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)mem, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse %s", TEST_ANIM_INDEX_JSON_NAME);
        return;
    }

    if (!cJSON_IsArray(root)) {
        ESP_LOGW(TAG, "%s root must be a JSON array", TEST_ANIM_INDEX_JSON_NAME);
        cJSON_Delete(root);
        return;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (s_index_count >= TEST_ANIM_INDEX_MAX) {
            ESP_LOGW(TAG, "index: truncated at %d entries", TEST_ANIM_INDEX_MAX);
            break;
        }

        cJSON *jn = cJSON_GetObjectItem(item, "name");
        cJSON *jf = cJSON_GetObjectItem(item, "file");
        cJSON *jx = cJSON_GetObjectItem(item, "x");
        cJSON *jy = cJSON_GetObjectItem(item, "y");
        cJSON *jloop = cJSON_GetObjectItem(item, "loop");

        test_anim_index_item_t *e = &s_index_items[s_index_count];
        memset(e, 0, sizeof(*e));
        e->asset_id = -1;

        if (cJSON_IsString(jn) && jn->valuestring) {
            strncpy(e->name, jn->valuestring, sizeof(e->name) - 1);
        }
        if (cJSON_IsString(jf) && jf->valuestring) {
            strncpy(e->file, jf->valuestring, sizeof(e->file) - 1);
        }
        if (cJSON_IsNumber(jx)) {
            e->x = jx->valueint;
        }
        if (cJSON_IsNumber(jy)) {
            e->y = jy->valueint;
        }
        if (cJSON_IsArray(jloop) && cJSON_GetArraySize(jloop) >= 2) {
            cJSON *a0 = cJSON_GetArrayItem(jloop, 0);
            cJSON *a1 = cJSON_GetArrayItem(jloop, 1);
            if (cJSON_IsNumber(a0) && cJSON_IsNumber(a1)) {
                e->loop_start = a0->valueint;
                e->loop_end = a1->valueint;
                e->has_loop = true;
            }
        }

        e->asset_id = test_anim_mmap_find_asset_id_by_name(assets_handle, e->file);
        if (e->asset_id < 0) {
            ESP_LOGW(TAG, "index: mmap has no file \"%s\" (name=%s)", e->file, e->name);
        }

        ESP_LOGI(TAG, "index[%zu] asset_id=%d pos=(%d,%d) has_loop=%d loop=[%d,%d]",
                 s_index_count, e->asset_id, e->x, e->y, (int)e->has_loop, e->loop_start, e->loop_end);
        s_index_count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "index.json: loaded %zu entries", s_index_count);
}

static void test_anim_show_index_entry(mmap_assets_handle_t assets_handle, gfx_obj_t *anim_obj, const test_anim_index_item_t *item)
{
    const void *anim_data = NULL;
    size_t anim_size = 0;
    gfx_anim_segment_t segments[3];

    if (item == NULL) {
        return;
    }

    test_app_log_step(TAG, item->name);

    if (item->asset_id < 0) {
        ESP_LOGW(TAG, "skip (no mmap file for \"%s\"): %s", item->file, item->name);
        return;
    }

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);

    anim_data = mmap_assets_get_mem(assets_handle, item->asset_id);
    anim_size = (size_t)mmap_assets_get_size(assets_handle, item->asset_id);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src(anim_obj, anim_data, anim_size));

    gfx_obj_set_size(anim_obj, 200, 150);
    gfx_anim_set_auto_mirror(anim_obj, false);

    if (item->x == 0 && item->y == 0) {
        gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    } else {
        gfx_obj_set_pos(anim_obj, item->x, item->y);
    }

    if (item->has_loop) {
        segments[0].start = 0;
        segments[0].end = (uint32_t)item->loop_start - 1;
        segments[0].fps = 50;
        segments[0].play_count = 1;
        segments[0].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[1].start = (uint32_t)item->loop_start;
        segments[1].end = (uint32_t)item->loop_end - 1;
        segments[1].fps = 50;
        segments[1].play_count = 5;
        segments[1].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[2].start = (uint32_t)item->loop_end;
        segments[2].end = 0xFFFFFFFF;
        segments[2].fps = 50;
        segments[2].play_count = 1;
        segments[2].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        ESP_LOGI("", "[0] segments: [%d, %d], (%d, %d)", segments[0].start, segments[0].end, segments[0].fps, segments[0].play_count);
        ESP_LOGI("", "[1] segments: [%d, %d], (%d, %d)", segments[1].start, segments[1].end, segments[1].fps, segments[1].play_count);
        ESP_LOGI("", "[2] segments: [%d, %d], (%d, %d)", segments[2].start, segments[2].end, segments[2].fps, segments[2].play_count);

        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segments(anim_obj, segments, TEST_APP_ARRAY_SIZE(segments)));
    } else {
        ESP_LOGI("", "[0] segments: [%d, %d], (%d, %d)", 0, 0xFFFFFFFF, 50, 1);
        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_obj, 0, 0xFFFFFFFF, 50, true));
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    test_app_unlock();
}

static void test_anim_run(mmap_assets_handle_t assets_handle)
{
    test_anim_index_load(assets_handle);
    TEST_ASSERT(s_index_count > 0);

    size_t case_index = 1;

    test_app_log_case(TAG, "Animation decoder validation (index.json)");
    s_anim_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_anim_events);

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    TEST_ASSERT_NOT_NULL(disp_default);
    gfx_disp_set_bg_color(disp_default, GFX_COLOR_HEX(0x101820));
    gfx_obj_t *anim_obj = gfx_anim_create(disp_default);
    gfx_obj_t *next_btn = gfx_button_create(disp_default);
    TEST_ASSERT_NOT_NULL(anim_obj);
    TEST_ASSERT_NOT_NULL(next_btn);

    gfx_obj_set_size(next_btn, 100, 40);
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_text(next_btn, "Next"));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_font(next_btn, (gfx_font_t)&font_puhui_16_4));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color(next_btn, GFX_COLOR_HEX(0x2A6DF4)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_bg_color_pressed(next_btn, GFX_COLOR_HEX(0x163D87)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_color(next_btn, GFX_COLOR_HEX(0xDCE8FF)));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_button_set_border_width(next_btn, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_touch_cb(next_btn, test_anim_next_btn_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(next_btn, GFX_ALIGN_TOP_MID, 0, 0));
    test_app_unlock();

    while (1) {
        test_anim_show_index_entry(assets_handle, anim_obj, &s_index_items[case_index % s_index_count]);

        while (1) {
            xEventGroupWaitBits(s_anim_events,
                                TEST_ANIM_EVENT_NEXT,
                                pdTRUE,
                                pdFALSE,
                                portMAX_DELAY);

            if (gfx_anim_drain_plan_blocking(anim_obj) == ESP_OK) {
                ESP_LOGI(TAG, "Play remaining done");
            }

            case_index = (case_index + 1) % s_index_count;
            break;
        }
    }
}

void test_anim_run_case_web(void)
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "anim_icon"));
    test_anim_run(runtime.assets_handle);
    test_app_runtime_close(&runtime);
}
