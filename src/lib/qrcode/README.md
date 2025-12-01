# QR Code Wrapper Library

[![Component Registry](https://components.espressif.com/components/espressif/qrcode/badge.svg)](https://components.espressif.com/components/espressif/qrcode)

## Overview

This is a QR code generation wrapper library based on the [qrcodegen](https://www.nayuki.io/page/qr-code-generator-library) library, re-encapsulated from the [Espressif QR Code component](https://components.espressif.com/components/espressif/qrcode).

## Why Re-encapsulate?

To avoid dependency issues with the `espressif__qrcode` component, we directly integrated the core `qrcodegen` library into the project and provided an API interface compatible with the original component.

## File Structure

```
src/lib/qrcode/
├── qrcodegen.h          # QR Code generator library header
├── qrcodegen.c          # QR Code generator library implementation
├── qrcode_wrapper.h    # Wrapper interface header
├── qrcode_wrapper.c    # Wrapper interface implementation
└── README.md           # This file
```

## Core Library

- **qrcodegen**: QR Code generator library developed by Project Nayuki
  - License: MIT License
  - Project URL: https://www.nayuki.io/page/qr-code-generator-library
  - Supports QR Code Model 2 specification, versions 1-40, all 4 error correction levels

## API Interface

### Main Functions

- `qrcode_wrapper_generate()` - Generate QR code
- `qrcode_wrapper_get_size()` - Get QR code size
- `qrcode_wrapper_get_module()` - Get module value at specified coordinates
- `qrcode_wrapper_print_console()` - Print QR code to console (optional)

### Error Correction Levels

- `QRCODE_WRAPPER_ECC_LOW` - 7% error tolerance
- `QRCODE_WRAPPER_ECC_MED` - 15% error tolerance
- `QRCODE_WRAPPER_ECC_QUART` - 25% error tolerance
- `QRCODE_WRAPPER_ECC_HIGH` - 30% error tolerance

## Usage Example

```c
#include "lib/qrcode/qrcode_wrapper.h"

// Configure QR code generation parameters
qrcode_wrapper_config_t cfg = {
    .display_func = my_display_callback,
    .max_qrcode_version = 5,
    .qrcode_ecc_level = QRCODE_WRAPPER_ECC_LOW,
    .user_data = NULL
};

// Generate QR code
esp_err_t ret = qrcode_wrapper_generate(&cfg, "Hello, World!");
if (ret == ESP_OK) {
    // QR code generated successfully
}
```

## License

- qrcodegen library: MIT License (Copyright (c) Project Nayuki)
- Wrapper code: Apache-2.0 (Copyright 2024-2025 Espressif Systems)

## References

- [Espressif QR Code Component](https://components.espressif.com/components/espressif/qrcode)
- [qrcodegen Library](https://www.nayuki.io/page/qr-code-generator-library)
- [QR Code ISO/IEC 18004 Standard](https://www.iso.org/standard/62021.html)
