---
name: esp-emote-gfx-design-review
description: Review or design esp_emote_gfx modules with a fixed checklist covering call chain, file placement, naming, log style, error handling, render extensibility, swap policy, and color-type consistency. Use when auditing widget/core graphics code or proposing standards for this repository.
---

# ESP Emote GFX Design Review

Use this skill when the task is to review, refactor, or extend `esp_emote_gfx` architecture and coding style.

## Goal

Produce actionable design guidance, not a generic code summary.

Always review against the seven dimensions in [references/review_dimensions.md](references/review_dimensions.md).

## Workflow

1. Scan code-bearing files under `include/` and `src/`.
2. Sample `test_apps/main/` only when API usage or integration behavior matters.
3. Reconstruct the call chain before judging local style.
4. Separate stable architectural strengths from inconsistent local patterns.
5. Cite concrete files and lines for every important finding.
6. End with a repository-specific standard, not just defects.
7. If the task is to define or revise conventions, also read:
   - `docs/esp_emote_gfx_design_guidelines.md`
   - [references/code_standard_template.md](references/code_standard_template.md)

## What To Inspect First

- Core render path:
  - `src/core/object/gfx_widget_class.c`
  - `src/core/object/gfx_obj_priv.h`
  - `src/core/display/gfx_render.c`
  - `src/core/draw/gfx_blend.c`
  - `src/core/draw/gfx_sw_draw.c`
- Registration points:
  - `src/widget/anim/gfx_anim_decoder.c`
  - `src/widget/anim/gfx_anim_decoder_priv.h`
- Widget families:
  - `src/widget/img/`
  - `src/widget/face/`
  - `src/widget/label/`
  - `src/widget/basic/`
- Public headers:
  - `include/core/`
  - `include/widget/`

## Reference Loading

Load only the reference file you need:

- For the 7-dimension checklist:
  - [references/review_dimensions.md](references/review_dimensions.md)
- For output structure and review wording:
  - [references/output_contract.md](references/output_contract.md)
- For code standard drafting:
  - [references/code_standard_template.md](references/code_standard_template.md)

If the repository already contains a local design summary, treat it as prior art, not absolute truth.

## Reference

If a repository-local design summary exists, read it:

- `docs/esp_emote_gfx_design_guidelines.md`
