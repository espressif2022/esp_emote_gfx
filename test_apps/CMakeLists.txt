# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
# Only add unit-test-app components if the directory exists (for compatibility with ESP-IDF v6.0+)
if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_anim_player)
