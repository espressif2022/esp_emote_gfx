idf_component_register(
    SRCS
        "main.c"
        "../../../common/gfx_display_port.c"
        "../../../format_playground/assets/font_puhui_16_4.c"
    INCLUDE_DIRS
        "."
        "../../../common"
        "../../../ai_scene_pkg"
        "../../../../include"
        "../../../../src"
)

set(GSP_FONT_ASSET_DIR "${PROJECT_DIR}/../../assets/fonts"
    CACHE PATH "GSP font asset directory")

spiffs_create_partition_assets(
    fonts
    ${GSP_FONT_ASSET_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".ttf,.ttc,.bin"
)
