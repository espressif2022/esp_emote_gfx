# Face Spine-Like Runtime Design

## Why change

The current `image + deform` path in `esp_emote_gfx/test_apps/main/test_face_parts.c` and `test_mesh_emote.c` is driven by hand-written heuristics:

- the artist intent is compiled into C formulas instead of data
- mesh deformation is tied to one-off demo logic
- gaze, blink, smile, and head-follow are not authored in one coordinate system
- state transitions are easy to demo but hard to scale

`gfx_mesh_img` already provides the core render primitive we need:

- rest mesh points
- runtime mesh points
- image source switching

That means we do not need a full Spine port first. We can add a lightweight authoring/runtime layer on top of the existing renderer.

## Target architecture

Split the system into five layers.

### 1. Rig asset

Author once in a visual editor, export a compact binary:

- skeleton bones
- slots / draw order
- mesh attachments
- vertex skin weights
- constraints
- animation curves
- state machine graph
- controller parameter bindings

Recommended runtime file split:

- `face_rig.bin`: static rig data
- `face_anim.bin`: clips, curves, state graph
- `face_atlas.bin` or existing `gfx_image_dsc_t` assets: textures

### 2. Runtime graph

Per frame:

1. sample controller inputs
2. update state machine
3. evaluate animation tracks
4. solve bone local transforms
5. solve constraints / IK
6. skin mesh vertices
7. write final points to `gfx_mesh_img`
8. update non-mesh parts such as slot visibility or attachment switches

### 3. Controller layer

Inputs should be semantic parameters instead of raw formulas:

- `look_x`
- `look_y`
- `head_yaw`
- `head_pitch`
- `blink`
- `smile`
- `mouth_open`
- `phoneme`
- `emotion`
- `focus`

This is the layer fed by cursor/touch, audio viseme, IMU, or higher-level AI state.

### 4. Constraint layer

Use Spine-like constraints, but keep the first version intentionally small:

- bone parent hierarchy
- transform constraint
- aim / look-at constraint
- 2-bone IK
- weighted blend of authored poses

For face animation, this covers most of the value already:

- eyes track cursor with a limit cone
- head follows gaze with reduced amplitude
- upper/lower eyelids follow eyeball pitch
- mouth corners and cheeks are driven by smile / viseme bones

### 5. Render binding

Each slot binds to one of:

- `gfx_img`
- `gfx_mesh_img`

For a face, preferred structure is:

- head base: `gfx_img`
- left eye white / iris / highlight: one or more `gfx_mesh_img` or `gfx_img`
- right eye white / iris / highlight: one or more `gfx_mesh_img` or `gfx_img`
- mouth: `gfx_mesh_img`
- optional brows / cheeks / shadows: `gfx_mesh_img`

## Minimal runtime data model

Suggested C-side structures:

```c
typedef struct {
    int16_t x;
    int16_t y;
    int16_t rot_deg_x10;
    int16_t scale_x_permille;
    int16_t scale_y_permille;
    int16_t shear_x_permille;
    int16_t shear_y_permille;
    int16_t parent_idx;
} face_bone_bind_t;

typedef struct {
    uint16_t vertex_count;
    uint16_t bone_weight_offset;
    uint16_t rest_point_offset;
    uint16_t uv_offset;
    uint16_t triangle_offset;
    uint16_t triangle_count;
    uint16_t texture_id;
    uint16_t slot_idx;
} face_mesh_t;

typedef struct {
    uint8_t bone_idx;
    uint8_t weight_permille;
    int16_t local_x;
    int16_t local_y;
} face_vertex_influence_t;

typedef struct {
    uint8_t type;
    uint8_t target_bone_idx;
    uint8_t source_a_idx;
    uint8_t source_b_idx;
    int16_t mix_permille;
    int16_t limit_min;
    int16_t limit_max;
} face_constraint_t;

typedef struct {
    const face_bone_bind_t *bones;
    const face_mesh_t *meshes;
    const face_vertex_influence_t *weights;
    const face_constraint_t *constraints;
    uint16_t bone_count;
    uint16_t mesh_count;
    uint16_t constraint_count;
} face_rig_t;
```

Runtime working set:

```c
typedef struct {
    face_transform_t local;
    face_transform_t world;
} face_bone_pose_t;

typedef struct {
    gfx_obj_t *obj;
    const face_mesh_t *mesh;
    gfx_mesh_img_point_q8_t *deformed_points;
} face_slot_runtime_t;

typedef struct {
    const face_rig_t *rig;
    face_bone_pose_t *bones;
    face_slot_runtime_t *slots;
    face_param_value_t params[FACE_PARAM_COUNT];
    face_state_machine_t sm;
} face_runtime_t;
```

## Binary export format

Keep it simple and streaming-friendly.

### File layout

```text
header
string table
bone table
slot table
mesh table
rest point blob
weight blob
constraint table
parameter table
clip table
curve blob
state machine blob
```

### Design rules

- little-endian
- 4-byte aligned sections
- all references are offsets, not pointers
- versioned header
- optional CRC
- editor exports quantized integers where possible

Recommended quantization:

- positions: `int16`
- rotation: `int16` fixed-point
- weights: `uint8` or `uint16`
- curve values: `int16`

This matches the user's goal of a very small binary parameter file.

## What to reuse from current code

### Reuse directly

- `gfx_mesh_img_set_points()`
- `gfx_mesh_img_set_rest_points()`
- `gfx_mesh_img_set_points_q8()`
- `gfx_mesh_img_set_rest_points_q8()`
- `gfx_img_set_src()`
- `gfx_timer_create()`

### Replace

- per-demo hand-authored deformation formulas in `test_face_parts.c`
- per-demo expression-to-pose math in `test_mesh_emote.c`
- hard-coded action arrays as the primary authoring format

### Keep only as temporary compatibility layer

- current `test_mesh_emote_clip_t`
- current blink / hold-motion helper logic

Those can become:

- test input generators
- fallback procedural layers on top of authored clips

## Suggested module split

Add new modules on top of `esp_emote_gfx`:

- `include/face/face_rig.h`
- `include/face/face_runtime.h`
- `include/face/face_state_machine.h`
- `src/face/face_rig.c`
- `src/face/face_runtime.c`
- `src/face/face_constraints.c`
- `src/face/face_skinning.c`
- `src/face/face_state_machine.c`
- `src/face/face_asset_loader.c`

## Frame evaluation

Per tick:

```c
void face_runtime_tick(face_runtime_t *rt, uint32_t dt_ms)
{
    face_state_machine_update(&rt->sm, rt->params, dt_ms);
    face_eval_tracks(rt);
    face_eval_bone_hierarchy(rt);
    face_eval_constraints(rt);
    face_eval_mesh_skinning(rt);
    face_push_slots(rt);
}
```

`face_push_slots()` should only touch gfx objects. Everything before that should stay renderer-agnostic.

## Migration path from current demo

### Phase 1

Create a runtime with no IK yet:

- one head root bone
- left eye root bone
- right eye root bone
- jaw bone
- mouth corner bones

Drive current `face_parts_*` assets through authored weights instead of formulas.

Expected outcome:

- same assets
- much better artist control
- no more C-side mouth deformation heuristics

### Phase 2

Add constraints:

- eye look-at
- head follow
- lid follow
- jaw open limit

Expected outcome:

- cursor follow and head perspective are data-authored
- less tuning in runtime code

### Phase 3

Replace `test_mesh_emote` clip tables with exported clips and state machine.

Expected outcome:

- runtime only reads data
- animation iteration moves to the editor

### Phase 4

Add optional advanced features only if needed:

- additive animation layers
- per-slot color / alpha animation
- mesh attachment swap
- lip-sync track
- event callbacks

## Practical constraint for ESP-class devices

Do not chase full Spine compatibility first. Keep the runtime deliberately narrow:

- max 16 to 24 bones
- max 6 to 8 weighted influences per vertex offline, reduced to 2 to 4 online
- max 4 to 6 mesh slots on screen
- fixed memory pools where possible

This keeps CPU and RAM predictable while still giving artists a much better workflow.

## Immediate next implementation step

The best first deliverable is not a full editor. It is a runtime test that proves the data model:

1. create one `face_runtime_demo.c`
2. define a tiny rig in C tables first
3. skin the existing mouth mesh and eye meshes from bone transforms
4. expose semantic params like `look_x`, `look_y`, `smile`, `blink`
5. replace `test_face_parts_apply_mouth_pose()` with bone-driven skinning

If that lands cleanly, the editor/exporter can be developed against a stable runtime contract instead of today's demo-specific math.

## Bottom line

Your direction is the right one.

For this codebase, the missing piece is not “better deform formulas”. The missing piece is a data-driven face rig runtime:

- artists author relationships visually
- exporter writes compact rig data
- runtime only evaluates bones, constraints, skinning, and state transitions

That is the clean path from today's demo code to a maintainable production facial system.

## Setup guide

Use this document as the working contract for the face-rig migration.

### Working rules

- keep runtime code data-driven where possible
- prefer semantic params like `smile`, `mouth_open`, `look_x`
- treat demo formulas only as temporary scaffolding
- whenever a milestone lands, update the progress tracker below
- whenever a new task appears, add it to the task list below before implementing it

### Recommended implementation order

1. prove one isolated feature with a standalone test case
2. replace direct per-vertex heuristics with bone-driven control
3. replace textured placeholder rendering with slot-based display primitives
4. add auto-play clips or a tiny state machine
5. extract reusable runtime helpers from the demo into shared modules

### Current reference testbeds

- `esp_emote_gfx/test_apps/main/test_face_parts.c`
- `esp_emote_gfx/test_apps/main/test_mesh_emote.c`
- `esp_emote_gfx/test_apps/main/test_face_emote.c`

## Procedural rig layer — `test_face_emote.c` reference architecture

This section documents the concrete five-layer architecture that is already
implemented in `test_face_emote.c`.  It serves as the ground-truth contract
for the future full runtime described above, and as the immediate design
reference for everyone working on the demo today.

### Design goals

- Every parameter that affects how a widget looks is **explicit** in a named
  struct field — no formula indirection, no reconstructed weights.
- Repositioning any widget is a one-line constant change with no solver edits.
- Adding a new expression is a one-line table entry.
- Transitioning between expressions is handled by a single shared easing
  function — the expression table never encodes timing details beyond
  `hold_ticks`.

---

### Layer 0 — Display geometry and screen anchors

All widget positions are expressed as **absolute pixel coordinates** from the
display top-left corner (y-axis pointing down).  Changing a widget's position
requires only editing the corresponding constant.

```c
/* Display dimensions — adjust to match your panel */
#define FACE_DISPLAY_W    320
#define FACE_DISPLAY_H    240
#define FACE_DISPLAY_CX   (FACE_DISPLAY_W / 2)   /* 160 */
#define FACE_DISPLAY_CY   (FACE_DISPLAY_H / 2)   /* 120 */

/* Widget anchor centres, absolute screen pixels                         */
/*   Mouth anchor — centre of the invisible bone-mesh object             */
#define FACE_MOUTH_CX           FACE_DISPLAY_CX          /*  160 px */
#define FACE_MOUTH_CY          (FACE_DISPLAY_CY + 50)    /*  170 px */
#define FACE_MOUTH_OBJ_W        160
#define FACE_MOUTH_OBJ_H         80

/*   Eyes                                                                */
#define FACE_LEFT_EYE_CX       (FACE_MOUTH_CX - 52)      /*  108 px */
#define FACE_LEFT_EYE_CY       (FACE_MOUTH_CY - 58)      /*  112 px */
#define FACE_RIGHT_EYE_CX      (FACE_MOUTH_CX + 52)      /*  212 px */
#define FACE_RIGHT_EYE_CY       FACE_LEFT_EYE_CY

/*   Eyebrows (raise offset applied at runtime per expression)           */
#define FACE_LEFT_BROW_CX       FACE_LEFT_EYE_CX         /*  108 px */
#define FACE_LEFT_BROW_CY      (FACE_LEFT_EYE_CY - 38)   /*   74 px */
#define FACE_RIGHT_BROW_CX      FACE_RIGHT_EYE_CX        /*  212 px */
#define FACE_RIGHT_BROW_CY      FACE_LEFT_BROW_CY
```

Initial object placement uses `GFX_ALIGN_CENTER` with the offset derived from
the constants above (`FACE_*_CX - FACE_DISPLAY_CX`, `FACE_*_CY - FACE_DISPLAY_CY`).
The vertex solver then repositions the mesh bounding box on every tick via
`GFX_ALIGN_TOP_LEFT`.  To move a widget, change only the `FACE_*` constant;
the initialisation offset is computed automatically from the same value.

---

### Layer 1 — Widget keyframe types

Each widget's visible state is captured in a compact keyframe struct.  These
are the **only** parameters the solver functions read.

```c
/* Eye shape.
 *   ry_top    : upper eyelid arc height [px].  Smaller → squints down.
 *   ry_bottom : lower eyelid arc height [px].  Smaller → squints up.
 *   tilt      : left eye only; +ve = outer-left corner droops.
 *               Right eye solver always receives -tilt for mirroring.
 */
typedef struct {
    int16_t ry_top;
    int16_t ry_bottom;
    int16_t tilt;
} face_eye_kf_t;

/* Eyebrow shape.
 *   arch   : centre arch height [px], +ve = convex upward.
 *            Negative = V-shape (angry).
 *   raise  : whole-brow vertical offset [px], +ve = moves up.
 *   tilt   : same sign convention as eye.tilt.
 */
typedef struct {
    int16_t arch;
    int16_t raise;
    int16_t tilt;
} face_brow_kf_t;

/* Mouth shape.
 *   smile : [-42..+42]  corner height.  +ve = smile, -ve = frown.
 *   open  : [0..40]     jaw opening depth.
 */
typedef struct {
    int16_t smile;
    int16_t open;
} face_mouth_kf_t;
```

---

### Layer 2 — Expression preset

An expression preset fully specifies the **target keyframe** for every widget
simultaneously, plus a `hold_ticks` duration.

```c
typedef struct {
    const char      *name;        /* display name, also used for labels   */
    face_eye_kf_t    eye;         /* both eyes; right eye mirrors tilt    */
    face_brow_kf_t   brow;        /* both brows; right brow mirrors tilt  */
    face_mouth_kf_t  mouth;
    uint16_t         hold_ticks;  /* frames to hold after settling        */
} face_expr_t;
```

The expression **library** is a flat `const face_expr_t[]` table.  Adding a
new expression or tweaking an existing one requires only editing one row:

```c
/*  name          ry_top ry_bot  e_tilt   arch  raise  b_tilt  smile  open  hold */
{ "neutral",  { 24,  20,   0 }, { 4,   0,   0 }, {  0,  2 },  28U },
{ "happy",    {  5,  14,  -2 }, { 1,   2,  -1 }, { 42, 10 },  28U },
{ "sad",      { 22,  18,   7 }, { 4,  -1,   7 }, {-36,  4 },  28U },
{ "surprise", { 34,  26,   0 }, { 7,   7,   0 }, {  0, 30 },  24U },
{ "angry",    { 22,  18,  -6 }, {-4,  -3,  -6 }, {-28, 10 },  24U },
/* … */
```

---

### Layer 3 — Per-widget animation state

Each widget has its own `cur` (current) and `tgt` (target) keyframe pair
stored in the scene state struct.  The easing function advances `cur` toward
`tgt` every tick.

```c
face_eye_kf_t    eye_cur,   eye_tgt;
face_brow_kf_t   brow_cur,  brow_tgt;
face_mouth_kf_t  mouth_cur, mouth_tgt;
```

When the expression sequence advances, the code simply copies the new
expression's keyframes into `*_tgt`.  The easing layer handles the rest.

---

### Layer 4 — Easing / transition function

A single function type drives all interpolations.

```c
/*
 * face_ease_fn_t — steps `cur` one tick closer to `tgt`.
 * Returns the new value of `cur`.
 */
typedef int16_t (*face_ease_fn_t)(int16_t cur, int16_t tgt);

/* Default: exponential spring — step = diff/4, minimum ±1.
 * Full 42-unit transition completes in ≈ 5 ticks (165 ms at 33 ms/tick). */
static int16_t face_ease_spring(int16_t cur, int16_t tgt);
```

Swapping the easing function (e.g. linear, smoothstep, ease-in-out) is a
one-line change per widget.

---

### Layer 5 — Vertex solvers

Each widget has a dedicated solver function that translates a keyframe into
`gfx_mesh_img_point_q8_t` vertices and calls `gfx_obj_align(GFX_ALIGN_TOP_LEFT)`
with absolute coordinates.  Solvers are pure functions: same keyframe → same
vertices, no hidden state.

```c
/* Eye: lens (almond) mesh using quadratic parabola arc weighting.        */
esp_err_t face_solve_eye(gfx_obj_t *obj,
                          int32_t screen_cx, int32_t screen_cy,
                          int32_t rx, const face_eye_kf_t *kf,
                          bool mirror);

/* Eyebrow: curved band mesh using cubic-smoothstep arc weighting.        */
esp_err_t face_solve_brow(gfx_obj_t *obj,
                           int32_t screen_cx, int32_t screen_cy,
                           int32_t half_w, int32_t thickness,
                           const face_brow_kf_t *kf,
                           bool mirror);

/* Mouth: bone-driven procedural mesh (five lip components).              */
esp_err_t face_solve_mouth(test_face_emote_scene_t *scene,
                            const face_mouth_kf_t *kf);
```

All three solvers use `smooth_tent` or the quadratic parabola arc function
internally; they never read expression weights or blended parameters.

---

### Raster quality and performance findings

Current best visual result on the procedural face test comes from keeping the
Bezier solver and mesh deformation in `Q8` all the way into the triangle
rasterizer, while **not** enabling expensive edge supersampling.

Observed practical result:

- integer mesh points gave the fastest frame time, but left a small visible
  stair-step on high-contrast mouth / brow edges
- soft-edge source textures improved the edge slightly, but introduced a dark
  halo because the alpha-softened texture was stretched across the whole mesh,
  not only the silhouette
- aggressive triangle AA (`MSAA16`-style coverage sampling) improved the edge
  only slightly, but pushed frame time from roughly `10+ ms` to `140+ ms`,
  which is not usable
- increasing mesh segment counts also added CPU cost with only modest visual
  benefit
- the best tradeoff so far is:
  - hard white source image
  - default segment counts (`24 / 24 / 32`)
  - `Q8` mesh point storage and `Q8` triangle vertex input
  - lightweight edge AA only
  - resulting frame time around `30-40 ms`

Root cause analysis:

- the largest quality gain came from removing the **early integer rounding**
  between Bezier evaluation and rasterization
- once vertices stay in `Q8`, the rasterizer sees a more accurate silhouette,
  so even a cheap AA pass behaves better
- the largest performance loss came from per-edge extra sampling work, not from
  the `Q8` coordinate storage itself
- for this workload, mesh count and per-cell triangle count already create a
  large pixel loop, so multiplying edge work by 4x or 16x is too expensive on
  ESP-class hardware

Conclusion for runtime design:

- subpixel vertex input is worth keeping as a baseline renderer capability
- heavy coverage AA should be optional and off by default
- source-texture soft edges are the wrong fix for white procedural ribbons
- if more edge quality is still needed later, a dedicated ribbon/path scanline
  filler is more promising than pushing generic textured-triangle AA harder

Redundant or low-value processing identified in this experiment:

- `RGB565A8` soft-edge brush source for the procedural white face widgets:
  low value, can be removed from the default path
- `MSAA16` edge coverage evaluation in the triangle rasterizer:
  too expensive for the measured gain, keep only as a debug/experiment path if
  retained at all
- boosted mesh segment counts above the original demo defaults:
  modest quality gain, clear CPU increase
- duplicate edge-shaping strategies stacked together:
  using soft-edge textures, extra segment density, and heavy triangle AA at the
  same time is redundant because all three try to hide the same silhouette error

Recommended default policy going forward:

- keep `Q8` mesh points and subpixel triangle vertices
- keep default mesh segment counts unless a specific slot proves it needs more
- keep hard white procedural sources for ribbon-like widgets
- keep triangle AA lightweight and deterministic
- measure any future quality tweak against both frame time and visible gain

---

### Per-tick evaluation order

```
1. timer fires (33 ms)
2. anim_cb:
   a. face_ease_spring applied to each of the 8 keyframe fields
   b. face_solve_mouth  → bone pose → lip-strip + corner-arc meshes
   c. face_solve_eye    (left,  +tilt)
   d. face_solve_eye    (right, -tilt)
   e. face_solve_brow   (left,  +tilt)
   f. face_solve_brow   (right, -tilt)
   g. hint label updated from current expression name
3. settle check: if all cur == tgt, increment hold_tick
4. when hold_tick >= hold_ticks: advance expr_idx, load next expression targets
```

---

### How to customise

| Want to …                          | Edit …                                              |
|------------------------------------|-----------------------------------------------------|
| Move an eye / brow                 | Change `FACE_LEFT_EYE_CX` / `FACE_LEFT_BROW_CY` … |
| Resize eyes                        | Change `EYE_RX`, `EYE_RY`                          |
| Add an expression                  | Append one row to `s_face_sequence[]`               |
| Change the demo story              | Reorder / remove rows in `s_face_sequence[]`        |
| Tune a transition speed            | Replace `face_ease_spring` with a custom easing fn |
| Change eye shape (round vs almond) | Swap parabola ↔ smooth_tent in `face_solve_eye`     |
| Add a wink or per-eye expression   | Extend `face_eye_kf_t` with `left`/`right` pairs   |

---

## Progress tracker

Single source of truth for delivery status. Keep this board updated on every milestone.

### Board rules

- Date format: `YYYY-MM-DD`
- Owner: Git name or handle (current default: `xuxin`)
- Status values: `TODO`, `IN_PROGRESS`, `BLOCKED`, `DONE`
- Update order:
  - move card in `Active board`
  - append summary to `Change log`
  - if unfinished, keep next action in `Backlog`

### Active board

| ID | Task | Date | Owner | Status | Notes |
|----|------|------|-------|--------|-------|
| FSR-001 | Face runtime architecture doc + migration path | 2026-03-24 | xuxin | DONE | this document baseline completed |
| FSR-002 | Face emote widget testbed (`test_face_emote.c`) | 2026-03-24 | xuxin | DONE | standalone demo available |
| FSR-003 | Five-layer runtime structure in demo | 2026-03-24 | xuxin | DONE | L0-L5 contract in place |
| FSR-004 | Auto expression playback + settle/hold sequencing | 2026-03-24 | xuxin | DONE | timer-driven sequence validated |
| FSR-005 | White procedural lip strips + corner arcs | 2026-03-24 | xuxin | DONE | replaced texture-shaped mouth rendering |
| FSR-006 | Web editor source-of-truth (`test_apps/face_expressions_vivid.html`) | 2026-03-24 | xuxin | DONE | page now exports runtime-ready `.inc` tables |
| FSR-007 | Direct `.inc` export workflow for face assets | 2026-03-24 | xuxin | DONE | exported tables map directly to `gfx_face_emote_*` types |
| FSR-008 | Runtime integration of exported tables (`face_emote_expr_assets.inc`) | 2026-03-24 | xuxin | DONE | standard widget now consumes exported include directly |
| FSR-009 | Legacy JSON/Python keyframe pipeline retirement | 2026-03-24 | xuxin | DONE | old path documented as deprecated |
| FSR-010 | One-command export+sync helper | 2026-03-24 | xuxin | TODO | optional browser-side or build-side convenience only |
| FSR-011 | Debug overlay (bones/control points/anchors) | 2026-03-24 | xuxin | TODO | pending runtime debug switch |
| FSR-012 | Phoneme presets (`A/E/I/O/U`) in web editor + export preview | 2026-03-24 | xuxin | TODO | pending data pass |
| FSR-013 | Extract mouth/eye/brow solvers to `src/face` | 2026-03-24 | xuxin | TODO | pending module split |
| FSR-014 | `face_runtime_demo.c` with shared abstractions | 2026-03-24 | xuxin | TODO | pending after solver split |
| FSR-015 | Draft binary schema (rig/clip/version/CRC) | 2026-03-24 | xuxin | TODO | pending format doc |

### Change log

| Date | Owner | Status | Change |
|------|-------|--------|--------|
| 2026-03-24 | xuxin | DONE | Switched face asset source-of-truth to `face_expressions_vivid.html` direct `.inc` export |
| 2026-03-24 | xuxin | DONE | Aligned exported table types with `gfx_face_emote_*` runtime API |
| 2026-03-24 | xuxin | DONE | Reworked this section into date/owner/status board format |
| 2026-03-24 | xuxin | DONE | Documented raster quality/performance findings: Q8 vertices worth keeping, heavy AA and soft-edge source not worth default cost |

### Backlog

| Priority | Task | Date | Owner | Status | Next action |
|----------|------|------|-------|--------|-------------|
| P1 | one-command export sync helper | 2026-03-24 | xuxin | TODO | only if browser export copy/paste becomes painful |
| P1 | debug overlay for mouth/eye/brow anchors | 2026-03-24 | xuxin | TODO | define overlay toggles and draw API |
| P1 | phoneme preset pass in web editor | 2026-03-24 | xuxin | TODO | add `A/E/I/O/U` rows and export coverage |
| P2 | solver extraction into reusable modules | 2026-03-24 | xuxin | TODO | create `src/face/face_*_solver.c` skeletons |
| P2 | runtime demo unification | 2026-03-24 | xuxin | TODO | add `face_runtime_demo.c` and shared interfaces |
| P3 | binary schema draft | 2026-03-24 | xuxin | TODO | write v0 header + section table draft |
