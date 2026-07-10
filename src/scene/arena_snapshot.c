/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gfx/scene/arena_snapshot.h"

#include <string.h>

/* Runs shorter than this encode no better than literals; keep them literal
 * so the decoder spends fewer token round-trips. */
#define ARENA_SNAPSHOT_MIN_RUN 3u

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint32_t arena_snapshot_crc32(const uint8_t *data, size_t size)
{
    /* IEEE reflected CRC32, nibble table (small, fast enough for KB blobs). */
    static const uint32_t k_tbl[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc = k_tbl[(crc ^ data[i]) & 0x0Fu] ^ (crc >> 4);
        crc = k_tbl[(crc ^ (data[i] >> 4)) & 0x0Fu] ^ (crc >> 4);
    }
    return crc ^ 0xFFFFFFFFu;
}

size_t arena_snapshot_rle_max_size(uint16_t w, uint16_t h)
{
    const size_t px = (size_t)w * (size_t)h;
    const size_t tokens = (px + ARENA_SNAPSHOT_TOK_MAX_LEN - 1u) / ARENA_SNAPSHOT_TOK_MAX_LEN;
    return sizeof(arena_snapshot_hdr_t) + px * 2u + tokens * 2u;
}

/** Flush pending literal pixels [lit_start, pos) as literal tokens. */
static size_t flush_literals(const uint16_t *px, size_t lit_start, size_t pos,
                             uint8_t *out, size_t off)
{
    while (lit_start < pos) {
        size_t n = pos - lit_start;
        if (n > ARENA_SNAPSHOT_TOK_MAX_LEN) {
            n = ARENA_SNAPSHOT_TOK_MAX_LEN;
        }
        put_u16(out + off, (uint16_t)n);
        off += 2u;
        for (size_t i = 0; i < n; i++) {
            put_u16(out + off + i * 2u, px[lit_start + i]);
        }
        off += n * 2u;
        lit_start += n;
    }
    return off;
}

int arena_snapshot_rle_encode(const uint16_t *px, uint16_t w, uint16_t h,
                              uint32_t arn_crc32,
                              uint8_t *out, size_t out_cap, size_t *out_size)
{
    if (px == NULL || out == NULL || out_size == NULL || w == 0u || h == 0u ||
            out_cap < arena_snapshot_rle_max_size(w, h)) {
        return -1;
    }

    const size_t total = (size_t)w * (size_t)h;
    size_t off = sizeof(arena_snapshot_hdr_t);
    size_t pos = 0;
    size_t lit_start = 0;

    while (pos < total) {
        size_t run = 1;
        while (pos + run < total && run < ARENA_SNAPSHOT_TOK_MAX_LEN &&
                px[pos + run] == px[pos]) {
            run++;
        }
        if (run >= ARENA_SNAPSHOT_MIN_RUN) {
            off = flush_literals(px, lit_start, pos, out, off);
            put_u16(out + off, (uint16_t)(ARENA_SNAPSHOT_TOK_RUN | (uint16_t)run));
            put_u16(out + off + 2u, px[pos]);
            off += 4u;
            pos += run;
            lit_start = pos;
        } else {
            pos += run;
        }
    }
    off = flush_literals(px, lit_start, pos, out, off);

    arena_snapshot_hdr_t hdr = {
        .magic = ARENA_SNAPSHOT_MAGIC,
        .version = ARENA_SNAPSHOT_VERSION,
        .reserved = 0,
        .w = w,
        .h = h,
        .arn_crc32 = arn_crc32,
        .data_size = (uint32_t)(off - sizeof(arena_snapshot_hdr_t)),
    };
    memcpy(out, &hdr, sizeof(hdr));
    *out_size = off;
    return 0;
}

int arena_snapshot_rle_decode(const uint8_t *data, size_t size,
                              uint16_t *buf, uint16_t w, uint16_t h)
{
    if (data == NULL || buf == NULL || size < sizeof(arena_snapshot_hdr_t)) {
        return -1;
    }

    arena_snapshot_hdr_t hdr;
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic != ARENA_SNAPSHOT_MAGIC || hdr.version != ARENA_SNAPSHOT_VERSION ||
            hdr.w != w || hdr.h != h ||
            (size_t)hdr.data_size + sizeof(hdr) > size) {
        return -2;
    }

    const uint8_t *p = data + sizeof(hdr);
    const uint8_t *end = p + hdr.data_size;
    const size_t total = (size_t)w * (size_t)h;
    size_t pos = 0;

    while (p + 2u <= end && pos < total) {
        const uint16_t tok = get_u16(p);
        p += 2u;
        if (tok & ARENA_SNAPSHOT_TOK_RUN) {
            const size_t n = tok & ARENA_SNAPSHOT_TOK_MAX_LEN;
            if (p + 2u > end || n == 0u || pos + n > total) {
                return -3;
            }
            const uint16_t v = get_u16(p);
            p += 2u;
            /* Runs dominate UI frames; fill word-wide so PSRAM writes are
             * not throttled by 16-bit stores. */
            uint16_t *dst = buf + pos;
            size_t left = n;
            if (((uintptr_t)dst & 3u) != 0u && left > 0u) {
                *dst++ = v;
                left--;
            }
            const uint32_t vv = ((uint32_t)v << 16) | v;
            uint32_t *dst32 = (uint32_t *)dst;
            for (size_t i = 0; i < left / 2u; i++) {
                dst32[i] = vv;
            }
            if ((left & 1u) != 0u) {
                dst[left - 1u] = v;
            }
            pos += n;
        } else {
            const size_t n = tok;
            if (n == 0u || p + n * 2u > end || pos + n > total) {
                return -3;
            }
            /* Pixels are LE u16; direct memcpy matches on LE targets. */
            memcpy(buf + pos, p, n * 2u);
            p += n * 2u;
            pos += n;
        }
    }
    return (pos == total) ? 0 : -4;
}
