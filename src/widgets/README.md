# Widget Implementations

Public APIs live in `include/gfx/widgets/`. Implementations live here, grouped by
category rather than one directory per widget.

| Directory | Widgets |
|-----------|---------|
| `anim/` | animation widget |
| `basic/` | button, container, list, wheel, coverflow, pageflow, qrcode |
| `img/` | image, mesh image, image resource |
| `label/` | label (draw + object split) |
| `motion/` | motion player / scene |

## Include convention

```c
#include "gfx/widgets/image.h"          /* public API */
#include "gfx/display.h"                /* public core types when needed */
#include "gfx/input.h"                  /* touch input (not legacy core paths) */
#include "core/display/gfx_refresh_priv.h"  /* src/ implementation only */
#include "widgets/img/gfx_image_resource_priv.h"
```

Rules:

- Public headers: always `include/gfx/...`
- Core/render private headers: `src/core/...`, `src/render/...`, `src/widgets/.../*_priv.h`
- Source files keep the `gfx_<widget>.c` prefix for grep and symbol consistency
