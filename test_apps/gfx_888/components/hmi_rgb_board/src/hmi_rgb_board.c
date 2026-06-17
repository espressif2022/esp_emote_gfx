/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hmi_rgb_board.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt1151.h"

static const char *TAG = "hmi_rgb_board";
static i2c_master_bus_handle_t s_touch_i2c_bus;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static const uint8_t s_touch_addr = 0x14U;

#define HMI_TOUCH_PROBE_TIMEOUT_MS 100
#define HMI_GT1151_PRODUCT_ID_REG_H 0x81
#define HMI_GT1151_PRODUCT_ID_REG_L 0x40

static void fill_data_gpio_nums(gpio_num_t data_gpio_nums[ESP_LCD_RGB_BUS_WIDTH_MAX])
{
    data_gpio_nums[0] = HMI_RGB_BOARD_LCD_DATA0;
    data_gpio_nums[1] = HMI_RGB_BOARD_LCD_DATA1;
    data_gpio_nums[2] = HMI_RGB_BOARD_LCD_DATA2;
    data_gpio_nums[3] = HMI_RGB_BOARD_LCD_DATA3;
    data_gpio_nums[4] = HMI_RGB_BOARD_LCD_DATA4;
    data_gpio_nums[5] = HMI_RGB_BOARD_LCD_DATA5;
    data_gpio_nums[6] = HMI_RGB_BOARD_LCD_DATA6;
    data_gpio_nums[7] = HMI_RGB_BOARD_LCD_DATA7;
    data_gpio_nums[8] = HMI_RGB_BOARD_LCD_DATA8;
    data_gpio_nums[9] = HMI_RGB_BOARD_LCD_DATA9;
    data_gpio_nums[10] = HMI_RGB_BOARD_LCD_DATA10;
    data_gpio_nums[11] = HMI_RGB_BOARD_LCD_DATA11;
    data_gpio_nums[12] = HMI_RGB_BOARD_LCD_DATA12;
    data_gpio_nums[13] = HMI_RGB_BOARD_LCD_DATA13;
    data_gpio_nums[14] = HMI_RGB_BOARD_LCD_DATA14;
    data_gpio_nums[15] = HMI_RGB_BOARD_LCD_DATA15;
    data_gpio_nums[16] = HMI_RGB_BOARD_LCD_DATA16;
    data_gpio_nums[17] = HMI_RGB_BOARD_LCD_DATA17;
    data_gpio_nums[18] = HMI_RGB_BOARD_LCD_DATA18;
    data_gpio_nums[19] = HMI_RGB_BOARD_LCD_DATA19;
    data_gpio_nums[20] = HMI_RGB_BOARD_LCD_DATA20;
    data_gpio_nums[21] = HMI_RGB_BOARD_LCD_DATA21;
    data_gpio_nums[22] = HMI_RGB_BOARD_LCD_DATA22;
    data_gpio_nums[23] = HMI_RGB_BOARD_LCD_DATA23;
}

static uint64_t gpio_pin_bit64(unsigned gpio_num)
{
    if (gpio_num >= 64U) {
        return 0ULL;
    }
    return 1ULL << gpio_num;
}

static esp_err_t hmi_rgb_board_touch_i2c_init(void)
{
    if (s_touch_i2c_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = HMI_RGB_BOARD_I2C_PORT,
        .sda_io_num = HMI_RGB_BOARD_TOUCH_I2C_SDA,
        .scl_io_num = HMI_RGB_BOARD_TOUCH_I2C_SCL,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_touch_i2c_bus), TAG,
                        "create touch I2C bus failed");
    return ESP_OK;
}

static void hmi_rgb_board_touch_scan_bus(void)
{
    if (s_touch_i2c_bus == NULL) {
        return;
    }

    bool found = false;
    ESP_LOGI(TAG, "touch scan on SDA=%d SCL=%d", HMI_RGB_BOARD_TOUCH_I2C_SDA, HMI_RGB_BOARD_TOUCH_I2C_SCL);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        esp_err_t ret = i2c_master_probe(s_touch_i2c_bus, addr, HMI_TOUCH_PROBE_TIMEOUT_MS);
        if (ret == ESP_OK) {
            found = true;
            ESP_LOGI(TAG, "I2C device found at 0x%02x", addr);
        }
    }
    if (!found) {
        ESP_LOGW(TAG, "no I2C device responded on touch bus");
    }
}

static esp_err_t hmi_rgb_board_touch_probe_product_id(uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = HMI_RGB_BOARD_TOUCH_I2C_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    esp_err_t ret = i2c_master_bus_add_device(s_touch_i2c_bus, &dev_cfg, &dev);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t reg[2] = {HMI_GT1151_PRODUCT_ID_REG_H, HMI_GT1151_PRODUCT_ID_REG_L};
    uint8_t buf[6] = {0};
    ret = i2c_master_transmit_receive(dev, reg, sizeof(reg), buf, sizeof(buf), HMI_TOUCH_PROBE_TIMEOUT_MS);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "addr 0x%02x product-id raw: %02x %02x %02x %02x %02x %02x",
                 addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    } else {
        ESP_LOGW(TAG, "addr 0x%02x product-id read failed: %s", addr, esp_err_to_name(ret));
    }

    i2c_master_bus_rm_device(dev);
    return ret;
}

static esp_err_t hmi_rgb_board_touch_select_addr(uint8_t addr)
{
    if (HMI_RGB_BOARD_TOUCH_RST == GPIO_NUM_NC || HMI_RGB_BOARD_TOUCH_INT == GPIO_NUM_NC) {
        return ESP_OK;
    }

    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = gpio_pin_bit64((unsigned)HMI_RGB_BOARD_TOUCH_RST) |
                        gpio_pin_bit64((unsigned)HMI_RGB_BOARD_TOUCH_INT),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_cfg), TAG, "configure touch rst/int failed");

    int int_level = (addr == 0x5DU) ? 1 : 0;

    ESP_RETURN_ON_ERROR(gpio_set_level(HMI_RGB_BOARD_TOUCH_RST, 0), TAG, "set touch rst low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(HMI_RGB_BOARD_TOUCH_INT, int_level), TAG, "set touch int level failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_RETURN_ON_ERROR(gpio_set_level(HMI_RGB_BOARD_TOUCH_RST, 1), TAG, "set touch rst high failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_direction(HMI_RGB_BOARD_TOUCH_INT, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static esp_err_t hmi_rgb_board_touch_try_init_addr(uint8_t addr, esp_lcd_touch_handle_t *out_touch)
{
    esp_lcd_touch_config_t touch_config = {
        .x_max = HMI_RGB_BOARD_LCD_H_RES,
        .y_max = HMI_RGB_BOARD_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
    };

    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT1151_CONFIG();
    touch_io_config.scl_speed_hz = HMI_RGB_BOARD_TOUCH_I2C_HZ;
    touch_io_config.dev_addr = addr;

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_touch_handle_t tp = NULL;
    esp_err_t ret = esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &touch_io_config, &io);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_lcd_touch_new_i2c_gt1151(io, &touch_config, &tp);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(io);
        return ret;
    }

    s_touch_io = io;
    s_touch = tp;
    *out_touch = tp;
    return ESP_OK;
}

esp_err_t hmi_rgb_board_backlight_init(void)
{
    const int bk_pin = (int)HMI_RGB_BOARD_LCD_BACKLIGHT;
    if (bk_pin < 0) {
        return ESP_OK;
    }

    gpio_config_t cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = gpio_pin_bit64((unsigned)bk_pin),
    };
    return gpio_config(&cfg);
}

void hmi_rgb_board_backlight_set(bool on)
{
    const int bk_pin = (int)HMI_RGB_BOARD_LCD_BACKLIGHT;
    if (bk_pin < 0 || bk_pin >= 64) {
        return;
    }
    gpio_set_level((gpio_num_t)bk_pin,
                   on ? HMI_RGB_BOARD_LCD_BACKLIGHT_ON_LEVEL : HMI_RGB_BOARD_LCD_BACKLIGHT_OFF_LEVEL);
}

esp_err_t hmi_rgb_board_rgb_panel_create(esp_lcd_panel_handle_t *out_panel, uint8_t num_fbs)
{
    ESP_RETURN_ON_FALSE(out_panel != NULL, ESP_ERR_INVALID_ARG, TAG, "panel handle is null");

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = HMI_RGB_BOARD_LCD_PIXEL_CLOCK_HZ,
            .h_res = HMI_RGB_BOARD_LCD_H_RES,
            .v_res = HMI_RGB_BOARD_LCD_V_RES,
            .hsync_pulse_width = HMI_RGB_BOARD_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = HMI_RGB_BOARD_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = HMI_RGB_BOARD_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = HMI_RGB_BOARD_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = HMI_RGB_BOARD_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = HMI_RGB_BOARD_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = HMI_RGB_BOARD_LCD_PCLK_ACTIVE_NEG,
            },
        },
        .data_width = HMI_RGB_BOARD_LCD_DATA_WIDTH,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .num_fbs = num_fbs,
        .dma_burst_size = 64,
        .hsync_gpio_num = HMI_RGB_BOARD_LCD_HSYNC,
        .vsync_gpio_num = HMI_RGB_BOARD_LCD_VSYNC,
        .de_gpio_num = HMI_RGB_BOARD_LCD_DE,
        .pclk_gpio_num = HMI_RGB_BOARD_LCD_PCLK,
        .disp_gpio_num = HMI_RGB_BOARD_LCD_DISP_EN,
        .flags = {
            .fb_in_psram = true,
        },
    };

    fill_data_gpio_nums(panel_config.data_gpio_nums);
    return esp_lcd_new_rgb_panel(&panel_config, out_panel);
}

esp_err_t hmi_rgb_board_rgb_panel_boot(esp_lcd_panel_handle_t panel)
{
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_ARG, TAG, "panel handle is null");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "RGB panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "RGB panel init failed");
    return ESP_OK;
}

esp_err_t hmi_rgb_board_touch_new(esp_lcd_touch_handle_t *out_touch)
{
    ESP_RETURN_ON_FALSE(out_touch != NULL, ESP_ERR_INVALID_ARG, TAG, "touch handle is null");

    if (s_touch != NULL) {
        *out_touch = s_touch;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(hmi_rgb_board_touch_i2c_init(), TAG, "touch I2C init failed");
    hmi_rgb_board_touch_scan_bus();

    (void)hmi_rgb_board_touch_probe_product_id(s_touch_addr);
    esp_err_t ret = hmi_rgb_board_touch_try_init_addr(s_touch_addr, out_touch);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "GT11xx touch initialized at I2C address 0x%02x", s_touch_addr);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "GT11xx probe at 0x%02x failed: %s", s_touch_addr, esp_err_to_name(ret));
    return ret;
}
