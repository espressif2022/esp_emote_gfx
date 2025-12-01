Examples
========

This section provides comprehensive examples demonstrating various features of ESP Emote GFX.

Basic Examples
--------------

Simple Label
~~~~~~~~~~~~

Create and display a simple text label:

.. code-block:: c

   #include "gfx.h"

   void app_main(void)
   {
       // Initialize GFX (assuming handle is already created)
       gfx_handle_t handle = ...;

       // Create label
       gfx_obj_t *label = gfx_label_create(handle);
       
       // Set text and position
       gfx_label_set_text(label, "Hello, World!");
       gfx_obj_set_pos(label, 50, 50);
       
       // Set color
       gfx_label_set_color(label, GFX_COLOR_HEX(0xFF0000));
       
       // Refresh display
       gfx_emote_refresh_all(handle);
   }

Image Display
~~~~~~~~~~~~~

Display an image:

.. code-block:: c

   #include "gfx.h"

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Create image object
       gfx_obj_t *img = gfx_img_create(handle);
       
       // Set image source (assuming image data is available)
       extern const gfx_image_dsc_t my_image;
       gfx_img_set_src(img, (void*)&my_image);
       
       // Center the image
       gfx_obj_align(img, GFX_ALIGN_CENTER, 0, 0);
   }

Advanced Examples
-----------------

Multiple Widgets
~~~~~~~~~~~~~~~~~

Create and manage multiple widgets:

.. code-block:: c

   #include "gfx.h"

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Create label
       gfx_obj_t *label = gfx_label_create(handle);
       gfx_label_set_text(label, "Status: OK");
       gfx_obj_set_pos(label, 10, 10);
       
       // Create image
       gfx_obj_t *img = gfx_img_create(handle);
       extern const gfx_image_dsc_t icon;
       gfx_img_set_src(img, (void*)&icon);
       gfx_obj_set_pos(img, 10, 50);
       
       // Create animation
       gfx_obj_t *anim = gfx_anim_create(handle);
       gfx_anim_set_src(anim, anim_data, anim_size);
       gfx_obj_set_size(anim, 100, 100);
       gfx_obj_set_pos(anim, 150, 50);
       gfx_anim_set_segment(anim, 0, 10, 30, true);
       gfx_anim_start(anim);
   }

Text Scrolling
~~~~~~~~~~~~~~

Create a scrolling text label:

.. code-block:: c

   #include "gfx.h"

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Create label with long text
       gfx_obj_t *label = gfx_label_create(handle);
       gfx_label_set_text(label, "This is a very long text that will scroll horizontally");
       
       // Set size to create scrolling area
       gfx_obj_set_size(label, 200, 30);
       gfx_obj_set_pos(label, 10, 100);
       
       // Enable scrolling
       gfx_label_set_long_mode(label, GFX_LABEL_LONG_SCROLL);
       gfx_label_set_scroll_speed(label, 30);  // 30ms per pixel
       gfx_label_set_scroll_loop(label, true);  // Loop continuously
   }

FreeType Font Usage
~~~~~~~~~~~~~~~~~~~

Use FreeType fonts for text rendering:

.. code-block:: c

   #include "gfx.h"
   #include <stdio.h>

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Load font file (assuming font data is in memory)
       extern const uint8_t font_data[];
       extern const size_t font_size;
       
       gfx_label_cfg_t font_cfg = {
           .name = "DejaVuSans.ttf",
           .mem = font_data,
           .mem_size = font_size,
           .font_size = 24,
       };
       
       gfx_font_t font;
       if (gfx_label_new_font(&font_cfg, &font) == ESP_OK) {
           // Create label with FreeType font
           gfx_obj_t *label = gfx_label_create(handle);
           gfx_label_set_font(label, font);
           gfx_label_set_text(label, "FreeType Font");
           gfx_obj_set_pos(label, 50, 50);
           
           // Clean up when done
           // gfx_label_delete_font(font);
       }
   }

Timer-Based Updates
~~~~~~~~~~~~~~~~~~~

Use timers to update widgets periodically:

.. code-block:: c

   #include "gfx.h"

   static gfx_obj_t *label = NULL;
   static int counter = 0;

   void timer_callback(void *user_data)
   {
       gfx_handle_t handle = (gfx_handle_t)user_data;
       
       gfx_emote_lock(handle);
       if (label) {
           gfx_label_set_text_fmt(label, "Counter: %d", counter++);
       }
       gfx_emote_unlock(handle);
   }

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Create label
       label = gfx_label_create(handle);
       gfx_obj_set_pos(label, 50, 50);
       
       // Create timer to update label every second
       gfx_timer_handle_t timer = gfx_timer_create(
           handle, 
           timer_callback, 
           1000,  // 1 second
           handle
       );
   }

QR Code Generation
~~~~~~~~~~~~~~~~~~

Generate and display a QR code:

.. code-block:: c

   #include "gfx.h"

   void app_main(void)
   {
       gfx_handle_t handle = ...;
       
       // Create QR code
       gfx_obj_t *qrcode = gfx_qrcode_create(handle);
       
       // Set data
       gfx_qrcode_set_data(qrcode, "https://www.espressif.com");
       
       // Set size
       gfx_qrcode_set_size(qrcode, 200);
       
       // Set error correction
       gfx_qrcode_set_ecc(qrcode, GFX_QRCODE_ECC_MEDIUM);
       
       // Center on screen
       gfx_obj_align(qrcode, GFX_ALIGN_CENTER, 0, 0);
   }

Thread-Safe Operations
~~~~~~~~~~~~~~~~~~~~~~

Perform thread-safe widget operations:

.. code-block:: c

   #include "gfx.h"

   void update_widgets_from_task(gfx_handle_t handle)
   {
       // Always lock before modifying widgets
       if (gfx_emote_lock(handle) == ESP_OK) {
           // Perform widget operations
           gfx_label_set_text(label, "Updated from task");
           gfx_obj_set_pos(img, new_x, new_y);
           
           // Unlock when done
           gfx_emote_unlock(handle);
       }
   }

Complete Application Example
-----------------------------

A complete example showing initialization and widget usage:

.. code-block:: c

   #include "gfx.h"
   #include "esp_log.h"

   static const char *TAG = "gfx_app";
   static gfx_handle_t gfx_handle = NULL;

   void flush_callback(gfx_handle_t handle, int x1, int y1, int x2, int y2, const void *data)
   {
       // Your display driver code here
       // Send RGB565 data to display
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

       gfx_handle = gfx_emote_init(&cfg);
       if (gfx_handle == NULL) {
           ESP_LOGE(TAG, "Failed to initialize GFX");
           return;
       }

       // Create widgets
       gfx_obj_t *title = gfx_label_create(gfx_handle);
       gfx_label_set_text(title, "ESP Emote GFX");
       gfx_obj_align(title, GFX_ALIGN_TOP_MID, 0, 10);
       gfx_label_set_color(title, GFX_COLOR_HEX(0x0000FF));

       // Trigger initial refresh
       gfx_emote_refresh_all(gfx_handle);

       ESP_LOGI(TAG, "GFX application started");
   }

For more examples, see the test applications in the ``test_apps/`` directory.

