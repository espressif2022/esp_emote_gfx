Type Definitions (gfx_types)
============================

Types
-----

gfx_opa_t
~~~~~~~~~

.. code-block:: c

   typedef uint8_t     gfx_opa_t;

gfx_coord_t
~~~~~~~~~~~

.. code-block:: c

   typedef int16_t     gfx_coord_t;

gfx_area_t
~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_coord_t x1;
       gfx_coord_t y1;
       gfx_coord_t x2;
       gfx_coord_t y2;
   } gfx_area_t;

Macros
------

GFX_BUFFER_OFFSET_16BPP()
~~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 16-bit format (RGB565)

.. code-block:: c

   #define GFX_BUFFER_OFFSET_16BPP(buffer, y_offset, stride, x_offset) \

GFX_BUFFER_OFFSET_8BPP()
~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 8-bit format

.. code-block:: c

   #define GFX_BUFFER_OFFSET_8BPP(buffer, y_offset, stride, x_offset) \

GFX_BUFFER_OFFSET_4BPP()
~~~~~~~~~~~~~~~~~~~~~~~~

Calculate buffer pointer with offset for 4-bit format (2 pixels per byte)

.. code-block:: c

   #define GFX_BUFFER_OFFSET_4BPP(buffer, y_offset, stride, x_offset) \

GFX_COLOR_HEX()
~~~~~~~~~~~~~~~

.. code-block:: c

   #define GFX_COLOR_HEX(color) ((gfx_color_t)gfx_color_hex(color))

Functions
---------

gfx_color_hex()
~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_color_t gfx_color_hex(uint32_t c);
