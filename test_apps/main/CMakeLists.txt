
idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS "."
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
