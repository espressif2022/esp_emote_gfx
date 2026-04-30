Motion, Mesh Image, and Rendering Architecture
==============================================

Purpose
-------

This document explains how the motion scene stack is split today and where
future optimization work should happen. It focuses on these files:

* ``src/widget/motion/gfx_motion_scene.c``
* ``src/widget/motion/gfx_motion_player.c``
* ``src/widget/motion/gfx_motion_primitives.c``
* ``src/widget/motion/gfx_motion_style.c``
* ``src/widget/img/gfx_mesh_img.c``
* ``src/core/draw/gfx_blend.c``

The key design rule is that scene playback, segment-to-mesh conversion, mesh
image drawing, and low-level blending are separate layers. Each layer owns one
kind of state and should not reach across the boundary unless it is exposing a
small, reusable API.

High-Level Flow
---------------

The runtime path is:

.. code-block:: text

   gfx_motion_asset_t
       |
       v
   gfx_motion_scene.c
       validate asset, own action timeline, update pose_cur/pose_tgt
       |
       v
   gfx_motion_player.c
       own mesh objects, callbacks, canvas mapping, segment dispatch
       |
       +--> gfx_motion_primitives.c
       |       generate capsule/ring/bezier mesh geometry
       |
       +--> gfx_motion_style.c
               bind palette/resource/opacity/layer/UV style
       |
       v
   gfx_mesh_img.c
       own mesh state, source image, UV/rest points, bounds,
       draw each mesh cell or scanline-filled polygon
       |
       v
   gfx_blend.c
       triangle rasterization, image sampling, polygon fill, AA, clipping
       |
       v
   display buffer

Layer Ownership
---------------

``gfx_motion_scene.c`` is the parser and timeline layer.

It owns:

* Asset validation.
* Pose state: ``pose_cur`` and ``pose_tgt``.
* Active action, active step, step ticks, loop override.
* Interpolation policy such as ``HOLD`` and ``DAMPED``.
* Facing/mirroring when loading a target pose.

It must not own:

* Display objects.
* Mesh grids.
* Pixel colors or image descriptors.
* Rendering decisions such as scanline fill or triangle fallback.

``gfx_motion_player.c`` is the presentation adapter for motion scenes.

It owns:

* One ``gfx_mesh_img`` object per segment.
* Mapping from design-space scene coordinates into the destination canvas.
* Per-segment grid setup and cached grid size.
* Motion driver callbacks that connect ``gfx_motion_t`` to the scene and mesh
  objects.
* Dispatching each segment to the primitive and style helpers.

It must not own:

* Asset timeline rules beyond calling ``gfx_motion_scene_*``.
* Generic mesh drawing.
* Low-level triangle rasterization or polygon fill.
* Primitive geometry algorithms or style/resource binding details.

``gfx_motion_primitives.c`` is the motion geometry algorithm layer.

It owns:

* Segment tessellation for capsule, ring, Bezier stroke, and Bezier fill.
* Primitive-local scratch usage through ``gfx_motion_player_runtime_scratch_t``.
* Cubic Bezier position/tangent evaluation.
* Stroke extrusion and fill mesh generation.

It must not own:

* Action playback.
* Display object lifetime.
* Palette/resource binding.
* Generic mesh drawing internals.

``gfx_motion_style.c`` is the motion style/resource binding layer.

It owns:

* Runtime solid color, palette color, opacity, texture source, UV crop, and
  layer visibility helpers.
* Resource UV mapping into mesh ``rest_points``.
* Binding the correct image source for each segment.

It must not own:

* Primitive geometry.
* Action playback.
* Mesh cell rasterization.

``gfx_mesh_img.c`` is the generic deformable image widget.

It owns:

* The current mesh grid size and point count.
* ``points``: current object-local mesh geometry.
* ``rest_points``: reference UV/sample coordinates for the source image.
* Source image descriptor and decoded image header.
* Object bounds derived from current points.
* Mesh options such as column wrapping, inward AA, opacity, control-point debug
  drawing, and scanline fill.
* Drawing by splitting each mesh cell into triangles, or by using scanline
  polygon fill for solid filled shapes.

It must not own:

* Motion scene semantics.
* Segment kinds such as capsule or Bezier.
* Action playback or pose interpolation.

``gfx_blend.c`` is the software raster backend.

It owns:

* Image triangle drawing with source UVs.
* Polygon fill coverage.
* Clipping against buffer and object clip areas.
* Alpha blending and RGB565 byte swap handling.
* Anti-aliasing policy for primitive edges.
* Chunking wide polygon coverage so large filled shapes do not disappear.

It must not own:

* Widget state.
* Mesh object layout.
* Motion-specific assumptions.

Scene Asset Model
-----------------

``gfx_motion_asset_t`` is the ROM-side bundle consumed by the runtime. It is
defined in ``include/widget/gfx_motion_scene.h`` and contains:

* ``meta``: schema version and design-space viewbox.
* ``joint_names`` and ``joint_count``: named control points.
* ``segments``: visual primitives referencing joints.
* ``poses``: complete joint coordinate snapshots.
* ``actions``: step sequences that select target poses.
* ``sequence``: optional default playback sequence.
* ``layout``: default stroke, mirror axis, timing, and damping hints.
* ``resources``: optional texture images with UV crop.
* ``color_palette``: optional fixed segment colors.

The scene layer validates structural invariants early:

* Viewbox dimensions must be positive.
* Joint, pose, action, and sequence pointers must match their counts.
* Segment joint ranges must stay within ``joint_count``.
* Bezier control counts must satisfy ``3k + 1``.
* Resource and palette indices must resolve.
* Resource UV crop must fit inside the image descriptor.
* Layer bits must be within the 32-bit layer mask range.

Playback Model
--------------

``gfx_motion_scene_init()`` validates the asset and initializes the first action
step. The scene starts with ``pose_cur`` snapped to ``pose_tgt``.

``gfx_motion_scene_advance()`` advances the action timeline. It updates the
active step when ``hold_ticks`` expires, loads the new target pose, and applies
the step's interpolation policy.

``gfx_motion_scene_tick()`` eases ``pose_cur`` toward ``pose_tgt`` for damped
steps. The function returns whether coordinates changed. The player uses that
signal, plus dirty flags, to decide whether to update mesh objects.

``GFX_MOTION_INTERP_HOLD`` means snap immediately to the target pose. It is
used both on action switch and on step advance.

Player Segment Pipeline
-----------------------

``gfx_motion_player_init()`` creates one ``gfx_mesh_img`` object per segment.
The initial grid is chosen from the segment kind:

* Capsule: ``1 x 1`` grid, four points.
* Ring: ``N x 1`` wrapped grid, two point rows.
* Bezier strip: sampled curve columns, non-wrapped grid.
* Bezier loop: sampled curve columns, wrapped grid.
* Bezier fill: preset eye/ellipse grid or generic closed-loop rim grid.

On each motion apply callback, the player:

1. Checks whether the scene or mesh is dirty.
2. Converts needed joints from design space into screen space.
3. Computes stroke width and radius in screen pixels.
4. Applies the matching segment primitive into the mesh object.
5. Sets object visibility from the layer mask.
6. Clears dirty flags after all visible segments have been updated.

Primitive conversion details in ``gfx_motion_primitives.c``:

* Capsule computes a thick rectangle aligned with the segment direction.
* Ring computes outer and inner circular point rows and enables wrapped columns.
* Bezier stroke evaluates cubic position and analytic tangent, then extrudes
  left/right normals into two mesh rows.
* Bezier fill either uses an eye/ellipse preset path or builds a hub/rim mesh
  for generic closed loops.

Styling and Resources
---------------------

``gfx_motion_style.c`` binds image sources in this priority order:

1. ``resource_idx`` texture image.
2. ``color_idx`` palette 1x1 image.
3. Runtime solid 1x1 image.

For texture resources, ``uv_x``, ``uv_y``, ``uv_w``, and ``uv_h`` are mapped
into mesh ``rest_points``. The mesh's current ``points`` still describe screen
geometry; ``rest_points`` describe where to sample the source image. This keeps
UV crop generic and lets the same mesh renderer draw both full-image and
cropped-resource segments.

For palette and runtime solid colors, the source is a 1x1 RGB565 image. Filled
Bezier segments may additionally enable scanline fill so solid closed shapes do
not need to be rasterized through textured triangles.

Mesh Image Model
----------------

``gfx_mesh_img`` stores two point arrays:

* ``points`` are object-local Q8 geometry coordinates.
* ``rest_points`` are object-local Q8 source sampling coordinates.

For a plain image, both arrays start as a regular grid over the image. For
motion segments, the player continuously updates ``points`` while ``rest_points``
remain the source UV reference. Texture crop updates only ``rest_points``.

When ``points`` change, ``gfx_mesh_img_update_bounds()`` recalculates the object
bounding box. The draw origin is derived from the object position minus the
minimum mesh bound. This allows meshes with negative local coordinates while
still drawing through the normal object geometry system.

Important mesh options:

* ``wrap_cols`` connects the last column back to the first. Rings and closed
  Bezier loops need this.
* ``aa_inward`` makes edge AA fade inward to avoid halos on thin strokes.
* ``scanline_fill`` bypasses textured triangle drawing for solid filled
  polygons when possible.
* ``opacity`` applies uniform per-segment alpha.

Draw Pipeline
-------------

``gfx_mesh_img_draw()`` opens the image decoder, resolves RGB565 or RGB565A8
payloads, computes the clipped object area, and chooses one of two paths.

Scanline fill path:

* Used for selected solid filled polygons.
* Builds a polygon from mesh points.
* Calls ``gfx_sw_blend_polygon_fill()``.
* Falls back to triangle drawing if the scanline scratch capacity is too small.

Triangle path:

* Iterates every mesh cell.
* Builds four vertices with screen position and source UV.
* Splits the quad into two triangles.
* Chooses the shorter diagonal to reduce cracks on deformed quads.
* Marks internal edges so AA does not darken shared seams.
* Calls ``gfx_sw_blend_img_triangle_draw()`` twice per cell.

Low-Level Drawing
-----------------

``gfx_sw_blend_img_triangle_draw()`` samples the source image inside a triangle
and blends into the destination buffer. It handles:

* Screen clipping.
* Source UV interpolation.
* RGB565/RGB565A8 source alpha.
* Uniform opacity.
* Internal edge suppression.
* Optional inward AA.

``gfx_sw_blend_polygon_fill()`` fills a polygon with a solid color. It clips to
the destination buffer, computes per-pixel coverage, and chunks wide polygons
across X so coverage scratch memory stays bounded.

Module Boundary Assessment
--------------------------

The current module split is intentionally layered:

* ``scene.c`` is pure playback state.
* ``player.c`` is motion-scene runtime orchestration.
* ``primitives.c`` is motion geometry generation.
* ``style.c`` is motion style/resource binding.
* ``mesh_img.c`` is reusable deformable image infrastructure.
* ``gfx_blend.c`` is low-level rasterization.

The main area to watch from here is whether primitive APIs stabilize. If more
primitive families are added, keep them in ``gfx_motion_primitives.c`` until the
file itself becomes too large; only then split by primitive family.

Optimization Guide
------------------

Useful optimization entry points:

* Player dirty flags: avoid applying meshes when pose and canvas are unchanged.
* Cached segment grids: avoid reallocating mesh points in hot paths.
* Bezier sampling density: tune stroke and fill samples separately.
* Resource UV updates: avoid recomputing rest points unless grid or resource
  crop changes.
* Mesh bounds: keep clamping warnings visible because excessive bounds can hide
  real coordinate bugs.
* Scanline fill: prefer it for solid large fills; keep triangle fallback for
  textured or unsupported cases.
* Blend chunk width: increase only if stack/static scratch budget allows it.
* Layer mask: hide inactive segment groups before tessellation if many layers
  become common.

Testing Checklist
-----------------

When changing this stack, test these cases:

* Empty segment assets still initialize and deinitialize safely.
* ``HOLD`` actions snap immediately on init, action switch, and step advance.
* Palette segments are not overwritten by ``gfx_motion_player_set_color()``.
* Texture segments respect resource UV crop.
* Ring grid changes do not lose UV crop.
* Layer mask hides and restores segment visibility.
* Bezier strokes do not show dashed/bowtie artifacts on tight curves.
* Oversized scanline fills fall back to triangle rendering instead of blanking.
* Wide polygon fills render in chunks instead of returning early.
* Mesh allocation failure preserves the previous grid when possible.
* Bounds that exceed geometry range are clamped and logged.

Change Guidelines
-----------------

Use these rules when iterating:

* Put timeline or action behavior in ``gfx_motion_scene.c``.
* Put segment-to-mesh conversion in ``gfx_motion_primitives.c``.
* Put palette/resource/layer/opacity handling in ``gfx_motion_style.c``.
* Put generic mesh storage, UV, bounds, and draw dispatch in ``gfx_mesh_img.c``.
* Put pixel coverage, sampling, AA, and blend math in ``gfx_blend.c``.
* Keep public structs in ``include/widget/gfx_motion_scene.h`` stable where
  possible because generated assets depend on them.
* Validate asset mistakes in the scene layer rather than letting the player or
  renderer fail later.
* Keep renderer fallbacks visible through logs instead of silently drawing
  nothing.
