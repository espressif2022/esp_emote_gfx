ESP Emote GFX Programming Guide
===============================

ESP Emote GFX provides a compact widget and rendering layer for embedded displays.
It includes common UI widgets, animation playback, QR code rendering, touch input,
and path-driven Motion scenes.

Start Here
----------

* :doc:`overview` - what the library provides and how the major modules fit together
* :doc:`quickstart` - minimal setup flow from graphics core to first widget
* :doc:`examples` - curated test app scenarios and where to find them
* :doc:`motion_widget` - Motion scene asset model and playback API
* :doc:`backend_architecture` - display backend model, memory backend, and SDL simulation plan
* :doc:`platform_port` - RTOS/time/heap platform abstraction for ESP-IDF and host ports
* :doc:`asset_spec` - image payload and font asset specifications
* :doc:`arena_stability` - Arena release boundary, render contract, and parity gates

Reference
---------

* :doc:`api/core/index` - core, display, object, timer, touch, and log APIs
* :doc:`api/widgets/index` - widget APIs
* :doc:`motion_mesh_rendering_architecture` - Motion, mesh image, and renderer internals
* `Doxygen API Reference <../doxygen/index.html>`_ - generated C/C++ reference
* :doc:`changelog` - release history

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   quickstart
   examples
   motion_widget
   backend_architecture
   platform_port
   asset_spec
   arena_stability
   api/core/index
   api/widgets/index
   motion_mesh_rendering_architecture
   changelog

Indices and Tables
------------------

* :ref:`genindex`
* :ref:`search`
