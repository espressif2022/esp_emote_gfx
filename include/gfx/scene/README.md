# Scene package APIs

Headers under `include/gfx/scene/`. Symbols use the `gfx_` prefix
(`gfx_arena_load`, `gfx_gsp_load`). File names stay short, like `widgets/label.h`.

**Experimental / opt-in** — not included by `gfx/all.h`. ARN1 freezes the
pointer-free package byte layout for versioned decoding. The C runtime structs
and scene APIs are not ABI-stable yet and may change before Arena becomes a
stable component surface.

## Layers

| Layer | Header | Audience |
|-------|--------|----------|
| App (package UI) | `arena.h`, `arena_scene.h` | Device / examples |
| App (legacy GSP) | `gsp.h` | Object-tree package / migration |
| Host / tools | `gsp_to_arena.h` | GSP→ARN materializer |
| Internal | `arena_draw.h` | `gfx_render.c` + raw-FB tests |

## Default path

```text
Export ARN1  →  gfx_arena_load  →  gfx_arena_scene_attach
Hand-written UI  →  gfx_*_create
```

## Docs

Architecture and current release boundary:
[`docs/architecture.md`](../../../docs/architecture.md) and
[`examples/README.md`](../../../examples/README.md).
