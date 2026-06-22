/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "unity.h"

#include "core/base/gfx_fs_priv.h"

TEST_CASE("fs: mmap-assets fs returns direct views", "[fs]")
{
    gfx_fs_t *fs = NULL;
    gfx_fs_entry_t entry;
    gfx_fs_file_t *file = NULL;

    TEST_ASSERT_EQUAL(GFX_OK, gfx_fs_open(&(gfx_fs_open_config_t) {
        .source_type = GFX_FS_SOURCE_PARTITION,
        .access_mode = GFX_FS_ACCESS_DIRECT,
        .path_or_label = "assets_test",
    }, &fs));
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(GFX_FS_ACCESS_DIRECT, gfx_fs_get_access_mode(fs));

    TEST_ASSERT_EQUAL(GFX_OK, gfx_fs_entry_open_by_name(fs, "icon_rgb565.bin", &entry));
    TEST_ASSERT_NOT_NULL(entry.data);
    gfx_fs_entry_close(&entry);

    file = gfx_fs_fopen(fs, "icon_rgb565.bin", "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(gfx_fs_fdata(file));
    TEST_ASSERT_GREATER_THAN_UINT32(0, gfx_fs_fsize(file));
    TEST_ASSERT_EQUAL(0, gfx_fs_fclose(file));

    gfx_fs_close(fs);
}

TEST_CASE("fs: VFS dir fs reports copy access", "[fs]")
{
    gfx_fs_t *fs = NULL;
    gfx_fs_entry_t entry;

    TEST_ASSERT_EQUAL(GFX_OK, gfx_fs_open_dir("/", &fs));
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(GFX_FS_ACCESS_COPY, gfx_fs_get_access_mode(fs));
    TEST_ASSERT_EQUAL(GFX_ERR_NOT_FOUND, gfx_fs_entry_open_by_name(fs, "__gfx_missing_asset__.bin", &entry));
    TEST_ASSERT_NULL(entry.data);
    gfx_fs_close(fs);
}
