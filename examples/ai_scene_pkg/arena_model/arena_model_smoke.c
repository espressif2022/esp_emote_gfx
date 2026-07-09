/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * arena_model_smoke — address-model only (no renderer).
 * Renderer / A-B: gfx_arena_draw_demo, gfx_arena_dirty_demo, gfx_arena_compare_demo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena_model.h"

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
    printf("OK  %s\n", msg); \
} while (0)

int main(void)
{
    printf("arena_model_smoke: relative-offset isomorphic arena\n");
    printf("sizeof(arena_hdr_t)=%zu sizeof(arena_node_t)=%zu\n",
           sizeof(arena_hdr_t), sizeof(arena_node_t));

    const arena_desc_t descs[] = {
        {
            ARENA_NODE_CONTAINER, ARENA_F_VISIBLE | ARENA_F_BG, 0, 0, 800, 480,
            0x1c1f2e, "root", -1
        },
        {
            ARENA_NODE_LABEL, ARENA_F_VISIBLE, 16, 12, 200, 28,
            0, "title", 0
        },
        {
            ARENA_NODE_BUTTON, ARENA_F_VISIBLE | ARENA_F_BG, 680, 420, 96, 40,
            0x2f8cff, "ok", 0
        },
    };
    const uint16_t count = (uint16_t)(sizeof(descs) / sizeof(descs[0]));

    size_t pkg_size = 0;
    uint8_t *pkg = arena_pack(descs, count, &pkg_size);
    CHECK(pkg != NULL, "pack returns buffer");

    arena_t arena = {0};
    CHECK(arena_load(pkg, pkg_size, &arena) == 0, "load = memcpy + validate");
    CHECK(arena.base != pkg, "runtime arena is a writable copy");

    printf("--- tree after load ---\n");
    arena_dump_tree(&arena, arena_hdr(&arena)->root_off, 0);

    arena_node_t *root = arena_node(&arena, arena_hdr(&arena)->root_off);
    CHECK(root != NULL && root->first_child != ARENA_NO_NODE, "root has children");
    arena_node_t *title = arena_node(&arena, root->first_child);
    CHECK(title != NULL && title->next_sibling != ARENA_NO_NODE, "title has sibling");
    CHECK(strcmp(arena_str(&arena, title->name_off), "title") == 0, "first child is title");

    arena_node_t *ok = arena_find_by_name(&arena, "ok");
    CHECK(ok != NULL, "find button by name");
    CHECK(ok == arena_node(&arena, title->next_sibling), "ok is title's next_sibling");

    const uint32_t old_bg = ok->bg_rgb;
    ok->bg_rgb = 0xff6644;
    CHECK(ok->bg_rgb == 0xff6644 && old_bg != ok->bg_rgb, "in-place mutate button bg");

    const arena_hdr_t *rom_hdr = (const arena_hdr_t *)pkg;
    const arena_node_t *rom_ok =
        (const arena_node_t *)(pkg + rom_hdr->nodes_off + 2u * sizeof(arena_node_t));
    CHECK(rom_ok->bg_rgb == 0x2f8cff, "ROM package unchanged after mutate");

    printf("--- tree after in-place mutate ---\n");
    arena_dump_tree(&arena, arena_hdr(&arena)->root_off, 0);

    printf("\nModel checklist:\n");
    printf("  [x] isomorphic / fast / in-place / portable\n");
    printf("  [ ] renderer  -> see gfx_arena_draw_demo / gfx_arena_compare_demo\n");

    arena_free(&arena);
    free(pkg);
    printf("PASS\n");
    return 0;
}
