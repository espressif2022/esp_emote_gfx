if(COMMAND idf_component_register)
    idf_build_get_property(GFX_IDF_PATH IDF_PATH)

    # Keep source discovery automatic while ensuring newly added files trigger
    # CMake regeneration in normal configure mode. ESP-IDF also evaluates
    # component CMakeLists in script mode while collecting requirements, where
    # CONFIGURE_DEPENDS is not accepted.
    if(CMAKE_SCRIPT_MODE_FILE)
        file(GLOB_RECURSE SRC_FILES "src/*.c")
    else()
        file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.c")
    endif()

    list(FILTER SRC_FILES EXCLUDE REGEX "/src/platform/linux/")
    list(FILTER SRC_FILES EXCLUDE REGEX "/src/platform/host/")
    list(FILTER SRC_FILES EXCLUDE REGEX "/src/backend/sdl/")

    set(PRIV_INCLUDE_DIRS
        "src"
    )

    set(GFX_IDF_REQUIRES
        esp_lcd
        esp_partition
        esp_timer
    )

    list(APPEND GFX_IDF_REQUIRES esp_mmap_assets)

    if(EXISTS "${GFX_IDF_PATH}/components/esp_driver_ppa/CMakeLists.txt")
        list(APPEND GFX_IDF_REQUIRES esp_driver_ppa esp_mm)
    elseif(EXISTS "${GFX_IDF_PATH}/components/esp_mm/CMakeLists.txt")
        list(APPEND GFX_IDF_REQUIRES esp_mm)
    endif()

    if(EXISTS "${GFX_IDF_PATH}/components/esp_driver_jpeg/CMakeLists.txt")
        list(APPEND GFX_IDF_REQUIRES esp_driver_jpeg)
    endif()

    idf_component_register(
        SRCS
            ${SRC_FILES}
        INCLUDE_DIRS
            "include"
            "src"
        PRIV_INCLUDE_DIRS
            ${PRIV_INCLUDE_DIRS}
        REQUIRES
            ${GFX_IDF_REQUIRES}
    )

    # esp_async_fbcpy.h lives in esp_lcd/priv_include (same as esp_lvgl_adapter).
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        if(EXISTS "${GFX_IDF_PATH}/components/esp_lcd/priv_include/esp_async_fbcpy.h")
            idf_component_get_property(ESP_LCD_DIR esp_lcd COMPONENT_DIR)
            if(ESP_LCD_DIR)
                target_include_directories(${COMPONENT_LIB} PRIVATE "${ESP_LCD_DIR}/priv_include")
            endif()
        endif()
    endif()

    return()
endif()

cmake_minimum_required(VERSION 3.16)
project(esp_emote_gfx_host C)

option(GFX_BUILD_HOST_SDL "Build the Linux host SDL simulator target" ON)

set(GFX_HEATSHRINK_ROOT "${CMAKE_CURRENT_LIST_DIR}/test_apps/managed_components/laride__heatshrink")
if(NOT EXISTS "${GFX_HEATSHRINK_ROOT}/heatshrink_decoder.c")
    set(GFX_HEATSHRINK_ROOT "${CMAKE_CURRENT_LIST_DIR}/managed_components/laride__heatshrink")
endif()
if(NOT EXISTS "${GFX_HEATSHRINK_ROOT}/heatshrink_decoder.c")
    set(GFX_HEATSHRINK_ROOT "${CMAKE_CURRENT_LIST_DIR}/examples/esp/format_rgb565/managed_components/laride__heatshrink")
endif()
if(NOT EXISTS "${GFX_HEATSHRINK_ROOT}/heatshrink_decoder.c")
    message(FATAL_ERROR "heatshrink not found; build an ESP example first to fetch managed_components")
endif()

set(GFX_HOST_CORE_SRCS
    "src/core/fs/gfx_fs.c"
    "src/core/fs/gfx_fs_file.c"
    "src/core/types/gfx_color.c"
    "src/core/log/gfx_log.c"
    "src/core/display/gfx_backend.c"
    "src/core/display/gfx_display.c"
    "src/core/display/gfx_refresh.c"
    "src/core/object/gfx_object.c"
    "src/core/object/gfx_widget_class.c"
    "src/core/runtime/gfx_core.c"
    "src/core/runtime/gfx_timer.c"
    "src/core/runtime/gfx_touch.c"
    "src/core/tween/gfx_tween.c"
    "src/backend/custom/custom_backend.c"
    "src/backend/memory/memory_backend.c"
    "src/codecs/anim/gfx_anim_decoder.c"
    "src/codecs/anim/gfx_anim_decoder_eaf.c"
    "src/codecs/image/gfx_image_decoder.c"
    "src/fonts/gfx_font_adapter.c"
    "src/fonts/gfx_font_lvgl.c"
    "src/render/gfx_render.c"
    "src/render/sw/gfx_blend.c"
    "src/render/sw/gfx_sw_draw.c"
    "src/lib/eaf/gfx_eaf_dec.c"
    "src/widgets/anim/gfx_anim.c"
    "src/widgets/basic/gfx_button.c"
    "src/widgets/basic/gfx_container.c"
    "src/widgets/basic/gfx_coverflow.c"
    "src/widgets/basic/gfx_image_button.c"
    "src/widgets/basic/gfx_list.c"
    "src/widgets/basic/gfx_pageflow.c"
    "src/widgets/basic/gfx_progress_bar.c"
    "src/widgets/basic/gfx_qrcode.c"
    "src/widgets/basic/gfx_wheel.c"
    "src/widgets/img/gfx_img.c"
    "src/widgets/img/gfx_image_resource.c"
    "src/widgets/img/gfx_mesh_img.c"
    "src/widgets/label/gfx_label.c"
    "src/widgets/label/gfx_label_draw.c"
    "src/widgets/label/gfx_label_obj.c"
    "src/widgets/motion/gfx_motion.c"
    "src/widgets/motion/gfx_motion_player.c"
    "src/widgets/motion/gfx_motion_primitives.c"
    "src/widgets/motion/gfx_motion_scene.c"
    "src/widgets/motion/gfx_motion_style.c"
    "src/lib/qrcode/qrcode_wrapper.c"
    "src/lib/qrcode/qrcodegen.c"
    "src/platform/host/host_font.c"
    "src/platform/host/host_accel_stub.c"
    "src/platform/host/host_jpeg_stub.c"
    "src/platform/host/host_touch_stub.c"
    "src/platform/linux/linux_fs.c"
    "src/platform/linux/linux_platform.c"
    "src/platform/host/host_subsystem_stub.c"
    "${GFX_HEATSHRINK_ROOT}/heatshrink_decoder.c"
)

add_library(gfx_host_core STATIC ${GFX_HOST_CORE_SRCS})
target_include_directories(gfx_host_core
    PUBLIC
        "include"
    PRIVATE
        "src"
        "simulation/port/include"
        "${GFX_HEATSHRINK_ROOT}/include"
)
target_compile_definitions(gfx_host_core PUBLIC GFX_HOST_BUILD=1)
target_compile_features(gfx_host_core PUBLIC c_std_11)
target_link_libraries(gfx_host_core PUBLIC pthread m)

if(GFX_BUILD_HOST_SDL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3 QUIET sdl3)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()

    if(SDL3_FOUND)
        set(GFX_SDL_INCLUDE_DIRS ${SDL3_INCLUDE_DIRS})
        set(GFX_SDL_LIBRARIES ${SDL3_LIBRARIES})
        set(GFX_SDL_COMPILE_DEFS GFX_SDL_USE_SDL3)
        set(GFX_SDL_LIBRARY_DIRS ${SDL3_LIBRARY_DIRS})
    elseif(SDL2_FOUND)
        set(GFX_SDL_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
        set(GFX_SDL_LIBRARIES ${SDL2_LIBRARIES})
        set(GFX_SDL_COMPILE_DEFS GFX_SDL_USE_SDL2)
        set(GFX_SDL_LIBRARY_DIRS ${SDL2_LIBRARY_DIRS})
    else()
        find_package(SDL2 QUIET)
        if(TARGET SDL2::SDL2)
            set(GFX_SDL_TARGET SDL2::SDL2)
            set(GFX_SDL_COMPILE_DEFS GFX_SDL_USE_SDL2)
        else()
            message(FATAL_ERROR
                "GFX_BUILD_HOST_SDL=ON requires SDL2 or SDL3 development files. "
                "Install SDL2/SDL3 or configure with -DGFX_BUILD_HOST_SDL=OFF."
            )
        endif()
    endif()

    find_package(JPEG REQUIRED)
    target_compile_definitions(gfx_host_core PRIVATE GFX_HOST_USE_LIBJPEG=1)
    target_include_directories(gfx_host_core PRIVATE ${JPEG_INCLUDE_DIRS})
    if(TARGET JPEG::JPEG)
        target_link_libraries(gfx_host_core PUBLIC JPEG::JPEG)
    else()
        target_link_libraries(gfx_host_core PUBLIC ${JPEG_LIBRARIES})
    endif()

    add_library(gfx_backend_sdl STATIC "src/backend/sdl/sdl_backend.c")
    target_include_directories(gfx_backend_sdl
        PUBLIC
            "include"
        PRIVATE
            "src"
            ${GFX_SDL_INCLUDE_DIRS}
    )
    target_compile_definitions(gfx_backend_sdl PUBLIC ${GFX_SDL_COMPILE_DEFS})
    target_compile_features(gfx_backend_sdl PUBLIC c_std_11)
    if(GFX_SDL_LIBRARY_DIRS)
        target_link_directories(gfx_backend_sdl PUBLIC ${GFX_SDL_LIBRARY_DIRS})
    endif()
    target_link_libraries(gfx_backend_sdl PUBLIC gfx_host_core ${GFX_SDL_TARGET} ${GFX_SDL_LIBRARIES})

    add_library(gfx_host_runner STATIC "simulation/host/gfx_host_runner.c")
    target_include_directories(gfx_host_runner
        PUBLIC
            "include"
            "simulation/host"
    )
    target_compile_features(gfx_host_runner PUBLIC c_std_11)
    target_link_libraries(gfx_host_runner PUBLIC gfx_host_core gfx_backend_sdl)

    set(GFX_FORMAT_DEMO_SRCS
        "examples/common/gfx_display_port.c"
        "examples/format_playground/assets/font_puhui_16_4.c"
        "examples/format_playground/assets/image_button_assets.c"
        "examples/format_playground/playground_scene.c"
        "examples/format_playground/widgets/widget_anim.c"
        "examples/format_playground/widgets/widget_button.c"
        "examples/format_playground/widgets/widget_coverflow.c"
        "examples/format_playground/widgets/widget_image.c"
        "examples/format_playground/widgets/widget_list.c"
        "examples/format_playground/widgets/widget_motion.c"
        "examples/format_playground/widgets/widget_nav_list.c"
        "examples/format_playground/widgets/widget_pageflow.c"
        "examples/format_playground/widgets/widget_wheel.c"
    )

    add_executable(gfx_host_sdl_demo
        "simulation/host/gfx_host_sdl_demo.c"
        ${GFX_FORMAT_DEMO_SRCS}
    )
    target_include_directories(gfx_host_sdl_demo
        PRIVATE
            "include"
            "src"
            "simulation/host"
            "simulation/port/include"
            "examples/common"
            "examples/format_playground"
            "examples/motion"
    )
    target_compile_features(gfx_host_sdl_demo PRIVATE c_std_11)
    target_link_libraries(gfx_host_sdl_demo PRIVATE gfx_host_core gfx_backend_sdl gfx_host_runner)

    add_executable(gfx_host_fs_smoke "simulation/host/gfx_host_fs_smoke.c")
    target_include_directories(gfx_host_fs_smoke PRIVATE "include" "src")
    target_compile_features(gfx_host_fs_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_fs_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_font_smoke "simulation/host/gfx_host_font_smoke.c")
    target_include_directories(gfx_host_font_smoke PRIVATE "include" "src" "simulation/port/include")
    target_compile_features(gfx_host_font_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_font_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_alignment_smoke "simulation/host/gfx_host_alignment_smoke.c")
    target_include_directories(gfx_host_alignment_smoke PRIVATE "include" "src" "simulation/port/include")
    target_compile_features(gfx_host_alignment_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_alignment_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_roundup_smoke "simulation/host/gfx_host_roundup_smoke.c")
    target_include_directories(gfx_host_roundup_smoke PRIVATE "include" "src" "test_apps/unit/main")
    target_compile_features(gfx_host_roundup_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_roundup_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_list_smoke "simulation/host/gfx_host_list_smoke.c")
    target_include_directories(gfx_host_list_smoke PRIVATE "include" "simulation/port/include")
    target_compile_features(gfx_host_list_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_list_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_coverflow_smoke "simulation/host/gfx_host_coverflow_smoke.c")
    target_include_directories(gfx_host_coverflow_smoke PRIVATE "include" "simulation/port/include")
    target_compile_features(gfx_host_coverflow_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_coverflow_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_wheel_smoke "simulation/host/gfx_host_wheel_smoke.c")
    target_include_directories(gfx_host_wheel_smoke PRIVATE "include" "simulation/port/include")
    target_compile_features(gfx_host_wheel_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_wheel_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_container_smoke "simulation/host/gfx_host_container_smoke.c")
    target_include_directories(gfx_host_container_smoke PRIVATE "include" "src" "simulation/port/include")
    target_compile_features(gfx_host_container_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_container_smoke PRIVATE gfx_host_core)

    add_executable(gfx_host_tween_smoke "simulation/host/gfx_host_tween_smoke.c")
    target_include_directories(gfx_host_tween_smoke PRIVATE "include" "simulation/port/include")
    target_compile_features(gfx_host_tween_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_tween_smoke PRIVATE gfx_host_core)

    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME gfx_host_fs_smoke
            COMMAND gfx_host_fs_smoke
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/format
                icon_rgb565.bin)
        add_test(NAME gfx_host_font_smoke
            COMMAND gfx_host_font_smoke
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME gfx_host_alignment_smoke COMMAND gfx_host_alignment_smoke)
        add_test(NAME gfx_host_roundup_smoke COMMAND gfx_host_roundup_smoke)
        add_test(NAME gfx_host_list_smoke COMMAND gfx_host_list_smoke)
        add_test(NAME gfx_host_coverflow_smoke COMMAND gfx_host_coverflow_smoke)
        add_test(NAME gfx_host_wheel_smoke COMMAND gfx_host_wheel_smoke)
        add_test(NAME gfx_host_container_smoke COMMAND gfx_host_container_smoke)
        add_test(NAME gfx_host_tween_smoke COMMAND gfx_host_tween_smoke)
        add_test(NAME gfx_host_sdl_dummy_smoke
            COMMAND ${CMAKE_COMMAND}
                -D GFX_HOST_SDL_DEMO=$<TARGET_FILE:gfx_host_sdl_demo>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/simulation/host/run_sdl_dummy_smoke.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    add_custom_target(gfx_host_smoke
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS gfx_host_fs_smoke gfx_host_font_smoke gfx_host_alignment_smoke gfx_host_roundup_smoke gfx_host_list_smoke
            gfx_host_coverflow_smoke gfx_host_wheel_smoke gfx_host_container_smoke
            gfx_host_tween_smoke gfx_host_sdl_demo
        USES_TERMINAL
    )
endif()
