QR Code Widget (gfx_qrcode)
============================

The QR code widget generates and displays QR codes dynamically.

Types
-----

gfx_qrcode_ecc_t
~~~~~~~~~~~~~~~~

QR Code error correction level.

.. code-block:: c

   typedef enum {
       GFX_QRCODE_ECC_LOW = 0,      ///< ~7% error tolerance
       GFX_QRCODE_ECC_MEDIUM,        ///< ~15% error tolerance
       GFX_QRCODE_ECC_QUARTILE,      ///< ~25% error tolerance
       GFX_QRCODE_ECC_HIGH           ///< ~30% error tolerance
   } gfx_qrcode_ecc_t;

Functions
---------

Object Creation
~~~~~~~~~~~~~~~

gfx_qrcode_create()
~~~~~~~~~~~~~~~~~~~

Create a QR Code object.

.. code-block:: c

   gfx_obj_t *gfx_qrcode_create(gfx_handle_t handle);

**Parameters:**

* ``handle`` - Animation player handle

**Returns:**

* Pointer to the created QR Code object, NULL on error

QR Code Operations
~~~~~~~~~~~~~~~~~~

gfx_qrcode_set_data()
~~~~~~~~~~~~~~~~~~~~~

Set the data/text for a QR Code object.

.. code-block:: c

   esp_err_t gfx_qrcode_set_data(gfx_obj_t *obj, const char *data);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``data`` - Pointer to the null-terminated string to encode

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

The length is automatically calculated using strlen().

**Example:**

.. code-block:: c

   gfx_obj_t *qrcode = gfx_qrcode_create(handle);
   gfx_qrcode_set_data(qrcode, "https://www.espressif.com");

gfx_qrcode_set_size()
~~~~~~~~~~~~~~~~~~~~~

Set the size for a QR Code object.

.. code-block:: c

   esp_err_t gfx_qrcode_set_size(gfx_obj_t *obj, uint16_t size);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``size`` - Size in pixels (both width and height)

**Returns:**

* ESP_OK on success, error code otherwise

**Note:**

The QR code is always square, so this sets both width and height.

**Example:**

.. code-block:: c

   // Create a 200x200 pixel QR code
   gfx_qrcode_set_size(qrcode, 200);

gfx_qrcode_set_ecc()
~~~~~~~~~~~~~~~~~~~~

Set the error correction level for a QR Code object.

.. code-block:: c

   esp_err_t gfx_qrcode_set_ecc(gfx_obj_t *obj, gfx_qrcode_ecc_t ecc);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``ecc`` - Error correction level

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Use high error correction for damaged QR codes
   gfx_qrcode_set_ecc(qrcode, GFX_QRCODE_ECC_HIGH);

Styling
~~~~~~~

gfx_qrcode_set_color()
~~~~~~~~~~~~~~~~~~~~~~

Set the foreground color for a QR Code object.

.. code-block:: c

   esp_err_t gfx_qrcode_set_color(gfx_obj_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``color`` - Foreground color (QR modules color)

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // Black QR code (default)
   gfx_qrcode_set_color(qrcode, GFX_COLOR_HEX(0x000000));

   // Blue QR code
   gfx_qrcode_set_color(qrcode, GFX_COLOR_HEX(0x0000FF));

gfx_qrcode_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the background color for a QR Code object.

.. code-block:: c

   esp_err_t gfx_qrcode_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``bg_color`` - Background color

**Returns:**

* ESP_OK on success, error code otherwise

**Example:**

.. code-block:: c

   // White background (default)
   gfx_qrcode_set_bg_color(qrcode, GFX_COLOR_HEX(0xFFFFFF));

   // Light gray background
   gfx_qrcode_set_bg_color(qrcode, GFX_COLOR_HEX(0xF0F0F0));

Complete Example
----------------

.. code-block:: c

   // Create QR code object
   gfx_obj_t *qrcode = gfx_qrcode_create(handle);

   // Set data to encode
   gfx_qrcode_set_data(qrcode, "https://www.espressif.com");

   // Set size (200x200 pixels)
   gfx_qrcode_set_size(qrcode, 200);

   // Set error correction level
   gfx_qrcode_set_ecc(qrcode, GFX_QRCODE_ECC_MEDIUM);

   // Set colors
   gfx_qrcode_set_color(qrcode, GFX_COLOR_HEX(0x000000));  // Black foreground
   gfx_qrcode_set_bg_color(qrcode, GFX_COLOR_HEX(0xFFFFFF)); // White background

   // Position the QR code
   gfx_obj_set_pos(qrcode, 50, 50);

Error Correction Levels
-----------------------

The error correction level determines how much of the QR code can be damaged or obscured while still being readable:

* **LOW (L)**: ~7% error tolerance - Smallest QR code size
* **MEDIUM (M)**: ~15% error tolerance - Balanced size and reliability
* **QUARTILE (Q)**: ~25% error tolerance - Good for damaged codes
* **HIGH (H)**: ~30% error tolerance - Largest QR code size, most reliable

Choose a higher error correction level if:
* The QR code may be partially obscured
* The display may have low contrast
* The QR code may be printed or displayed in challenging conditions

