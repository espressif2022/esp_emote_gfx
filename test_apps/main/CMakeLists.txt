
idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE)

spiffs_create_partition_assets(
    test_assets
    "../test_assets"
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf"
)

set(DIR_PREBUILT    "${PROJECT_DIR}/assets_prebuilt")

spiffs_create_partition_assets(
    anim_icon
    ${DIR_PREBUILT}
    FLASH_IN_PROJECT
    COPY_PREBUILT_BIN ${DIR_PREBUILT}/emote_assets.bin
)

