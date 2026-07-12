# gsp_protocol

Protocol documentation for the GSP binary scene package.

| File | Purpose |
|---|---|
| `gsp_binary_protocol_zh.md` | Chinese protocol reference. |
| `gsp_binary_protocol_en.md` | English protocol reference. |

Keep both files synchronized when any of these change:

- package layout or field sizes
- loader validation rules
- object/action/blob semantics
- exporter requirements
- manifest expectations

The implementation reference is `include/gfx/scene/gsp.h` plus
`src/scene/gsp_load.c`.

ARN 运行时入口和验证方式见 [`examples/README.md`](../../README.md) 及
[`arena_model/README.md`](../arena_model/README.md)。
