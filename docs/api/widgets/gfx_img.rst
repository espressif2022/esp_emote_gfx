Image (gfx_img)
===============

Types
-----

gfx_color_format_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_COLOR_FORMAT_RGB565   = 0x04,  /**< RGB565 format without alpha channel */
       GFX_COLOR_FORMAT_RGB565A8 = 0x0A,  /**< RGB565 format with separate alpha channel */
   } gfx_color_format_t;

gfx_image_header_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t magic: 8;          /**< Magic number. Must be GFX_IMAGE_HEADER_MAGIC */
       uint32_t cf : 8;            /**< Color format: See `gfx_color_format_t` */
       uint32_t flags: 16;         /**< Image flags */
       uint32_t w: 16;             /**< Width of the image */
       uint32_t h: 16;             /**< Height of the image */
       uint32_t stride: 16;        /**< Number of bytes in a row */
       uint32_t reserved: 16;      /**< Reserved for future use */
   } gfx_image_header_t;

gfx_image_dsc_t
~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       gfx_image_header_t header;   /**< A header describing the basics of the image */
       uint32_t data_size;         /**< Size of the image in bytes */
       const uint8_t *data;        /**< Pointer to the data of the image */
       const void *reserved;       /**< Reserved field for future use */
       const void *reserved_2;     /**< Reserved field for future use */
   } gfx_image_dsc_t;

Functions
---------

gfx_img_create()
~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_obj_t * gfx_img_create(gfx_disp_t *disp);

gfx_img_set_src()
~~~~~~~~~~~~~~~~~

Set the source data for an image object

.. code-block:: c

   esp_err_t gfx_img_set_src(gfx_obj_t *obj, void *src);

**Parameters:**

* ``obj`` - Pointer to the image object
* ``src`` - Pointer to the image source data

**Returns:**

* Pointer to the object
