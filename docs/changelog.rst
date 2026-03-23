Changelog
=========

All notable changes to the ESP Emote GFX component will be documented in this file.

[Unreleased]
------------

Changed
~~~~~~~
* Require each delivered change to record a changelog entry describing the modification, affected modules, and known risk points.
* Improve button press-state behavior so visual feedback is cancelled when the pointer moves outside the control and restored when it moves back in.
* Allow button borders to be disabled by setting border width to `0`, without changing the existing public API shape.
* Resolve touch hit-testing against aligned or layout-dirty objects before evaluating pointer containment.
* Add public API documentation coverage for `gfx_button`.
* Fix the button demo/test observation wait so it no longer blocks for an accidental ultra-long duration.

Affected modules
~~~~~~~~~~~~~~~~
* `src/widget/basic/gfx_button.c`
* `src/core/runtime/gfx_touch.c`
* `include/widget/gfx_button.h`
* `docs/api/widgets/*`
* `test_apps/main/test_button.c`

Risk points
~~~~~~~~~~~
* Touch interactions on aligned widgets may behave slightly differently because hit-testing now resolves pending layout before matching coordinates.
* Applications that intentionally relied on a pressed visual state staying active after dragging out of a button will now see the more standard “cancel on leave” feedback.
* Border width `0` now has semantic meaning (“no border”), so rendering may change for callers that previously only used positive widths.

[3.0.1] - 2026-02-13
--------------------
* Add CI build action for P4
* Optimize multi-buffer switching logic
* Fix crash when text is NULL
* Fix missing API documentation (e.g. gfx_touch_add)

[3.0.0] - 2026-01-22
--------------------
* Add documentation build action
* Optimize EAF 8-bit render
* Fix FreeType parsing performance
* Remove duplicated label-related APIs

[2.1.0] - 2026-01-28
--------------------
* Support for decoding Heatshrink-compressed image slices

[2.0.4] - 2026-01-22
--------------------
* Fix Huffman+RLE decoding buffer sizing to prevent oversized output errors (Issue `#18 <https://github.com/espressif2022/esp_emote_gfx/issues/18>`_)

[2.0.3] - 2026-01-08
--------------------
* Delete local assets
* Build acion for ['release-v5.2', 'release-v5.3', 'release-v5.4', 'release-v5.5']
* Fix ESP-IDF version compatibility issues
* Change flush_callback timeout from 20 ms to wait forever

[2.0.2] - 2025-12-26
--------------------
* Add optional JPEG decoding support for EAF animations
* Center QR code rendering in UI layout
* Add alpha channel support for animations

[2.0.1] - 2025-12-05
--------------------
* Add Touch event

[2.0.0] - 2025-12-01
--------------------
* Added partial refresh mode support
* Added QR code widget (gfx_qrcode)

[1.2.0] - 2025-09-0
-------------------
* use eaf as a lib

[1.1.2] - 2025-09-29
--------------------

Upgrade dependencies
~~~~~~~~~~~~~~~~~~~~
* Update `espressif/esp_new_jpeg` to 0.6.x by @Kevincoooool. `#8 <https://github.com/espressif2022/esp_emote_gfx/pull/8>`_

[1.1.1] - 2025-09-23
--------------------

Fixed
~~~~~
* Resolve image block decoding failure in specific cases. `#6 <https://github.com/espressif2022/esp_emote_gfx/issues/6>`_

[1.0.0] - 2025-08-01
--------------------

Added
~~~~~
* Initial release of ESP Emote GFX framework
* Core graphics rendering engine
* Object system for images and labels
* Basic drawing functions and color utilities
* Software blending capabilities
* Timer system for animations
* Support for ESP-IDF 5.0+
* FreeType font rendering integration
* JPEG image decoding support

Features
~~~~~~~~
* Lightweight graphics framework optimized for embedded systems
* Memory-efficient design for resource-constrained environments
