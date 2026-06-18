idf_component_register(
    SRCS
        "main.c"
        "../../format_demo/format_demo_app.c"
        "../../format_demo/playground_scene.c"
        "../../format_demo/widgets/widget_button.c"
        "../../format_demo/widgets/widget_coverflow.c"
        "../../format_demo/widgets/widget_image.c"
        "../../format_demo/widgets/widget_anim_jpg.c"
        "../../format_demo/widgets/widget_list.c"
        "../../format_demo/widgets/widget_motion.c"
        "../../format_demo/widgets/widget_nav_list.c"
        "../../format_demo/widgets/widget_pageflow.c"
        "../../format_demo/widgets/widget_wheel.c"
        "../../main/assets/font_puhui_16_4.c"
    INCLUDE_DIRS "." "../../format_demo" "../../../src"
)

set(DIR_TEST "${PROJECT_DIR}/../assets_test")

spiffs_create_partition_assets(
    assets_test
    ${DIR_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf,.jpg,.jpeg"
)
