# Keep source discovery automatic while ensuring newly added files trigger
# CMake regeneration in normal configure mode. ESP-IDF also evaluates component
# CMakeLists in script mode while collecting requirements, where
# CONFIGURE_DEPENDS is not accepted.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE SRC_FILES "src/*.c")
else()
    file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.c")
endif()

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
    REQUIRES esp_timer
)
