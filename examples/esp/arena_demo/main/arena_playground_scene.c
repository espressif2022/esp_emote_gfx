/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arena_playground_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/widgets/anim.h"
#include "gfx/widgets/font_lvgl.h"
#include "gfx/widgets/image.h"
#include "gfx/widgets/progress_bar.h"
#include "lvgl.h"

#include "claw_motion.inc"

/* Screen coords match format_playground (absolute); panels are at 252,80. */
#define ARENA_PG_PANEL_X  252
#define ARENA_PG_PANEL_Y   80
#define ARENA_PG_PANEL_W  532
#define ARENA_PG_PANEL_H  384

/* Match format_playground DEMO_PREVIEW_* bottom-centered action button. */
#define ARENA_PG_PREVIEW_X            276
#define ARENA_PG_PREVIEW_BTN_W        190
#define ARENA_PG_PREVIEW_BTN_H         42
#define ARENA_PG_PREVIEW_BTN_Y        (ARENA_PG_SCREEN_H - ARENA_PG_PREVIEW_BTN_H - 22)
#define ARENA_PG_PREVIEW_BTN_X \
    (ARENA_PG_PREVIEW_X + ((ARENA_PG_SCREEN_W - ARENA_PG_PREVIEW_X) - ARENA_PG_PREVIEW_BTN_W) / 2)

#define ARENA_PG_ANIM_X   378
#define ARENA_PG_ANIM_Y   100
#define ARENA_PG_ANIM_W   280
#define ARENA_PG_ANIM_H   280
#define ARENA_PG_MOTION_X 423
#define ARENA_PG_MOTION_Y 122
#define ARENA_PG_MOTION_W 230
#define ARENA_PG_MOTION_H 230
#define ARENA_PG_ZOOM_X   746
#define ARENA_PG_ZOOM_Y   122
#define ARENA_PG_ZOOM_W    32
#define ARENA_PG_ZOOM_H   230
#define ARENA_PG_ZOOM_MIN  700
#define ARENA_PG_ZOOM_MAX 2100
#define ARENA_PG_ZOOM_DEF  500

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

static const char *const s_anim_clips[] = {
    "angry_20s.eaf",
    "confident_08.eaf",
    "badminton_12.eaf",
    "cry_10s_10s.eaf",
    "yawn_20s.eaf",
    "yummy_20_s.eaf",
};

static const char *const s_motion_actions[] = {
    "Move", "Connecting", "Connect Fail", "Connect OK",
    "Disconnect", "Command Received", "Command Failed", "Work",
};

static int playground_load_anim_clip(gfx_arena_playground_t *pg, size_t index);

/* Same order as gfx_format_demo_widget_names. */
static const char *const s_nav_items[ARENA_PG_NAV_COUNT] = {
    "Motion",
    "Anim",
    "Coverflow",
    "Pageflow",
    "Image",
    "Button",
    "Image Button",
    "Progress Bar",
    "List",
    "Wheel",
    "Pick Stamp",
};

static const char *const s_panel_names[ARENA_PG_NAV_COUNT] = {
    "panel_motion",
    "panel_anim",
    "panel_coverflow",
    "panel_pageflow",
    "panel_image",
    "panel_button",
    "panel_imgbtn",
    "panel_progress",
    "panel_list",
    "panel_wheel",
    "panel_pick_stamp",
};

/* Fixed-width slots so title text can be rewritten in-place. */
static const char *const s_title_slots[ARENA_PG_NAV_COUNT] = {
    "Motion Preview                   ",
    "Anim Preview                     ",
    "Coverflow Preview                ",
    "Pageflow Preview                 ",
    "Image Preview                    ",
    "Button Preview                   ",
    "Image Button Preview             ",
    "Progress Bar Preview             ",
    "List Preview                     ",
    "Wheel Preview                    ",
    "Pick Stamp Preview               ",
};

static const char *const s_list_demo_items[] = {
    "JPEG decode", "Status card", "Quick action", "Network", "Display",
    "Storage", "About", "Audio", "Battery", "Weather",
    "Schedule", "Messages", "System",
};
static const char *const s_wheel_demo_items[] = {
    "Low", "Medium", "High", "Turbo", "Sleep", "Focus", "Play",
};
static const char *const s_coverflow_demo_items[] = {
    "Page1", "Page2", "Page3", "Page4", "Page5",
};
static const char *const s_pageflow_demo_items[] = {
    "Page1", "Page2", "Page3", "Page4", "Page5",
};
static const char *const s_flow_image_files[] = {
    "01_esp32_c5_devkitc_320x240.jpg",
    "02_esp32_c6_devkitc_320x240.jpg",
    "03_esp32_s3_devkitc_320x240.jpg",
    "04_esp32_p4_eye_front_320x240.jpg",
    "05_esp32_p4_eye_back_320x240.jpg",
};

enum {
    IMGBTN_W = 260,
    IMGBTN_H = 112,
};

static int16_t panel_rel_x(int abs_x)
{
    return (int16_t)(abs_x - ARENA_PG_PANEL_X);
}

static int16_t panel_rel_y(int abs_y)
{
    return (int16_t)(abs_y - ARENA_PG_PANEL_Y);
}

static uint16_t *make_imgbtn_face(void)
{
    static uint16_t px[IMGBTN_W * IMGBTN_H];
    static int ready;
    if (!ready) {
        for (int y = 0; y < IMGBTN_H; y++) {
            for (int x = 0; x < IMGBTN_W; x++) {
                const int edge = (x < 4 || y < 4 || x >= IMGBTN_W - 4 || y >= IMGBTN_H - 4);
                const uint8_t r = edge ? 0x86u : 0x2Fu;
                const uint8_t g = edge ? 0xC8u : 0x8Cu;
                const uint8_t b = edge ? 0xFFu : 0xFFu;
                px[y * IMGBTN_W + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
        }
        ready = 1;
    }
    return px;
}

static uint16_t *make_imgbtn_pressed(void)
{
    static uint16_t px[IMGBTN_W * IMGBTN_H];
    static int ready;
    if (!ready) {
        for (int y = 0; y < IMGBTN_H; y++) {
            for (int x = 0; x < IMGBTN_W; x++) {
                const int edge = (x < 4 || y < 4 || x >= IMGBTN_W - 4 || y >= IMGBTN_H - 4);
                const uint8_t r = edge ? 0x1Fu : 0x1Au;
                const uint8_t g = edge ? 0x66u : 0x55u;
                const uint8_t b = edge ? 0xB8u : 0x92u;
                px[y * IMGBTN_W + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
        }
        ready = 1;
    }
    return px;
}

static void progress_label_sync(gfx_arena_playground_t *pg)
{
    if (pg == NULL || pg->scene == NULL || pg->progress_off == GFX_ARENA_NO_NODE ||
            pg->progress_label_off == GFX_ARENA_NO_NODE) {
        return;
    }
    gfx_arena_node_t *pn = gfx_arena_node(&pg->scene->arena, pg->progress_off);
    const gfx_arena_progress_hdr_t *ph =
        (pn != NULL) ? gfx_arena_progress(&pg->scene->arena, pn->reserved) : NULL;
    gfx_arena_node_t *lab = gfx_arena_node(&pg->scene->arena, pg->progress_label_off);
    if (ph == NULL || lab == NULL) {
        return;
    }
    char *s = (char *)gfx_arena_str(&pg->scene->arena, lab->name_off);
    if (s == NULL) {
        return;
    }
    char buf[32];
    const unsigned pct = (unsigned)((ph->value + 5u) / 10u);
    (void)snprintf(buf, sizeof(buf), "Progress: %u%%          ", pct);
    size_t cap = strlen(s);
    size_t n = strlen(buf);
    if (n > cap) {
        n = cap;
    }
    memcpy(s, buf, n);
    if (n < cap) {
        s[n] = '\0';
    }
    (void)gfx_arena_scene_mark_dirty(pg->scene, pg->progress_label_off);
}

uint8_t *gfx_arena_playground_pack(size_t *out_size)
{
    enum { MAX_DESCS = 96 };
    gfx_arena_desc_t *descs = (gfx_arena_desc_t *)calloc(MAX_DESCS, sizeof(gfx_arena_desc_t));
    if (descs == NULL) {
        return NULL;
    }
    uint16_t n = 0;

    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_CONTAINER,
        .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG,
        .x = 0, .y = 0, .w = ARENA_PG_SCREEN_W, .h = ARENA_PG_SCREEN_H,
        .bg_rgb = 0x101418, .name = "root", .parent = -1,
    };
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_LABEL,
        .flags = GFX_ARENA_F_VISIBLE,
        .x = 22, .y = 18, .w = 756, .h = 34,
        .bg_rgb = 0xF3F7FA, .name = "GFX Arena Playground", .parent = 0,
    };
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_LABEL,
        .flags = GFX_ARENA_F_VISIBLE,
        .x = 22, .y = 56, .w = 220, .h = 24,
        .bg_rgb = 0xA8B3BD, .name = "Widgets   ARN", .parent = 0,
    };
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_LABEL,
        .flags = GFX_ARENA_F_VISIBLE,
        .x = 276, .y = 56, .w = 360, .h = 24,
        .bg_rgb = 0xA8B3BD, .name = s_title_slots[0], .parent = 0,
    };
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_LABEL,
        .flags = GFX_ARENA_F_VISIBLE,
        .x = 640, .y = 56, .w = 150, .h = 24,
        .bg_rgb = 0x76B7E8, .name = "FPS: --              ", .parent = 0,
    };
    /* Nav: match format_playground widget_list (22,112 / 214x348 / h=46). */
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_LIST,
        .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
        .x = 22, .y = 112, .w = 214, .h = 348,
        .bg_rgb = 0x161C22, .name = "nav", .parent = 0,
        .u.items = {
            .items = s_nav_items, .item_count = ARENA_PG_NAV_COUNT,
            .selected = 0, .item_height = 46, .text_rgb = 0xDCE4EC,
            .flags = GFX_ARENA_ITEMS_F_SNAP,
        },
    };
    /* Bottom-centered preview action button (same geometry as object playground). */
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_BUTTON,
        .flags = GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE, /* hidden until Motion/Anim/Image focus */
        .x = (int16_t)ARENA_PG_PREVIEW_BTN_X,
        .y = (int16_t)ARENA_PG_PREVIEW_BTN_Y,
        .w = ARENA_PG_PREVIEW_BTN_W,
        .h = ARENA_PG_PREVIEW_BTN_H,
        .bg_rgb = 0x245C8F,
        .name = "Next Action  1/8     ",
        .parent = 0,
        .action = "on_preview_next",
    };

    for (int i = 0; i < ARENA_PG_NAV_COUNT; i++) {
        const uint16_t panel_idx = n;
        const uint16_t flags = (i == 0)
            ? (GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG)
            : GFX_ARENA_F_BG;
        descs[n++] = (gfx_arena_desc_t) {
            .type = GFX_ARENA_NODE_CONTAINER,
            .flags = flags,
            .x = ARENA_PG_PANEL_X, .y = ARENA_PG_PANEL_Y,
            .w = ARENA_PG_PANEL_W, .h = ARENA_PG_PANEL_H,
            .bg_rgb = 0x101418, .name = s_panel_names[i], .parent = 0,
        };

        if (i == ARENA_PG_NAV_MOTION) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = 24, .y = 8, .w = 480, .h = 42,
                .bg_rgb = 0xDCE4EC,
                .name = "Current action: Move                    ",
                .parent = (int)panel_idx,
            };
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_MOTION,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = panel_rel_x(ARENA_PG_MOTION_X),
                .y = panel_rel_y(ARENA_PG_MOTION_Y),
                .w = ARENA_PG_MOTION_W,
                .h = ARENA_PG_MOTION_H,
                .bg_rgb = 0, .name = "demo_motion", .parent = (int)panel_idx,
                .u.motion = {
                    .asset_key = "claw",
                    .action_idx = 0,
                    .flags = GFX_ARENA_MOTION_F_LOOP,
                    .stroke_rgb = 0xFF4D2B,
                },
            };
        } else if (i == ARENA_PG_NAV_ANIM) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = 24, .y = 8, .w = 480, .h = 42,
                .bg_rgb = 0xDCE4EC,
                .name = "Anim ARN node (bottom Next Anim)        ",
                .parent = (int)panel_idx,
            };
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_ANIM,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = panel_rel_x(ARENA_PG_ANIM_X),
                .y = panel_rel_y(ARENA_PG_ANIM_Y),
                .w = ARENA_PG_ANIM_W,
                .h = ARENA_PG_ANIM_H,
                .bg_rgb = 0, .name = "demo_anim", .parent = (int)panel_idx,
                .u.anim = {
                    .file_path = s_anim_clips[0],
                    .fps = 50,
                    .flags = GFX_ARENA_ANIM_F_LOOP,
                    .start_frame = 0,
                    .end_frame = 0xFFFFFFFFu,
                },
            };
        } else if (i == ARENA_PG_NAV_COVERFLOW) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_COVERFLOW,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(351), .y = panel_rel_y(142),
                .w = 374, .h = 238,
                .bg_rgb = 0x101418, .name = "demo_coverflow", .parent = (int)panel_idx,
                .u.items = {
                    .items = s_coverflow_demo_items,
                    .item_count = (uint16_t)(sizeof(s_coverflow_demo_items) /
                                             sizeof(s_coverflow_demo_items[0])),
                    .selected = 1, .item_height = 0, .text_rgb = 0xF3F7FA,
                },
            };
        } else if (i == ARENA_PG_NAV_PAGEFLOW) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_PAGEFLOW,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(372), .y = panel_rel_y(138),
                .w = 332, .h = 244,
                .bg_rgb = 0x101418, .name = "demo_pageflow", .parent = (int)panel_idx,
                .u.items = {
                    .items = s_pageflow_demo_items,
                    .item_count = (uint16_t)(sizeof(s_pageflow_demo_items) /
                                             sizeof(s_pageflow_demo_items[0])),
                    .selected = 1, .item_height = 0, .text_rgb = 0xF3F7FA,
                },
            };
        } else if (i == ARENA_PG_NAV_IMAGE) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = 24, .y = 8, .w = 480, .h = 24,
                .bg_rgb = 0xDCE4EC, .name = "mmap JPEG (same assets as object demo)",
                .parent = (int)panel_idx,
            };
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_IMAGE,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = panel_rel_x(378), .y = panel_rel_y(164),
                .w = 320, .h = 240,
                .bg_rgb = 0, .name = "jpeg_demo", .parent = (int)panel_idx,
                .u.image = {
                    .file_path = "01_esp32_c5_devkitc_320x240.jpg",
                    .w = 0, .h = 0,
                },
            };
        } else if (i == ARENA_PG_NAV_BUTTON) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_BUTTON,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(413), .y = panel_rel_y(206),
                .w = 250, .h = 74,
                .bg_rgb = 0x245C8F, .name = "Press Button", .parent = (int)panel_idx,
                .action = "on_pg_btn",
            };
        } else if (i == ARENA_PG_NAV_IMAGE_BUTTON) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_IMAGE_BUTTON,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(408), .y = panel_rel_y(188),
                .w = IMGBTN_W, .h = IMGBTN_H,
                .bg_rgb = 0, .name = "Image Button", .parent = (int)panel_idx,
                .u.image = {
                    .rgb565 = make_imgbtn_face(),
                    .pressed_rgb565 = make_imgbtn_pressed(),
                    .w = IMGBTN_W, .h = IMGBTN_H,
                },
            };
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = 24, .y = 300, .w = 400, .h = 28,
                .bg_rgb = 0xF3F7FA, .name = "Image Button: idle              ",
                .parent = (int)panel_idx,
            };
        } else if (i == ARENA_PG_NAV_PROGRESS_BAR) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_PROGRESS,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(404), .y = panel_rel_y(210),
                .w = 268, .h = 40,
                .bg_rgb = 0, .name = "progress", .parent = (int)panel_idx,
                .u.progress = {
                    .value = 420,
                    .radius = 20,
                    .track_rgb = 0x26313B,
                    .fill_rgb = 0x2F8CFF,
                },
            };
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = panel_rel_x(404), .y = panel_rel_y(270),
                .w = 280, .h = 28,
                .bg_rgb = 0xF3F7FA, .name = "Progress: 42%          ",
                .parent = (int)panel_idx,
            };
        } else if (i == ARENA_PG_NAV_LIST) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LIST,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(386), .y = panel_rel_y(146),
                .w = 304, .h = 252,
                .bg_rgb = 0x171D24, .name = "demo_list", .parent = (int)panel_idx,
                .u.items = {
                    .items = s_list_demo_items,
                    .item_count = (uint16_t)(sizeof(s_list_demo_items) / sizeof(s_list_demo_items[0])),
                    .selected = 0, .item_height = 46, .text_rgb = 0xDCE4EC,
                    .flags = GFX_ARENA_ITEMS_F_SNAP,
                },
            };
        } else if (i == ARENA_PG_NAV_WHEEL) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_WHEEL,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = panel_rel_x(388), .y = panel_rel_y(146),
                .w = 300, .h = 252,
                .bg_rgb = 0x171D24, .name = "demo_wheel", .parent = (int)panel_idx,
                .u.items = {
                    .items = s_wheel_demo_items,
                    .item_count = (uint16_t)(sizeof(s_wheel_demo_items) / sizeof(s_wheel_demo_items[0])),
                    .selected = 1, .item_height = 48, .text_rgb = 0x9AA7B2,
                    .flags = GFX_ARENA_ITEMS_F_SNAP | GFX_ARENA_ITEMS_F_CYCLIC,
                },
            };
        } else if (i == ARENA_PG_NAV_PICK_STAMP) {
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_LABEL,
                .flags = GFX_ARENA_F_VISIBLE,
                .x = 24, .y = 8, .w = 480, .h = 24,
                .bg_rgb = 0xDCE4EC,
                .name = "Pick Stamp: object image overlay",
                .parent = (int)panel_idx,
            };
        }
    }

    if (n > MAX_DESCS) {
        free(descs);
        return NULL;
    }
    uint8_t *pkg = gfx_arena_pack(descs, n, out_size);
    free(descs);
    return pkg;
}

static void on_pg_btn(gfx_arena_t *arena, gfx_arena_node_t *node,
                      const gfx_touch_event_t *event, void *user_data)
{
    (void)event;
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    if (node == NULL || pg == NULL || pg->scene == NULL) {
        return;
    }
    node->bg_rgb = (node->bg_rgb == 0x245C8Fu) ? 0x2E7D32u : 0x245C8Fu;
    uint32_t off = gfx_arena_node_offset(arena, node);
    if (off != GFX_ARENA_NO_NODE) {
        (void)gfx_arena_scene_mark_dirty(pg->scene, off);
    }
}

static void on_nav(gfx_arena_t *arena, gfx_arena_node_t *node,
                   const gfx_touch_event_t *event, void *user_data)
{
    (void)event;
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    if (arena == NULL || node == NULL || pg == NULL) {
        return;
    }
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(arena, node->reserved);
    if (ih == NULL || ih->selected == GFX_ARENA_ITEMS_SELECTED_NONE ||
            ih->selected >= ARENA_PG_NAV_COUNT) {
        return;
    }
    if (ih->selected == pg->focus) {
        return;
    }
    (void)gfx_arena_playground_set_focus(pg, ih->selected);
}

static void on_imgbtn(gfx_arena_t *arena, gfx_arena_node_t *node,
                      const gfx_touch_event_t *event, void *user_data)
{
    (void)node;
    (void)event;
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    if (arena == NULL || pg == NULL || pg->scene == NULL ||
            pg->imgbtn_status_off == GFX_ARENA_NO_NODE) {
        return;
    }
    gfx_arena_node_t *st = gfx_arena_node(arena, pg->imgbtn_status_off);
    if (st == NULL) {
        return;
    }
    char *s = (char *)gfx_arena_str(arena, st->name_off);
    if (s != NULL) {
        const char *msg = "Image Button: released           ";
        size_t cap = strlen(s);
        size_t n = strlen(msg);
        if (n > cap) {
            n = cap;
        }
        memcpy(s, msg, n);
        if (n < cap) {
            s[n] = '\0';
        }
        (void)gfx_arena_scene_mark_dirty(pg->scene, pg->imgbtn_status_off);
    }
}

static void on_progress(gfx_arena_t *arena, gfx_arena_node_t *node,
                        const gfx_touch_event_t *event, void *user_data)
{
    (void)arena;
    (void)node;
    (void)event;
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    progress_label_sync(pg);
}

static void playground_write_node_text(gfx_arena_playground_t *pg, uint32_t node_off,
                                       const char *text)
{
    gfx_arena_node_t *n;
    char *s;
    size_t cap;
    size_t nlen;

    if (pg == NULL || pg->scene == NULL || node_off == GFX_ARENA_NO_NODE || text == NULL) {
        return;
    }
    n = gfx_arena_node(&pg->scene->arena, node_off);
    if (n == NULL) {
        return;
    }
    s = (char *)gfx_arena_str(&pg->scene->arena, n->name_off);
    if (s == NULL) {
        return;
    }
    cap = strlen(s);
    nlen = strlen(text);
    if (nlen > cap) {
        nlen = cap;
    }
    memcpy(s, text, nlen);
    if (nlen < cap) {
        s[nlen] = '\0';
    }
    (void)gfx_arena_scene_mark_dirty(pg->scene, node_off);
}

static void playground_sync_preview_button(gfx_arena_playground_t *pg)
{
    gfx_arena_node_t *btn;
    char buf[32];
    bool show = false;
    const char *label = NULL;
    unsigned idx = 0;
    unsigned count = 0;

    if (pg == NULL || pg->scene == NULL || pg->preview_btn_off == GFX_ARENA_NO_NODE) {
        return;
    }
    btn = gfx_arena_node(&pg->scene->arena, pg->preview_btn_off);
    if (btn == NULL) {
        return;
    }

    if (pg->focus == ARENA_PG_NAV_MOTION) {
        show = true;
        label = "Next Action";
        idx = (unsigned)pg->motion_action;
        count = (unsigned)(sizeof(s_motion_actions) / sizeof(s_motion_actions[0]));
    } else if (pg->focus == ARENA_PG_NAV_ANIM) {
        show = true;
        label = "Next Anim";
        idx = (unsigned)pg->anim_clip;
        count = (unsigned)(sizeof(s_anim_clips) / sizeof(s_anim_clips[0]));
    } else if (pg->focus == ARENA_PG_NAV_IMAGE) {
        show = true;
        label = "Next Image";
        idx = (unsigned)pg->image_clip;
        count = (unsigned)(sizeof(s_flow_image_files) / sizeof(s_flow_image_files[0]));
    }

    if (!show || count <= 1U) {
        btn->flags = (uint16_t)(btn->flags & ~GFX_ARENA_F_VISIBLE);
    } else {
        btn->flags = (uint16_t)(btn->flags | GFX_ARENA_F_VISIBLE);
        (void)snprintf(buf, sizeof(buf), "%s  %u/%u", label, idx + 1U, count);
        playground_write_node_text(pg, pg->preview_btn_off, buf);
    }
    (void)gfx_arena_scene_mark_dirty(pg->scene, pg->preview_btn_off);
}

static int playground_load_image_clip(gfx_arena_playground_t *pg, size_t index)
{
    gfx_arena_node_t *jpeg;
    uint32_t off;
    gfx_image_src_t src;
    const size_t count = sizeof(s_flow_image_files) / sizeof(s_flow_image_files[0]);

    if (pg == NULL || pg->scene == NULL || index >= count) {
        return -1;
    }
    jpeg = gfx_arena_find_by_name(&pg->scene->arena, "jpeg_demo");
    if (jpeg == NULL) {
        return -2;
    }
    off = gfx_arena_node_offset(&pg->scene->arena, jpeg);
    src = (gfx_image_src_t) {
        .type = GFX_IMAGE_SRC_TYPE_FILE,
        .data = s_flow_image_files[index],
    };
    if (gfx_arena_scene_bind_image_src(pg->scene, off, GFX_ARENA_IMG_ITEM_NONE, 0, &src) != 0) {
        return -3;
    }
    pg->image_clip = index;
    (void)gfx_arena_scene_mark_dirty(pg->scene, off);
    return 0;
}

static void on_preview_next(gfx_arena_t *arena, gfx_arena_node_t *node,
                            const gfx_touch_event_t *event, void *user_data)
{
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    (void)arena;
    (void)node;
    (void)event;
    if (pg == NULL) {
        return;
    }

    if (pg->focus == ARENA_PG_NAV_MOTION) {
        const uint16_t count =
            (uint16_t)(sizeof(s_motion_actions) / sizeof(s_motion_actions[0]));
        if (pg->motion_off == GFX_ARENA_NO_NODE || count == 0) {
            return;
        }
        pg->motion_action = (uint16_t)((pg->motion_action + 1U) % count);
        (void)gfx_arena_scene_motion_set_action(pg->scene, pg->motion_off, pg->motion_action, false);
        {
            gfx_arena_node_t *note = gfx_arena_find_by_name(&pg->scene->arena,
                                                    "Current action: Move                    ");
            if (note != NULL) {
                char buf[48];
                uint32_t off = gfx_arena_node_offset(&pg->scene->arena, note);
                (void)snprintf(buf, sizeof(buf), "Current action: %s",
                               s_motion_actions[pg->motion_action]);
                playground_write_node_text(pg, off, buf);
            }
        }
    } else if (pg->focus == ARENA_PG_NAV_ANIM) {
        const size_t count = sizeof(s_anim_clips) / sizeof(s_anim_clips[0]);
        size_t next;
        if (pg->anim_off == GFX_ARENA_NO_NODE || count == 0) {
            return;
        }
        next = (pg->anim_clip + 1U) % count;
        for (size_t step = 0; step < count; step++) {
            size_t idx = (next + step) % count;
            if (playground_load_anim_clip(pg, idx) == 0) {
                break;
            }
        }
    } else if (pg->focus == ARENA_PG_NAV_IMAGE) {
        const size_t count = sizeof(s_flow_image_files) / sizeof(s_flow_image_files[0]);
        if (count == 0) {
            return;
        }
        (void)playground_load_image_clip(pg, (pg->image_clip + 1U) % count);
    }

    playground_sync_preview_button(pg);
}

static gfx_arena_action_entry_t s_actions[5];

static void playground_set_node_visible(gfx_arena_playground_t *pg, uint32_t node_off, bool visible)
{
    gfx_arena_node_t *n;
    if (pg == NULL || pg->scene == NULL || node_off == GFX_ARENA_NO_NODE) {
        return;
    }
    n = gfx_arena_node(&pg->scene->arena, node_off);
    if (n == NULL) {
        return;
    }
    if (visible) {
        n->flags = (uint16_t)(n->flags | GFX_ARENA_F_VISIBLE);
    } else {
        n->flags = (uint16_t)(n->flags & ~GFX_ARENA_F_VISIBLE);
    }
    (void)gfx_arena_scene_mark_dirty(pg->scene, node_off);
}

static void playground_set_overlays_visible(gfx_arena_playground_t *pg, bool pick_on, bool motion_zoom_on)
{
    size_t i;
    if (pg == NULL) {
        return;
    }
    if (pg->motion_zoom != NULL) {
        (void)gfx_object_set_visible(pg->motion_zoom, motion_zoom_on);
    }
    if (pg->pick_stamp != NULL) {
        (void)gfx_object_set_visible(pg->pick_stamp, pick_on);
    }
    for (i = 0; i < sizeof(pg->pick_stamp_date) / sizeof(pg->pick_stamp_date[0]); i++) {
        if (pg->pick_stamp_date[i] != NULL) {
            (void)gfx_object_set_visible(pg->pick_stamp_date[i], pick_on);
        }
    }
    for (i = 0; i < sizeof(pg->pick_stamp_time) / sizeof(pg->pick_stamp_time[0]); i++) {
        if (pg->pick_stamp_time[i] != NULL) {
            (void)gfx_object_set_visible(pg->pick_stamp_time[i], pick_on);
        }
    }
}

static void playground_apply_motion_zoom(gfx_arena_playground_t *pg)
{
    uint32_t zoom;
    uint16_t canvas_w;
    uint16_t canvas_h;
    int16_t abs_x;
    int16_t abs_y;
    gfx_arena_node_t *n;

    if (pg == NULL || pg->scene == NULL || pg->motion_off == GFX_ARENA_NO_NODE) {
        return;
    }
    n = gfx_arena_node(&pg->scene->arena, pg->motion_off);
    if (n == NULL) {
        return;
    }
    zoom = ARENA_PG_ZOOM_MIN +
           (((uint32_t)pg->motion_zoom_value * (ARENA_PG_ZOOM_MAX - ARENA_PG_ZOOM_MIN)) / 1000U);
    canvas_w = (uint16_t)(((uint32_t)ARENA_PG_MOTION_W * zoom) / 1000U);
    canvas_h = (uint16_t)(((uint32_t)ARENA_PG_MOTION_H * zoom) / 1000U);
    if (canvas_w < 1U) {
        canvas_w = 1U;
    }
    if (canvas_h < 1U) {
        canvas_h = 1U;
    }
    abs_x = (int16_t)(ARENA_PG_MOTION_X +
                       ((int)ARENA_PG_MOTION_W - (int)canvas_w) / 2);
    abs_y = (int16_t)(ARENA_PG_MOTION_Y +
                       ((int)ARENA_PG_MOTION_H - (int)canvas_h) / 2);
    n->x = panel_rel_x(abs_x);
    n->y = panel_rel_y(abs_y);
    n->w = canvas_w;
    n->h = canvas_h;
    (void)gfx_arena_scene_motion_sync(pg->scene, pg->motion_off);
    (void)gfx_arena_scene_mark_dirty(pg->scene, pg->motion_off);
}

static void playground_motion_zoom_cb(gfx_object_t *obj, uint16_t permille, void *user_data)
{
    gfx_arena_playground_t *pg = (gfx_arena_playground_t *)user_data;
    (void)obj;
    if (pg == NULL) {
        return;
    }
    pg->motion_zoom_value = permille;
    playground_apply_motion_zoom(pg);
}

static int playground_load_anim_clip(gfx_arena_playground_t *pg, size_t index)
{
    gfx_anim_src_t src = {0};
    const size_t count = sizeof(s_anim_clips) / sizeof(s_anim_clips[0]);

    if (pg == NULL || pg->scene == NULL || pg->anim_off == GFX_ARENA_NO_NODE || index >= count) {
        return -1;
    }
    src.type = GFX_ANIM_SRC_TYPE_FILE;
    src.data = s_anim_clips[index];
    if (gfx_arena_scene_bind_anim_src(pg->scene, pg->anim_off, &src) != 0) {
        return -2;
    }
    if (gfx_arena_scene_anim_set_playing(pg->scene, pg->anim_off, true) != 0) {
        return -3;
    }
    pg->anim_clip = index;
    return 0;
}

static gfx_coord_t pick_stamp_sloped_y(gfx_coord_t base_y, gfx_coord_t row_x, gfx_coord_t offset_x)
{
    return (gfx_coord_t)(base_y + ((offset_x - row_x) * PICK_STAMP_SLOPE_Y) / PICK_STAMP_SLOPE_X);
}

static int playground_create_pick_stamp_labels(gfx_arena_playground_t *pg, gfx_object_t **labels,
        size_t label_count, const char *text, gfx_font_t font,
        gfx_coord_t base_x, gfx_coord_t base_y, gfx_coord_t char_w,
        gfx_coord_t char_h, gfx_coord_t step_x)
{
    size_t i;
    for (i = 0; i < label_count; i++) {
        gfx_object_t *label = gfx_label_create(pg->disp);
        char ch[2] = { text[i], '\0' };
        gfx_coord_t offset_x = (gfx_coord_t)(base_x + (gfx_coord_t)i * step_x);

        if (label == NULL) {
            return -1;
        }
        labels[i] = label;
        (void)gfx_object_set_pos(label, (gfx_coord_t)(PICK_STAMP_X + offset_x),
                                 pick_stamp_sloped_y((gfx_coord_t)(PICK_STAMP_Y + base_y),
                                         base_x, offset_x));
        (void)gfx_object_set_size(label, char_w, char_h);
        (void)gfx_label_set_text(label, ch);
        (void)gfx_label_set_font(label, font);
        (void)gfx_label_set_color(label, GFX_COLOR_HEX(0x111111));
        (void)gfx_label_set_bg_enable(label, false);
        (void)gfx_label_set_text_align(label, GFX_TEXT_ALIGN_CENTER);
        (void)gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);
        (void)gfx_object_set_visible(label, false);
    }
    return 0;
}

static int playground_bind_arn_hosts(gfx_arena_playground_t *pg)
{
    size_t i;
    const size_t anim_count = sizeof(s_anim_clips) / sizeof(s_anim_clips[0]);

    if (pg == NULL || pg->scene == NULL || pg->disp == NULL) {
        return -1;
    }

    if (pg->anim_off != GFX_ARENA_NO_NODE) {
        playground_set_node_visible(pg, pg->anim_off, false);
        for (i = 0; i < anim_count; i++) {
            if (playground_load_anim_clip(pg, i) == 0) {
                (void)gfx_arena_scene_anim_set_playing(pg->scene, pg->anim_off, false);
                break;
            }
        }
    }

    if (pg->motion_off != GFX_ARENA_NO_NODE) {
        playground_set_node_visible(pg, pg->motion_off, false);
        if (gfx_arena_scene_bind_motion_asset(pg->scene, pg->motion_off,
                                          &claw_motion_scene_asset) == 0) {
            pg->motion_zoom_value = ARENA_PG_ZOOM_DEF;
            pg->motion_action = 0;
            playground_apply_motion_zoom(pg);
            (void)gfx_arena_scene_motion_set_action(pg->scene, pg->motion_off, 0, true);
        }
    }

    pg->motion_zoom = gfx_progress_bar_create(pg->disp);
    if (pg->motion_zoom != NULL) {
        (void)gfx_object_set_pos(pg->motion_zoom, ARENA_PG_ZOOM_X, ARENA_PG_ZOOM_Y);
        (void)gfx_object_set_size(pg->motion_zoom, ARENA_PG_ZOOM_W, ARENA_PG_ZOOM_H);
        (void)gfx_progress_bar_set_direction(pg->motion_zoom, GFX_PROGRESS_BAR_DIR_VERTICAL);
        (void)gfx_progress_bar_set_colors(pg->motion_zoom, GFX_COLOR_HEX(0x26313B),
                                          GFX_COLOR_HEX(0x2F8CFF));
        (void)gfx_progress_bar_set_thumb_style(pg->motion_zoom, GFX_COLOR_HEX(0xF7FBFF),
                                               GFX_COLOR_HEX(0xF7FBFF), 0);
        (void)gfx_progress_bar_set_radius(pg->motion_zoom, 16);
        (void)gfx_progress_bar_set_fill_pad(pg->motion_zoom, 4);
        (void)gfx_progress_bar_set_interactive(pg->motion_zoom, true);
        (void)gfx_progress_bar_set_value_changed_cb(pg->motion_zoom, playground_motion_zoom_cb, pg);
        (void)gfx_progress_bar_set_value(pg->motion_zoom, pg->motion_zoom_value);
        (void)gfx_object_set_visible(pg->motion_zoom, false);
    }

    pg->pick_stamp = gfx_image_create(pg->disp);
    if (pg->pick_stamp != NULL) {
        const gfx_image_src_t src = {
            .type = GFX_IMAGE_SRC_TYPE_IMAGE_DSC,
            .data = &my_pick_stamp,
        };
        (void)gfx_object_set_pos(pg->pick_stamp, PICK_STAMP_X, PICK_STAMP_Y);
        (void)gfx_image_set_source_desc(pg->pick_stamp, &src);
        (void)gfx_object_set_visible(pg->pick_stamp, false);
        (void)playground_create_pick_stamp_labels(pg, pg->pick_stamp_date,
                sizeof(pg->pick_stamp_date) / sizeof(pg->pick_stamp_date[0]),
                "06.13", (gfx_font_t)&font_match_bogle_oblique_27_4,
                PICK_STAMP_DATE_X, PICK_STAMP_DATE_Y,
                PICK_STAMP_DATE_W, PICK_STAMP_DATE_H, PICK_STAMP_DATE_STEP_X);
        (void)playground_create_pick_stamp_labels(pg, pg->pick_stamp_time,
                sizeof(pg->pick_stamp_time) / sizeof(pg->pick_stamp_time[0]),
                "14:13:45", (gfx_font_t)&font_match_bogle_oblique_20_4,
                PICK_STAMP_TIME_X, PICK_STAMP_TIME_Y,
                PICK_STAMP_TIME_W, PICK_STAMP_TIME_H, PICK_STAMP_TIME_STEP_X);
    }

    return 0;
}

static void playground_bind_flow_images(gfx_arena_scene_t *scene, const char *node_name,
                                        uint16_t count)
{
    gfx_arena_node_t *n;
    uint16_t i;
    if (scene == NULL || node_name == NULL || count == 0) {
        return;
    }
    n = gfx_arena_find_by_name(&scene->arena, node_name);
    if (n == NULL) {
        return;
    }
    {
        uint32_t off = gfx_arena_node_offset(&scene->arena, n);
        for (i = 0; i < count; i++) {
            const char *path = s_flow_image_files[i %
                               (sizeof(s_flow_image_files) / sizeof(s_flow_image_files[0]))];
            gfx_image_src_t src = {
                .type = GFX_IMAGE_SRC_TYPE_FILE,
                .data = path,
            };
            (void)gfx_arena_scene_bind_image_src(scene, off, i, 0, &src);
        }
    }
}

int gfx_arena_playground_bind(gfx_display_t *disp, const uint8_t *pkg, size_t pkg_size,
                          gfx_font_t font, gfx_arena_scene_t *out_scene,
                          gfx_arena_playground_t *out_pg)
{
    if (disp == NULL || pkg == NULL || out_scene == NULL || out_pg == NULL) {
        return -1;
    }

    gfx_arena_t arena = {0};
    if (gfx_arena_load(pkg, pkg_size, &arena) != 0) {
        return -2;
    }
    if (gfx_arena_scene_attach(disp, &arena, out_scene) != 0) {
        gfx_arena_free(&arena);
        return -3;
    }
    gfx_arena_scene_set_font(out_scene, font);

    memset(out_pg, 0, sizeof(*out_pg));
    out_pg->scene = out_scene;
    out_pg->disp = disp;
    out_pg->focus = 0;
    out_pg->fps_off = GFX_ARENA_NO_NODE;
    out_pg->preview_btn_off = GFX_ARENA_NO_NODE;
    out_pg->anim_off = GFX_ARENA_NO_NODE;
    out_pg->motion_off = GFX_ARENA_NO_NODE;
    out_pg->progress_off = GFX_ARENA_NO_NODE;
    out_pg->progress_label_off = GFX_ARENA_NO_NODE;
    out_pg->imgbtn_status_off = GFX_ARENA_NO_NODE;
    out_pg->motion_zoom_value = ARENA_PG_ZOOM_DEF;

    gfx_arena_node_t *title = gfx_arena_find_by_name(&out_scene->arena, s_title_slots[0]);
    out_pg->title_off = GFX_ARENA_NO_NODE;
    if (title != NULL) {
        out_pg->title_off = gfx_arena_node_offset(&out_scene->arena, title);
    }

    gfx_arena_node_t *fps = gfx_arena_find_by_name(&out_scene->arena, "FPS: --              ");
    if (fps != NULL) {
        out_pg->fps_off = gfx_arena_node_offset(&out_scene->arena, fps);
    }

    gfx_arena_node_t *pbtn = gfx_arena_find_by_name(&out_scene->arena, "Next Action  1/8     ");
    if (pbtn != NULL) {
        out_pg->preview_btn_off = gfx_arena_node_offset(&out_scene->arena, pbtn);
    }

    gfx_arena_node_t *nav = gfx_arena_find_by_name(&out_scene->arena, "nav");
    if (nav == NULL) {
        gfx_arena_scene_detach(out_scene);
        return -4;
    }
    out_pg->nav_off = gfx_arena_node_offset(&out_scene->arena, nav);

    for (int i = 0; i < ARENA_PG_NAV_COUNT; i++) {
        gfx_arena_node_t *p = gfx_arena_find_by_name(&out_scene->arena, s_panel_names[i]);
        if (p == NULL) {
            gfx_arena_scene_detach(out_scene);
            return -5;
        }
        out_pg->panel_offs[i] = gfx_arena_node_offset(&out_scene->arena, p);
    }

    gfx_arena_node_t *prog = gfx_arena_find_by_name(&out_scene->arena, "progress");
    if (prog != NULL) {
        out_pg->progress_off = gfx_arena_node_offset(&out_scene->arena, prog);
    }
    gfx_arena_node_t *plab = gfx_arena_find_by_name(&out_scene->arena, "Progress: 42%          ");
    if (plab != NULL) {
        out_pg->progress_label_off = gfx_arena_node_offset(&out_scene->arena, plab);
    }
    gfx_arena_node_t *ist = gfx_arena_find_by_name(&out_scene->arena, "Image Button: idle              ");
    if (ist != NULL) {
        out_pg->imgbtn_status_off = gfx_arena_node_offset(&out_scene->arena, ist);
    }
    gfx_arena_node_t *anim = gfx_arena_find_by_name(&out_scene->arena, "demo_anim");
    if (anim != NULL) {
        out_pg->anim_off = gfx_arena_node_offset(&out_scene->arena, anim);
    }
    gfx_arena_node_t *mot = gfx_arena_find_by_name(&out_scene->arena, "demo_motion");
    if (mot != NULL) {
        out_pg->motion_off = gfx_arena_node_offset(&out_scene->arena, mot);
    }

    s_actions[0] = (gfx_arena_action_entry_t) {
        .name = "on_pg_btn", .cb = on_pg_btn, .user_data = out_pg,
    };
    s_actions[1] = (gfx_arena_action_entry_t) {
        .name = "nav", .cb = on_nav, .user_data = out_pg,
    };
    s_actions[2] = (gfx_arena_action_entry_t) {
        .name = "Image Button", .cb = on_imgbtn, .user_data = out_pg,
    };
    s_actions[3] = (gfx_arena_action_entry_t) {
        .name = "progress", .cb = on_progress, .user_data = out_pg,
    };
    s_actions[4] = (gfx_arena_action_entry_t) {
        .name = "on_preview_next", .cb = on_preview_next, .user_data = out_pg,
    };
    gfx_arena_scene_set_actions(out_scene, s_actions, 5);

    {
        gfx_arena_node_t *jpeg = gfx_arena_find_by_name(&out_scene->arena, "jpeg_demo");
        if (jpeg != NULL) {
            uint32_t off = gfx_arena_node_offset(&out_scene->arena, jpeg);
            (void)gfx_arena_scene_ensure_pkg_images(out_scene, off);
        }
        playground_bind_flow_images(out_scene, "demo_coverflow", 5);
        playground_bind_flow_images(out_scene, "demo_pageflow", 5);
    }

    (void)playground_bind_arn_hosts(out_pg);

    (void)gfx_arena_playground_set_focus(out_pg, 0);
    (void)gfx_arena_scene_mark_dirty_all(out_scene);
    return 0;
}

void gfx_arena_playground_unbind(gfx_arena_playground_t *pg)
{
    size_t i;
    if (pg == NULL) {
        return;
    }
    playground_set_overlays_visible(pg, false, false);
    if (pg->anim_off != GFX_ARENA_NO_NODE && pg->scene != NULL) {
        (void)gfx_arena_scene_anim_set_playing(pg->scene, pg->anim_off, false);
        playground_set_node_visible(pg, pg->anim_off, false);
    }
    if (pg->motion_off != GFX_ARENA_NO_NODE) {
        playground_set_node_visible(pg, pg->motion_off, false);
    }
    if (pg->motion_zoom != NULL) {
        (void)gfx_object_delete(pg->motion_zoom);
        pg->motion_zoom = NULL;
    }
    if (pg->pick_stamp != NULL) {
        (void)gfx_object_delete(pg->pick_stamp);
        pg->pick_stamp = NULL;
    }
    for (i = 0; i < sizeof(pg->pick_stamp_date) / sizeof(pg->pick_stamp_date[0]); i++) {
        if (pg->pick_stamp_date[i] != NULL) {
            (void)gfx_object_delete(pg->pick_stamp_date[i]);
            pg->pick_stamp_date[i] = NULL;
        }
    }
    for (i = 0; i < sizeof(pg->pick_stamp_time) / sizeof(pg->pick_stamp_time[0]); i++) {
        if (pg->pick_stamp_time[i] != NULL) {
            (void)gfx_object_delete(pg->pick_stamp_time[i]);
            pg->pick_stamp_time[i] = NULL;
        }
    }
    pg->disp = NULL;
}

int gfx_arena_playground_set_focus(gfx_arena_playground_t *pg, uint16_t index)
{
    if (pg == NULL || pg->scene == NULL || index >= ARENA_PG_NAV_COUNT) {
        return -1;
    }

    const uint16_t prev = pg->focus;
    if (prev != index) {
        if (prev < ARENA_PG_NAV_COUNT) {
            gfx_arena_node_t *old_p = gfx_arena_node(&pg->scene->arena, pg->panel_offs[prev]);
            if (old_p != NULL) {
                old_p->flags = (uint16_t)(old_p->flags & ~GFX_ARENA_F_VISIBLE);
            }
        }
        gfx_arena_node_t *new_p = gfx_arena_node(&pg->scene->arena, pg->panel_offs[index]);
        if (new_p != NULL) {
            new_p->flags = (uint16_t)(new_p->flags | GFX_ARENA_F_VISIBLE);
        }
        (void)gfx_arena_scene_mark_dirty(pg->scene, pg->panel_offs[index]);

        gfx_arena_node_t *title = gfx_arena_node(&pg->scene->arena, pg->title_off);
        if (title != NULL) {
            char *s = (char *)gfx_arena_str(&pg->scene->arena, title->name_off);
            if (s != NULL) {
                const char *src = s_title_slots[index];
                size_t cap = strlen(s);
                size_t n = strlen(src);
                if (n > cap) {
                    n = cap;
                }
                memcpy(s, src, n);
                if (n < cap) {
                    s[n] = '\0';
                }
                (void)gfx_arena_scene_mark_dirty(pg->scene, pg->title_off);
            }
        }

        gfx_arena_node_t *nav = gfx_arena_node(&pg->scene->arena, pg->nav_off);
        if (nav != NULL) {
            gfx_arena_items_hdr_t *ih = gfx_arena_items_mut(&pg->scene->arena, nav->reserved);
            if (ih != NULL && ih->selected != index) {
                ih->selected = index;
                (void)gfx_arena_scene_mark_dirty(pg->scene, pg->nav_off);
            }
        }
        pg->focus = index;
    }

    playground_set_node_visible(pg, pg->anim_off, index == ARENA_PG_NAV_ANIM);
    playground_set_node_visible(pg, pg->motion_off, index == ARENA_PG_NAV_MOTION);
    playground_set_overlays_visible(pg,
                                    index == ARENA_PG_NAV_PICK_STAMP,
                                    index == ARENA_PG_NAV_MOTION);
    if (index == ARENA_PG_NAV_ANIM && pg->anim_off != GFX_ARENA_NO_NODE) {
        (void)playground_load_anim_clip(pg, pg->anim_clip);
    } else if (pg->anim_off != GFX_ARENA_NO_NODE) {
        (void)gfx_arena_scene_anim_set_playing(pg->scene, pg->anim_off, false);
    }
    if (index == ARENA_PG_NAV_MOTION && pg->motion_off != GFX_ARENA_NO_NODE) {
        playground_apply_motion_zoom(pg);
        (void)gfx_arena_scene_motion_set_action(pg->scene, pg->motion_off, pg->motion_action, true);
        {
            gfx_arena_node_t *note = gfx_arena_find_by_name(&pg->scene->arena,
                                                    "Current action: Move                    ");
            if (note != NULL) {
                char buf[48];
                uint32_t off = gfx_arena_node_offset(&pg->scene->arena, note);
                (void)snprintf(buf, sizeof(buf), "Current action: %s",
                               s_motion_actions[pg->motion_action]);
                playground_write_node_text(pg, off, buf);
            }
        }
    }
    playground_sync_preview_button(pg);
    return 0;
}

int gfx_arena_playground_poll_nav(gfx_arena_playground_t *pg)
{
    if (pg == NULL || pg->scene == NULL) {
        return 0;
    }
    gfx_arena_node_t *nav = gfx_arena_node(&pg->scene->arena, pg->nav_off);
    if (nav == NULL) {
        return 0;
    }
    const gfx_arena_items_hdr_t *ih = gfx_arena_items(&pg->scene->arena, nav->reserved);
    if (ih == NULL || ih->selected == GFX_ARENA_ITEMS_SELECTED_NONE ||
        ih->selected >= ARENA_PG_NAV_COUNT) {
        return 0;
    }
    if (ih->selected == pg->focus) {
        return 0;
    }
    (void)gfx_arena_playground_set_focus(pg, ih->selected);
    return 1;
}

void gfx_arena_playground_update_perf_label(gfx_arena_playground_t *pg, uint32_t fps, uint32_t frame_ms)
{
    char buf[32];
    char *s;
    size_t cap;
    size_t n;
    gfx_arena_node_t *lab;

    if (pg == NULL || pg->scene == NULL || pg->fps_off == GFX_ARENA_NO_NODE) {
        return;
    }
    lab = gfx_arena_node(&pg->scene->arena, pg->fps_off);
    if (lab == NULL) {
        return;
    }
    s = (char *)gfx_arena_str(&pg->scene->arena, lab->name_off);
    if (s == NULL) {
        return;
    }
    (void)snprintf(buf, sizeof(buf), "FPS: %u  %u ms", (unsigned)fps, (unsigned)frame_ms);
    cap = strlen(s);
    n = strlen(buf);
    if (n > cap) {
        n = cap;
    }
    memcpy(s, buf, n);
    if (n < cap) {
        s[n] = '\0';
    }
    (void)gfx_arena_scene_mark_dirty(pg->scene, pg->fps_off);
}
