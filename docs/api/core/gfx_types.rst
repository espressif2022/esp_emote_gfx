Type Definitions (gfx_types)
==============================

This module provides basic type definitions and utility macros used throughout the framework.

Types
-----

gfx_opa_t
~~~~~~~~~

Opacity type (0-255).

.. code-block:: c

   typedef uint8_t gfx_opa_t;

gfx_coord_t
~~~~~~~~~~~

Coordinate type.

.. code-block:: c

   typedef int16_t gfx_coord_t;

gfx_color_t
~~~~~~~~~~~

Color type with full member for compatibility.

.. code-block:: c

   typedef union {
       uint16_t full;  ///< Full 16-bit color value
   } gfx_color_t;

gfx_area_t
~~~~~~~~~~

Area structure defining a rectangular region.

.. code-block:: c

   typedef struct {
       gfx_coord_t x1;  ///< Left coordinate
       gfx_coord_t y1;  ///< Top coordinate
       gfx_coord_t x2;  ///< Right coordinate
       gfx_coord_t y2;  ///< Bottom coordinate
   } gfx_area_t;

Functions
---------

gfx_color_hex()
~~~~~~~~~~~~~~~

Convert a 32-bit hexadecimal color to gfx_color_t.

.. code-block:: c

   gfx_color_t gfx_color_hex(uint32_t c);

**Parameters:**

* ``c`` - The 32-bit hexadecimal color to convert (e.g., 0xFF0000 for red)

**Returns:**

* Converted color in gfx_color_t type

**Example:**

.. code-block:: c

   gfx_color_t red = gfx_color_hex(0xFF0000);
   gfx_color_t green = gfx_color_hex(0x00FF00);
   gfx_color_t blue = gfx_color_hex(0x0000FF);

Macros
------

Pixel Size Constants
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define GFX_PIXEL_SIZE_16BPP   2  ///< 16-bit color format: 2 bytes per pixel
   #define GFX_PIXEL_SIZE_8BPP    1  ///< 8-bit format: 1 byte per pixel

Buffer Offset Macros
~~~~~~~~~~~~~~~~~~~~

GFX_BUFFER_OFFSET_16BPP()
~~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 16-bit format (RGB565).

.. code-block:: c

   #define GFX_BUFFER_OFFSET_16BPP(buffer, y_offset, stride, x_offset)

**Parameters:**

* ``buffer`` - Base buffer pointer (any type)
* ``y_offset`` - Vertical offset in pixels
* ``stride`` - Width of buffer in pixels
* ``x_offset`` - Horizontal offset in pixels

**Returns:**

* Calculated gfx_color_t pointer with offset applied

GFX_BUFFER_OFFSET_8BPP()
~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 8-bit format.

.. code-block:: c

   #define GFX_BUFFER_OFFSET_8BPP(buffer, y_offset, stride, x_offset)

**Parameters:**

* ``buffer`` - Base buffer pointer (any type)
* ``y_offset`` - Vertical offset in pixels
* ``stride`` - Width of buffer in pixels
* ``x_offset`` - Horizontal offset in pixels

**Returns:**

* Calculated uint8_t pointer with offset applied

GFX_BUFFER_OFFSET_4BPP()
~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 4-bit format (2 pixels per byte).

.. code-block:: c

   #define GFX_BUFFER_OFFSET_4BPP(buffer, y_offset, stride, x_offset)

**Parameters:**

* ``buffer`` - Base buffer pointer (any type)
* ``y_offset`` - Vertical offset in pixels
* ``stride`` - Width of buffer in pixels (will be divided by 2)
* ``x_offset`` - Horizontal offset in pixels (will be divided by 2)

**Returns:**

* Calculated uint8_t pointer with offset applied

GFX_COLOR_HEX()
~~~~~~~~~~~~~~~

Macro wrapper for gfx_color_hex().

.. code-block:: c

   #define GFX_COLOR_HEX(color) ((gfx_color_t)gfx_color_hex(color))

**Example:**

.. code-block:: c

   gfx_color_t red = GFX_COLOR_HEX(0xFF0000);

