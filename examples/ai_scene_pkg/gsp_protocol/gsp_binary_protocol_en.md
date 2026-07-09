# GSP Binary Scene Package Protocol

> Status: draft v0.1  
> Scope: `examples/ai_scene_pkg` proof of concept  
> Purpose: keep the host exporter, generated `.inc`, manifest, and device loader aligned.

This document is the binary contract for GSP scene packages. Exporters must generate bytes that follow this contract. Loaders must validate this contract before creating any GFX object.

The current demo package is `home.inc`:

```text
home_fonts[]      runtime font binding metadata
home_scene_pkg[]  pointer-free GSP1 binary package
home_scene_pkg_len
```

`home_scene_pkg[]` is the runtime payload. It must not contain native pointers.

---

## 1. Core Rules

1. Package bytes are little-endian.
2. Package bytes are position-independent.
3. Runtime payload contains no native C pointers.
4. Object references use `u16` object indexes.
5. String, params, blob table, and blob data references use `u32` byte offsets from package base.
6. All table entry sizes are fixed:

| Entry | Size |
|---|---:|
| Header | `56` bytes |
| Object entry | `64` bytes |
| Blob entry | `20` bytes |
| Action entry | `24` bytes |

7. `header.total_size` must equal the exported runtime package byte length.
8. `header.crc32` must match `gsp_crc32_scene()`, with the CRC field itself treated as zero during calculation.
9. Loader must reject invalid packages with error codes. It must not crash on bad offsets, bad counts, bad parent indexes, bad strings, bad blobs, or CRC mismatch.

---

## 2. Runtime Package Layout

Current layout:

```text
[Header 56B]
[Object table: obj_count * 64B]
[Blob table: blob_count * 20B]
[Action table: action_count * 24B]
[Params area]
[String table]
[Blob data]
```

The regions are addressed by absolute byte offsets from the beginning of `home_scene_pkg[]`.

Generic offset relationship:

```text
header_off     = 0
obj_table_off  = GSP_HEADER_SIZE
blob_table_off = obj_table_off + obj_count * GSP_OBJ_SIZE
action_table_off = blob_table_off + blob_count * GSP_BLOB_SIZE
params_off     = action_table_off + action_count * GSP_ACTION_SIZE
str_table_off  = params_off + params_total_size
blob_data_off  = str_table_off + string_table_size
total_size     = blob_data_off + blob_data_size
```

---

## 3. Header

Header size: `GSP_HEADER_SIZE = 56`.

| Offset | Type | Field | Meaning |
|---:|---|---|---|
| `0` | `u32` | `magic` | Must be `GSP_MAGIC`, bytes `GSP1` |
| `4` | `u32` | `version` | Must match `GSP_VERSION` |
| `8` | `u16` | `screen_w` | Logical scene width |
| `10` | `u16` | `screen_h` | Logical scene height |
| `12` | `u32` | `screen_bg` | RGB888 background color |
| `16` | `u32` | `obj_count` | Number of object entries |
| `20` | `u32` | `obj_table_off` | Absolute offset to object table |
| `24` | `u32` | `str_table_off` | Absolute offset to string table |
| `28` | `u32` | `blob_count` | Number of blob entries |
| `32` | `u32` | `blob_table_off` | Absolute offset to blob table |
| `36` | `u32` | `total_size` | Total package size in bytes |
| `40` | `u32` | `crc32` | Whole-package CRC32, this field zeroed while calculating |
| `44` | `u32` | `action_count` | Number of action entries; `0` means no action table |
| `48` | `u32` | `action_table_off` | Absolute offset to action table; `0` when no action table |
| `52` | `u32` | `reserved` | Must be `0` for v4 |

Validation:

- `magic == GSP_MAGIC`
- `version == GSP_VERSION`
- `total_size <= supplied_buffer_size`
- `total_size >= GSP_HEADER_SIZE`
- `gsp_crc32_scene(buf, total_size) == crc32`
- `obj_count > 0`
- `obj_count <= 0xFFFF`

---

## 4. Object Entry

Object entry size: `GSP_OBJ_SIZE = 64`.

Objects are stored in preorder. A child must appear after its parent.

| Offset | Type | Field | Meaning |
|---:|---|---|---|
| `0` | `u16` | `type` | `GSP_OBJ_*` |
| `2` | `u16` | `parent_idx` | Parent object index, or `GSP_NO_PARENT` |
| `4` | `i16` | `x` | Local x relative to parent |
| `6` | `i16` | `y` | Local y relative to parent |
| `8` | `u16` | `w` | Width |
| `10` | `u16` | `h` | Height |
| `12` | `u32` | `flags` | `GSP_F_*` bits |
| `16` | `u32` | `fg_color` | RGB888 foreground color |
| `20` | `u32` | `bg_color` | RGB888 background color |
| `24` | `u32` | `border_color` | RGB888 border color |
| `28` | `u16` | `border_width` | Border width |
| `30` | `u16` | `radius` | Corner radius |
| `32` | `u32` | `text_off` | Absolute offset to NUL string |
| `36` | `u32` | `callback_off` | Absolute offset to callback-name string |
| `40` | `u32` | `name_off` | Absolute offset to object-name string |
| `44` | `u32` | `blob_idx` | Blob table index |
| `48` | `u32` | `params_off` | Absolute offset to private params |
| `52` | `u16` | `params_len` | Private params length |
| `54` | `u8` | `opacity` | `0..255` when `GSP_F_OPACITY` is set |
| `55` | `u8` | `text_align` | `GSP_ALIGN_*` when `GSP_F_ALIGN` is set |
| `56` | `u16` | `font_id` | Runtime font binding id |
| `58` | `u16` | `bind_id` | Data binding id |
| `60` | `u32` | `reserved0` | Must be `0` for v4 |

Parent validation:

- Root object uses `parent_idx = GSP_NO_PARENT`.
- Non-root object must satisfy `parent_idx < current_object_index`.
- Loader must reject `parent_idx >= current_object_index`.

String validation:

- If a string flag is set, the corresponding offset must point inside `[0, total_size)`.
- The string must be NUL-terminated before `total_size`.

Params validation:

- If `GSP_F_PARAMS` is set, `params_off + params_len <= total_size`.
- The common loader validates bounds first; each widget may then parse its own private params profile.

---

## 5. Object Types: Base Component Identity

`type` answers only one question: which base component this entry creates. It does not define the property set, and it does not mean every field is meaningful for that component.

Current v4 object type registry:

| Type ID | Symbol | Base Component | Loader Creation | Resource Dependency |
|---:|---|---|---|---|
| `1` | `GSP_OBJ_CONTAINER` | container | `gfx_container_create()` | none |
| `2` | `GSP_OBJ_LABEL` | label | `gfx_label_create()` | optional `font_id` |
| `3` | `GSP_OBJ_BUTTON` | button | `gfx_button_create()` | optional `font_id`, optional `callback_off` |
| `4` | `GSP_OBJ_IMAGE` | image | `gfx_image_create()` | `blob_idx` into Blob table |
| `5` | `GSP_OBJ_LIST` | list | `gfx_list_create()` | optional `font_id`, optional `GSP_F_PARAMS` |
| `6` | `GSP_OBJ_WHEEL` | wheel | `gfx_wheel_create()` | optional `font_id`, optional `GSP_F_PARAMS` |
| `7` | `GSP_OBJ_LAYER` | layer | `gfx_container_create()` | may be targeted by `GSP_ACT_GOTO` |

Unknown object types must be rejected with `GSP_ERR_TYPE`.

---

## 6. Object Property Fields: Common Validity Bits

Flags are a common bitset that says which Object Entry fields are valid. They are not the component type definition. Whether a property is meaningful for a component is defined by the Component Profile in section 7.

Current v4 property field registry:

| Property | Flag | Bit / Hex | Object Entry Fields | Value Type | Common Validation |
|---|---|---:|---|---|---|
| text | `GSP_F_TEXT` | `0 / 0x001` | `text_off` | UTF-8 NUL string offset | offset must be in package and NUL-terminated |
| fg color | `GSP_F_FG_COLOR` | `1 / 0x002` | `fg_color` | RGB888 | no extra offset validation |
| bg color | `GSP_F_BG_COLOR` | `2 / 0x004` | `bg_color` | RGB888 | no extra offset validation |
| border | `GSP_F_BORDER` | `3 / 0x008` | `border_color`, `border_width` | RGB888 + u16 | no extra offset validation |
| radius | `GSP_F_RADIUS` | `4 / 0x010` | `radius` | u16 | no extra offset validation |
| callback | `GSP_F_CALLBACK` | `5 / 0x020` | `callback_off` | UTF-8 NUL string offset | offset must be in package and NUL-terminated |
| image resource | `GSP_F_IMAGE` | `6 / 0x040` | `blob_idx` | u32 blob index | `blob_idx < blob_count` |
| name | `GSP_F_NAME` | `7 / 0x080` | `name_off` | UTF-8 NUL string offset | offset must be in package and NUL-terminated |
| hidden | `GSP_F_HIDDEN` | `8 / 0x100` | flags only | boolean | no extra field |
| opacity | `GSP_F_OPACITY` | `9 / 0x200` | `opacity` | u8, `0..255` | reserved; current loader does not apply it yet |
| text align | `GSP_F_ALIGN` | `10 / 0x400` | `text_align` | `GSP_ALIGN_*` | value should be a defined enum |
| private params | `GSP_F_PARAMS` | `11 / 0x800` | `params_off`, `params_len` | byte range | `params_off + params_len <= total_size` |

Exporter should not set fields without the matching flag. Loader may ignore disabled fields, but it must validate enabled offsets/indexes.

---

## 7. Component Profile: Base Component Property Matrix

A Component Profile defines how each base component interprets the common property fields from section 6. Exporters and device loaders should treat this section as the negotiation surface for component property capability.

### 7.1 Common Fields

All objects must support these structural fields:

| Field | Requires Flag | Meaning |
|---|---|---|
| `type` | no | Base component type |
| `parent_idx` | no | Parent-child relationship |
| `x/y/w/h` | no | Local layout rectangle relative to parent |
| `flags` | no | Valid-field bitset |
| `GSP_F_HIDDEN` | yes | Initial visibility |
| `GSP_F_NAME` | yes | Object-name offset; loader validates it and builds a runtime `name -> gfx_object_t` lookup table |
| `GSP_F_PARAMS` | yes | Component-private extension parameter block; list/wheel define params-v1 |

### 7.2 v4 Property Support Matrix

`Y` means the v4 loader consumes this property. Blank means the component should not export it.

| Property | Field / Resource | container | label | button | image | list | wheel | layer | Notes |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|
| layout | `x/y/w/h` | Y | Y | Y | Y | Y | Y | Y | All components set position and size |
| hidden | `GSP_F_HIDDEN` | Y | Y | Y | Y | Y | Y | Y | Initial hidden state |
| name | `GSP_F_NAME` | Y | Y | Y | Y | Y | Y | Y | Runtime name lookup |
| params | `GSP_F_PARAMS` |  |  |  |  | Y | Y |  | list/wheel params-v1 items |
| bg color | `GSP_F_BG_COLOR`, `bg_color` | Y |  | Y |  | Y | Y | Y | Background color |
| fg color | `GSP_F_FG_COLOR`, `fg_color` |  | Y | Y |  | Y | Y |  | Text color |
| border | `GSP_F_BORDER`, `border_color/border_width` | Y |  | Y |  | Y | Y | Y | Border |
| radius | `GSP_F_RADIUS`, `radius` | Y |  | Y |  |  |  | Y | Container/button/layer radius |
| text | `GSP_F_TEXT`, `text_off` |  | Y | Y |  |  |  |  | Text content |
| font | `font_id` |  | Y | Y |  | Y | Y |  | Runtime font binding |
| text align | `GSP_F_ALIGN`, `text_align` |  | Y |  |  |  |  |  | button/list/wheel align not wired yet |
| callback | `GSP_F_CALLBACK`, `callback_off` |  |  | Y |  |  |  |  | Callback name binds to runtime function |
| image | `GSP_F_IMAGE`, `blob_idx` |  |  |  | Y |  |  |  | Blob decoded and bound as image source |
| opacity | `GSP_F_OPACITY`, `opacity` | reserved | reserved | reserved | reserved | reserved | reserved | reserved | Field reserved; current loader does not apply it |

### 7.3 Exporter Constraints

| Rule | Requirement |
|---|---|
| Property scope | Set only property flags supported by the target component matrix |
| Properties outside profile | Extend the protocol first, or define private data with `GSP_F_PARAMS` |
| `font_id` | Use only for text-like components; non-text components should use 0 |
| `callback_off` | Use only for interactive components; v3 currently consumes it only for button |
| `blob_idx` | Use only when `GSP_F_IMAGE` is set; v3 currently consumes it only for image |
| `opacity` | Treat as reserved; do not rely on runtime visual effect |

### 7.4 Device Constraints

| Stage | Requirement |
|---|---|
| Common validation | Validate offsets/indexes with Header/Object/Blob/String rules first |
| Property application | Apply properties according to Component Profile afterwards |
| Unsupported flag | Reject during bring-up; product behavior may ignore or reject based on version negotiation |
| Scene topology | Do not hard-code demo object order or widget names |

---

## 8. Blob Entry

Blob entry size: `GSP_BLOB_SIZE = 20`.

Blob entries describe baked binary resources, currently image pixels.

| Offset | Type | Field | Meaning |
|---:|---|---|---|
| `0` | `u16` | `w` | Image width |
| `2` | `u16` | `h` | Image height |
| `4` | `u8` | `cf` | `gfx_color_format_t` |
| `5` | `u8` | `codec` | `GSP_CODEC_*` |
| `6` | `u16` | `stride` | Row bytes, or `0` for default |
| `8` | `u32` | `raw_size` | Decoded byte size |
| `12` | `u32` | `comp_size` | Stored byte size |
| `16` | `u32` | `data_off` | Absolute offset to compressed bytes |

Current codecs:

| ID | Name | Meaning |
|---:|---|---|
| `0` | `GSP_CODEC_STORE` | Raw bytes stored as-is |
| `1` | `GSP_CODEC_RLE16` | RGB565 run-length encoding, token = `u16 count + u16 pixel` |

Blob validation:

- `blob_idx < blob_count`
- `data_off + comp_size <= total_size`
- `raw_size > 0`
- For `STORE`, `comp_size == raw_size`
- For `RLE16`, decoded bytes must exactly equal `raw_size`

Current image flow:

```text
BlobEntry
  -> blob_get()
      -> decode STORE/RLE16 into scene-owned RAM
      -> fill gfx_image_dsc_t
      -> gfx_image_set_source_desc()
```

---

## 9. String Table

Strings are UTF-8 and NUL-terminated.

Offsets are absolute byte offsets from package base.

The string table may contain:

- label text
- button text
- callback names
- object names

String table bytes are not required to be sorted. Exporter may deduplicate identical strings.

---

## 10. Font Binding Contract

Current v3 keeps font descriptions outside `home_scene_pkg[]`:

```c
static const gsp_font_desc_t home_fonts[] = { ... };
```

This is C metadata in the `.inc`, not part of the pointer-free binary runtime payload.

Object entries reference fonts through `font_id`. Runtime code maps:

```text
ObjEntry.font_id -> gsp_font_binding_t.id -> gfx_font_t
```

Exporter constraints:

- Every label/button `font_id` should have a matching `home_fonts[].id`.
- Font path is logical authoring metadata. Host may resolve it differently from device.

Loader constraints:

- If a `font_id` is not found, loader may use `default_font`.
- Device integration should decide whether fonts are loaded from file, flash asset, or prebuilt font handles.

Future direction:

- Move font table records into the binary package.
- Keep `gfx_font_t` handles runtime-only.

---

## 11. `.inc` Metadata Contract

The generated `.inc` may contain C preprocessor metadata for compile-time convenience:

```c
#define HOME_SCREEN_W
#define HOME_SCREEN_H
#define HOME_COLOR_FMT
#define HOME_OBJ_COUNT
#define HOME_FONT_COUNT
```

Important:

- `home_scene_pkg[]` header is the source of truth for runtime loading.
- `HOME_SCREEN_W/H` should match header `screen_w/screen_h`.
- `HOME_OBJ_COUNT` should match header `obj_count`.
- `HOME_COLOR_FMT` is not part of the v3 binary header yet.

Current `HOME_COLOR_FMT` meaning:

```text
0 = unspecified / display decides output format
```

Image color format is stored per blob in `BlobEntry.cf`.

If device/exporter needs a scene-level output format later, promote `HOME_COLOR_FMT` into a real header field or reserved header extension. Do not silently infer runtime framebuffer format from this macro today.

---

## 12. Exporter Requirements

An exporter that produces `.inc` must also produce a matching manifest.

Required outputs:

```text
home.inc
gsp_export/home.manifest.md
optional source previews, e.g. gsp_export/home_preview.bmp
```

Exporter must:

1. Fill all fields with fixed-width little-endian writes.
2. Use only indexes and offsets in runtime payload.
3. Compute table offsets after final layout is known.
4. Compute CRC after the full package is assembled.
5. Emit `home_scene_pkg_len = sizeof(home_scene_pkg)`.
6. Emit or update `gsp_export/home.manifest.md`.
7. Run loader validation after export.

Manifest must include:

- header values
- binary region layout
- object table
- string table
- font bindings
- blob table
- validation result

---

## 13. Device Loader Requirements

Device loader must validate before object creation:

1. header size
2. magic
3. version
4. total size
5. CRC
6. object table bounds
7. blob table bounds
8. object count limits
9. parent indexes
10. enabled string offsets
11. enabled params offsets
12. enabled blob indexes

Device loader should create objects only after enough validation is done to avoid partial bad trees where possible.

If creation fails halfway:

- destroy all objects already created
- free decoded blob buffers
- return an error code

---

## 14. Versioning

Current version:

```text
GSP_VERSION = 3
```

Version must change when any binary field layout changes.

Compatible changes that may not require a version bump:

- adding new object type IDs if old loaders reject unknown types
- adding new flag bits if old exporters do not emit them
- adding new codec IDs if old loaders reject unknown codecs

Incompatible changes that require a version bump:

- changing header/object/blob entry sizes
- reusing existing field offsets
- changing endian
- changing CRC coverage
- changing string/offset interpretation

Planned versions:

- **v4**: introduces the Action Table (event→action); header grows to `56` bytes, see §16.

---

## 15. Non-Protocol Scene Content

This protocol does not define any fixed scene object list.

The following are scene-specific and must be described by each package manifest, not by this protocol:

- object count
- object order
- widget names and roles
- parent-child tree shape
- text content
- callback names
- blob count and resource meaning
- actual colors, sizes, and positions

For example, `home.inc` currently has its own object table and blob table, but those details are not part of the general binary protocol. They belong in `gsp_export/home.manifest.md`.

The protocol only defines how an arbitrary package describes those things:

```text
ObjectEntry[N]  -> type, parent_idx, rect, flags, string offsets, blob index, font id
BlobEntry[M]    -> dimensions, format, codec, raw/comp sizes, data offset
String table    -> UTF-8 strings referenced by offset
```

Device loaders must implement the general rules above and must not hard-code any demo scene topology.

---

## 16. Action Table (v4 extension, proposed)

> Status: v4 proposal. The current v3 loader does not implement this section. It
> defines the **position-independent event→action table** needed for interactive
> scenes. It mirrors ITE's `ITUAction[]` (event-triggered, target by name/index,
> with a parameter), but the runtime payload contains **only u16 indices + u32
> offsets and no native pointers**, so the same bytes parse identically on
> host(64) / device(32) and every reference is bounds-checkable.

### 16.0 Purpose

Turn behavior into data: which source object, on which event, does what action to
which target object, with what parameter. Interaction then needs no firmware
change — the exporter / LLM emits the action table and the loader dispatches it.

### 16.1 v4 Header Changes

v4 grows the header from `48` to `GSP_HEADER_SIZE_V4 = 56` bytes, adding two
fields (a version-bump change per §14):

| Offset | Type | Field | Meaning |
|---:|---|---|---|
| `44` | `u32` | `action_count` | number of action entries; `0` = no action table (equals v3 behavior) |
| `48` | `u32` | `action_table_off` | absolute offset of the action table; `0` when `action_count==0` |
| `52` | `u32` | `reserved` | must be `0` in v4 |

- The v3 `reserved` at off 44 is redefined as `action_count` in v4. Since v3
  requires it to be `0`, a v4 package is rejected by an old v3 loader on the
  `version` check (never misread), so the compatibility boundary is safe.

### 16.2 Runtime Package Layout (v4)

```text
[Header 56B]
[Object table: obj_count * 64B]
[Blob table: blob_count * 20B]
[Action table: action_count * 24B]     <- new in v4
[Params area]
[String table]
[Blob data]
```

Offset relations (v4):

```text
obj_table_off    = GSP_HEADER_SIZE_V4                        (= 56)
blob_table_off   = obj_table_off    + obj_count    * GSP_OBJ_SIZE
action_table_off = blob_table_off   + blob_count   * GSP_BLOB_SIZE
params_off       = action_table_off + action_count * GSP_ACTION_SIZE
str_table_off    = params_off + params_total_size
blob_data_off    = str_table_off + string_table_size
total_size       = blob_data_off + blob_data_size
```

### 16.3 Action Entry

Action entry size: `GSP_ACTION_SIZE = 24`.

| Offset | Type | Field | Meaning |
|---:|---|---|---|
| `0` | `u16` | `src_idx` | source object index that emits the event; `GSP_ACTION_GLOBAL = 0xFFFF` = scene-level (e.g. ENTER/LEAVE/TIMER) |
| `2` | `u16` | `event` | `GSP_EV_*`, trigger condition |
| `4` | `u16` | `action` | `GSP_ACT_*`, action to run |
| `6` | `u16` | `target_idx` | target object index; `GSP_ACTION_NO_TARGET = 0xFFFF` = no index target (use name or act on src) |
| `8` | `u32` | `target_name_off` | optional: absolute offset of target/callback name string; `0` = use `target_idx` |
| `12` | `u32` | `param_off` | optional: absolute offset of a param blob (e.g. SET_TEXT string); `0` = none |
| `16` | `u16` | `param_len` | param blob length in bytes |
| `18` | `u16` | `flags` | `GSP_AF_*` (reserved, must be `0` in v4) |
| `20` | `u32` | `arg` | inline small-integer argument (e.g. GOTO scene id, SET_BG_COLOR RGB888, SET_VAR slot/value, delay frames) |

### 16.4 Event Enum `GSP_EV_*` (append-only, frozen values)

| Value | Symbol | Meaning |
|---:|---|---|
| `0` | `GSP_EV_CLICK` | press then release inside widget (click) |
| `1` | `GSP_EV_PRESS` | press |
| `2` | `GSP_EV_RELEASE` | release |
| `3` | `GSP_EV_LONG_PRESS` | long press |
| `4` | `GSP_EV_VALUE_CHANGED` | value changed (slider/list/etc.) |
| `5` | `GSP_EV_TIMER` | periodic (scene-level) |
| `6` | `GSP_EV_ENTER` | scene entered |
| `7` | `GSP_EV_LEAVE` | scene left |
| `8` | `GSP_EV_SLIDE_LEFT` | slide left |
| `9` | `GSP_EV_SLIDE_RIGHT` | slide right |
| `10` | `GSP_EV_SLIDE_UP` | slide up |
| `11` | `GSP_EV_SLIDE_DOWN` | slide down |
| `100` | `GSP_EV_CUSTOM` | custom event (reserved gap for extension) |

### 16.5 Action Enum `GSP_ACT_*` (append-only, frozen values)

| Value | Symbol | Fields used | Meaning |
|---:|---|---|---|
| `0` | `GSP_ACT_NONE` | — | no-op |
| `1` | `GSP_ACT_SHOW` | `target_idx` | show target |
| `2` | `GSP_ACT_HIDE` | `target_idx` | hide target |
| `3` | `GSP_ACT_TOGGLE` | `target_idx` | toggle visibility |
| `4` | `GSP_ACT_SET_TEXT` | `target_idx`, `param_off/len` | set target text |
| `5` | `GSP_ACT_SET_BG_COLOR` | `target_idx`, `arg`(RGB888) | set background color |
| `6` | `GSP_ACT_SET_OPACITY` | `target_idx`, `arg`(0..255) | set opacity |
| `7` | `GSP_ACT_ENABLE` | `target_idx` | enable |
| `8` | `GSP_ACT_DISABLE` | `target_idx` | disable |
| `9` | `GSP_ACT_GOTO_SCENE` | `arg`(scene id) | navigate to scene |
| `10` | `GSP_ACT_BACK` | — | back to previous scene |
| `11` | `GSP_ACT_CALL` | `target_name_off` | call host-registered function (name binding, same as current callback) |
| `12` | `GSP_ACT_SET_VAR` | `arg`(slot/value), `param_off`(opt) | write runtime variable (data binding) |
| `13` | `GSP_ACT_PLAY_ANIM` | `target_idx`, `arg` | play animation |
| `14` | `GSP_ACT_STOP_ANIM` | `target_idx` | stop animation |
| `100` | `GSP_ACT_CUSTOM` | `param_off/len` | custom action (reserved gap) |

Unknown `event` / `action`: reject during bring-up (`GSP_ERR_ACTION`); may be
ignored in production via version negotiation.

### 16.6 Validation Rules

- `src_idx == GSP_ACTION_GLOBAL` or `src_idx < obj_count`.
- `target_idx == GSP_ACTION_NO_TARGET` or `target_idx < obj_count`.
- if `target_name_off != 0`: offset within `[0, total_size)` and NUL-terminated before `total_size`.
- if `param_off != 0`: `param_off + param_len <= total_size`.
- `flags` must be `0` (v4).
- `event` / `action` within defined enums (see 16.5 for unknown handling).
- All references are index/offset; after tree build the loader resolves
  `src_idx/target_idx` to `gfx_object_t*` caches — **pointers exist only at runtime**.

### 16.7 Runtime Dispatch Model (loader)

```text
1. Build the object tree per §13.
2. Walk the action table, validate each entry (16.6); resolve src_idx/target_idx
   to gfx_object_t*, resolve target_name_off (if any) to a callback name, and
   build a runtime action list rt_actions[].
3. For each source object that has inbound event actions, hook the low-level
   listener by event kind:
     - touch (CLICK/PRESS/RELEASE/LONG_PRESS/SLIDE_*) -> synthesized via gfx_object_set_touch_cb
     - VALUE_CHANGED -> widget value-change callback
     - TIMER/ENTER/LEAVE -> scene-level hooks
4. On event: scan rt_actions[] for matching (src, event) and run each action:
     SHOW/HIDE/TOGGLE/SET_* -> call the matching gfx setter on target
     GOTO_SCENE/BACK        -> hand to the scene manager
     CALL                   -> look up host function by target_name in the cb table
     SET_VAR                -> write the runtime variable table (triggers binding refresh)
5. On any validation failure: destroy created objects/decoded blobs, return an
   error code, do not crash (same as §13).
```

- Dispatch relies only on runtime-resolved pointer caches; **the package always
  holds index/offset**, per the §1 core rules.

### 16.8 Contrast with ITU (why this is position-independent)

| Dimension | ITU `ITUAction[]` | GSP Action Table |
|---|---|---|
| Target reference | inline `char target[N]` + runtime `void* cachedTarget` | `target_idx`(u16) / `target_name_off`(u32); pointer cached only at runtime |
| Parameter | inline fixed `char param[64]` | variable `param_off/len` + inline `arg` |
| Attachment | inline `actions[N]` per widget/layer | separate action table keyed by `src_idx` (no struct bloat) |
| Platform | 32-bit memory image + pointer relocation | position independent, identical host/device parse |
| Bad package | wild pointers, hard to validate | all index/offset, fully bounds-checked |

In one line: **adopt ITU's "behavior-as-data" idea, but replace "inline struct +
pointer" with "table + index/offset".**

### 16.9 Exporter Requirements

- Write the action table only when `action_count > 0`, and set the header
  `action_count` / `action_table_off` accordingly.
- `src_idx` / `target_idx` must reference existing objects; cross-scene targets
  are only reachable via `GSP_ACT_GOTO_SCENE`.
- `GSP_ACT_CALL` `target_name_off` must resolve in the host callback binding
  table (otherwise ignored at runtime).
- The action table participates in `total_size` and CRC (like every other region).
- The manifest must list the action table (src/event/action/target/param) for
  review and LLM alignment.

### 16.10 Versioning

- Introducing the action table = **v4** (header size changes, must bump per §14).
- A v4 package with `action_count == 0` is semantically equal to v3 (static scene).
