# Keep source discovery automatic while ensuring newly added files trigger
# CMake regeneration in normal configure mode. ESP-IDF also evaluates component
# CMakeLists in script mode while collecting requirements, where
# CONFIGURE_DEPENDS is not accepted.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE SRC_FILES "src/*.c")
else()
    file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.c")
endif()

set(LIBTESS2_SRC_FILES
    "components/libtess2/Source/bucketalloc.c"
    "components/libtess2/Source/dict.c"
    "components/libtess2/Source/geom.c"
    "components/libtess2/Source/mesh.c"
    "components/libtess2/Source/priorityq.c"
    "components/libtess2/Source/sweep.c"
    "components/libtess2/Source/tess.c"
)

idf_component_register(
    SRCS
        ${SRC_FILES}
        ${LIBTESS2_SRC_FILES}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
        "components/libtess2/Include"
        "components/libtess2/Source"
    REQUIRES esp_timer
)
