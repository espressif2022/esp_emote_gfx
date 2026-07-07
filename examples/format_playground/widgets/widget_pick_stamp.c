/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playground_scene_priv.h"

#include "common/gfx_check.h"

#define PICK_STAMP_X            430
#define PICK_STAMP_Y            170
#define PICK_STAMP_DATE_X       19
#define PICK_STAMP_DATE_Y       24
#define PICK_STAMP_DATE_W       22
#define PICK_STAMP_DATE_H       30
#define PICK_STAMP_DATE_STEP_X  12
#define PICK_STAMP_TIME_X       21
#define PICK_STAMP_TIME_Y       48
#define PICK_STAMP_TIME_W       15
#define PICK_STAMP_TIME_H       22
#define PICK_STAMP_TIME_STEP_X  8
#define PICK_STAMP_SLOPE_X      64
#define PICK_STAMP_SLOPE_Y      (-16)

extern const gfx_image_dsc_t my_pick_stamp;
extern const lv_font_t font_match_bogle_oblique_20_4;
extern const lv_font_t font_match_bogle_oblique_27_4;

static gfx_coord_t pick_stamp_sloped_y(gfx_coord_t base_y, gfx_coord_t row_x, gfx_coord_t offset_x)
{
    return (gfx_coord_t)(base_y + ((offset_x - row_x) * PICK_STAMP_SLOPE_Y) / PICK_STAMP_SLOPE_X);
}

static gfx_err_t pick_stamp_create_char_labels(format_playground_scene_t *scene,
        gfx_object_t **labels,
        size_t label_count,
        const char *text,
        gfx_font_t font,
        gfx_coord_t base_x,
        gfx_coord_t base_y,
        gfx_coord_t char_w,
        gfx_coord_t char_h,
        gfx_coord_t step_x)
{
    for (size_t i = 0; i < label_count; i++) {
        gfx_object_t *label = gfx_label_create(scene->disp);
        char ch[2] = { text[i], '\0' };
        gfx_coord_t offset_x = (gfx_coord_t)(base_x + (gfx_coord_t)i * step_x);

        GFX_RETURN_ON_FALSE(label != NULL, GFX_ERR_NO_MEM, "playground", "create stamp label failed");
        labels[i] = label;
        (void)gfx_object_set_pos(label, (gfx_coord_t)(PICK_STAMP_X + offset_x),
                                 pick_stamp_sloped_y((gfx_coord_t)(PICK_STAMP_Y + base_y), base_x, offset_x));
        (void)gfx_object_set_size(label, char_w, char_h);
        (void)gfx_label_set_text(label, ch);
        (void)gfx_label_set_font(label, font);
        (void)gfx_label_set_color(label, GFX_COLOR_HEX(0x111111));
        (void)gfx_label_set_bg_enable(label, false);
        (void)gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
        (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
        (void)gfx_object_set_visible(label, false);
    }
    return GFX_OK;
}

gfx_err_t gfx_format_demo_build_pick_stamp_demo(format_playground_scene_t *scene)
{
    gfx_object_t *obj;
    const gfx_image_src_t src = {
        .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
        .data = &my_pick_stamp,
    };

    GFX_RETURN_ON_FALSE(scene != NULL && scene->disp != NULL, GFX_ERR_INVALID_ARG,
                        "playground", "scene is invalid");

    obj = gfx_image_create(scene->disp);
    GFX_RETURN_ON_FALSE(obj != NULL, GFX_ERR_NO_MEM, "playground", "create pick stamp image failed");
    scene->pick_stamp_demo = obj;
    (void)gfx_object_set_pos(obj, PICK_STAMP_X, PICK_STAMP_Y);
    GFX_RETURN_ON_ERROR(gfx_image_set_source_desc(obj, &src), "playground", "set pick stamp image failed");
    (void)gfx_object_set_visible(obj, false);

    GFX_RETURN_ON_ERROR(pick_stamp_create_char_labels(scene, scene->pick_stamp_date,
                        sizeof(scene->pick_stamp_date) / sizeof(scene->pick_stamp_date[0]),
                        "06.13", (gfx_font_t)&font_match_bogle_oblique_27_4,
                        PICK_STAMP_DATE_X, PICK_STAMP_DATE_Y,
                        PICK_STAMP_DATE_W, PICK_STAMP_DATE_H, PICK_STAMP_DATE_STEP_X),
                        "playground", "create stamp date failed");
    GFX_RETURN_ON_ERROR(pick_stamp_create_char_labels(scene, scene->pick_stamp_time,
                        sizeof(scene->pick_stamp_time) / sizeof(scene->pick_stamp_time[0]),
                        "14:13:45", (gfx_font_t)&font_match_bogle_oblique_20_4,
                        PICK_STAMP_TIME_X, PICK_STAMP_TIME_Y,
                        PICK_STAMP_TIME_W, PICK_STAMP_TIME_H, PICK_STAMP_TIME_STEP_X),
                        "playground", "create stamp time failed");
    return GFX_OK;
}
