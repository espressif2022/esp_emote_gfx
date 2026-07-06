# Public Headers

All public API headers live under `include/gfx/`.

| Header | Role |
|--------|------|
| `base.h` | Core runtime: types, log, core, display, object, timer, input |
| `gfx.h` | Same as `base.h` (lightweight entry) |
| `all.h` | Full bundle: core + fs + backends + all widgets |
| `fs.h` | Asset filesystem |
| `backends/*.h` | Display backend factories |
| `widgets/*.h` | Widget APIs |

Top-level `include/gfx.h` includes the full `gfx/all.h` bundle.
