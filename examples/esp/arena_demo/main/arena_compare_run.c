/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arena_compare_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena_gfx_bridge.h"
#include "gfx/display.h"
#include "gfx/scene/arena_scene.h"
#include "platform/gfx_platform.h"

#define GRID_COLS ARENA_COMPARE_GRID_COLS
#define GRID_ROWS ARENA_COMPARE_GRID_ROWS
#define NAME_SLOT 8
#define NODE_COUNT (1u + (uint16_t)GRID_ROWS + (uint16_t)(GRID_COLS * GRID_ROWS))
#define DIRTY_ROW_CELLS GRID_COLS

uint8_t *gfx_arena_compare_pack_grid(uint16_t screen_w, uint16_t screen_h,
                                 size_t *out_size, uint16_t *out_count)
{
    gfx_arena_desc_t *descs = (gfx_arena_desc_t *)calloc(NODE_COUNT, sizeof(gfx_arena_desc_t));
    char *names = (char *)calloc((size_t)(GRID_COLS * GRID_ROWS), NAME_SLOT);
    if (descs == NULL || names == NULL) {
        free(descs);
        free(names);
        return NULL;
    }

    uint16_t n = 0;
    descs[n++] = (gfx_arena_desc_t) {
        .type = GFX_ARENA_NODE_CONTAINER,
        .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG,
        .x = 0, .y = 0, .w = screen_w, .h = screen_h,
        .bg_rgb = 0x1c1f2e, .name = "root", .parent = -1,
    };

    const int cell_w = (int)screen_w / GRID_COLS;
    const int cell_h = (int)screen_h / GRID_ROWS;
    if (cell_w < 8 || cell_h < 8) {
        free(descs);
        free(names);
        return NULL;
    }

    const uint16_t row0 = n;
    for (int row = 0; row < GRID_ROWS; row++) {
        descs[n++] = (gfx_arena_desc_t) {
            .type = GFX_ARENA_NODE_CONTAINER,
            .flags = GFX_ARENA_F_VISIBLE,
            .x = 0,
            .y = (int16_t)(row * cell_h),
            .w = screen_w,
            .h = (uint16_t)cell_h,
            .bg_rgb = 0,
            .name = NULL,
            .parent = 0,
        };
    }

    uint16_t cell_i = 0;
    for (int row = 0; row < GRID_ROWS; row++) {
        const int parent = (int)row0 + row;
        for (int col = 0; col < GRID_COLS; col++) {
            char *slot = names + (size_t)cell_i * NAME_SLOT;
            (void)snprintf(slot, NAME_SLOT, "B%03u", (unsigned)cell_i);
            const int pad = 1;
            descs[n++] = (gfx_arena_desc_t) {
                .type = GFX_ARENA_NODE_BUTTON,
                .flags = GFX_ARENA_F_VISIBLE | GFX_ARENA_F_BG | GFX_ARENA_F_CLICKABLE,
                .x = (int16_t)(col * cell_w + pad),
                .y = (int16_t)pad,
                .w = (uint16_t)(cell_w - pad * 2),
                .h = (uint16_t)(cell_h - pad * 2),
                .bg_rgb = (uint32_t)(0x204060 + (uint32_t)(cell_i * 0x010203)),
                .name = slot,
                .parent = parent,
                .action = NULL,
            };
            cell_i++;
        }
    }

    if (out_count != NULL) {
        *out_count = n;
    }
    uint8_t *pkg = gfx_arena_pack(descs, n, out_size);
    free(descs);
    free(names);
    return pkg;
}

static uint32_t node_off_at(gfx_arena_t *arena, uint16_t idx)
{
    const gfx_arena_hdr_t *hdr = gfx_arena_hdr(arena);
    return hdr->nodes_off + (uint32_t)idx * (uint32_t)sizeof(gfx_arena_node_t);
}

uint32_t gfx_arena_compare_row_container_off(gfx_arena_t *arena, int row)
{
    if (arena == NULL || row < 0 || row >= GRID_ROWS) {
        return GFX_ARENA_NO_NODE;
    }
    /* indices: 0=root, 1..GRID_ROWS = row containers */
    return node_off_at(arena, (uint16_t)(1 + row));
}

static uint32_t first_cell_off_of_row(gfx_arena_t *arena, int row)
{
    /* after root + GRID_ROWS containers */
    const uint16_t idx = (uint16_t)(1u + (uint16_t)GRID_ROWS + (uint16_t)row * (uint16_t)GRID_COLS);
    return node_off_at(arena, idx);
}

void gfx_arena_compare_recolor_row(gfx_arena_t *arena, int row, uint32_t color_base)
{
    if (arena == NULL || row < 0 || row >= GRID_ROWS) {
        return;
    }
    const uint32_t base = first_cell_off_of_row(arena, row);
    for (int i = 0; i < DIRTY_ROW_CELLS; i++) {
        const uint32_t off = base + (uint32_t)i * (uint32_t)sizeof(gfx_arena_node_t);
        gfx_arena_node_t *btn = gfx_arena_node(arena, off);
        if (btn != NULL) {
            btn->bg_rgb = color_base + (uint32_t)i;
        }
    }
}

static void accum_perf(gfx_display_t *disp, int64_t *render_sum, int64_t *flush_sum)
{
    gfx_display_perf_stats_t st = {0};
    if (gfx_display_get_perf_stats(disp, &st) != GFX_OK) {
        return;
    }
    *render_sum += (int64_t)st.render_time_us;
    *flush_sum += (int64_t)st.flush_time_us;
}

static void dirty_first_row_arena(gfx_arena_scene_t *scene, int iter)
{
    gfx_arena_compare_recolor_row(&scene->arena, 0, 0xff0000u + (uint32_t)(iter & 0xff));
    (void)gfx_arena_scene_mark_dirty(scene, gfx_arena_compare_row_container_off(&scene->arena, 0));
}

static void dirty_first_row_object(gfx_arena_gfx_scene_t *scene, int iter)
{
    gfx_arena_compare_recolor_row(&scene->arena, 0, 0xff0000u + (uint32_t)(iter & 0xff));
    const uint32_t base = first_cell_off_of_row(&scene->arena, 0);
    for (int i = 0; i < DIRTY_ROW_CELLS; i++) {
        const uint32_t off = base + (uint32_t)i * (uint32_t)sizeof(gfx_arena_node_t);
        (void)gfx_arena_gfx_sync_node(scene, off);
    }
}

static int run_arena(gfx_handle_t gfx, gfx_display_t *disp, gfx_font_t font,
                     const uint8_t *pkg, size_t pkg_size, int iters,
                     gfx_arena_compare_path_stats_t *out)
{
    memset(out, 0, sizeof(*out));
    gfx_arena_scene_t scene;
    memset(&scene, 0, sizeof(scene));

    int64_t load_sum = 0;
    for (int i = 0; i < iters; i++) {
        if (i > 0) {
            gfx_arena_scene_detach(&scene);
        }
        gfx_arena_t arena = {0};
        const int64_t t0 = gfx_platform_time_us();
        if (gfx_arena_load(pkg, pkg_size, &arena) != 0 ||
                gfx_arena_scene_attach(disp, &arena, &scene) != 0) {
            gfx_arena_free(&arena);
            return -1;
        }
        gfx_arena_scene_set_font(&scene, font);
        (void)gfx_arena_scene_mark_dirty_all(&scene);
        load_sum += gfx_platform_time_us() - t0;
    }
    out->load_us = load_sum;

    (void)gfx_core_refresh_now(gfx); /* warmup */

    const int64_t f0 = gfx_platform_time_us();
    for (int i = 0; i < iters; i++) {
        (void)gfx_arena_scene_mark_dirty_all(&scene);
        (void)gfx_core_refresh_now(gfx);
        accum_perf(disp, &out->full_render_us, &out->full_flush_us);
    }
    out->full_wall_us = gfx_platform_time_us() - f0;

    const int64_t d0 = gfx_platform_time_us();
    for (int i = 0; i < iters; i++) {
        dirty_first_row_arena(&scene, i);
        (void)gfx_core_refresh_now(gfx);
        accum_perf(disp, &out->dirty_render_us, &out->dirty_flush_us);
    }
    out->dirty_wall_us = gfx_platform_time_us() - d0;

    gfx_arena_scene_detach(&scene);
    return 0;
}

static int run_object(gfx_handle_t gfx, gfx_display_t *disp, gfx_font_t font,
                      const uint8_t *pkg, size_t pkg_size, int iters,
                      gfx_arena_compare_path_stats_t *out)
{
    memset(out, 0, sizeof(*out));
    gfx_arena_gfx_scene_t scene;
    memset(&scene, 0, sizeof(scene));

    int64_t load_sum = 0;
    for (int i = 0; i < iters; i++) {
        if (i > 0) {
            gfx_arena_gfx_free(&scene);
        }
        const int64_t t0 = gfx_platform_time_us();
        if (gfx_arena_gfx_bind(pkg, pkg_size, disp, font, &scene) != 0) {
            return -1;
        }
        (void)gfx_core_lock(gfx);
        gfx_display_refresh_all(disp);
        (void)gfx_core_unlock(gfx);
        load_sum += gfx_platform_time_us() - t0;
    }
    out->load_us = load_sum;

    (void)gfx_core_refresh_now(gfx); /* warmup */

    const int64_t f0 = gfx_platform_time_us();
    for (int i = 0; i < iters; i++) {
        (void)gfx_core_lock(gfx);
        gfx_display_refresh_all(disp);
        (void)gfx_core_unlock(gfx);
        (void)gfx_core_refresh_now(gfx);
        accum_perf(disp, &out->full_render_us, &out->full_flush_us);
    }
    out->full_wall_us = gfx_platform_time_us() - f0;

    const int64_t d0 = gfx_platform_time_us();
    for (int i = 0; i < iters; i++) {
        dirty_first_row_object(&scene, i);
        (void)gfx_core_refresh_now(gfx);
        accum_perf(disp, &out->dirty_render_us, &out->dirty_flush_us);
    }
    out->dirty_wall_us = gfx_platform_time_us() - d0;

    gfx_arena_gfx_free(&scene);
    return 0;
}

int gfx_arena_compare_run(gfx_handle_t gfx, gfx_display_t *disp, gfx_font_t font,
                      const gfx_arena_compare_config_t *cfg,
                      gfx_arena_compare_result_t *out)
{
    if (gfx == NULL || disp == NULL || cfg == NULL || out == NULL ||
            cfg->screen_w == 0 || cfg->screen_h == 0 || cfg->iters <= 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->iters = cfg->iters;

    uint8_t *pkg = gfx_arena_compare_pack_grid(cfg->screen_w, cfg->screen_h,
                                           &out->pkg_size, &out->node_count);
    if (pkg == NULL) {
        return -2;
    }

    if (run_arena(gfx, disp, font, pkg, out->pkg_size, cfg->iters, &out->arena) != 0) {
        free(pkg);
        return -3;
    }
    if (run_object(gfx, disp, font, pkg, out->pkg_size, cfg->iters, &out->object) != 0) {
        free(pkg);
        return -4;
    }

    free(pkg);
    return 0;
}
