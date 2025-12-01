Quick Start Guide
=================

This guide will help you get started with ESP Emote GFX in just a few steps.

Installation
------------

Add ESP Emote GFX to your ESP-IDF project by including it as a component. The component is available through the ESP Component Registry.

Basic Setup
-----------

1. Include the main header:

.. code-block:: c

   #include "gfx.h"

2. Define a flush callback function:

.. code-block:: c

   void flush_callback(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data)
   {
       // Send data to your display
       // data is a pointer to RGB565 pixel data
       // (x1, y1) to (x2, y2) defines the area to update
   }

3. Initialize the graphics framework:

.. code-block:: c

   gfx_core_config_t gfx_cfg = {
       .flush_cb = flush_callback,
       .h_res = 320,  // Screen width
       .v_res = 240,  // Screen height
       .fps = 30,     // Target frame rate
       .buffers = {
           .buf1 = NULL,      // NULL = auto-allocate
           .buf2 = NULL,      // NULL = auto-allocate
           .buf_pixels = 0,   // 0 = auto-calculate
       },
       .task = GFX_EMOTE_INIT_CONFIG(),
   };

   gfx_handle_t handle = gfx_emote_init(&gfx_cfg);
   if (handle == NULL) {
       ESP_LOGE(TAG, "Failed to initialize GFX");
       return;
   }

Creating Your First Widget
---------------------------

Creating a Label
~~~~~~~~~~~~~~~~

.. code-block:: c

   // Create a label object
   gfx_obj_t *label = gfx_label_create(handle);

   // Set text
   gfx_label_set_text(label, "Hello, World!");

   // Set position
   gfx_obj_set_pos(label, 50, 50);

   // Set color
   gfx_label_set_color(label, GFX_COLOR_HEX(0xFF0000));  // Red

Creating an Image
~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Create an image object
   gfx_obj_t *img = gfx_img_create(handle);

   // Set image source (assuming you have image data)
   extern const gfx_image_dsc_t my_image;
   gfx_img_set_src(img, (void*)&my_image);

   // Set position
   gfx_obj_set_pos(img, 100, 100);

Creating an Animation
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // Create an animation object
   gfx_obj_t *anim = gfx_anim_create(handle);

   // Load animation data
   gfx_anim_set_src(anim, anim_data, anim_size);

   // Set size
   gfx_obj_set_size(anim, 200, 150);

   // Configure playback
   gfx_anim_set_segment(anim, 0, 10, 30, true);  // Frames 0-10, 30 FPS, loop
   gfx_anim_start(anim);

Thread Safety
-------------

Always use the graphics lock when modifying objects from outside the graphics task:

.. code-block:: c

   gfx_emote_lock(handle);
   gfx_label_set_text(label, "Updated text");
   gfx_obj_set_pos(img, new_x, new_y);
   gfx_emote_unlock(handle);

Complete Example
----------------

Here's a complete example that creates a label and displays it:

.. code-block:: c

   #include "gfx.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_example";

   void flush_callback(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data)
   {
       // Your display driver code here
       ESP_LOGD(TAG, "Flush: (%d,%d) to (%d,%d)", x1, y1, x2, y2);
   }

   void app_main(void)
   {
       // Initialize GFX
       gfx_core_config_t cfg = {
           .flush_cb = flush_callback,
           .h_res = 320,
           .v_res = 240,
           .fps = 30,
           .buffers = { NULL, NULL, 0 },
           .task = GFX_EMOTE_INIT_CONFIG(),
       };

       gfx_handle_t handle = gfx_emote_init(&cfg);
       if (handle == NULL) {
           ESP_LOGE(TAG, "Failed to initialize GFX");
           return;
       }

       // Create and configure a label
       gfx_obj_t *label = gfx_label_create(handle);
       gfx_label_set_text(label, "Hello, ESP Emote GFX!");
       gfx_obj_set_pos(label, 50, 50);
       gfx_label_set_color(label, GFX_COLOR_HEX(0x00FF00));  // Green

       // Trigger initial refresh
       gfx_emote_refresh_all(handle);

       // Your application code continues here...
   }

Next Steps
----------

* Read the :doc:`Core API Reference <api/core/index>` for detailed API documentation
* Check out the :doc:`Widget API Reference <api/widgets/index>` for widget-specific functions
* See :doc:`Examples <examples>` for more complex usage patterns

