Mesh Image (mesh_image)
=======================

Types
-----

gfx_mesh_img_point_t
~~~~~~~~~~~~~~~~~~~~

Mesh control point in integer pixel coordinates.

.. code-block:: c

   typedef struct {
       gfx_coord_t x;
       gfx_coord_t y;
   } gfx_mesh_img_point_t;

gfx_mesh_img_point_q8_t
~~~~~~~~~~~~~~~~~~~~~~~

Mesh control point in Q8 fixed-point coordinates.

.. code-block:: c

   typedef struct {
       int32_t x_q8;
       int32_t y_q8;
   } gfx_mesh_img_point_q8_t;

Functions
---------

gfx_mesh_img_create()
~~~~~~~~~~~~~~~~~~~~~

Mesh control point in integer pixel coordinates.

.. code-block:: c

   gfx_object_t * gfx_mesh_img_create(gfx_display_t *disp);

**Parameters:**

* ``disp`` - Display that owns the object

**Returns:**

* Created object, or NULL on failure

gfx_mesh_img_set_src_desc()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set a typed image source descriptor for the mesh.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_src_desc(gfx_object_t *obj, const gfx_image_src_t *src);

**Parameters:**

* ``obj`` - Mesh-image object
* ``src`` - Typed image source descriptor

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_grid()
~~~~~~~~~~~~~~~~~~~~~~~

Configure mesh grid density.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_grid(gfx_object_t *obj, uint8_t cols, uint8_t rows);

**Parameters:**

* ``obj`` - Mesh-image object
* ``cols`` - Horizontal cell count
* ``rows`` - Vertical cell count

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_get_point_count()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get the current mesh point count.

.. code-block:: c

   size_t gfx_mesh_img_get_point_count(gfx_object_t *obj);

**Parameters:**

* ``obj`` - Mesh-image object

**Returns:**

* Number of points in the current mesh

gfx_mesh_img_get_point()
~~~~~~~~~~~~~~~~~~~~~~~~

Get one mesh point in object-local pixel coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_get_point(gfx_object_t *obj, size_t point_idx, gfx_mesh_img_point_t *point);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``point`` - Output point

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_get_point_screen()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get one mesh point in screen coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_get_point_screen(gfx_object_t *obj, size_t point_idx, gfx_coord_t *x, gfx_coord_t *y);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``x`` - Output screen x
* ``y`` - Output screen y

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_get_point_q8()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get one mesh point in object-local Q8 coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_get_point_q8(gfx_object_t *obj, size_t point_idx, gfx_mesh_img_point_q8_t *point);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``point`` - Output Q8 point

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_get_point_screen_q8()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Get one mesh point in screen-space Q8 coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_get_point_screen_q8(gfx_object_t *obj, size_t point_idx, int32_t *x_q8, int32_t *y_q8);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``x_q8`` - Output screen x in Q8
* ``y_q8`` - Output screen y in Q8

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_point()
~~~~~~~~~~~~~~~~~~~~~~~~

Set one mesh point in object-local pixel coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_point(gfx_object_t *obj, size_t point_idx, gfx_coord_t x, gfx_coord_t y);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``x`` - Local x
* ``y`` - Local y

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_points()
~~~~~~~~~~~~~~~~~~~~~~~~~

Set all mesh points in object-local pixel coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_points(gfx_object_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);

**Parameters:**

* ``obj`` - Mesh-image object
* ``points`` - Point array
* ``point_count`` - Number of entries in `points`

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_point_q8()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set one mesh point in object-local Q8 coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_point_q8(gfx_object_t *obj, size_t point_idx, int32_t x_q8, int32_t y_q8);

**Parameters:**

* ``obj`` - Mesh-image object
* ``point_idx`` - Point index in the current grid
* ``x_q8`` - Local x in Q8
* ``y_q8`` - Local y in Q8

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_points_q8()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set all mesh points in object-local Q8 coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_points_q8(gfx_object_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);

**Parameters:**

* ``obj`` - Mesh-image object
* ``points`` - Q8 point array
* ``point_count`` - Number of entries in `points`

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_rest_points()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the rest pose points in object-local pixel coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_rest_points(gfx_object_t *obj, const gfx_mesh_img_point_t *points, size_t point_count);

**Parameters:**

* ``obj`` - Mesh-image object
* ``points`` - Point array
* ``point_count`` - Number of entries in `points`

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_rest_points_q8()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the rest pose points in object-local Q8 coordinates.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_rest_points_q8(gfx_object_t *obj, const gfx_mesh_img_point_q8_t *points, size_t point_count);

**Parameters:**

* ``obj`` - Mesh-image object
* ``points`` - Q8 point array
* ``point_count`` - Number of entries in `points`

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_reset_points()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reset current points back to the stored rest pose.

.. code-block:: c

   gfx_err_t gfx_mesh_img_reset_points(gfx_object_t *obj);

**Parameters:**

* ``obj`` - Mesh-image object

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_ctrl_points_visible()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Show or hide mesh control points for debugging.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_ctrl_points_visible(gfx_object_t *obj, bool visible);

**Parameters:**

* ``obj`` - Mesh-image object
* ``visible`` - Whether control points should be drawn

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_opa()
~~~~~~~~~~~~~~~~~~~~~~

Set uniform mesh opacity.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_opa(gfx_object_t *obj, gfx_opa_t opa);

**Parameters:**

* ``obj`` - Mesh image object
* ``opa`` - Uniform opacity (0-255)

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_aa_inward()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enable inward-only edge anti-aliasing.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_aa_inward(gfx_object_t *obj, bool inward);

**Parameters:**

* ``obj`` - Mesh image object.
* ``inward`` - true = inward AA (no outward bleed); false = default outward AA.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_wrap_cols()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Treat first and last grid columns as adjacent (closed strip).

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_wrap_cols(gfx_object_t *obj, bool wrap);

**Parameters:**

* ``obj`` - Mesh image object.
* ``wrap`` - Whether the first and last grid columns should be treated as adjacent.

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise

gfx_mesh_img_set_scanline_fill()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use scanline polygon fill instead of triangle rasterization.

.. code-block:: c

   gfx_err_t gfx_mesh_img_set_scanline_fill(gfx_object_t *obj, bool enable, gfx_color_t fill_color);

**Parameters:**

* ``obj`` - Mesh image object
* ``enable`` - Whether scanline fill mode should be enabled
* ``fill_color`` - Solid fill color (typically white for strokes).

**Returns:**

* GFX_OK on success, GFX_ERR_* otherwise
