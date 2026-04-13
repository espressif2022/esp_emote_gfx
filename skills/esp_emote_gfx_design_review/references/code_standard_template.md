# Code Standard Template

When the user asks for a standard or convention set, cover these sections:

## Header And Source Placement

- Public API in `include/`
- Internal contracts in `src/<domain>/*_priv.h`
- Avoid representation leakage in public headers

## Naming

- Public API naming rule
- Internal helper naming rule
- Struct suffix rules: `*_cfg_t`, `*_state_t`, `*_ops_t`, `*_desc_t`

## Logging

- Logging family
- Message casing
- Message grammar
- Boundary for error vs warning vs debug

## Error Handling

- Public API validation macros
- Constructor failure policy
- Callback/runtime failure policy

## Rendering And Registration

- Where registration is required
- Which layers may know about backends
- Which layers may know about payload formats

## Color And Swap

- Semantic color type
- Raw pixel type
- Single swap handoff rule

## `void *` Usage

- Allowed internal cases
- Disallowed public cases
- Preferred typed descriptor replacements
