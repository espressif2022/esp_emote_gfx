cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/components/hmi_rgb_board
    /home/xuxin/esp_work/ESP32-S31-Korvo/common_components/esp_lvgl_adapter
    /home/xuxin/esp_work/ESP32-S31-Korvo/common_components/esp_lv_decoder
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gfx_565)
