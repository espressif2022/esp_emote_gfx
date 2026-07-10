/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * RLE-compressed RGB565 page snapshot (ARS1).
 *
 * Storage format for pre-rendered full-frame static layers used by the
 * arena pager. Produced on the host by the gfx_arn_bake tool (same C
 * renderer and font source as the device, pixel-identical by
 * construction; ARENA_DRAW_MODE_BAKE). Runtime-bound text
 * (ARENA_F_TEXT_DYN) is excluded and drawn on device as an overlay pass
 * (ARENA_DRAW_MODE_OVERLAY).
 *
 * Layout (little-endian, packed):
 *   arena_snapshot_hdr_t
 *   token stream: u16 tok
 *     tok & 0x8000: run  of (tok & 0x7FFF) pixels, next u16 = pixel value
 *     else:         literal count tok (> 0), tok u16 pixels follow
 *   Runs/literals may cross row boundaries; the stream covers w*h pixels.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARENA_SNAPSHOT_MAGIC   0x31535241u /* 'ARS1' LE */
#define ARENA_SNAPSHOT_VERSION 2u

#define ARENA_SNAPSHOT_TOK_RUN      0x8000u
#define ARENA_SNAPSHOT_TOK_MAX_LEN  0x7FFFu

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint16_t w;
    uint16_t h;
    /* CRC32 (IEEE) of the ARN blob this layer was baked from; 0 = no check.
     * Guards against stale layers when the page package changed but the
     * bake step was not re-run: mismatch falls back to node-tree render. */
    uint32_t arn_crc32;
    uint32_t data_size; /* token stream bytes after this header */
} arena_snapshot_hdr_t;
#pragma pack(pop)

/** CRC32 (IEEE, reflected) used for arena_snapshot_hdr_t.arn_crc32. */
uint32_t arena_snapshot_crc32(const uint8_t *data, size_t size);

/** Worst-case encoded size (all-literal stream) for a w*h RGB565 frame. */
size_t arena_snapshot_rle_max_size(uint16_t w, uint16_t h);

/**
 * Encode a w*h RGB565 frame into `out` (header + token stream).
 * out_cap must be >= arena_snapshot_rle_max_size(w, h). arn_crc32 is the
 * CRC of the source ARN blob (0 = skip the runtime staleness check).
 * Returns 0 on success and sets *out_size.
 */
int arena_snapshot_rle_encode(const uint16_t *px, uint16_t w, uint16_t h,
                              uint32_t arn_crc32,
                              uint8_t *out, size_t out_cap, size_t *out_size);

/**
 * Decode an ARS1 blob into a w*h RGB565 buffer. The blob dimensions must
 * match w/h exactly. Returns 0 on success.
 */
int arena_snapshot_rle_decode(const uint8_t *data, size_t size,
                              uint16_t *buf, uint16_t w, uint16_t h);

#ifdef __cplusplus
}
#endif
