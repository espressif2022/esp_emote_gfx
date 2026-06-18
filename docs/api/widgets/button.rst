Button (button)
===============

Functions
---------

gfx_button_create()
~~~~~~~~~~~~~~~~~~~

Create a button object on a display

.. code-block:: c

   gfx_object_t * gfx_button_create(gfx_display_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_display_add()

**Returns:**

* Pointer to the created button object

gfx_button_set_text()
~~~~~~~~~~~~~~~~~~~~~

Set the label text for a button

.. code-block:: c

   gfx_err_t gfx_button_set_text(gfx_object_t *obj, const char *text);

**Parameters:**

* ``obj`` - Button object
* ``text`` - Text string; NULL is treated as an empty string

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_text_fmt()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the label text for a button using printf-style formatting

.. code-block:: c

   gfx_err_t gfx_button_set_text_fmt(gfx_object_t *obj, const char *fmt, ...);

**Parameters:**

* ``obj`` - Button object
* ``fmt`` - Format string

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_font()
~~~~~~~~~~~~~~~~~~~~~

Set the font used by the button label

.. code-block:: c

   gfx_err_t gfx_button_set_font(gfx_object_t *obj, gfx_font_t font);

**Parameters:**

* ``obj`` - Button object
* ``font`` - Font handle

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_text_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the label text color for a button

.. code-block:: c

   gfx_err_t gfx_button_set_text_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Button object
* ``color`` - Text color

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the normal background color for a button

.. code-block:: c

   gfx_err_t gfx_button_set_bg_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Button object
* ``color`` - Background color

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_bg_color_pressed()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the pressed background color for a button

.. code-block:: c

   gfx_err_t gfx_button_set_bg_color_pressed(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Button object
* ``color`` - Pressed background color

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_border_color()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the border color for a button

.. code-block:: c

   gfx_err_t gfx_button_set_border_color(gfx_object_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Button object
* ``color`` - Border color

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_border_width()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the border width for a button

.. code-block:: c

   gfx_err_t gfx_button_set_border_width(gfx_object_t *obj, uint16_t width);

**Parameters:**

* ``obj`` - Button object
* ``width`` - Border width in pixels; 0 disables the border

**Returns:**

* GFX_OK on success, error code otherwise

gfx_button_set_text_align()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the text alignment for a button label

.. code-block:: c

   gfx_err_t gfx_button_set_text_align(gfx_object_t *obj, gfx_text_align_t align);

**Parameters:**

* ``obj`` - Button object
* ``align`` - Text alignment

**Returns:**

* GFX_OK on success, error code otherwise
