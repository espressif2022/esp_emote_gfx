QR Code Widget (gfx_qrcode)
===========================

Types
-----

gfx_qrcode_ecc_t
~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_QRCODE_ECC_LOW = 0,      /**< The QR Code can tolerate about 7% erroneous codewords */
       GFX_QRCODE_ECC_MEDIUM,       /**< The QR Code can tolerate about 15% erroneous codewords */
       GFX_QRCODE_ECC_QUARTILE,     /**< The QR Code can tolerate about 25% erroneous codewords */
       GFX_QRCODE_ECC_HIGH          /**< The QR Code can tolerate about 30% erroneous codewords */
   } gfx_qrcode_ecc_t;

Functions
---------

gfx_qrcode_set_data()
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   esp_err_t gfx_qrcode_set_data(gfx_obj_t *obj, const char *data);

gfx_qrcode_set_size()
~~~~~~~~~~~~~~~~~~~~~

Set the size for a QR Code object

.. code-block:: c

   esp_err_t gfx_qrcode_set_size(gfx_obj_t *obj, uint16_t size);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``size`` - Size in pixels (both width and height)

**Returns:**

* ESP_OK on success, error code otherwise

gfx_qrcode_set_ecc()
~~~~~~~~~~~~~~~~~~~~

Set the error correction level for a QR Code object

.. code-block:: c

   esp_err_t gfx_qrcode_set_ecc(gfx_obj_t *obj, gfx_qrcode_ecc_t ecc);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``ecc`` - Error correction level

**Returns:**

* ESP_OK on success, error code otherwise

gfx_qrcode_set_color()
~~~~~~~~~~~~~~~~~~~~~~

Set the foreground color for a QR Code object

.. code-block:: c

   esp_err_t gfx_qrcode_set_color(gfx_obj_t *obj, gfx_color_t color);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``color`` - Foreground color (QR modules color)

**Returns:**

* ESP_OK on success, error code otherwise

gfx_qrcode_set_bg_color()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set the background color for a QR Code object

.. code-block:: c

   esp_err_t gfx_qrcode_set_bg_color(gfx_obj_t *obj, gfx_color_t bg_color);

**Parameters:**

* ``obj`` - Pointer to the QR Code object
* ``bg_color`` - Background color

**Returns:**

* ESP_OK on success, error code otherwise
