Overview
========

ESP Emote GFX is a lightweight graphics library for embedded displays. It
provides a small object model, common widgets, software rendering, animation
playback, and Motion scene support while keeping memory use predictable.

Architecture
------------

The library is organized around a display-owned object tree:

* ``gfx_core`` initializes the graphics context and owns the render/timer runtime.
* ``gfx_disp`` owns display buffers, dirty-region refresh, and the flush callback.
* ``gfx_obj`` provides shared widget behavior such as position, size, alignment,
  visibility, lifecycle, and touch dispatch.
* Widget modules render images, labels, buttons, animations, QR codes, mesh
  images, and Motion scenes.
* Draw modules provide software blending, clipping, image sampling, and mesh
  rasterization.

Widgets
-------

The public widget set currently includes:

* Image: RGB565 and RGB565A8 image descriptors.
* Mesh Image: deformable image grids used directly or by Motion scenes.
* Label: bitmap/LVGL fonts, FreeType fonts, wrapping, clipping, and scrolling.
* Button: text button with pressed state and border styling.
* Animation: EAF playback with segments, looping, mirroring, and frame decoding.
* QR Code: generated QR code widget with configurable size and colors.
* Motion Scene: path-driven emote/character playback from generated scene assets.

Runtime Model
-------------

Applications normally follow this flow:

1. Initialize ``gfx_handle_t`` with ``gfx_emote_init()``.
2. Add one or more displays with ``gfx_disp_add()``.
3. Create widgets on a display.
4. Update widgets from the graphics task, or hold ``gfx_emote_lock()`` when
   updating from another task.
5. Deinitialize the graphics context with ``gfx_emote_deinit()`` when finished.

Memory and Refresh
------------------

Frame buffers can be allocated by the library or supplied by the application.
Displays track dirty areas and only refresh changed regions unless configured
otherwise. Widgets should invalidate themselves when their visual state changes.

Optional Features
-----------------

Some features depend on optional components:

* FreeType support for TTF/OTF fonts.
* JPEG decoding through ``esp_new_jpeg``.
* Touch input through ``esp_lcd_touch``.
* Heatshrink decoding for compressed animation assets.

Next Steps
----------

* Follow :doc:`quickstart` for a minimal setup.
* Browse :doc:`examples` for ready-to-run test app scenarios.
* Read :doc:`motion_widget` for Motion scene usage.
* Use :doc:`api/core/index` and :doc:`api/widgets/index` for API details.
