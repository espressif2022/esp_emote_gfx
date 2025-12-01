Overview
========

ESP Emote GFX is a lightweight graphics framework for ESP-IDF that provides a simple yet powerful API for rendering graphics on embedded displays. It is designed with memory efficiency and performance in mind, making it ideal for resource-constrained embedded systems.

Architecture
-----------

The framework is built around a core object system where all graphical elements (images, labels, animations, QR codes) are treated as objects. These objects share common properties like position, size, visibility, and alignment.

Core Components
---------------

Core System
~~~~~~~~~~~

The core system (`gfx_core`) manages:

* Graphics context initialization and deinitialization
* Buffer management (internal or external)
* Rendering pipeline
* Thread safety with mutex locking
* Screen refresh and invalidation

Object System
~~~~~~~~~~~~~

The object system (`gfx_obj`) provides:

* Base object structure for all graphical elements
* Position and size management
* Alignment system (similar to LVGL)
* Visibility control
* Object lifecycle management

Timer System
~~~~~~~~~~~~

The timer system (`gfx_timer`) provides:

* High-resolution timers for animations
* Callback-based timer events
* Repeat count and period control
* System tick management

Widgets
-------

Image Widget
~~~~~~~~~~~~

The image widget supports:

* RGB565 format (16-bit color)
* RGB565A8 format (16-bit color with 8-bit alpha)
* C array and binary formats
* Automatic format detection

Label Widget
~~~~~~~~~~~~

The label widget provides:

* Text rendering with multiple font formats
* LVGL font support
* FreeType TTF/OTF font support
* Text alignment (left, center, right)
* Long text handling (wrap, scroll, clip)
* Background colors and opacity

Animation Widget
~~~~~~~~~~~~~~~~

The animation widget supports:

* EAF (ESP Animation Format) files
* Frame-by-frame playback control
* Segment playback (start/end frames)
* FPS control
* Loop and repeat options
* Mirror effects

QR Code Widget
~~~~~~~~~~~~~~

The QR code widget provides:

* Dynamic QR code generation
* Configurable size and error correction
* Custom foreground and background colors

Memory Management
-----------------

The framework supports two buffer management modes:

Internal Buffers
~~~~~~~~~~~~~~~~

The framework automatically allocates and manages frame buffers internally. This is the simplest mode but requires sufficient heap memory.

External Buffers
~~~~~~~~~~~~~~~~

You can provide your own buffers, allowing you to:

* Use memory-mapped regions
* Control buffer placement (SRAM, SPIRAM, etc.)
* Optimize for specific memory constraints

Thread Safety
-------------

All widget operations should be performed within a graphics lock to ensure thread safety:

.. code-block:: c

   gfx_emote_lock(handle);
   // Perform operations
   gfx_obj_set_pos(obj, x, y);
   gfx_label_set_text(label, "New text");
   gfx_emote_unlock(handle);

Dependencies
------------

* ESP-IDF 5.0 or higher
* FreeType (for TTF/OTF font support)
* ESP New JPEG (for JPEG decoding)

License
-------

This project is licensed under the Apache License 2.0.

