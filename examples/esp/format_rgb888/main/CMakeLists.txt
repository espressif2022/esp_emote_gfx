idf_component_register(
    SRCS
        "main.c"
        "../../../format_playground/format_demo_app.c"
        "../../../format_playground/playground_scene.c"
        "../../../format_playground/widgets/widget_button.c"
        "../../../format_playground/widgets/widget_coverflow.c"
        "../../../format_playground/widgets/widget_image.c"
        "../../../format_playground/widgets/widget_anim.c"
        "../../../format_playground/widgets/widget_list.c"
        "../../../format_playground/widgets/widget_motion.c"
        "../../../format_playground/widgets/widget_nav_list.c"
        "../../../format_playground/widgets/widget_pageflow.c"
        "../../../format_playground/widgets/widget_wheel.c"
        "../../../format_playground/assets/font_puhui_16_4.c"
    INCLUDE_DIRS "." "../../../format_playground" "../../../motion" "../../../../src"
)

set(DIR_TEST "${PROJECT_DIR}/../../assets/format")

spiffs_create_partition_assets(
    assets_test
    ${DIR_TEST}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".aaf,.ttf,.bin,.eaf,.jpg,.jpeg"
)
