
idf_component_register(
    SRC_DIRS "." "./assets"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE)

spiffs_create_partition_assets(
    test_assets
    "../mp4"
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
)
