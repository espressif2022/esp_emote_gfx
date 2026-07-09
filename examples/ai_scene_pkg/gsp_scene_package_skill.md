---
name: gsp-scene-package
description: Use when generating or updating esp_emote_gfx GSP scene package .inc files, especially binary u32-offset scene packages exported for host/device loading. Requires updating the companion manifest that explains the binary package contents, component rules, object relationships, offsets, fonts, blobs, and validation results.
---

# GSP Scene Package Skill Rules

When generating or modifying a GSP binary `.inc`, always produce two synchronized artifacts:

1. `*.inc`: machine-consumed package data.
2. `*.manifest.md`: human-readable package map.

Do not leave a binary `.inc` without its manifest. The manifest is the review surface for users and the handoff surface for later AI edits.

## Required `.inc` Shape

The `.inc` package must avoid native pointers in runtime-consumed binary data.

- Object references use `u16` indexes.
- String, params, blob table, and blob data references use `u32` byte offsets from package base.
- Multi-byte fields are little-endian fixed-width values.
- Header `total_size` must equal `sizeof(scene_pkg)`.
- Header CRC must verify with `gsp_crc32_scene()`.

Host-only binding tables, such as temporary font descriptors, may stay as C arrays while the format is being explored, but mark them clearly as runtime bindings, not pointer-bearing package payload.

## Required Manifest Sections

Every time a package `.inc` changes, update the matching manifest with:

- Source and export reason.
- Package header: magic, version, size, CRC, screen size, background, table offsets.
- Binary layout: header range, object table range, blob table range, params range if any, string table range, blob data range.
- Component rules: object type IDs, flag meanings used by this package, parent/index constraints.
- Object tree: index, type, parent, rect, flags, text/name/callback offsets, font id, bind id, blob id, and short semantic role.
- Resource bindings: font ids, family/path/size/weight/style; blob ids with codec/raw/comp size/format.
- Runtime path: which loader API consumes it and how callbacks/fonts are bound.
- Validation: command run, CRC result, self-check result, object count, known limitations.

If the package contains image data, expand the blob table in the manifest. Include codec, color format, width/height, stride, raw size, compressed size, data offset, and intended widget reference.

## Update Workflow

1. Generate or edit the author-side scene.
2. Export the binary package `.inc`.
3. Dump the package with `gsp_dump()` or equivalent.
4. Update the manifest from the dump and the author scene.
5. Run the headless demo or loader test.
6. Record the exact validation result in the manifest.

For `home.inc`, the companion manifest is `gsp_export/home.manifest.md`.
