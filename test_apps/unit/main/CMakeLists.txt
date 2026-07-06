
idf_component_register(
    SRC_DIRS "." "harness"
    INCLUDE_DIRS "." "harness"
    PRIV_INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../../../src"
    WHOLE_ARCHIVE)

set(DIR_ASSETS_TEST "${CMAKE_CURRENT_LIST_DIR}/../../../examples/assets/format")

spiffs_create_partition_assets(
    assets_test
    ${DIR_ASSETS_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf"
)
