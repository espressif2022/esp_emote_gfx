Label Widget (gfx_label)
==========================

The label widget provides text rendering capabilities with support for multiple font formats, text alignment, and long text handling.

Types
-----

gfx_font_t
~~~~~~~~~~

Font handle type (hides internal implementation).

.. code-block:: c

   typedef void *gfx_font_t;

gfx_label_cfg_t
~~~~~~~~~~~~~~~

Font configuration structure.

.. code-block:: c

   typedef struct {
       const char *name;       ///< The name of the font file
       const void *mem;        ///< The pointer to the font file
       size_t mem_size;        ///< The size of the memory
       uint16_t font_size;     ///< The size of the font
   } gfx_label_cfg_t;

gfx_text_align_t
~~~~~~~~~~~~~~~~

Text alignment enumeration.

.. code-block:: c

   typedef enum {
       GFX_TEXT_ALIGN_AUTO,    ///< Align text auto
       GFX_TEXT_ALIGN_LEFT,     ///< Align text to left
       GFX_TEXT_ALIGN_CENTER,   ///< Align text to center
       GFX_TEXT_ALIGN_RIGHT,    ///< Align text to right
   } gfx_text_align_t;

gfx_label_long_mode_t
~~~~~~~~~~~~~~~~~~~~~

Long text mode enumeration.

.. code-block:: c

   typedef enum {
       GFX_LABEL_LONG_WRAP,         ///< Break the long lines (word wrap)
       GFX_LABEL_LONG_SCROLL,       ///< Make the text scrolling horizontally smoothly
       GFX_LABEL_LONG_CLIP,         ///< Simply clip the parts which don't fit
       GFX_LABEL_LONG_SCROLL_SNAP,  ///< Jump to next section after interval
   } gfx_label_long_mode_t;

Functions
---------

Object Creation
~~~~~~~~~~~~~~~

gfx_label_create()
~~~~~~~~~~~~~~~~~~

Create a label object.

.. code-block:: c

   gfx_obj_t *gfx_label_create(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Animation player handle

**Returns:**

* Pointer to the created label object, NULL on error

Font Management
~~~~~~~~~~~~~~~

gfx_label_new_font()
~~~~~~~~~~~~~~~~~~~~

Create a new font (FreeType only, requires CONFIG_GFX_FONT_FREETYPE_SUPPORT).

.. code-block:: c

   esp_err_t gfx_label_new_font(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font);

**Parameters:**

* ``cfg`` - Font configuration
* ``ret_font`` - Pointer to store the font handle

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   gfx_label_cfg_t font_cfg = {
       .name = "DejaVuSans.ttf",
       .mem = font_data,
       .mem_size = font_size,
       .font_size = 20,
   };
   gfx_font_t font;
   gfx_label_new_font(&font_cfg, &font);
   gfx_label_set_font(label, font);

gfx_label_delete_font()
~~~~~~~~~~~~~~~~~~~~~~~~

Delete a font and free its resources (FreeType only).

.. code-block:: c

   esp_err_t gfx_label_delete_font(gfx_font_t font);

**Parameters:**

* ``font`` - Font handle to delete

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_font()
~~~~~~~~~~~~~~~~~~~~

Set the font for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``font`` - Font handle (LVGL font pointer or FreeType font handle)

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Using LVGL font
   extern const lv_font_t font_puhui_16_4;
   gfx_label_set_font(label, (gfx_font_t)&font_puhui_16_4);

   // Using FreeType font
   gfx_label_set_font(label, freetype_font);

Text Operations
~~~~~~~~~~~~~~~

gfx_label_set_text()
~~~~~~~~~~~~~~~~~~~~

Set the text for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_text(gfx_obj_t *obj, const char *text);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``text`` - Text string to display

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_text_fmt()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the text for a label object with format string.

.. code-block:: c

   esp_err_t gfx_label_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``fmt`` - Format string (printf-style)
* ``...`` - Variable arguments

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   gfx_label_set_text_fmt(label, "Count: %d, Value: %.2f", 42, 3.14);

Styling
~~~~~~~

gfx_label_set_color()
~~~~~~~~~~~~~~~~~~~~~

Set the text color for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``color`` - Color value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the background color for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``bg_color`` - Background color value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_bg_enable()
~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable background for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``enable`` - True to enable background, false to disable

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_opa()
~~~~~~~~~~~~~~~~~~~

Set the opacity for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_opa(gfx_obj_t *obj, gfx_opa_t opa);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``opa`` - Opacity value (0-255)

**Returns:**

* ESP_OK on success, error code otherwise

Text Layout
~~~~~~~~~~~

gfx_label_set_text_align()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the text alignment for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``align`` - Text alignment value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_long_mode()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the long text mode for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_long_mode(gfx_obj_t *obj, gfx_label_long_mode_t long_mode);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``long_mode`` - Long text handling mode

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Wrap long text
   gfx_label_set_long_mode(label, GFX_LABEL_LONG_WRAP);

   // Scroll long text
   gfx_label_set_long_mode(label, GFX_LABEL_LONG_SCROLL);

   // Clip long text
   gfx_label_set_long_mode(label, GFX_LABEL_LONG_CLIP);

gfx_label_set_line_spacing()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the line spacing for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``spacing`` - Line spacing in pixels

**Returns:**

* ESP_OK on success, error code otherwise

Scrolling Configuration
~~~~~~~~~~~~~~~~~~~~~~~

gfx_label_set_scroll_speed()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the horizontal scrolling speed for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``speed_ms`` - Scrolling speed in milliseconds per pixel

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL.

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_scroll_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set whether scrolling should loop continuously.

.. code-block:: c

   esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``loop`` - True to enable continuous looping, false for one-time scroll

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL.

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_scroll_step()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the scroll step size for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_scroll_step(gfx_obj_t *obj, int32_t step);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``step`` - Scroll step size in pixels per timer tick (default: 1, can be negative)

**Note:**

* Only effective when long_mode is GFX_LABEL_LONG_SCROLL
* Step cannot be zero

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_snap_interval()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the snap scroll interval time for a label object.

.. code-block:: c

   esp_err_t gfx_label_set_snap_interval(gfx_obj_t *obj, uint32_t interval_ms);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``interval_ms`` - Interval time in milliseconds to stay on each section before jumping

**Note:**

* Only effective when long_mode is GFX_LABEL_LONG_SCROLL_SNAP
* The jump offset is automatically calculated as the label width

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_snap_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set whether snap scrolling should loop continuously.

.. code-block:: c

   esp_err_t gfx_label_set_snap_loop(gfx_obj_t *obj, bool loop);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``loop`` - True to enable continuous looping, false to stop at end

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL_SNAP.

**Returns:**

* ESP_OK on success, error code otherwise

