Image (image)
=============

Types
-----

gfx_image_src_type_t
~~~~~~~~~~~~~~~~~~~~

Public image source type.

.. code-block:: c

   typedef enum {
       GFX_IMAGE_SRC_TYPE_IMAGE_DSC = 0, /**< In-memory gfx_image_dsc_t payload */
   } gfx_image_src_type_t;

gfx_image_header_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       uint32_t magic: 8;          /**< Magic number. Must be GFX_IMAGE_HEADER_MAGIC */
       uint32_t cf : 8;            /**< Color format: See `gfx_color_format_t` */
       uint32_t flags: 16;         /**< Reserved image flags */
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

gfx_image_src_t
~~~~~~~~~~~~~~~

Typed image source descriptor.

.. code-block:: c

   typedef struct {
       gfx_image_src_type_t type;    /**< Source payload type */
       const void *data;           /**< Type-specific payload pointer */
   } gfx_image_src_t;

Functions
---------

gfx_image_create()
~~~~~~~~~~~~~~~~~~

Public image source type.

.. code-block:: c

   gfx_object_t * gfx_image_create(gfx_display_t *disp);

**Parameters:**

* ``disp`` - Display from gfx_display_add()

**Returns:**

* Pointer to the created image object, NULL on error

gfx_image_set_source_desc()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the typed source descriptor for an image object

.. code-block:: c

   gfx_err_t gfx_image_set_source_desc(gfx_object_t *obj, const gfx_image_src_t *src);

**Parameters:**

* ``obj`` - Pointer to the image object
* ``src`` - Pointer to the typed source descriptor

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise
