set(TEST_APPS_SRCS "")
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB TEST_APPS_SRCS "*.c" "./assets/*.c")
else()
    file(GLOB TEST_APPS_SRCS CONFIGURE_DEPENDS "*.c" "./assets/*.c")
endif()

set(TEST_APPS_INCLUDE_DIRS ".")
set(TEST_APPS_PRIV_INCLUDE_DIRS "")

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/components/json/cJSON/cJSON.c")
    list(APPEND TEST_APPS_SRCS "$ENV{IDF_PATH}/components/json/cJSON/cJSON.c")
    list(APPEND TEST_APPS_PRIV_INCLUDE_DIRS "$ENV{IDF_PATH}/components/json/cJSON")
endif()

idf_component_register(
    SRCS ${TEST_APPS_SRCS}
    INCLUDE_DIRS ${TEST_APPS_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${TEST_APPS_PRIV_INCLUDE_DIRS}
    WHOLE_ARCHIVE)

set(DIR_PREBUILT "${PROJECT_DIR}/assets_gen")
set(DIR_TEST "${PROJECT_DIR}/assets_test")

spiffs_create_partition_assets(
    assets_test
    ${DIR_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf"
)

spiffs_create_partition_assets(
    emote_gen
    ${DIR_PREBUILT}
    FLASH_IN_PROJECT
    COPY_PREBUILT_BIN ${DIR_PREBUILT}/emote_assets.bin
)
