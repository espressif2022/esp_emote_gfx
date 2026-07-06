
set(SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/../shared")

idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS "." "${SHARED_DIR}"
    PRIV_INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../examples/motion"
    WHOLE_ARCHIVE)

target_sources(${COMPONENT_LIB} PRIVATE
    "${SHARED_DIR}/gfx_test_util.c"
    "${SHARED_DIR}/gfx_hw_runtime.c"
    "${SHARED_DIR}/test_board.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/format_playground/assets/font_puhui_16_4.c")

set(DIR_TEST "${CMAKE_CURRENT_LIST_DIR}/../../examples/assets/format")

spiffs_create_partition_assets(
    assets_test
    ${DIR_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf"
)

set(DIR_ASSETS_GEN "${CMAKE_CURRENT_LIST_DIR}/../assets_gen")
file(MAKE_DIRECTORY "${DIR_ASSETS_GEN}")

set(EMOTE_ASSETS_URL "https://dl.espressif.com/AE/emote_assets.bin")
set(EMOTE_ASSETS_BIN "${CMAKE_CURRENT_LIST_DIR}/../prebuilt/emote_assets.bin")

if(NOT EXISTS "${EMOTE_ASSETS_BIN}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../prebuilt")
    message(STATUS "Downloading emote_assets.bin from ${EMOTE_ASSETS_URL}")
    file(DOWNLOAD "${EMOTE_ASSETS_URL}" "${EMOTE_ASSETS_BIN}"
        SHOW_PROGRESS
        STATUS _emote_assets_dl_status)
    list(GET _emote_assets_dl_status 0 _emote_assets_dl_code)
    if(NOT _emote_assets_dl_code EQUAL 0)
        list(GET _emote_assets_dl_status 1 _emote_assets_dl_msg)
        message(FATAL_ERROR "Failed to download emote_assets.bin: ${_emote_assets_dl_msg} (code ${_emote_assets_dl_code})")
    endif()
endif()

if(NOT EXISTS "${EMOTE_ASSETS_BIN}")
    message(FATAL_ERROR "emote_assets.bin missing: ${EMOTE_ASSETS_BIN}")
endif()

spiffs_create_partition_assets(
    emote_gen
    ${DIR_ASSETS_GEN}
    FLASH_IN_PROJECT
    COPY_PREBUILT_BIN ${EMOTE_ASSETS_BIN}
)
