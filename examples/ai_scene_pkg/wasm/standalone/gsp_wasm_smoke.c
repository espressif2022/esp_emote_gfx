/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gsp_core.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *read_file(const char *path, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *out_size = (size_t)size;
    return buf;
}

static int write_ppm(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return 1;
    }
    fprintf(fp, "P6\n%u %u\n255\n", w, h);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = rgba + ((size_t)y * w + x) * 4u;
            fwrite(p, 1, 3, fp);
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    const char *in = argc > 1 ? argv[1] : "examples/ai_scene_pkg/gsp_export/home.gsp";
    const char *out = argc > 2 ? argv[2] : "/tmp/gsp_wasm_smoke.ppm";
    size_t size = 0;
    uint8_t *pkg = read_file(in, &size);
    if (pkg == NULL) {
        fprintf(stderr, "failed to read %s\n", in);
        return 1;
    }

    int rc = gsp_core_validate(pkg, size);
    if (rc != GSP_CORE_OK) {
        fprintf(stderr, "validate failed: %s (%d)\n", gsp_core_err_name(rc), rc);
        free(pkg);
        return 1;
    }
    gsp_core_header_t hdr;
    rc = gsp_core_parse_header(pkg, size, &hdr);
    if (rc != GSP_CORE_OK) {
        free(pkg);
        return 1;
    }
    uint8_t *rgba = (uint8_t *)malloc((size_t)hdr.screen_w * hdr.screen_h * 4u);
    if (rgba == NULL) {
        free(pkg);
        return 1;
    }
    rc = gsp_core_render_rgba(pkg, size, rgba, hdr.screen_w, hdr.screen_h,
                              (uint32_t)hdr.screen_w * 4u);
    if (rc != GSP_CORE_OK) {
        fprintf(stderr, "render failed: %s (%d)\n", gsp_core_err_name(rc), rc);
        free(rgba);
        free(pkg);
        return 1;
    }

    unsigned long checksum = 0;
    for (size_t i = 0; i < (size_t)hdr.screen_w * hdr.screen_h * 4u; i++) {
        checksum = (checksum * 131u) + rgba[i];
    }
    if (checksum == 0) {
        fprintf(stderr, "render produced a blank checksum\n");
        free(rgba);
        free(pkg);
        return 1;
    }

    gsp_core_runtime_t *rt = gsp_core_runtime_create(pkg, size);
    if (rt == NULL) {
        fprintf(stderr, "runtime create failed\n");
        free(rgba);
        free(pkg);
        return 1;
    }
    int hit = gsp_core_runtime_click(rt, 344, 344);
    if (hit < 0) {
        fprintf(stderr, "runtime click failed: %d\n", hit);
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    rc = gsp_core_runtime_render_rgba(rt, rgba, hdr.screen_w, hdr.screen_h,
                                      (uint32_t)hdr.screen_w * 4u);
    if (rc != GSP_CORE_OK) {
        fprintf(stderr, "runtime render failed: %s (%d)\n", gsp_core_err_name(rc), rc);
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    unsigned long runtime_checksum = 0;
    for (size_t i = 0; i < (size_t)hdr.screen_w * hdr.screen_h * 4u; i++) {
        runtime_checksum = (runtime_checksum * 131u) + rgba[i];
    }
    if (runtime_checksum == checksum) {
        fprintf(stderr, "runtime click did not change the rendered frame\n");
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    const char *last_call = gsp_core_runtime_last_call(rt);
    if (last_call == NULL) {
        fprintf(stderr, "runtime click did not expose callback name\n");
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    int list_hit = gsp_core_runtime_click(rt, 86, 258);
    if (list_hit < 0) {
        fprintf(stderr, "runtime list click failed: %d\n", list_hit);
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    rc = gsp_core_runtime_render_rgba(rt, rgba, hdr.screen_w, hdr.screen_h,
                                      (uint32_t)hdr.screen_w * 4u);
    if (rc != GSP_CORE_OK) {
        fprintf(stderr, "runtime list render failed: %s (%d)\n", gsp_core_err_name(rc), rc);
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    unsigned long list_checksum = 0;
    for (size_t i = 0; i < (size_t)hdr.screen_w * hdr.screen_h * 4u; i++) {
        list_checksum = (list_checksum * 131u) + rgba[i];
    }
    if (list_checksum == runtime_checksum) {
        fprintf(stderr, "runtime list click did not change selected rendering\n");
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    if (write_ppm(out, rgba, hdr.screen_w, hdr.screen_h) != 0) {
        fprintf(stderr, "failed to write %s\n", out);
        gsp_core_runtime_destroy(rt);
        free(rgba);
        free(pkg);
        return 1;
    }
    printf("GSP wasm smoke: %ux%u objects=%u blobs=%u actions=%u checksum=%lu runtime=%lu list=%lu hit=%d list_hit=%d call=%s out=%s\n",
           hdr.screen_w, hdr.screen_h, hdr.obj_count, hdr.blob_count,
           hdr.action_count, checksum, runtime_checksum, list_checksum, hit, list_hit, last_call, out);

    gsp_core_runtime_destroy(rt);
    free(rgba);
    free(pkg);
    return 0;
}
