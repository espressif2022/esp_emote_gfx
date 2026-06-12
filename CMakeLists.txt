if(COMMAND idf_component_register)
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

    idf_component_register(
        SRCS
            ${SRC_FILES}
        INCLUDE_DIRS
            "include"
        PRIV_INCLUDE_DIRS
            ${PRIV_INCLUDE_DIRS}
        REQUIRES
            esp_timer
    )
    return()
endif()

cmake_minimum_required(VERSION 3.16)
project(esp_emote_gfx_host C)

option(GFX_BUILD_HOST_SDL "Build the Linux host SDL simulator target" ON)

set(GFX_HOST_CORE_SRCS
    "src/core/base/gfx_asset.c"
    "src/core/base/gfx_color.c"
    "src/core/base/gfx_log.c"
    "src/core/display/gfx_backend.c"
    "src/core/display/gfx_display.c"
    "src/core/display/gfx_refresh.c"
    "src/core/object/gfx_object.c"
    "src/core/object/gfx_widget_class.c"
    "src/core/runtime/gfx_core.c"
    "src/core/runtime/gfx_timer.c"
    "src/core/runtime/gfx_touch.c"
    "src/backend/memory/gfx_backend_memory.c"
    "src/codecs/anim/gfx_anim_decoder.c"
    "src/codecs/anim/gfx_anim_decoder_eaf.c"
    "src/codecs/image/gfx_image_decoder.c"
    "src/fonts/gfx_font_adapter.c"
    "src/render/gfx_render.c"
    "src/render/sw/gfx_blend.c"
    "src/render/sw/gfx_sw_draw.c"
    "src/lib/eaf/gfx_eaf_dec.c"
    "src/widgets/anim/gfx_anim.c"
    "src/widgets/basic/gfx_button.c"
    "src/widgets/basic/gfx_list.c"
    "src/widgets/img/gfx_img.c"
    "src/widgets/img/gfx_mesh_img.c"
    "src/widgets/label/gfx_label.c"
    "src/widgets/label/gfx_label_draw.c"
    "src/widgets/label/gfx_label_obj.c"
    "src/widgets/motion/gfx_motion.c"
    "src/widgets/motion/gfx_motion_player.c"
    "src/widgets/motion/gfx_motion_primitives.c"
    "src/widgets/motion/gfx_motion_scene.c"
    "src/widgets/motion/gfx_motion_style.c"
    "src/platform/host/gfx_font_host.c"
    "src/platform/host/gfx_touch_port_host.c"
    "src/platform/linux/gfx_asset_fs_linux.c"
    "src/platform/linux/gfx_platform_linux.c"
    "src/platform/host/gfx_subsystem_host_stub.c"
    "test_apps/managed_components/laride__heatshrink/heatshrink_decoder.c"
)

add_library(gfx_host_core STATIC ${GFX_HOST_CORE_SRCS})
target_include_directories(gfx_host_core
    PUBLIC
        "include"
        "sim/port/include"
    PRIVATE
        "src"
        "test_apps/managed_components/laride__heatshrink/include"
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

    add_library(gfx_backend_sdl STATIC "src/backend/sdl/gfx_backend_sdl.c")
    target_include_directories(gfx_backend_sdl
        PUBLIC
            "include"
        PRIVATE
            "src"
            "sim/port/include"
            ${GFX_SDL_INCLUDE_DIRS}
    )
    target_compile_definitions(gfx_backend_sdl PUBLIC ${GFX_SDL_COMPILE_DEFS})
    target_compile_features(gfx_backend_sdl PUBLIC c_std_11)
    if(GFX_SDL_LIBRARY_DIRS)
        target_link_directories(gfx_backend_sdl PUBLIC ${GFX_SDL_LIBRARY_DIRS})
    endif()
    target_link_libraries(gfx_backend_sdl PUBLIC gfx_host_core ${GFX_SDL_TARGET} ${GFX_SDL_LIBRARIES})

    add_executable(gfx_host_sdl_demo "sim/host/gfx_host_sdl_demo.c")
    target_include_directories(gfx_host_sdl_demo PRIVATE "include" "sim/host" "sim/port/include")
    target_compile_features(gfx_host_sdl_demo PRIVATE c_std_11)
    target_link_libraries(gfx_host_sdl_demo PRIVATE gfx_host_core gfx_backend_sdl)

    add_executable(gfx_host_asset_smoke "sim/host/gfx_host_asset_smoke.c")
    target_include_directories(gfx_host_asset_smoke PRIVATE "include" "sim/port/include")
    target_compile_features(gfx_host_asset_smoke PRIVATE c_std_11)
    target_link_libraries(gfx_host_asset_smoke PRIVATE gfx_host_core)
endif()
