/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "gfx/fs.h"

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : "examples/assets/format";
    const char *name = argc > 2 ? argv[2] : "icon_rgb565.bin";
    gfx_asset_source_t *store = NULL;
    gfx_fs_file_t *file = NULL;
    unsigned char head[4] = {0};
    unsigned char head_copy[4] = {0};
    size_t got;
    long pos;
    int rc = 1;

    if (gfx_fs_open_dir(root, &store) != GFX_OK || store == NULL) {
        fprintf(stderr, "gfx_fs_open_dir failed: root=%s\n", root);
        return 1;
    }

    (void)gfx_fs_mount("", store);

    file = gfx_fs_fopen(name);
    if (file == NULL) {
        fprintf(stderr, "gfx_fs_fopen failed: name=%s root=%s\n", name, root);
        goto cleanup;
    }

    {
        size_t fsize = gfx_fs_fsize(file);
        const unsigned char *base = gfx_fs_fdata(file);

        got = gfx_fs_fread(file, head, sizeof(head));
        pos = gfx_fs_ftell(file);
        if (fsize == 0U || got != sizeof(head) || pos != (long)sizeof(head)) {
            fprintf(stderr, "file read failed: size=%zu got=%zu pos=%ld\n", fsize, got, pos);
            goto cleanup;
        }
        if (base != NULL && memcmp(head, base, sizeof(head)) != 0) {
            fprintf(stderr, "fdata/fread mismatch for direct-mapped file\n");
            goto cleanup;
        }
        printf("asset: root=%s file=%s size=%zu mapped=%s mode=%s\n",
               root, name, fsize, base != NULL ? "yes" : "no",
               base != NULL ? "direct" : "streamed");
    }

    if (gfx_fs_fseek(file, 0, SEEK_SET) != 0 || gfx_fs_ftell(file) != 0) {
        fprintf(stderr, "fseek SEEK_SET failed\n");
        goto cleanup;
    }

    got = gfx_fs_fread(file, head_copy, sizeof(head_copy));
    if (got != sizeof(head_copy) || memcmp(head, head_copy, sizeof(head)) != 0) {
        fprintf(stderr, "rewind read mismatch\n");
        goto cleanup;
    }

    if (gfx_fs_fseek(file, 0, SEEK_END) != 0 ||
            gfx_fs_ftell(file) != (long)gfx_fs_fsize(file) ||
            gfx_fs_fseek(file, -1, SEEK_SET) != -1 ||
            gfx_fs_fseek(file, (long)gfx_fs_fsize(file) + 1, SEEK_SET) != -1) {
        fprintf(stderr, "seek boundary semantics failed\n");
        goto cleanup;
    }

    gfx_fs_fclose(file);
    file = NULL;

    /* fopen_from bypasses mount table; still resolves within store. */
    file = gfx_fs_fopen_from(store, name);
    if (file == NULL) {
        fprintf(stderr, "gfx_fs_fopen_from failed: name=%s\n", name);
        goto cleanup;
    }

    rc = 0;

cleanup:
    gfx_fs_fclose(file);
    gfx_fs_close(store);   /* automatically unmounts all prefixes for this fs */
    return rc;
}
