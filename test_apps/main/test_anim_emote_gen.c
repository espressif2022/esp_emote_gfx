/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*********************
 *      INCLUDES
 *********************/

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mmap_generate_assets_test.h"
#include "unity.h"
#include "common.h"

/*********************
 *      DEFINES
 *********************/

#define TEST_ANIM_EVENT_NEXT        BIT0
#define TEST_ANIM_EVENT_END         BIT1
#define TEST_ANIM_INDEX_JSON_NAME   "index.json"
#define TEST_ANIM_INDEX_MAX         64
#define TEST_ANIM_INDEX_STR_LEN     96

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    char name[TEST_ANIM_INDEX_STR_LEN];
    char file[TEST_ANIM_INDEX_STR_LEN];
    int x;
    int y;
    int stop_frame;
    int loop_start;
    int loop_end;
    bool has_stop_frame;
    bool has_loop_range;
    int asset_id;
} test_anim_index_item_t;

/**********************
 *  STATIC VARIABLES
 **********************/

static const char *TAG = "anim_emote_gen";
static EventGroupHandle_t s_anim_events;
static gfx_obj_t *s_anim_wait_obj;
static test_anim_index_item_t s_index_items[TEST_ANIM_INDEX_MAX];
static size_t s_index_count;

/**********************
 * STATIC PROTOTYPES
 **********************/

static const char *test_anim_segment_action_str(gfx_anim_segment_action_t action);
static const char *test_anim_disp_event_str(gfx_disp_event_t event);
static void test_anim_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);
static void test_anim_disp_update_cb(gfx_disp_t *disp, gfx_disp_event_t event, const void *obj, void *user_data);
static int test_anim_mmap_find_asset_id_by_name(mmap_assets_handle_t assets_handle, const char *filename);
static int test_anim_mmap_find_index_json_id(mmap_assets_handle_t assets_handle);
static void test_anim_index_load(mmap_assets_handle_t assets_handle);
static bool test_anim_show_index_entry(mmap_assets_handle_t assets_handle, gfx_obj_t *anim_obj,
                                       const test_anim_index_item_t *item);
static void test_anim_run_case_emote_gen(mmap_assets_handle_t assets_handle);

/**********************
 *   STATIC FUNCTIONS
 **********************/

static const char *test_anim_segment_action_str(gfx_anim_segment_action_t action)
{
    switch (action) {
    case GFX_ANIM_SEGMENT_ACTION_CONTINUE:
        return "CONTINUE";
    case GFX_ANIM_SEGMENT_ACTION_PAUSE:
        return "PAUSE";
    default:
        return "UNKNOWN";
    }
}

static const char *test_anim_disp_event_str(gfx_disp_event_t event)
{
    switch (event) {
    case GFX_DISP_EVENT_IDLE:
        return "IDLE";
    case GFX_DISP_EVENT_ONE_FRAME_DONE:
        return "ONE_FRAME_DONE";
    case GFX_DISP_EVENT_PART_FRAME_DONE:
        return "PART_DONE";
    case GFX_DISP_EVENT_ALL_FRAME_DONE:
        return "ALL_DONE";
    default:
        return "UNKNOWN";
    }
}

static void test_anim_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    (void)touch;
    (void)user_data;

    if (event != NULL && event->type == GFX_TOUCH_EVENT_PRESS && s_anim_events != NULL) {
        xEventGroupSetBits(s_anim_events, TEST_ANIM_EVENT_NEXT);
        ESP_LOGI(TAG, "Next");
    }
}

static void test_anim_disp_update_cb(gfx_disp_t *disp, gfx_disp_event_t event, const void *obj, void *user_data)
{
    (void)disp;
    (void)user_data;

    if (s_anim_events == NULL || obj != s_anim_wait_obj) {
        return;
    }

    if (event == GFX_DISP_EVENT_PART_FRAME_DONE) {
        ESP_LOGI("", "disp_update_cb(%p): event:%s", obj, test_anim_disp_event_str(event));
        return;
    }

    if (event == GFX_DISP_EVENT_ALL_FRAME_DONE) {
        ESP_LOGI("", "disp_update_cb(%p): event:%s", obj, test_anim_disp_event_str(event));
        xEventGroupSetBits(s_anim_events, TEST_ANIM_EVENT_END);
    }
}

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
        if (cJSON_IsArray(jloop)) {
            int loop_size = cJSON_GetArraySize(jloop);
            cJSON *a0 = cJSON_GetArrayItem(jloop, 0);
            cJSON *a1 = cJSON_GetArrayItem(jloop, 1);
            if (loop_size == 1 && cJSON_IsNumber(a0)) {
                e->stop_frame = a0->valueint;
                e->has_stop_frame = true;
            } else if (loop_size >= 2 && cJSON_IsNumber(a0) && cJSON_IsNumber(a1)) {
                e->loop_start = a0->valueint;
                e->loop_end = a1->valueint;
                e->has_loop_range = true;
            }
        }

        e->asset_id = test_anim_mmap_find_asset_id_by_name(assets_handle, e->file);
        if (e->asset_id < 0) {
            ESP_LOGW(TAG, "index: mmap has no file \"%s\" (name=%s)", e->file, e->name);
        }

        ESP_LOGI("", "index[%2zu] name:%-20s pos:(%2d,%2d) stop:%3d has_loop:%d loop:[%3d,%3d]",
                 s_index_count, e->name, e->x, e->y, e->stop_frame, (int)e->has_loop_range, e->loop_start, e->loop_end);
        s_index_count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "index.json: loaded %zu entries", s_index_count);
}

static bool test_anim_show_index_entry(mmap_assets_handle_t assets_handle, gfx_obj_t *anim_obj, const test_anim_index_item_t *item)
{
    const void *anim_data = NULL;
    size_t anim_size = 0;
    gfx_anim_src_t anim_src;
    gfx_anim_segment_t segments[3];

    if (item == NULL) {
        return false;
    }

    test_app_log_step(TAG, item->name);

    if (item->asset_id < 0) {
        ESP_LOGW(TAG, "skip (no mmap file for \"%s\"): %s", item->file, item->name);
        return false;
    }

    TEST_ASSERT_EQUAL(ESP_OK, test_app_lock());
    gfx_anim_stop(anim_obj);

    anim_data = mmap_assets_get_mem(assets_handle, item->asset_id);
    anim_size = (size_t)mmap_assets_get_size(assets_handle, item->asset_id);
    anim_src.type = GFX_ANIM_SRC_TYPE_MEMORY;
    anim_src.data = anim_data;
    anim_src.data_len = anim_size;
    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_src_desc(anim_obj, &anim_src));

    gfx_obj_set_size(anim_obj, 200, 150);
    gfx_anim_set_auto_mirror(anim_obj, false);

    if (item->x == 0 && item->y == 0) {
        gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    } else {
        gfx_obj_set_pos(anim_obj, item->x, item->y);
    }

    if (item->has_loop_range) {
        segments[0].start = 0;
        segments[0].end = (uint32_t)item->loop_start - 1;
        segments[0].fps = 25;
        segments[0].play_count = 1;
        segments[0].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[1].start = (uint32_t)item->loop_start;
        segments[1].end = (uint32_t)item->loop_end - 1;
        segments[1].fps = 25;
        segments[1].play_count = 2;
        segments[1].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[2].start = (uint32_t)item->loop_end;
        segments[2].end = 0xFFFFFFFF;
        segments[2].fps = 25;
        segments[2].play_count = 1;
        segments[2].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        ESP_LOGD("", "[0] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ", action:%s)",
                 segments[0].start, segments[0].end, segments[0].fps, segments[0].play_count,
                 test_anim_segment_action_str(segments[0].end_action));
        ESP_LOGD("", "[1] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ", action:%s)",
                 segments[1].start, segments[1].end, segments[1].fps, segments[1].play_count,
                 test_anim_segment_action_str(segments[1].end_action));
        ESP_LOGD("", "[2] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ", action:%s)",
                 segments[2].start, segments[2].end, segments[2].fps, segments[2].play_count,
                 test_anim_segment_action_str(segments[2].end_action));

        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segments(anim_obj, segments, TEST_APP_ARRAY_SIZE(segments)));
    } else if (item->has_stop_frame) {
        segments[0].start = 0;
        segments[0].end = (uint32_t)item->stop_frame;
        segments[0].fps = 25;
        segments[0].play_count = 1;
        segments[0].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        segments[1].start = (uint32_t)item->stop_frame;
        segments[1].end = 0xFFFFFFFF;
        segments[1].fps = 25;
        segments[1].play_count = 1;
        segments[1].end_action = GFX_ANIM_SEGMENT_ACTION_CONTINUE;

        ESP_LOGD("", "[0] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ", action:%s)",
                 segments[0].start, segments[0].end, segments[0].fps, segments[0].play_count,
                 test_anim_segment_action_str(segments[0].end_action));
        ESP_LOGD("", "[1] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ", action:%s)",
                 segments[1].start, segments[1].end, segments[1].fps, segments[1].play_count,
                 test_anim_segment_action_str(segments[1].end_action));

        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segments(anim_obj, segments, 2));
    } else {
        ESP_LOGD("", "[0] segments: [%" PRIu32 ", %" PRIu32 "], (fps:%" PRIu32 ", play_count:%" PRIu32 ")",
                 (uint32_t)0, UINT32_MAX, (uint32_t)50, (uint32_t)1);
        TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_set_segment(anim_obj, 0, 0xFFFFFFFF, 25, false));
    }

    TEST_ASSERT_EQUAL(ESP_OK, gfx_anim_start(anim_obj));
    test_app_unlock();
    return true;
}

static void test_anim_run_case_emote_gen(mmap_assets_handle_t assets_handle)
{
    test_anim_index_load(assets_handle);
    TEST_ASSERT(s_index_count > 0);

    size_t case_index = 0;

    test_app_log_case(TAG, "Animation decoder validation (index.json)");
    s_anim_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_anim_events);
    test_app_set_touch_event_cb(test_anim_touch_event_cb, NULL);
    test_app_set_disp_update_cb(test_anim_disp_update_cb, NULL);

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
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_align(next_btn, GFX_ALIGN_TOP_MID, 0, 0));
    TEST_ASSERT_EQUAL(ESP_OK, gfx_obj_set_visible(next_btn, false));
    test_app_unlock();

    while (case_index < s_index_count) {
        const test_anim_index_item_t *item = &s_index_items[case_index];
        s_anim_wait_obj = anim_obj;
        xEventGroupClearBits(s_anim_events, TEST_ANIM_EVENT_END | TEST_ANIM_EVENT_NEXT);

        if (test_anim_show_index_entry(assets_handle, anim_obj, item)) {
            EventBits_t bits = xEventGroupWaitBits(s_anim_events,
                                                   TEST_ANIM_EVENT_END | TEST_ANIM_EVENT_NEXT,
                                                   pdTRUE,
                                                   pdFALSE,
                                                   portMAX_DELAY);
            if (bits & TEST_ANIM_EVENT_NEXT) {
                if (gfx_anim_play_left_to_tail(anim_obj) != ESP_OK) {
                    test_app_log_step(TAG, "drain remaining segments failed");
                }
            } else if (bits & TEST_ANIM_EVENT_END) {
                test_app_log_step(TAG, "segment plan completed");
                gfx_anim_stop(anim_obj);
            }
        }
        case_index++;
    }
}

/**********************
 *   TEST CASES
 **********************/

TEST_CASE("anim: emote create scene", "[widget][anim][emote_gen]")
{
    test_app_runtime_t runtime;

    TEST_ASSERT_EQUAL(ESP_OK, test_app_runtime_open(&runtime, "emote_gen"));

    test_anim_run_case_emote_gen(runtime.assets_handle);

    test_app_runtime_close(&runtime);
}
