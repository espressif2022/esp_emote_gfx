Asset Specifications
====================

Image Descriptor
----------------

GFX image descriptors use ``gfx_image_dsc_t``:

.. code-block:: c

   typedef struct {
       gfx_image_header_t header;
       uint32_t data_size;
       const uint8_t *data;
       const void *reserved;
       const void *reserved_2;
   } gfx_image_dsc_t;

The binary image header is 12 bytes:

.. code-block:: text

   word0: magic[7:0] cf[15:8] flags[31:16]
   word1: width[15:0] height[31:16]
   word2: stride[15:0] reserved[31:16]

Fields:

* ``magic``: ``GFX_IMAGE_HEADER_MAGIC`` (0x19)
* ``cf``: one of ``gfx_color_format_t``
* ``flags``: reserved, must be 0 for now
* ``width`` / ``height``: image dimensions in pixels
* ``stride``: color-plane bytes per row. ``0`` means tightly packed rows
  (``width * color_pixel_size``) for C-array compatibility.

Descriptor validation rules:

* ``data`` must be non-NULL.
* ``width`` and ``height`` must be greater than 0.
* ``cf`` must be one of the supported image payload formats below.
* ``stride`` is the color-plane byte stride. If it is ``0``, GFX treats it as
  ``width * color_pixel_size``. If it is non-zero, it must be at least
  ``width * color_pixel_size`` and must be aligned to ``color_pixel_size``.
* ``data_size`` must be at least
  ``stride * height + alpha_bytes``.
* ``alpha_bytes`` is ``width * height`` for alpha-plane formats and 0
  otherwise.
* No additional source row alignment is required by GFX. Converters may pad
  color rows, but the exact padded value must be stored in ``stride``.

Color Formats
-------------

GFX keeps colors semantic. Source image format describes payload layout; target
framebuffer byte order is applied only at the final backend/write boundary.

Supported image payloads:

.. list-table::
   :header-rows: 1

   * - Format
     - Value
     - Layout
     - Stride
   * - ``GFX_COLOR_FORMAT_RGB565``
     - ``0x04``
     - RGB565 high byte, low byte
     - ``width * 2``
   * - ``GFX_COLOR_FORMAT_RGB565_SWAPPED``
     - ``0x05``
     - RGB565 low byte, high byte
     - ``width * 2``
   * - ``GFX_COLOR_FORMAT_RGB565A8``
     - ``0x0A``
     - RGB565 color plane followed by A8 alpha plane
     - ``width * 2``
   * - ``GFX_COLOR_FORMAT_RGB565A8_SWAPPED``
     - ``0x0B``
     - swapped RGB565 color plane followed by A8 alpha plane
     - ``width * 2``
   * - ``GFX_COLOR_FORMAT_RGB888``
     - ``0x0F``
     - RGB888 color plane
     - ``width * 3``
   * - ``GFX_COLOR_FORMAT_RGB888A8``
     - ``0x10``
     - RGB888 color plane followed by A8 alpha plane
     - ``width * 3``

Alpha-plane formats store all color pixels first, then ``width * height`` alpha
bytes. Alpha stride is always image width in pixels.

For example, a 16x16 ``RGB888A8`` image with no color-row padding uses:

.. code-block:: text

   stride       = 16 * 3 = 48
   color bytes  = 48 * 16 = 768
   alpha bytes  = 16 * 16 = 256
   data_size    = 1024

The first 768 bytes are RGB triples. The next 256 bytes are tightly packed A8
values. A padded 16x16 ``RGB888A8`` image may use ``stride = 60`` for the color
plane, but the alpha plane still remains 256 bytes with alpha stride 16.

Debug checklist:

* Use ``scripts/image_converter.py`` to generate C arrays or binary payloads.
* Check that ``cf`` matches the emitted byte layout. ``RGB888`` and
  ``RGB888A8`` do not use 16-bit swap.
* Check that ``stride`` is bytes, not pixels.
* Check that alpha-plane formats append alpha after the complete color plane,
  not interleaved per pixel.
* Use ``test_apps/main/test_image.c`` to compare RGB565, RGB565A8, RGB888, and
  RGB888A8 rendering.

Font Assets
-----------

The public font surface is intentionally narrow while GFX moves away from
LVGL-shaped internals:

* ``gfx_label_font_create()`` creates a scalable FreeType-backed ``gfx_font_t``
  when FreeType support is enabled.
* ``gfx_label_font_delete()`` releases fonts created by
  ``gfx_label_font_create()``.
* ``gfx_font_lv_load_from_binary()`` and ``gfx_font_lv_delete()`` remain public
  for LVGL binary font compatibility.
* ``gfx_label_set_font()`` accepts either a ``gfx_font_t`` or an LVGL font
  pointer and adapts it through the internal font adapter.

Current font rendering rules:

* Text is UTF-8 input and is resolved per Unicode code point.
* Missing printable glyphs render a placeholder box instead of disappearing.
* Control and zero-width code points do not render placeholders.
* FreeType font size is the nominal pixel size requested in
  ``gfx_label_cfg_t.font_size``.
* Scalable FreeType fonts render at the requested pixel size.
* Fixed-size FreeType bitmap fonts select the nearest available strike. The
  selected strike becomes the actual render size, so requesting a size larger
  than the embedded bitmap size does not make text disappear.
* LVGL binary fonts render at their embedded bitmap size. Scaling LVGL bitmap
  fonts by requesting a larger size is not part of the current format contract.

The next stable binary font format should add an explicit GFX header with:

* format magic and version
* nominal pixel size and baseline metrics
* glyph coverage metadata
* bitmap format and stride
* optional kerning table metadata
