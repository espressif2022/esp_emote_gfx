Arena stability and parity
==========================

This page defines the release boundary for the ARN/GSP Arena path and the
compatibility contract it must satisfy before becoming a stable public UI
runtime. Arena is currently experimental. The ARN1 byte layout is versioned,
but the C runtime structures and APIs are not ABI-frozen.

Runtime paths
-------------

GFX currently has two application paths:

* **Object path**: widgets are ``gfx_object_t`` instances attached to a
  display-owned object tree. Rendering, input capture, resource lifecycle, and
  invalidation are provided by the normal widget runtime.
* **Arena path**: an ARN package is copied into writable RAM and interpreted by
  ``gfx_arena_scene``. Basic nodes draw directly into the render surface;
  Anim/Motion nodes may host normal GFX objects through runtime side tables.

The two paths may share primitive render helpers, decoders, fonts, and image
resources. They are not yet a promise that two same-named components have
identical behavior.

Component parity
----------------

The following table is the minimum parity checklist. A component is ready for
Arena stability only when its Object and Arena rows agree for geometry, style,
resource ownership, input, dirty invalidation, and animation timing.

.. list-table::
   :header-rows: 1
   :widths: 16 24 24 36

   * - Component
     - Object path
     - Arena path
     - Stability gate
   * - Container
     - Stable
     - Package node / direct draw
     - Parent-local geometry and child clipping must match.
   * - Label
     - Stable font adapter and text draw
     - Direct text draw with scene font
     - Wrapping, clipping, fallback glyphs, and baseline must match.
   * - Image
     - Typed source and image resource lifecycle
     - Package blob/path plus runtime binding
     - File, memory, alpha, cache, and release behavior must match.
   * - Button
     - Object press/click callbacks
     - Named action callback and pressed state
     - Press, move, release, cancel, and z-order semantics must match.
   * - List / Wheel
     - Pixel scroll, inertia, overscroll, snap
     - Arena side-table scroll state
     - Selection timing, snap easing, bounds, and dirty areas must match.
   * - Progress
     - Permille value and style
     - Mutable package progress blob
     - Value range, fill geometry, colors, and invalidation must match.
   * - Anim
     - ``gfx_anim`` runtime object
     - Arena hosted animation object
     - Source ownership, play/pause, frame timing, and geometry must match.
   * - Motion
     - ``gfx_motion_player`` runtime object
     - Arena hosted motion player
     - Canvas, action, color, visibility, and timer lifecycle must match.

Rendering contract
------------------

Both paths are required to preserve the following pipeline:

.. code-block:: text

   state/resource change
       -> invalidate affected bounds
       -> traverse visible content in z order
       -> clip to render surface and dirty area
       -> use format-aware software draw or eligible backend operation
       -> software fallback when backend capability/alignment is unsuitable
       -> flush the resulting surface region

Arena direct draw must not bypass clipping, destination-format rules, dirty
area ownership, or backend fallback. Hosted Object nodes must use the same
object lifecycle and must not leave side-table resources after scene detach.

Resource contract
-----------------

Package bytes, decoded resources, runtime objects, and display buffers have
different owners. A future stable Arena API must make these ownership rules
explicit:

* package bytes are immutable input;
* the loaded ARN RAM copy is owned by the scene until detach;
* decoded images and fonts are owned by a resource/cache layer;
* hosted Anim/Motion objects are owned by the Arena scene;
* detach releases every decoded resource and hosted object exactly once.

Input contract
-------------

Before Arena becomes stable, Object and Arena must agree on topmost hit order,
press capture, move/release delivery, cancel behavior, invisible/disabled
nodes, and action callback ownership. Keyboard, encoder, and focus navigation
are not part of the current Arena stability guarantee.

Validation gates
----------------

The release gate is deliberately stronger than a demo opening successfully:

* malformed package tests reject invalid offsets, sizes, versions, and strings;
* host CTest covers direct draw, dirty updates, GSP-to-ARN conversion, SDL,
  and comparison targets;
* parity tests compare Object and Arena output for shared scenes;
* event-sequence tests cover press, drag, release, cancel, and deletion;
* ESP-IDF builds cover at least RGB565 and RGB888 board projects;
* performance reports include RAM, render, and flush time for both paths.

Until the parity and ownership gates are complete, applications should treat
Arena as an experimental package runtime and use the Object path for the
stable widget API.
