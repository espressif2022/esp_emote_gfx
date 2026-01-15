Label (gfx_label)
=================

Types
-----

gfx_font_t
~~~~~~~~~~

.. code-block:: c

   typedef void *gfx_font_t;

gfx_text_align_t
~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_TEXT_ALIGN_AUTO,    /**< Align text auto */
       GFX_TEXT_ALIGN_LEFT,    /**< Align text to left */
       GFX_TEXT_ALIGN_CENTER,  /**< Align text to center */
       GFX_TEXT_ALIGN_RIGHT,   /**< Align text to right */
   } gfx_text_align_t;

gfx_label_long_mode_t
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_LABEL_LONG_WRAP,         /**< Break the long lines (word wrap) */
       GFX_LABEL_LONG_SCROLL,       /**< Make the text scrolling horizontally smoothly */
       GFX_LABEL_LONG_CLIP,         /**< Simply clip the parts which don't fit */
       GFX_LABEL_LONG_SCROLL_SNAP,  /**< Jump to next section after interval (horizontal paging) */
   } gfx_label_long_mode_t;

gfx_label_cfg_t
~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const char *name;       /**< The name of the font file */
       const void *mem;        /**< The pointer to the font file */
       size_t mem_size;        /**< The size of the memory */
       uint16_t font_size;     /**< The size of the font */
   } gfx_label_cfg_t;

Functions
---------

gfx_label_new_font()
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_label_new_font(const gfx_label_cfg_t *cfg, gfx_font_t *ret_font);

gfx_label_delete_font()
~~~~~~~~~~~~~~~~~~~~~~~

Delete a font and free its resources

.. code-block:: c

   esp_err_t gfx_label_delete_font(gfx_font_t font);

**Parameters:**

* ``font`` - Font handle to delete

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_text()
~~~~~~~~~~~~~~~~~~~~

Set the text for a label object

.. code-block:: c

   esp_err_t gfx_label_set_text(gfx_obj_t *obj, const char *text);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``text`` - Text string to display

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_text_fmt()
~~~~~~~~~~~~~~~~~~~~~~~~

Set the text for a label object with format

.. code-block:: c

   esp_err_t gfx_label_set_text_fmt(gfx_obj_t *obj, const char *fmt, ...);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``fmt`` - Format string

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_color()
~~~~~~~~~~~~~~~~~~~~~

Set the color for a label object

.. code-block:: c

   esp_err_t gfx_label_set_color(gfx_obj_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``color`` - Color value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~

Set the background color for a label object

.. code-block:: c

   esp_err_t gfx_label_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``bg_color`` - Background color value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_bg_enable()
~~~~~~~~~~~~~~~~~~~~~~~~~

Enable or disable background for a label object

.. code-block:: c

   esp_err_t gfx_label_set_bg_enable(gfx_obj_t *obj, bool enable);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``enable`` - True to enable background, false to disable

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_opa()
~~~~~~~~~~~~~~~~~~~

Set the opacity for a label object

.. code-block:: c

   esp_err_t gfx_label_set_opa(gfx_obj_t *obj, gfx_opa_t opa);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``opa`` - Opacity value (0-255)

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_font()
~~~~~~~~~~~~~~~~~~~~

Set the font for a label object

.. code-block:: c

   esp_err_t gfx_label_set_font(gfx_obj_t *obj, gfx_font_t font);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``font`` - Font handle

gfx_label_set_text_align()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the text alignment for a label object

.. code-block:: c

   esp_err_t gfx_label_set_text_align(gfx_obj_t *obj, gfx_text_align_t align);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``align`` - Text alignment value

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_long_mode()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the long text mode for a label object

.. code-block:: c

   esp_err_t gfx_label_set_long_mode(gfx_obj_t *obj, gfx_label_long_mode_t long_mode);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``long_mode`` - Long text handling mode (wrap, scroll, or clip)

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_line_spacing()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the line spacing for a label object

.. code-block:: c

   esp_err_t gfx_label_set_line_spacing(gfx_obj_t *obj, uint16_t spacing);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``spacing`` - Line spacing in pixels

**Returns:**

* ESP_OK on success, error code otherwise

gfx_label_set_scroll_speed()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the horizontal scrolling speed for a label object

.. code-block:: c

   esp_err_t gfx_label_set_scroll_speed(gfx_obj_t *obj, uint32_t speed_ms);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``speed_ms`` - Scrolling speed in milliseconds per pixel

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL

gfx_label_set_scroll_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set whether scrolling should loop continuously

.. code-block:: c

   esp_err_t gfx_label_set_scroll_loop(gfx_obj_t *obj, bool loop);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``loop`` - True to enable continuous looping, false for one-time scroll

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL

gfx_label_set_scroll_step()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the scroll step size for a label object

.. code-block:: c

   esp_err_t gfx_label_set_scroll_step(gfx_obj_t *obj, int32_t step);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``step`` - Scroll step size in pixels per timer tick (default: 1, can be negative)

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL

**Note:**

Step cannot be zero

gfx_label_set_snap_interval()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the snap scroll interval time for a label object

.. code-block:: c

   esp_err_t gfx_label_set_snap_interval(gfx_obj_t *obj, uint32_t interval_ms);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``interval_ms`` - Interval time in milliseconds to stay on each section before jumping

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL_SNAP

**Note:**

The jump offset is automatically calculated as the label width

gfx_label_set_snap_loop()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set whether snap scrolling should loop continuously

.. code-block:: c

   esp_err_t gfx_label_set_snap_loop(gfx_obj_t *obj, bool loop);

**Parameters:**

* ``obj`` - Pointer to the label object
* ``loop`` - True to enable continuous looping, false to stop at end

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

Only effective when long_mode is GFX_LABEL_LONG_SCROLL_SNAP
