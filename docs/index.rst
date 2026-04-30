ESP Emote GFX Documentation
===========================

Welcome to the ESP Emote GFX API documentation. This is a lightweight graphics framework for ESP-IDF with support for images, labels, animations, buttons, QR codes, fonts, and path-driven motion scenes.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   quickstart
   motion_widget
   api/core/index
   api/widgets/index
   examples
   changelog

Overview
--------

ESP Emote GFX is a graphics framework designed for embedded systems, providing:

* **Images**: Display images in RGB565A8 format with alpha transparency
* **Animations**: GIF animations with ESP32 tools (EAF format)
* **Buttons**: Interactive button widgets with text, border, and pressed-state styling
* **Motion Scenes**: Path-based articulated widgets using joints, poses, actions, and mesh segments
* **Fonts**: LVGL fonts and FreeType TTF/OTF support
* **Timers**: Built-in timing system for smooth animations
* **Memory Optimized**: Designed for embedded systems with limited resources

Features
--------

* Lightweight and memory-efficient
* Thread-safe operations with mutex locking
* Support for multiple object types (images, labels, animations, buttons, QR codes, motion scenes)
* Flexible buffer management (internal or external buffers)
* Rich text rendering with scrolling and wrapping
* Animation playback control with segments and loops
* Path-driven character playback with touch-friendly scene runtime

Quick Links
-----------

* :doc:`Quick Start Guide <quickstart>`
* :doc:`Motion Widget Guide <motion_widget>`
* :doc:`Core API Reference <api/core/index>`
* :doc:`Widget API Reference <api/widgets/index>`
* :doc:`Examples <examples>`
* `Doxygen API Reference <../doxygen/index.html>`_ - Auto-generated C/C++ API documentation

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
