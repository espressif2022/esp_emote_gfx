# Keep source discovery automatic while ensuring newly added files trigger
# CMake regeneration in normal configure mode. ESP-IDF also evaluates component
# CMakeLists in script mode while collecting requirements, where
# CONFIGURE_DEPENDS is not accepted.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE SRC_FILES "src/*.c")
else()
    file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.c")
endif()

set(OPTIONAL_SRC_FILES "")
set(PRIV_INCLUDE_DIRS
    "src"
)
set(GFX_HAVE_LIBTESS2 0)
set(GFX_HAVE_NANOVG 0)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/components/libtess2/Source/tess.c")
    set(LIBTESS2_SRC_FILES
        "components/libtess2/Source/bucketalloc.c"
        "components/libtess2/Source/dict.c"
        "components/libtess2/Source/geom.c"
        "components/libtess2/Source/mesh.c"
        "components/libtess2/Source/priorityq.c"
        "components/libtess2/Source/sweep.c"
        "components/libtess2/Source/tess.c"
    )
    list(APPEND OPTIONAL_SRC_FILES
        ${LIBTESS2_SRC_FILES}
    )
    list(APPEND PRIV_INCLUDE_DIRS
        "components/libtess2/Include"
        "components/libtess2/Source"
    )
    set(GFX_HAVE_LIBTESS2 1)
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/components/nanovg/src/nanovg.c")
    set(NANOVG_SRC_FILES
        "components/nanovg/src/nanovg.c"
    )
    list(APPEND OPTIONAL_SRC_FILES
        ${NANOVG_SRC_FILES}
    )
    list(APPEND PRIV_INCLUDE_DIRS
        "components/nanovg/src"
    )
    set(GFX_HAVE_NANOVG 1)
endif()

idf_component_register(
    SRCS
        ${SRC_FILES}
        ${OPTIONAL_SRC_FILES}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        ${PRIV_INCLUDE_DIRS}
    REQUIRES esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    GFX_HAVE_LIBTESS2=${GFX_HAVE_LIBTESS2}
    GFX_HAVE_NANOVG=${GFX_HAVE_NANOVG}
)

if(GFX_HAVE_LIBTESS2)
    set_source_files_properties(
        "components/libtess2/Source/tess.c"
        PROPERTIES COMPILE_OPTIONS "-Wno-clobbered"
    )
endif()

if(GFX_HAVE_NANOVG)
    set_source_files_properties(
        "components/nanovg/src/nanovg.c"
        PROPERTIES COMPILE_OPTIONS "-Wno-misleading-indentation;-Wno-shift-negative-value;-Wno-implicit-fallthrough"
    )
endif()
