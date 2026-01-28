ESP Emote GFX Documentation
============================

Welcome to the ESP Emote GFX API documentation. This is a lightweight graphics framework for ESP-IDF with support for images, labels, animations, and fonts.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   quickstart
   api/core/index
   api/widgets/index
   examples
   changelog

Overview
--------

ESP Emote GFX is a graphics framework designed for embedded systems, providing:

* **Images**: Display images in RGB565A8 format with alpha transparency
* **Animations**: GIF animations with ESP32 tools (EAF format)
* **Fonts**: LVGL fonts and FreeType TTF/OTF support
* **Timers**: Built-in timing system for smooth animations
* **Memory Optimized**: Designed for embedded systems with limited resources

Features
--------

* Lightweight and memory-efficient
* Thread-safe operations with mutex locking
* Support for multiple object types (images, labels, animations, QR codes)
* Flexible buffer management (internal or external buffers)
* Rich text rendering with scrolling and wrapping
* Animation playback control with segments and loops

Quick Links
-----------

* :doc:`Quick Start Guide <quickstart>`
* :doc:`Core API Reference <api/core/index>`
* :doc:`Widget API Reference <api/widgets/index>`
* :doc:`Examples <examples>`
* `Doxygen API Reference <doxygen/index.html>`_ - Auto-generated C/C++ API documentation

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

