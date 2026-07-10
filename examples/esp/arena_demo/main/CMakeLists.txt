set(ARENA_DEMO_SRCS
    "main.c"
    "../../../common/gfx_display_port.c"
    "../../../format_playground/assets/font_puhui_16_4.c"
    "arena_demo_scene.c"
)

if(NOT DEFINED ARENA_DEMO_COMPARE)
    set(ARENA_DEMO_COMPARE 0)
endif()
if(NOT DEFINED ARENA_DEMO_SWEEP)
    set(ARENA_DEMO_SWEEP 0)
endif()
if(NOT DEFINED ARENA_DEMO_PLAYGROUND)
    set(ARENA_DEMO_PLAYGROUND 0)
endif()
if(NOT DEFINED ARENA_DEMO_PATH)
    set(ARENA_DEMO_PATH A)
endif()
if(NOT DEFINED ARENA_DEMO_FROM_GSP)
    set(ARENA_DEMO_FROM_GSP 0)
endif()
if(NOT DEFINED ARENA_GSP_SCENE)
    set(ARENA_GSP_SCENE home)
endif()
if(NOT DEFINED ARENA_COMPARE_ITERS)
    set(ARENA_COMPARE_ITERS 30)
endif()
# Normalize PATH: A/a → 0 (arena), B/b → 1 (object)
set(ARENA_DEMO_PATH_IS_OBJECT 0)
if(ARENA_DEMO_PATH STREQUAL "B" OR ARENA_DEMO_PATH STREQUAL "b" OR ARENA_DEMO_PATH STREQUAL "1")
    set(ARENA_DEMO_PATH_IS_OBJECT 1)
endif()
# GSP scene: home (12 objs) | control (home_control_v4, 69 objs, 800x480)
set(ARENA_GSP_SCENE_IS_CONTROL 0)
if(ARENA_GSP_SCENE STREQUAL "control" OR ARENA_GSP_SCENE STREQUAL "CONTROL" OR
   ARENA_GSP_SCENE STREQUAL "home_control" OR ARENA_GSP_SCENE STREQUAL "home_control_v4")
    set(ARENA_GSP_SCENE_IS_CONTROL 1)
endif()

if(ARENA_DEMO_COMPARE OR ARENA_DEMO_SWEEP)
    list(APPEND ARENA_DEMO_SRCS
        "arena_compare_run.c"
        "arena_gfx_bridge.c"
    )
endif()
if(ARENA_DEMO_PLAYGROUND)
    list(APPEND ARENA_DEMO_SRCS
        "arena_playground_scene.c"
    )
endif()

idf_component_register(
    SRCS ${ARENA_DEMO_SRCS}
    INCLUDE_DIRS
        "."
        "../../../common"
        "../../../ai_scene_pkg"
        "../../../../include"
        "../../../../src"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ARENA_DEMO_FROM_GSP=${ARENA_DEMO_FROM_GSP}
    ARENA_DEMO_COMPARE=${ARENA_DEMO_COMPARE}
    ARENA_DEMO_SWEEP=${ARENA_DEMO_SWEEP}
    ARENA_DEMO_PLAYGROUND=${ARENA_DEMO_PLAYGROUND}
    ARENA_DEMO_PATH_IS_OBJECT=${ARENA_DEMO_PATH_IS_OBJECT}
    ARENA_GSP_SCENE_IS_CONTROL=${ARENA_GSP_SCENE_IS_CONTROL}
    ARENA_COMPARE_ITERS=${ARENA_COMPARE_ITERS}
)
