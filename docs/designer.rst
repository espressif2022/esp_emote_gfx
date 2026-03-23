UI Designer Prototype
=====================

This repository now includes a browser-based prototype for a host-side UI designer.
The goal is to make ESP Emote GFX screens configurable from a web page without
changing the current C-facing widget APIs.

Why this exists
---------------

The current runtime is already capable of rendering multiple widget types, but it is
still library-centric:

* Screen composition is written manually in C.
* Widget metadata is distributed across headers instead of being exposed as an editor-friendly schema.
* There is no stable interchange format for layout, styling, and resource binding.
* Designers and firmware developers cannot share a single source of truth for screen structure.

For a modern host-side editor, those are the biggest product gaps rather than low-level
rendering limitations.

Prototype goals
---------------

The web prototype focuses on the workflow that is currently missing:

* Add widgets from a palette.
* Drag widgets directly on a screen canvas.
* Edit common and type-specific properties from an inspector.
* Reorder layers without rewriting code.
* Export a JSON scene description that can later drive code generation.

The prototype is intentionally schema-first. It does not replace the embedded runtime.
Instead, it prepares a configuration layer that can map cleanly onto the existing APIs
such as ``gfx_obj_set_pos()``, ``gfx_obj_set_size()``, ``gfx_label_set_text()``,
``gfx_button_set_bg_color()``, ``gfx_qrcode_set_data()``, and similar setters.

Scene schema
------------

The prototype exports a JSON document with three stable concepts:

``screen``
  The display size, background color, and editor grid settings.

``widgets``
  A flat list of widgets with absolute position, size, visibility, z-order, and a
  ``props`` object for widget-specific fields.

``theme``
  Reserved for future style tokens so common colors, spacing, and typography can be
  applied consistently across widgets.

Example:

.. code-block:: json

   {
     "version": 1,
     "screen": {
       "width": 320,
       "height": 240,
       "bgColor": "#0f172a",
       "gridSize": 8
     },
     "theme": {
       "accent": "#3b82f6",
       "surface": "#111827",
       "text": "#e5eefb"
     },
     "widgets": [
       {
         "id": "button_1",
         "type": "button",
         "name": "Primary CTA",
         "x": 24,
         "y": 176,
         "w": 136,
         "h": 40,
         "visible": true,
         "zIndex": 3,
         "props": {
           "text": "Start",
           "textColor": "#f8fafc",
           "bgColor": "#2563eb",
           "bgColorPressed": "#1d4ed8",
           "borderColor": "#60a5fa",
           "borderWidth": 1,
           "textAlign": "center"
         }
       }
     ]
   }

Mapping to current APIs
-----------------------

The exported schema is designed so a future generator can keep the existing public API:

* ``x``, ``y``, ``w``, ``h`` map to ``gfx_obj_set_pos()`` and ``gfx_obj_set_size()``.
* ``visible`` maps to ``gfx_obj_set_visible()``.
* Widget-specific ``props`` map to the already exposed widget setters.
* ``type`` stays aligned with existing widget families:
  ``label``, ``button``, ``list``, ``image``, ``qrcode``, ``anim``, ``mesh_img``.

This keeps firmware compatibility high and avoids introducing editor-only C interfaces.

Current limitations and follow-up work
--------------------------------------

The prototype is intentionally practical, but not complete:

* The current export is JSON only; there is no C code generator yet.
* Resource binding for fonts, binary images, and EAF animation assets is still manual.
* Layout is absolute-position first. Alignment and constraints should be added in a
  later iteration for more adaptive screens.
* Widget events and action flows are not modeled yet.

Modern UI design considerations
-------------------------------

The prototype UI follows several modern editor design principles:

* Direct manipulation: widgets are moved directly on the canvas instead of through
  modal dialogs.
* Progressive disclosure: common properties are always visible, type-specific fields
  only appear when relevant.
* Strong hierarchy: palette, canvas, layers, and inspector are visually separated so
  scanning cost stays low.
* Contrast and focus: the canvas is bright enough for content inspection while control
  chrome stays subdued.
* Responsive tooling: the layout collapses cleanly on smaller screens instead of
  assuming a desktop-only viewport.

Preview
-------

After building the docs, open:

* ``/designer/index.html`` for the interactive prototype
* ``/index.html`` for the main documentation portal

