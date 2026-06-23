#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#

if(NOT DEFINED GFX_HOST_SDL_DEMO)
    message(FATAL_ERROR "GFX_HOST_SDL_DEMO is not set")
endif()

set(ENV{SDL_VIDEODRIVER} "dummy")
execute_process(
    COMMAND timeout 3s "${GFX_HOST_SDL_DEMO}"
    RESULT_VARIABLE result
)

if(result EQUAL 0 OR result EQUAL 124)
    message(STATUS "SDL dummy smoke completed with expected result=${result}")
else()
    message(FATAL_ERROR "SDL dummy smoke failed with result=${result}")
endif()
