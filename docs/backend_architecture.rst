Backend Architecture
====================

Goal
----

GFX separates widget rendering from the physical display target. Widgets draw
into a GFX render buffer; a display backend owns the final handoff to hardware,
simulation, or a test framebuffer.

The first backend layer is intentionally small:

* ``flush`` receives an exclusive rectangle and RGB565 pixels.
* ``wait_flush`` waits until the backend has consumed the pixels.
* ``destroy`` releases backend-owned resources.

The existing ``gfx_display_config_t.flush_cb`` path is implemented as a callback
backend, so current ESP LCD integrations keep working while newer targets can
provide a backend object directly.

Capability And Fallback
-----------------------

Backends may expose optional draw operations such as fill, blit, blend, scale,
transform, glyph drawing, and present. These operations are acceleration hooks,
not widget APIs. Widgets continue to draw through renderer helpers; the renderer
chooses a backend operation only when the backend advertises the matching
capability and the request satisfies format and alignment requirements.

If an accelerated operation is missing or rejects a request, the renderer must
fall back to software drawing and still use ``flush`` for the final handoff.

Alignment And Roundup
---------------------

Hardware backends may need dimensions, row pitch, or addresses aligned to 4
bytes, 8 bytes, cache-line size, DMA burst size, or accelerator block size.
This is a renderer/backend contract, not a widget concern.

The planned shape is:

* Backend exposes render alignment requirements.
* Renderer uses one central roundup helper when selecting chunk areas,
  temporary surface pitch, and accelerated operation rectangles.
* The helper rounds outwards and clips to the valid display/surface limit.
* If alignment expansion would change visible widget semantics or exceed the
  valid limit, the renderer uses software fallback.

Image descriptor ``stride`` remains source payload metadata. It should not be
used as a replacement for destination framebuffer pitch or accelerator
alignment rules.

Current implementation status:

* ``gfx_render_alignment_t`` records width, height, stride-byte, and address
  alignment requirements.
* Callback, memory, and SDL backends currently use default alignment of 1.
* ``gfx_backend_get_alignment()`` returns normalized defaults for unspecified
  fields.
* ``gfx_render_roundup_area()`` rounds half-open dirty/render areas outward and
  clips them to the display limit.
* ``gfx_render_roundup_stride_bytes()`` centralizes stride-byte alignment for
  future temporary surfaces and accelerator setup.
* Dirty render chunks use area roundup and, in non full-frame mode, an aligned
  physical row stride.
* Internal backend flush receives the source stride. Memory and SDL backends use
  it when reading non-compact chunks. The legacy public ``flush_cb`` remains a
  compact-row or full-frame contract.
* ``gfx_render_is_addr_aligned()`` centralizes address checks for future
  accelerated backend operations.
* Internal buffer allocation uses ``gfx_platform_aligned_alloc()`` with the
  backend ``addr_bytes`` requirement.
* ``gfx_render_backend_op_can_use_dst()`` checks capability, destination
  address, stride, and area alignment before a renderer uses an accelerated
  backend operation. Background fill already uses this helper and falls back to
  software drawing when the target is not eligible.
* Image widgets now try backend ``blit`` for opaque sources and backend
  ``blend`` for alpha/opacity sources. Missing operations, unsupported
  alignment, or backend rejection fall back to software image drawing.
* Host alignment smoke verifies internal buffer address alignment, aligned fill
  op usage, aligned image blit/blend usage, and external misaligned-buffer
  fallback.

Ownership
---------

Backend objects are single-owner objects. When a backend is passed to
``gfx_display_config_t.backend`` and ``gfx_display_add()`` succeeds, the display
owns that backend and destroys it from ``gfx_display_delete()``. If display
creation fails before ownership is accepted, the caller should delete the
backend.

Built-in Backends
-----------------

Callback backend
~~~~~~~~~~~~~~~~

The callback backend wraps the legacy ``flush_cb`` and ``gfx_display_flush_ready()``
flow. It is selected automatically when ``gfx_display_config_t.backend`` is NULL
and ``flush_cb`` is set.

Memory backend
~~~~~~~~~~~~~~

The memory backend is a software framebuffer target for tests, screenshots, and
headless rendering. It stores semantic RGB565 pixels in a caller-provided or
internally allocated buffer. When attached to a display, it derives source
format from the display's explicit output color format.

Example:

.. code-block:: c

   gfx_backend_t *backend = gfx_memory_backend_create(&(gfx_memory_backend_config_t) {
       .h_res = 240,
       .v_res = 240,
   });

   gfx_display_t *disp = gfx_display_add(handle, &(gfx_display_config_t) {
       .h_res = 240,
       .v_res = 240,
       .backend = backend,
       .buffers = {
           .buf_pixels = 240 * 40,
       },
   });

   const uint16_t *pixels = gfx_memory_backend_get_buffer(backend);

When a memory backend is attached to a display, keep the backend pointer only
for inspection helpers such as ``gfx_memory_backend_get_buffer()``. Do not call
``gfx_memory_backend_delete()`` separately after display creation succeeds.

SDL Simulation Plan
-------------------

The SDL backend lives outside the embedded-only build path under
``src/backend/sdl/`` and follows the same backend contract:

* Create an SDL window, renderer, and streaming texture.
* Receive GFX RGB565 flush rectangles.
* Convert the flushed pixels to the texture format, preferably XRGB8888 for
  simple desktop presentation.
* Present once a frame or after the last flush chunk.
* Convert SDL mouse events into GFX touch events.

This keeps SDL as a backend adapter instead of allowing desktop-specific types
to leak into widget, renderer, or asset code.

The first adapter implementation converts flushed RGB565 rectangles into an
XRGB8888 streaming texture. A host CMake target can compile
``src/backend/sdl/sdl_backend.c`` with either ``GFX_SDL_USE_SDL3`` or
``GFX_SDL_USE_SDL2`` and link the matching SDL library.

Next Steps
----------

* Add a desktop/test app CMake target that links SDL only on host builds.
* Add screenshot dumping from the memory backend.
* Add golden-image tests for image, label, mesh image, and Motion scenes.
* Add an SDL input adapter for mouse/touch and keyboard events.
