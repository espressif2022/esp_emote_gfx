Animation (gfx_anim)
====================

Types
-----

gfx_anim_segment_action_t
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_ANIM_SEGMENT_ACTION_CONTINUE = 0,
       GFX_ANIM_SEGMENT_ACTION_PAUSE,
   } gfx_anim_segment_action_t;

gfx_anim_src_type_t
~~~~~~~~~~~~~~~~~~~

Public animation source type.

.. code-block:: c

   typedef enum {
       GFX_ANIM_SRC_TYPE_MEMORY = 0, /**< In-memory animation payload */
   } gfx_anim_src_type_t;

gfx_anim_segment_t
~~~~~~~~~~~~~~~~~~

Playback description for one animation segment.

.. code-block:: c

   typedef struct {
       uint32_t start;      /* inclusive start frame */
       uint32_t end;        /* inclusive end frame */
       uint32_t fps;        /* playback fps for this segment */
       uint32_t play_count; /* total plays for this segment, 0 means forever */
       gfx_anim_segment_action_t end_action; /* action after the last play finishes */
   } gfx_anim_segment_t;

gfx_anim_src_t
~~~~~~~~~~~~~~

Typed animation source descriptor.

.. code-block:: c

   typedef struct {
       gfx_anim_src_type_t type; /**< Source payload type */
       const void *data;         /**< Type-specific payload pointer */
       size_t data_len;          /**< Payload length in bytes */
   } gfx_anim_src_t;

Functions
---------

gfx_anim_create()
~~~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_obj_t * gfx_anim_create(gfx_disp_t *disp);

gfx_anim_set_src_desc()
~~~~~~~~~~~~~~~~~~~~~~~

Set the typed source descriptor for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_src_desc(gfx_obj_t *obj, const gfx_anim_src_t *src);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``src`` - Pointer to the typed source descriptor

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_src()
~~~~~~~~~~~~~~~~~~

Set the source data for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_src(gfx_obj_t *obj, const void *src_data, size_t src_len);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``src_data`` - Source data
* ``src_len`` - Source data length

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_segment()
~~~~~~~~~~~~~~~~~~~~~~

Set the segment for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_segment(gfx_obj_t *obj, uint32_t start, uint32_t end, uint32_t fps, bool repeat);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``start`` - Start frame index
* ``end`` - End frame index
* ``fps`` - Frames per second
* ``repeat`` - Whether to repeat the animation

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_segments()
~~~~~~~~~~~~~~~~~~~~~~~

Set a segment playback plan for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_segments(gfx_obj_t *obj, const gfx_anim_segment_t *segments, size_t segment_count);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``segments`` - Segment plan array
* ``segment_count`` - Number of segment entries in the array

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_play_left_to_tail()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Drain the remaining segment plan and block until playback finishes

.. code-block:: c

   esp_err_t gfx_anim_play_left_to_tail(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, ESP_ERR_NOT_FOUND if there is no remaining work, or another ESP_ERR_* code on failure

gfx_anim_start()
~~~~~~~~~~~~~~~~

Start the animation

.. code-block:: c

   esp_err_t gfx_anim_start(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_stop()
~~~~~~~~~~~~~~~

Stop the animation

.. code-block:: c

   esp_err_t gfx_anim_stop(gfx_obj_t *obj);

**Parameters:**

* ``obj`` - Pointer to the animation object

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_mirror()
~~~~~~~~~~~~~~~~~~~~~

Set mirror display for an animation object

.. code-block:: c

   esp_err_t gfx_anim_set_mirror(gfx_obj_t *obj, bool enabled, int16_t offset);

**Parameters:**

* ``obj`` - Pointer to the animation object
* ``enabled`` - Whether to enable mirror display
* ``offset`` - Mirror offset in pixels

**Returns:**

* ESP_OK on success, error code otherwise

gfx_anim_set_auto_mirror()
~~~~~~~~~~~~~~~~~~~~~~~~~~

Set auto mirror alignment for animation object

.. code-block:: c

   esp_err_t gfx_anim_set_auto_mirror(gfx_obj_t *obj, bool enabled);

**Parameters:**

* ``obj`` - Animation object
* ``enabled`` - Whether to enable auto mirror alignment

**Returns:**

* ESP_OK on success, ESP_ERR_* otherwise
