Touch (gfx_touch)
=================

Types
-----

gfx_touch_event_cb_t
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef void (*gfx_touch_event_cb_t)(gfx_touch_t *touch, const gfx_touch_event_t *event, void *user_data);

gfx_touch_event_type_t
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef enum {
       GFX_TOUCH_EVENT_PRESS = 0,
       GFX_TOUCH_EVENT_RELEASE,
       GFX_TOUCH_EVENT_MOVE,   /**< Finger moved while pressed (slide) */
   } gfx_touch_event_type_t;

gfx_touch_config_t
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       esp_lcd_touch_handle_t handle;           /**< LCD touch driver handle */
       gfx_touch_event_cb_t event_cb;           /**< Event callback */
       uint32_t poll_ms;                        /**< Poll interval ms (0 = default) */
       gfx_disp_t *disp;                        /**< Display handle */
       void *user_data;                         /**< User data for callback */
   } gfx_touch_config_t;

Functions
---------

gfx_touch_add()
~~~~~~~~~~~~~~~

.. code-block:: c

   gfx_touch_t * gfx_touch_add(gfx_handle_t handle, const gfx_touch_config_t *cfg);

gfx_touch_set_disp()
~~~~~~~~~~~~~~~~~~~~

Bind a display to a touch device

.. code-block:: c

   esp_err_t gfx_touch_set_disp(gfx_touch_t *touch, gfx_disp_t *disp);

**Parameters:**

* ``touch`` - Touch pointer returned from gfx_touch_add
* ``disp`` - Display to receive touch hit-testing and dispatch

**Returns:**

* ESP_OK on success, ESP_ERR_INVALID_ARG if touch is NULL

gfx_touch_del()
~~~~~~~~~~~~~~~

Remove a touch device from the list and release resources (stops polling, disables IRQ). Does not free the gfx_touch_t; caller must free(touch) after.

.. code-block:: c

   void gfx_touch_del(gfx_touch_t *touch);

**Parameters:**

* ``touch`` - Touch pointer returned from gfx_touch_add; safe to pass NULL
