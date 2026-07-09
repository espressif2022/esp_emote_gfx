cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../format_rgb565/components/hmi_rgb_board
)

set(GFX_LOCAL_COMMON_COMPONENTS_DIR "" CACHE PATH "Optional local board common_components directory")
foreach(component_name IN ITEMS esp_lvgl_adapter esp_lv_decoder)
    set(component_dir "${GFX_LOCAL_COMMON_COMPONENTS_DIR}/${component_name}")
    if(GFX_LOCAL_COMMON_COMPONENTS_DIR AND EXISTS "${component_dir}/CMakeLists.txt")
        list(APPEND EXTRA_COMPONENT_DIRS "${component_dir}")
    endif()
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gfx_arena_demo)
