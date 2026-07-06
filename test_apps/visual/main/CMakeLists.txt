
set(SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/../../shared")
set(HW_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../main")

idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS "." "${HW_MAIN}" "${SHARED_DIR}"
    PRIV_INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../../examples/motion"
    WHOLE_ARCHIVE)

target_sources(${COMPONENT_LIB} PRIVATE
    "${SHARED_DIR}/gfx_test_util.c"
    "${SHARED_DIR}/gfx_hw_runtime.c"
    "${SHARED_DIR}/test_board.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../../examples/format_playground/assets/font_puhui_16_4.c")

set(DIR_ASSETS_TEST "${CMAKE_CURRENT_LIST_DIR}/../../../examples/assets/format")

spiffs_create_partition_assets(
    assets_test
    ${DIR_ASSETS_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf"
)
