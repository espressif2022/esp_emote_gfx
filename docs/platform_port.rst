Platform Port
=============

GFX keeps RTOS, time, heap, and task primitives behind a small internal platform
layer. Core rendering code should use ``src/platform/gfx_platform.h`` instead of
including FreeRTOS or ESP-IDF runtime headers directly.

Current Scope
-------------

The platform layer covers:

* event bits
* recursive mutexes
* task creation, current-task deletion, and delay
* monotonic microsecond time
* heap allocation with coarse capability flags
* ISR context detection and ISR yield

The ESP-IDF implementation lives in:

.. code-block:: text

   src/platform/esp_idf/gfx_platform_esp_idf.c

The Linux/POSIX host implementation lives in:

.. code-block:: text

   src/platform/linux/gfx_platform_linux.c

The ESP-IDF component CMake excludes the Linux port. Host/simulator CMake
targets should explicitly compile the Linux port instead of the ESP-IDF port.
For host builds that do not use ESP-IDF headers, ``simulation/port/include`` provides
minimal compatibility headers such as ``esp_err.h``.

Core modules such as display, render, timer, log, animation state events, and
software blending no longer include FreeRTOS or ``esp_timer`` directly.

Heap Capability Mapping
-----------------------

Public and internal code should use GFX capability flags:

.. list-table::
   :header-rows: 1

   * - GFX flag
     - ESP-IDF mapping
   * - ``GFX_PLATFORM_HEAP_DEFAULT``
     - ``MALLOC_CAP_DEFAULT``
   * - ``GFX_PLATFORM_HEAP_DMA``
     - ``MALLOC_CAP_DMA``
   * - ``GFX_PLATFORM_HEAP_SPIRAM``
     - ``MALLOC_CAP_SPIRAM``
   * - ``GFX_PLATFORM_HEAP_INTERNAL``
     - ``MALLOC_CAP_INTERNAL``
   * - ``GFX_PLATFORM_HEAP_8BIT``
     - ``MALLOC_CAP_8BIT``

Host / Windows Direction
------------------------

A Linux desktop or WSL simulator can use the POSIX port directly. A native
Windows simulator can either use a Win32 implementation of ``gfx_platform.h`` or
a pthread-compatible layer. The SDL backend under ``src/backend/sdl/`` can then present
the rendered framebuffer without pulling ESP-IDF into the host build.

Linux port smoke test:

.. code-block:: sh

   cc -std=c11 -Wall -Wextra \
     -Iinclude -Isrc -Isimulation/port/include \
     -c src/platform/linux/gfx_platform_linux.c \
     -o /tmp/gfx_platform_linux.o \
     -pthread

Remaining ESP-IDF-specific areas are device-facing ports such as LCD touch and
panel integration. These should become separate input/display adapters rather
than part of core rendering.
