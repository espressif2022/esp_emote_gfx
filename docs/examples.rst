Examples and validation
=======================

The repository separates reusable demo content, host simulation entry points,
and ESP-IDF validation projects:

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Area
     - Location
     - Purpose
   * - Shared examples
     - ``examples/``
     - Widget scenes, assets, scene-package tools, and board projects.
   * - Host simulation
     - ``simulation/``
     - SDL executables, smoke tests, and host compatibility headers.
   * - Component tests
     - ``test_apps/``
     - Automatic assertions, hardware integration, and visual validation.

Host validation
---------------

From the repository root:

.. code-block:: bash

   cmake -S . -B build-host-sdl -DGFX_BUILD_HOST_SDL=ON -DBUILD_TESTING=ON
   cmake --build build-host-sdl --parallel
   ctest --test-dir build-host-sdl --output-on-failure

Run ``build-host-sdl/gfx_host_sdl_demo`` to open the widget playground. The
default loose-asset root is ``examples/assets/format`` and can be overridden
with the ``GFX_FS_ROOT`` environment variable.

GSP/ARN Arena targets are experimental and opt-in:

.. code-block:: bash

   cmake -S . -B build-host-arena -DGFX_BUILD_HOST_SDL=ON \
     -DBUILD_TESTING=ON -DGFX_BUILD_EXPERIMENTAL_ARENA=ON
   cmake --build build-host-arena --parallel
   ctest --test-dir build-host-arena --output-on-failure

ESP-IDF examples
----------------

The directories under ``examples/esp/`` are standalone projects. The
``format_rgb565`` and ``format_rgb888`` projects run the complete widget
playground. ``arena_demo`` validates the ARN package runtime, while
``ai_scene_pkg`` retains the legacy GSP object-loader path.

Tests
-----

``test_apps/unit`` contains automatic tests without a BSP. ``test_apps`` is
the hardware integration project, and ``test_apps/visual`` contains manual
visual cases. See the repository ``examples/README.md`` and
``test_apps/README.md`` for exact commands and supported board configurations.
