idf_component_register(
    SRCS "src/hmi_rgb_board.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_lcd_touch esp_lcd_touch_gt1151 esp_driver_gpio esp_driver_i2c
)
