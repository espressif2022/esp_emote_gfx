Rendering and Performance
=========================

This page explains how ESP Emote GFX renders a frame, how dirty-area refresh works,
and which configuration knobs have the biggest effect on memory usage and latency.

Rendering Pipeline
------------------

At a high level, one frame is processed in four stages:

1. Object update: widgets with update hooks advance their internal state.
2. Layout and invalidation merge: dirty areas are merged into a smaller set of redraw regions.
3. Software rendering: the framework draws each visible object into the active draw buffer.
4. Flush and synchronization: each rendered chunk is passed to the display flush callback and
   the render task waits for ``gfx_disp_flush_ready()``.

The public display API involved in this pipeline lives in :doc:`api/core/gfx_disp`.
The implementation is centered in ``src/core/display/gfx_render.c``, which splits a dirty
area into chunks according to the configured buffer size.

Dirty-Area Refresh Model
------------------------

ESP Emote GFX does not redraw the whole screen on every tick by default. It tracks dirty areas
and redraws only the regions affected by widget updates.

Relevant behavior:

* ``gfx_obj_set_pos()``, text changes, animation frame changes, and layout updates invalidate object regions.
* Dirty areas are merged before rendering to reduce redundant work.
* ``gfx_disp_refresh_all()`` forces a full-screen invalidation when a complete redraw is needed.

This model is the main reason the framework stays usable on small MCUs even when software blending
and alpha images are enabled.

Buffer Modes
------------

The display configuration in ``gfx_disp_config_t`` exposes three practical buffer strategies:

Partial Buffer
~~~~~~~~~~~~~~

Use a smaller draw buffer by setting ``buffers.buf_pixels`` to only a subset of the screen.

When to use it:

* RAM is limited.
* The panel IO bandwidth is acceptable.
* Typical UI changes are localized.

Tradeoff:

* Lower memory usage, but the framework may split a dirty area into many flush operations.

Double Buffer
~~~~~~~~~~~~~

Enable ``flags.double_buffer`` to allocate a second draw buffer when internal allocation is used,
or provide ``buf1`` and ``buf2`` manually.

When to use it:

* You want rendering and flush phases to interfere less with each other.
* The panel path benefits from buffer flipping.

Tradeoff:

* Higher RAM usage, but fewer stalls and better throughput in many display pipelines.

Full-Frame Buffer
~~~~~~~~~~~~~~~~~

Enable ``flags.full_frame`` when ``buf1`` and ``buf2`` are full-screen framebuffers.

When to use it:

* RGB panel or scanout hardware expects a complete frame buffer.
* You want the inactive framebuffer to preserve previous screen content.

Tradeoff:

* Highest memory cost, but simplest integration for true framebuffer-style panels.

In full-frame mode, the renderer can synchronize untouched regions from the inactive buffer before
the next frame is displayed. This reduces visual corruption when only part of the screen is redrawn.

Flush Path and Synchronization
------------------------------

The display flush callback is responsible for transferring rendered pixels to the panel or panel IO
layer. The render task blocks until the driver signals completion by calling
``gfx_disp_flush_ready(disp, swap_act_buf)``.

Practical guidance:

* Always register the panel-IO completion callback when the underlying driver is asynchronous.
* Do not delay ``gfx_disp_flush_ready()`` longer than necessary, because the render task is waiting.
* If the display hardware or BSP already owns the framebuffer, validate whether ``full_frame`` mode
  is a better fit than repeatedly flushing small chunks.

Performance Counters
--------------------

``gfx_disp_get_perf_stats()`` exposes frame-level statistics:

* ``dirty_pixels``: total pixels rendered in the latest frame.
* ``frame_time_us``: total frame time.
* ``render_time_us``: time spent drawing into buffers.
* ``flush_time_us``: time spent waiting for panel flush completion.
* ``flush_count``: number of flush calls used for the frame.
* ``blend.*``: counters for fill, color draw, image draw, and triangle draw stages.

How to interpret them:

* High ``flush_time_us`` with low ``render_time_us`` usually means the panel bus is the bottleneck.
* High ``flush_count`` often indicates the draw buffer is too small for the dirty regions you produce.
* High ``dirty_pixels`` during simple UI interactions usually means widgets invalidate too much area.

Tuning Checklist
----------------

If the UI feels slow or frame rate drops unexpectedly, check these items first:

* Increase ``buffers.buf_pixels`` to reduce chunking overhead.
* Move buffers to DMA-capable memory with ``flags.buff_dma`` when the panel path benefits from it.
* Use ``flags.buff_spiram`` only when external RAM bandwidth is acceptable for the target board.
* Enable ``double_buffer`` for workloads with long flush latency.
* Reduce unnecessary full-screen invalidations in app code.
* For text-heavy screens, verify scrolling labels and font rendering are not invalidating more area than needed.

Board Integration Notes
-----------------------

Two integration patterns show up frequently in this repository:

* SPI / command-mode panels: usually pair well with partial buffers and explicit flush completion callbacks.
* RGB / framebuffer-oriented panels: often benefit from full-frame buffers and double buffering.

The best configuration depends on available SRAM/PSRAM, panel bus bandwidth, and whether the BSP
already provides a stable framebuffer lifecycle.
