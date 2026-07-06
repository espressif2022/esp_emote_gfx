/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

#include "gfx/backends/esp_lcd.h"
#include "gfx_hw_runtime.h"
#include "mmap_generate_assets_test.h"
#include "test_board.h"

static const char *const TAG = "gfx_hw_runtime";

gfx_handle_t emote_handle = NULL;
gfx_display_t *disp_default = NULL;
gfx_touch_t *touch_default = NULL;

static test_app_touch_event_cb_t s_test_app_touch_event_cb = NULL;
static void *s_test_app_touch_event_user_data = NULL;
static test_app_disp_update_cb_t s_test_app_disp_update_cb = NULL;
static void *s_test_app_disp_update_user_data = NULL;

static void test_app_configure_gfx_log_levels(void)
{
    gfx_log_set_level_all(GFX_LOG_LEVEL_INFO);

    gfx_log_set_level(GFX_LOG_MODULE_LABEL_DRAW, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_LABEL, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_LABEL_OBJ, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_FONT_FREETYPE, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_FONT_LVGL, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_ANIM, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_IMG, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_MESH_IMG, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_QRCODE, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_BUTTON, GFX_LOG_LEVEL_INFO);
    gfx_log_set_level(GFX_LOG_MODULE_RENDER, GFX_LOG_LEVEL_INFO);
}

static void disp_update_callback(gfx_display_t *disp, gfx_display_event_t event, const void *obj)
{
    if (s_test_app_disp_update_cb != NULL) {
        s_test_app_disp_update_cb(disp, event, obj, s_test_app_disp_update_user_data);
    }
}

static void touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data)
{
    (void)user_data;

    if (s_test_app_touch_event_cb != NULL) {
        s_test_app_touch_event_cb(touch, event, s_test_app_touch_event_user_data);
    }

    switch (event->type) {
    case GFX_TOUCH_EVENT_PRESS:
        ESP_LOGI(TAG, "touch press  : %p, (%d, %d)", touch, event->x, event->y);
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        ESP_LOGI(TAG, "touch release: %p, (%d, %d)", touch, event->x, event->y);
        break;
    default:
        break;
    }
}

void test_app_set_touch_event_cb(test_app_touch_event_cb_t cb, void *user_data)
{
    s_test_app_touch_event_cb = cb;
    s_test_app_touch_event_user_data = user_data;
}

void test_app_set_disp_update_cb(test_app_disp_update_cb_t cb, void *user_data)
{
    s_test_app_disp_update_cb = cb;
    s_test_app_disp_update_user_data = user_data;
}

esp_err_t test_app_runtime_open(test_app_runtime_t *runtime, const char *partition_label)
{
    ESP_RETURN_ON_FALSE(runtime != NULL, ESP_ERR_INVALID_ARG, TAG, "runtime is NULL");
    ESP_RETURN_ON_FALSE(partition_label != NULL && partition_label[0] != '\0', ESP_ERR_INVALID_ARG, TAG,
                        "partition_label is NULL or empty");

    runtime->assets_handle = NULL;
    test_app_configure_gfx_log_levels();
    test_app_set_touch_event_cb(NULL, NULL);
    test_app_set_disp_update_cb(NULL, NULL);
    return display_and_graphics_init(partition_label, MMAP_ASSETS_TEST_FILES, MMAP_ASSETS_TEST_CHECKSUM,
                                     &runtime->assets_handle);
}

void test_app_runtime_close(test_app_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }

    display_and_graphics_clean(runtime->assets_handle);
    runtime->assets_handle = NULL;
    test_app_set_touch_event_cb(NULL, NULL);
    test_app_set_disp_update_cb(NULL, NULL);
}

esp_err_t test_app_lock(void)
{
    return gfx_core_lock(emote_handle);
}

void test_app_unlock(void)
{
    gfx_core_unlock(emote_handle);
}

void clock_tm_callback(void *user_data)
{
    gfx_object_t *label_obj = (gfx_object_t *)user_data;
    ESP_LOGI(TAG, "FPS: %d*%d: %" PRIu32, BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    if (label_obj) {
        gfx_label_set_text_fmt(label_obj, "%d*%d: %d", BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    }
}

esp_err_t load_image(mmap_assets_handle_t assets_handle, int asset_id, gfx_image_dsc_t *img_dsc)
{
    if (img_dsc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const void *img_data = mmap_assets_get_mem(assets_handle, asset_id);
    if (img_data == NULL) {
        return ESP_FAIL;
    }

    size_t img_size = mmap_assets_get_size(assets_handle, asset_id);
    if (img_size < sizeof(gfx_image_header_t)) {
        return ESP_FAIL;
    }

    memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));
    img_dsc->data = (const uint8_t *)img_data + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);

    return ESP_OK;
}

esp_err_t display_and_graphics_init(const char *partition_label, uint32_t max_files, uint32_t checksum,
                                    mmap_assets_handle_t *assets_handle)
{
    esp_err_t ret = ESP_OK;
    gfx_backend_t *backend = NULL;

    ret = mmap_assets_new(&(mmap_assets_config_t) {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {.mmap_enable = true, .full_check = true},
    }, assets_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize assets");

    ESP_GOTO_ON_ERROR(test_board_init(), err_assets, TAG, "Failed to initialize board");

    gfx_core_config_t gfx_cfg = {
        .fps = 30,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    };
    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 7;
    gfx_cfg.task.task_stack = 20 * 1024;
    emote_handle = gfx_core_init(&gfx_cfg);
    ESP_GOTO_ON_FALSE(emote_handle != NULL, ESP_FAIL, err_gfx, TAG, "Failed to initialize graphics system");

    backend = gfx_backend_esp_lcd_create(&(gfx_backend_esp_lcd_config_t) {
        .panel = panel_handle,
        .panel_io = io_handle,
        .interface = test_board_lcd_interface(),
    });
    ESP_GOTO_ON_FALSE(backend != NULL, ESP_FAIL, err_gfx, TAG, "Failed to create LCD backend");

    disp_default = gfx_display_add(emote_handle, &(gfx_display_config_t) {
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .color_format = GFX_COLOR_FORMAT_RGB565_SWAPPED,
        .backend = backend,
        .update_cb = disp_update_callback,
        .flags = { .buff_dma = true, .buff_spiram = false, .double_buffer = true },
        .buffers = { .buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16 },
    });
    ESP_GOTO_ON_FALSE(disp_default != NULL, ESP_FAIL, err_gfx, TAG, "Failed to add display");

    touch_default = gfx_touch_add(emote_handle, &(gfx_touch_config_t) {
        .driver_handle = test_board_touch(),
        .event_cb = touch_event_cb,
        .disp = disp_default,
        .poll_ms = 50,
        .user_data = emote_handle,
    });
    ESP_GOTO_ON_FALSE(touch_default != NULL, ESP_FAIL, err_gfx, TAG, "Failed to add touch");

    return ESP_OK;

err_gfx:
    if (backend != NULL && disp_default == NULL) {
        gfx_backend_esp_lcd_delete(backend);
        backend = NULL;
    }
    if (emote_handle != NULL) {
        gfx_core_deinit(emote_handle);
        emote_handle = NULL;
        disp_default = NULL;
        touch_default = NULL;
    }
    test_board_deinit();
err_assets:
    mmap_assets_del(*assets_handle);
    return ret;
}

void display_and_graphics_clean(mmap_assets_handle_t assets_handle)
{
    if (emote_handle != NULL) {
        gfx_core_deinit(emote_handle);
        emote_handle = NULL;
        disp_default = NULL;
        touch_default = NULL;
    }
    if (assets_handle != NULL) {
        mmap_assets_del(assets_handle);
    }
    test_board_deinit();
    test_app_wait_ms(1000);
}
