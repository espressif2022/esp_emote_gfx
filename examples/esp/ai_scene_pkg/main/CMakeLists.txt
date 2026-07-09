set(GSP_SCENE_INC "inc/home_control_v4.inc" CACHE STRING "GSP scene .inc used by ESP ai_scene_pkg demo")

idf_component_register(
    SRCS
        "main.c"
        "../../../common/gfx_display_port.c"
        "../../../format_playground/assets/font_puhui_16_4.c"
    INCLUDE_DIRS
        "."
        "../../../common"
        "../../../ai_scene_pkg"
        "../../../../src"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE "GSP_SCENE_INC=\"${GSP_SCENE_INC}\"")

set(GSP_FONT_ASSET_DIR "${PROJECT_DIR}/../../assets/fonts" CACHE PATH "TTF/TTC font asset directory for ESP GSP demo")

spiffs_create_partition_assets(
    fonts
    ${GSP_FONT_ASSET_DIR}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".ttf,.ttc"
)
