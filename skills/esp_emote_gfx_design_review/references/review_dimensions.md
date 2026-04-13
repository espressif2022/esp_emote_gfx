# Review Dimensions

Always inspect these seven dimensions:

1. Function placement and `.c` / `.h` organization
2. Naming consistency for functions, variables, and types
3. Log style consistency
4. Error handling and null-pointer checks
5. Render-chain extensibility and registration design
6. `swap` color handling and flag consistency
7. `gfx_color_t` / `uint16_t` / `void *` usage consistency

## Function Placement

- Public declarations belong in `include/`
- Internal contracts belong in `src/<domain>/*_priv.h`
- Representation-heavy helpers should stay private
- If a public header exposes implementation-shaped types, call that out

## Naming

- Prefer `gfx_<module>_<verb>` for public functions
- Prefer semantic names over shape-size or storage-size names in public API
- Distinguish:
  - `*_cfg_t` for config
  - `*_state_t` for runtime mutable state
  - `*_ops_t` for behavior tables
  - `*_desc_t` for external payload descriptors

## Logging

- Library code should use `GFX_LOGx`
- Format logs as `operation: reason`
- Prefer lower-case, no trailing punctuation
- Log once at the right boundary; do not spam lower layers

## Error Handling

- Public API should validate with `ESP_RETURN_ON_FALSE`
- Propagate failures with `ESP_RETURN_ON_ERROR`
- Constructors may return `NULL`, but should log one scoped reason
- Hot-path helpers may return silently when the caller owns logging

## Render Extensibility

- Identify whether variation is explicit or hardcoded
- Prefer ops tables and registries for expected variability
- Widget logic should not own byte-order or raw framebuffer policy

## Swap And Color Policy

- Trace `swap` from display flags to final pixel writes
- Flag any path where swap is applied in more than one layer
- Treat `gfx_color_t` as semantic color and `uint16_t` as raw buffer representation
- Public APIs should not expose naked `void *` for image-like payloads unless there is a strong reason

## Type Consistency

- `gfx_color_t` is the semantic color type
- `uint16_t` is the raw framebuffer/storage representation
- `void *` should be reserved for opaque internal handles or tightly scoped internals
