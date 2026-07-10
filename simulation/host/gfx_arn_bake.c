/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * gfx_arn_bake — bake an ARN1 page into an ARS1 RLE static layer.
 *
 * Renders the page with the same C renderer AND the same LVGL font C
 * array the device uses (arena_draw_clipped_mode BAKE), so baked pixels —
 * including static text — are identical to a device render. Only
 * runtime-bound text (ARENA_F_TEXT_DYN, GSP `bind`) is excluded and drawn
 * on device as an overlay pass (OVERLAY).
 *
 *   gfx_arn_bake <in.arn> <out.rle> <width> <height> [bg_rgb888_hex] [font]
 *   font: puhui16 (default) | none
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/base.h"
#include "gfx/backends/memory.h"
#include "gfx/scene/arena.h"
#include "gfx/scene/arena_draw.h"
#include "gfx/scene/arena_scene.h"
#include "gfx/scene/arena_snapshot.h"
#include "gfx/widgets/font_lvgl.h"
#include "core/display/gfx_display_priv.h"
#include "render/gfx_render_priv.h"

extern const lv_font_t font_puhui_16_4;

typedef struct {
    const char *name;
    gfx_font_t  font;
} bake_font_entry_t;

/* Fonts available to the baker; must be the same C arrays the device
 * binds via arena_scene_set_font for pixel-identical glyphs. */
static const bake_font_entry_t k_bake_fonts[] = {
    { "puhui16", (gfx_font_t)&font_puhui_16_4 },
};

static uint8_t *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr,
                "usage: %s <in.arn> <out.rle> <width> <height> [bg_rgb888_hex] [font]\n",
                argv[0]);
        return 2;
    }
    const char *in_path = argv[1];
    const char *out_path = argv[2];
    const int w = atoi(argv[3]);
    const int h = atoi(argv[4]);
    uint32_t bg_rgb = 0x000000u;
    if (argc > 5) {
        bg_rgb = (uint32_t)strtoul(argv[5], NULL, 16);
    }
    const char *font_name = (argc > 6) ? argv[6] : "puhui16";
    gfx_font_t font = NULL;
    if (strcmp(font_name, "none") != 0) {
        for (size_t i = 0; i < sizeof(k_bake_fonts) / sizeof(k_bake_fonts[0]); i++) {
            if (strcmp(k_bake_fonts[i].name, font_name) == 0) {
                font = k_bake_fonts[i].font;
                break;
            }
        }
        if (font == NULL) {
            fprintf(stderr, "unknown font `%s` (available: puhui16, none)\n", font_name);
            return 2;
        }
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        fprintf(stderr, "bad dimensions %dx%d\n", w, h);
        return 2;
    }

    size_t pkg_size = 0;
    uint8_t *pkg = read_file(in_path, &pkg_size);
    if (pkg == NULL) {
        fprintf(stderr, "cannot read %s\n", in_path);
        return 1;
    }

    arena_t arena = {0};
    if (arena_load(pkg, pkg_size, &arena) != 0) {
        fprintf(stderr, "arena_load failed for %s\n", in_path);
        free(pkg);
        return 1;
    }

    gfx_handle_t gfx = gfx_core_init(&(gfx_core_config_t) {
        .fps = 30,
        .manual_tick = true,
        .task = GFX_CORE_TASK_DEFAULT_CONFIG(),
    });
    gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
        .h_res = (uint16_t)w,
        .v_res = (uint16_t)h,
    });
    gfx_display_t *disp = (gfx != NULL && backend != NULL) ?
                          gfx_display_add(gfx, &(gfx_display_config_t) {
        .h_res = (uint16_t)w,
        .v_res = (uint16_t)h,
        .backend = backend,
    }) : NULL;
    if (disp == NULL) {
        fprintf(stderr, "host display init failed\n");
        return 1;
    }

    /* Scene attach provides the font adapter arena_draw resolves glyphs from. */
    gfx_arena_scene_t scene;
    if (arena_scene_attach(disp, &arena, &scene) != 0) {
        fprintf(stderr, "arena_scene_attach failed\n");
        return 1;
    }
    if (font != NULL) {
        arena_scene_set_font(&scene, font);
        if (scene.font_adapter == NULL) {
            fprintf(stderr, "font adapter init failed\n");
            return 1;
        }
    }

    const size_t px_count = (size_t)w * (size_t)h;
    uint16_t *frame = (uint16_t *)malloc(px_count * sizeof(uint16_t));
    const size_t rle_cap = arena_snapshot_rle_max_size((uint16_t)w, (uint16_t)h);
    uint8_t *rle = (uint8_t *)malloc(rle_cap);
    if (frame == NULL || rle == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    gfx_render_surface_t surf = {
        .buf = frame,
        .buf_area = { 0, 0, (gfx_coord_t)w, (gfx_coord_t)h },
        .clip_area = { 0, 0, (gfx_coord_t)w, (gfx_coord_t)h },
        .stride = (gfx_coord_t)w,
        .format = disp->format.render_format,
    };
    gfx_area_t full = { 0, 0, (gfx_coord_t)w, (gfx_coord_t)h };
    gfx_render_surface_fill(disp, &surf, &full, GFX_COLOR_HEX(bg_rgb), 0xFFU);

    if (arena_draw_clipped_mode(disp, &scene.arena, &surf, ARENA_DRAW_MODE_BAKE) != 0) {
        fprintf(stderr, "arena draw failed\n");
        return 1;
    }

    size_t rle_size = 0;
    const uint32_t arn_crc = arena_snapshot_crc32(pkg, pkg_size);
    if (arena_snapshot_rle_encode(frame, (uint16_t)w, (uint16_t)h, arn_crc,
                                  rle, rle_cap, &rle_size) != 0) {
        fprintf(stderr, "rle encode failed\n");
        return 1;
    }

    FILE *out = fopen(out_path, "wb");
    if (out == NULL || fwrite(rle, 1, rle_size, out) != rle_size) {
        fprintf(stderr, "cannot write %s\n", out_path);
        return 1;
    }
    fclose(out);

    printf("%s: %dx%d raw=%zu rle=%zu (%.1f%%)\n", out_path, w, h,
           px_count * 2u, rle_size,
           100.0 * (double)rle_size / (double)(px_count * 2u));

    free(rle);
    free(frame);
    arena_scene_detach(&scene);
    free(pkg);
    return 0;
}
