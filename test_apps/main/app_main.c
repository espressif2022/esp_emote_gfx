#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/esp-bsp.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"

#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_lcd_panel_rgb.h"
#endif

#include "gfx.h"
#include "mmap_generate_test_assets.h"

static const char *TAG = "player";

#define TEST_MEMORY_LEAK_THRESHOLD  (500)

extern const gfx_image_dsc_t icon1;
extern const gfx_image_dsc_t icon5;

static size_t before_free_8bit;
static size_t before_free_32bit;

static gfx_handle_t emote_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

static gfx_obj_t *label_tips = NULL;

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

static bool flush_io_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    gfx_handle_t emote_handle = (gfx_handle_t)user_ctx;
    if (emote_handle) {
        gfx_emote_flush_ready(emote_handle, true);
    }
    return true;
}

static void flush_callback(gfx_handle_t emote_handle, int x1, int y1, int x2, int y2, const void *data)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)gfx_emote_get_user_data(emote_handle);
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, data);
    gfx_emote_flush_ready(emote_handle, true);
}

void clock_tm_callback(void *user_data)
{
    gfx_obj_t *label_obj = (gfx_obj_t *)user_data;
    if (label_obj) {
        gfx_label_set_text_fmt(label_obj, "%d*%d: %d", BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
    }
    ESP_LOGI("FPS", "%d*%d: %" PRIu32 "", BSP_LCD_H_RES, BSP_LCD_V_RES, gfx_timer_get_actual_fps(emote_handle));
}

// Test timer functionality
static void test_timer_functionality(void)
{
    ESP_LOGI(TAG, "=== Testing Timer Functionality ===");

    ESP_LOGI(TAG, "Timer created\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_handle_t timer = gfx_timer_create(emote_handle, clock_tm_callback, 1000, label_tips);
    TEST_ASSERT_NOT_NULL(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer period set to 500ms\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_set_period(timer, 500);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer repeat count set to 5\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_set_repeat_count(timer, 5);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer paused\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_pause(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer resumed\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_resume(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer reset\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_reset(timer);
    gfx_emote_unlock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Timer deleted\r\n");
    gfx_emote_lock(emote_handle);
    gfx_timer_delete(emote_handle, timer);
    gfx_emote_unlock(emote_handle);
}

// Test animation functionality
static void test_animation_functionality(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Animation Functionality ===");

    // Define test cases for different bit depths and animation files
    struct {
        int asset_id;
        const char *name;
        int mirror_offset;
    } test_cases[] = {
        {MMAP_TEST_ASSETS_MI_1_EYE_4BIT_AAF, "MI_1_EYE 4-bit animation", 10},
        {MMAP_TEST_ASSETS_MI_1_EYE_8BIT_AAF, "MI_1_EYE 8-bit animation", 10},
        {MMAP_TEST_ASSETS_MI_1_EYE_24BIT_AAF, "MI_1_EYE 24-bit animation", 10},
        {MMAP_TEST_ASSETS_MI_2_EYE_4BIT_AAF, "MI_2_EYE 4-bit animation", 100},
        {MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF, "MI_2_EYE 8-bit animation", 100},
        {MMAP_TEST_ASSETS_MI_2_EYE_24BIT_AAF, "MI_2_EYE 24-bit animation", 100}
    };

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        ESP_LOGI(TAG, "\n--- Testing %s ---", test_cases[i].name);

        gfx_emote_lock(emote_handle);

        // Create animation object
        gfx_obj_t *anim_obj = gfx_anim_create(emote_handle);
        TEST_ASSERT_NOT_NULL(anim_obj);
        ESP_LOGI(TAG, "Animation object created successfully");

        // Set animation source
        const void *anim_data = mmap_assets_get_mem(assets_handle, test_cases[i].asset_id);
        size_t anim_size = mmap_assets_get_size(assets_handle, test_cases[i].asset_id);
        esp_err_t ret = gfx_anim_set_src(anim_obj, anim_data, anim_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        ESP_LOGI(TAG, "%s source set successfully", test_cases[i].name);

        // Test animation configuration functions
        if (i < 3) {
            gfx_obj_set_pos(anim_obj, 20, 10);
        } else {
            gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
        }
        gfx_anim_set_mirror(anim_obj, false, 0);

        gfx_obj_set_size(anim_obj, 200, 150);
        ESP_LOGI(TAG, "Animation size set to 200x150");

        gfx_anim_set_segment(anim_obj, 0, 90, 30, true);
        ESP_LOGI(TAG, "Animation segment set: frames 0-90, 30fps, repeat=true");

        // Start animation
        ret = gfx_anim_start(anim_obj);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        ESP_LOGI(TAG, "%s started successfully", test_cases[i].name);

        gfx_emote_unlock(emote_handle);

        // Wait for animation to run
        vTaskDelay(pdMS_TO_TICKS(3000));

        // Test with mirror enabled
        gfx_emote_lock(emote_handle);
        gfx_anim_set_mirror(anim_obj, true, test_cases[i].mirror_offset);
        ESP_LOGI(TAG, "Animation mirror enabled with offset %d", test_cases[i].mirror_offset);
        gfx_emote_unlock(emote_handle);

        // Wait for mirror animation to run
        vTaskDelay(pdMS_TO_TICKS(3000));

        // Stop animation
        gfx_emote_lock(emote_handle);
        
        ret = gfx_anim_stop(anim_obj);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        ESP_LOGI(TAG, "%s stopped successfully", test_cases[i].name);
        
        gfx_emote_unlock(emote_handle);

        vTaskDelay(pdMS_TO_TICKS(3000));

        // Delete animation object
        gfx_emote_lock(emote_handle);

        gfx_obj_delete(anim_obj);
        ESP_LOGI(TAG, "%s object deleted successfully", test_cases[i].name);

        gfx_emote_unlock(emote_handle);

        // Wait before next test
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "\n=== Animation Functionality Testing Completed ===");
}

// Test label functionality
static void test_label_functionality(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Label Functionality ===");

    gfx_emote_lock(emote_handle);

    // Create label object
    gfx_obj_t *label_obj = gfx_label_create(emote_handle);
    TEST_ASSERT_NOT_NULL(label_obj);
    ESP_LOGI(TAG, "Label object created successfully");

    // Set font
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .mem_size = (size_t)mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
    };

    gfx_font_t font_DejaVuSans;
    esp_err_t ret = gfx_label_new_font(emote_handle, &font_cfg, &font_DejaVuSans);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "Font loaded successfully");

    // Test label configuration functions
    gfx_label_set_font(label_obj, font_DejaVuSans);
    ESP_LOGI(TAG, "Font set for label");

    gfx_label_set_text(label_obj, "Hello World!");
    ESP_LOGI(TAG, "Label text set to 'Hello World!'");

    gfx_label_set_font_size(label_obj, 24);
    ESP_LOGI(TAG, "Label font size set to 24");

    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x00FF00));
    ESP_LOGI(TAG, "Label color set to green");

    gfx_obj_set_pos(label_obj, 100, 200);
    ESP_LOGI(TAG, "Label position set to (100, 200)");

    gfx_obj_align(label_obj, GFX_ALIGN_TOP_MID, 0, 20);
    ESP_LOGI(TAG, "Label aligned to top center with 20px offset");

    gfx_obj_set_size(label_obj, 300, 50);
    ESP_LOGI(TAG, "Label size set to 300x50");

    // Test formatted text
    gfx_label_set_text_fmt(label_obj, "Count: %d, Float: %.2f", 42, 3.14);
    ESP_LOGI(TAG, "Label formatted text set");

    gfx_emote_unlock(emote_handle);

    // Wait for display
    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0x0000FF));
    ESP_LOGI(TAG, "Label color set to blue");
    gfx_emote_unlock(emote_handle);

    // Wait for display
    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);
    // Delete label object
    gfx_obj_delete(label_obj);
    ESP_LOGI(TAG, "Label object deleted successfully");
    gfx_emote_unlock(emote_handle);
}

// Test unified image functionality (both C_ARRAY and BIN formats)
static void test_unified_image_functionality(mmap_assets_handle_t assets_handle)
{
    gfx_image_dsc_t img_dsc;

    ESP_LOGI(TAG, "=== Testing Unified Image Functionality ===");

    gfx_emote_lock(emote_handle);

    // Test 1: C_ARRAY format image
    ESP_LOGI(TAG, "--- Testing C_ARRAY format image ---");
    gfx_obj_t *img_obj_c_array = gfx_img_create(emote_handle);
    TEST_ASSERT_NOT_NULL(img_obj_c_array);

    gfx_img_set_src(img_obj_c_array, (void*)&icon1);

    gfx_obj_set_pos(img_obj_c_array, 100, 100);

    uint16_t width, height;
    gfx_obj_get_size(img_obj_c_array, &width, &height);
    ESP_LOGI(TAG, "C_ARRAY image size: %dx%d", width, height);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj_c_array);

    // Test 2: BIN format image
    ESP_LOGI(TAG, "--- Testing BIN format image ---");
    gfx_obj_t *img_obj_bin = gfx_img_create(emote_handle);
    TEST_ASSERT_NOT_NULL(img_obj_bin);

    img_dsc.data_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_ICON5_BIN);
    img_dsc.data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_ICON5_BIN);

	memcpy(&img_dsc.header, mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_ICON5_BIN), sizeof(gfx_image_header_t));
	img_dsc.data += sizeof(gfx_image_header_t);
	img_dsc.data_size -= sizeof(gfx_image_header_t);
    gfx_img_set_src(img_obj_bin, (void*)&img_dsc);

    gfx_obj_set_pos(img_obj_bin, 100, 180);

    gfx_obj_get_size(img_obj_bin, &width, &height);
    ESP_LOGI(TAG, "BIN image size: %dx%d", width, height);

    gfx_emote_unlock(emote_handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj_bin);

    // Test 3: Multiple images with different formats
    ESP_LOGI(TAG, "--- Testing multiple images with different formats ---");
    gfx_obj_t *img_obj1 = gfx_img_create(emote_handle);
    gfx_obj_t *img_obj2 = gfx_img_create(emote_handle);
    TEST_ASSERT_NOT_NULL(img_obj1);
    TEST_ASSERT_NOT_NULL(img_obj2);

    gfx_img_set_src(img_obj1, (void*)&icon5);  // C_ARRAY format

    img_dsc.data_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_ICON1_BIN);
    img_dsc.data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_ICON1_BIN);

	memcpy(&img_dsc.header, mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_ICON1_BIN), sizeof(gfx_image_header_t));
	img_dsc.data += sizeof(gfx_image_header_t);
	img_dsc.data_size -= sizeof(gfx_image_header_t);
    gfx_img_set_src(img_obj2, (void*)&img_dsc); // BIN format

    gfx_obj_set_pos(img_obj1, 150, 100);
    gfx_obj_set_pos(img_obj2, 150, 180);

    gfx_emote_unlock(emote_handle);

    // vTaskDelay(pdMS_TO_TICKS(30));
    // gfx_emote_lock(emote_handle);

    vTaskDelay(pdMS_TO_TICKS(3000));

    gfx_emote_lock(emote_handle);
    gfx_obj_delete(img_obj1);
    gfx_obj_delete(img_obj2);
    gfx_emote_unlock(emote_handle);
}

// Comprehensive test: Create multiple objects and test interaction
static void test_multiple_objects_interaction(mmap_assets_handle_t assets_handle)
{
    ESP_LOGI(TAG, "=== Testing Multiple Objects Interaction ===");

    gfx_emote_lock(emote_handle);

    // Create multiple objects
    gfx_obj_t *anim_obj = gfx_anim_create(emote_handle);
    gfx_obj_t *label_obj = gfx_label_create(emote_handle);
    gfx_obj_t *img_obj = gfx_img_create(emote_handle);
    gfx_timer_handle_t timer = gfx_timer_create(emote_handle, clock_tm_callback, 2000, label_obj);

    TEST_ASSERT_NOT_NULL(anim_obj);
    TEST_ASSERT_NOT_NULL(label_obj);
    TEST_ASSERT_NOT_NULL(img_obj);
    TEST_ASSERT_NOT_NULL(timer);
    ESP_LOGI(TAG, "Multiple objects created successfully");

    // Configure animation
    const void *anim_data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_MI_2_EYE_8BIT_AAF);
    gfx_anim_set_src(anim_obj, anim_data, anim_size);
    gfx_obj_align(anim_obj, GFX_ALIGN_CENTER, 0, 0);
    gfx_anim_set_segment(anim_obj, 0, 30, 15, true);
    gfx_anim_start(anim_obj);

    // Configure label
    // Set font
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
        .mem_size = (size_t)mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_DEJAVUSANS_TTF),
    };

    gfx_font_t font_DejaVuSans;
    esp_err_t ret = gfx_label_new_font(emote_handle, &font_cfg, &font_DejaVuSans);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    gfx_obj_set_size(label_obj, 150, 50);
    gfx_label_set_font(label_obj, font_DejaVuSans);
    gfx_label_set_text(label_obj, "Multi-Object Test");
    gfx_label_set_font_size(label_obj, 20);
    gfx_label_set_color(label_obj, GFX_COLOR_HEX(0xFF0000));
    gfx_obj_align(label_obj, GFX_ALIGN_CENTER, 0, 0); // Position above center

    // Configure image
    gfx_image_dsc_t img_dsc;
    const void *img_data = mmap_assets_get_mem(assets_handle, MMAP_TEST_ASSETS_ICON1_BIN);
    img_dsc.data_size = mmap_assets_get_size(assets_handle, MMAP_TEST_ASSETS_ICON1_BIN);
    img_dsc.data = img_data;

	memcpy(&img_dsc.header, img_data, sizeof(gfx_image_header_t));
	img_dsc.data += sizeof(gfx_image_header_t);
	img_dsc.data_size -= sizeof(gfx_image_header_t);

    gfx_img_set_src(img_obj, (void*)&img_dsc);  // Use BIN format image
    gfx_obj_align(img_obj, GFX_ALIGN_TOP_MID, 0, 0);

    ESP_LOGI(TAG, "All objects configured and started");

    gfx_emote_unlock(emote_handle);

    // Run for a while to observe interaction
    vTaskDelay(pdMS_TO_TICKS(1000 * 10));

    gfx_emote_lock(emote_handle);
    // Clean up all objects
    gfx_timer_delete(emote_handle, timer);
    gfx_obj_delete(anim_obj);
    gfx_obj_delete(label_obj);
    gfx_obj_delete(img_obj);
    ESP_LOGI(TAG, "All objects deleted successfully");
    gfx_emote_unlock(emote_handle);
}

// Initialize display and graphics system
static esp_err_t init_display_and_graphics(const char *partition_label, uint32_t max_files, uint32_t checksum, mmap_assets_handle_t *assets_handle)
{
    // Initialize assets
    const mmap_assets_config_t asset_config = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    esp_err_t ret = mmap_assets_new(&asset_config, assets_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize assets");
        return ret;
    }

    // Initialize display
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * 100) * sizeof(uint16_t),
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    bsp_display_brightness_init();
    bsp_display_backlight_on();

    gfx_core_config_t gfx_cfg = {
        .flush_cb = flush_callback,
        .update_cb = NULL,
        .user_data = panel_handle,
        .flags = {.swap = true, .double_buffer = true},
        .h_res = BSP_LCD_H_RES,
        .v_res = BSP_LCD_V_RES,
        .fps = 50,
        .buffers = {.buf1 = NULL, .buf2 = NULL, .buf_pixels = BSP_LCD_H_RES * 16},
        .task = GFX_EMOTE_INIT_CONFIG()
    };
    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 7;
    gfx_cfg.task.task_stack = 20 * 1024;

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = flush_io_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, emote_handle);

    emote_handle = gfx_emote_init(&gfx_cfg);
    if (emote_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize graphics system");
        mmap_assets_del(*assets_handle);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Clean up display and graphics system
static void cleanup_display_and_graphics(mmap_assets_handle_t assets_handle)
{
    if (emote_handle != NULL) {
        gfx_emote_deinit(emote_handle);
        emote_handle = NULL;
    }
    if (assets_handle != NULL) {
        mmap_assets_del(assets_handle);
    }

    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
}

TEST_CASE("test timer functionality", "[timer]")
{
    // Initialize display and graphics system
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = init_display_and_graphics("assets_8bit", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_timer_functionality();

    cleanup_display_and_graphics(assets_handle);
}

TEST_CASE("test animation functionality", "[animation]")
{
    // Initialize display and graphics system
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = init_display_and_graphics("assets_8bit", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_animation_functionality(assets_handle);

    cleanup_display_and_graphics(assets_handle);
}

TEST_CASE("test label functionality", "[label]")
{
    // Initialize display and graphics system
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = init_display_and_graphics("assets_8bit", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_label_functionality(assets_handle);

    cleanup_display_and_graphics(assets_handle);
}

TEST_CASE("test unified image functionality", "[unified_image]")
{
    // Initialize display and graphics system
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = init_display_and_graphics("assets_8bit", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_unified_image_functionality(assets_handle);

    cleanup_display_and_graphics(assets_handle);
}

TEST_CASE("test multiple objects interaction", "[interaction]")
{
    // Initialize display and graphics system
    mmap_assets_handle_t assets_handle = NULL;
    esp_err_t ret = init_display_and_graphics("assets_8bit", MMAP_TEST_ASSETS_FILES, MMAP_TEST_ASSETS_CHECKSUM, &assets_handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    test_multiple_objects_interaction(assets_handle);

    cleanup_display_and_graphics(assets_handle);
}

void app_main(void)
{
    printf("Animation player test\n");
    unity_run_menu();
}