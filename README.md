# Orison

Orison is a readable systems programming language with explicit ownership, explicit unsafe boundaries, modern tooling, and a surface syntax designed to welcome new systems engineers.

## Project goals

- Make systems programming more approachable without hiding core machine concepts.
- Keep memory, ownership, allocation, access control, concurrency, and unsafe operations explicit.
- Provide a strong statically typed language with practical low-level control.
- Ship a serious toolchain with compiler, formatter, and language server support.
- Preserve a path toward zero-cost abstractions through representation and compiler design, not surface complexity.

## Current design direction

Orison currently aims for:

- static, explicit, mostly nominal typing
- ownership-aware access modes: owned, `shared`, `exclusive`, raw pointers
- algebraic data types via `record` and `choice`
- record construction with explicit field-order constructor calls
- generics and constrained polymorphism
- interface-based abstraction via `interface` and `implements`
- three-level access control: `public`, `package`, `private`
- indentation-based primary syntax
- word-based boolean operators: `and`, `or`, `not`
- named bitwise operators: `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`
- ternary conditional expressions with `?:`
- null-safe member access with `?.`
- explicit `unsafe` boundaries
- first-class raw addresses and MMIO support
- package imports and explicit FFI declarations
- OS threads, runtime tasks, and async functions integrated with the safety model

## Core documents

- `ORISON_SPEC.md` - language feature and keyword reference
- `ORISON_TOUR.md` - end-to-end syntax tour
- `docs/adr/` - architecture decision records

## Tooling plan

The intended tool suite is:

- `orisonc` - compiler driver
- `orisonls` - language server
- `orisonfmt` - formatter

Each tool should be distributed as a monolithic statically linked executable. Internally, the implementation should remain modular and library-oriented.

## Minimal compiler demo

The smallest checked-in program is `examples/minimal.or`:

```text
package demo.minimal

function main() -> UInt32
    0 as UInt32
```

Configure, build, compile, link, and run it with:

```sh
cmake -S . -B build
cmake --build build -j 16
build/tools/orisonc/orisonc run examples/minimal.or
```

The demo exits successfully with status `0`. Use `--build examples/minimal.or -o build/minimal` to retain the
executable. The same file can be inspected with `--emit-llvm` or compiled with `--emit-object`.

Focused examples for the language-tour sections live under `examples/tour_*.or`. See `examples/README.md` for
the feature and validation matrix, including the runnable C `printf` hello-world example.

## Dynamic-Array Cleanup Audit Demo

The checked-in report-only fixture for the dynamic-array cleanup audit chain is
`tests/fixtures/dynamic_array_cleanup_audit.or`. Inspect the full metadata chain with:

```sh
build/tools/orisonc/orisonc --dynamic-array-cleanup-audit tests/fixtures/dynamic_array_cleanup_audit.or
```

Runtime-index partial-owner cleanup metadata for computed-index constructor moves can be inspected with:

```sh
build/tools/orisonc/orisonc --runtime-indexed-cleanup-audit tests/fixtures/choice_constructor_multi_variant_computed_index_member_path_move_rejected.or
```

This prints semantic descriptor origins, descriptor cleanup plans, cleanup obligations, sequence plans, verification,
emission gate, capability proof, and production-readiness status in order. Ordinary `--emit-llvm` keeps computed-index
constructor moves rejected; this audit command explicitly enables the runtime-index constructor-move, cleanup-emission,
module-insertion, and module-mutation gates for inspection.

To inspect the default production gate without enabling constructor-move acceptance, run:

```sh
build/tools/orisonc/orisonc --runtime-indexed-constructor-move-production-readiness tests/fixtures/choice_constructor_multi_variant_computed_index_member_path_move_rejected.or
```

This report confirms the fixed-array runtime-index constructor-move default path: cleanup proof, cleanup emission,
constructor-move acceptance, and ordinary LLVM emission are ready for the checked fixed-array fixture. DynamicArray
runtime-index owners remain rejected on the ordinary path with a static-length-owner diagnostic until runtime-index
cleanup CFG insertion is promoted.

The gated executable smoke path uses `tests/fixtures/choice_constructor_multi_variant_computed_index_member_path_move_run.or`
with `--test-only-runtime-indexed-constructor-move-run`. That command is a compiler test seam, not user-facing
language syntax; it validates constructor-move acceptance without enabling the pseudo module-mutation artifact.

## Repository expectations

The repository should evolve toward a layout similar to:

- `compiler/`
- `examples/`
- `runtime/`
- `tools/`
- `tests/`
- `docs/adr/`

The exact layout may change during early implementation.

## Development standards

- Update the spec and syntax tour when language surface syntax changes.
- Add or update tests for every meaningful change.
- Record material design decisions in ADRs.

For a quick compiler pipeline check, run the canonical demo smoke test:

```sh
ctest --test-dir build -j 16 --output-on-failure -L canonical_pipeline
```

This covers the canonical backend demos across LLVM IR emission, object emission, `run`, and retained `--build`
paths. The focused `orison_minimal_demo_smoke` target owns the smallest `examples/minimal.or` workflow.

## Status

This repository currently captures the initial language design and development conventions needed to begin implementation.

## Implementation Gap Analysis - 2026-08-05

- `orisonc` has a working C++23/LLVM pipeline for the current tested subset: parse, semantic analysis, LLVM IR
  emission, object emission, host linking, `run`, examples, and smoke/regression coverage.
- Core syntax coverage is partial. Packages, records, choices, functions, generics, casts, calls, control flow,
  unsafe blocks, loops, `defer`, FFI declarations, arrays, and `DynamicArray<T>` are covered through fixtures, but not
  yet as a complete spec-conformance matrix.
- Ownership and cleanup lowering are the strongest active area. Record/choice constructor moves, reassignment cleanup,
  owned-element `DynamicArray<T>` cleanup, and fixed literal indexed partial moves are covered; runtime-index partial
  ownership and generic descriptor-backed choice payload cleanup remain open.
- Type-system enforcement is still incomplete. Full interface/`implements` checking, access-control enforcement,
  borrow/exclusivity validation, and concurrency safety rules need dedicated semantic passes and negative tests.
- Backend support can emit and link native programs for the supported subset, but full ABI coverage, dynamic C binding
  IR generation, portable target validation, and complete runtime/standard-library integration remain unfinished.
- Tooling beyond `orisonc` is mostly future work. `orisonfmt`, `orisonls`, package/workspace tooling, and richer
  diagnostics should reuse the shared frontend and semantic model as they come online.
- Next highest-value implementation work is to finish partial-ownership proof coverage: reject reuse of moved literal
  indexed elements, prove sibling element access remains valid, then extend the model toward computed indexes.

## Implementation Gap Analysis - 2026-08-12

- Nested generic fixed-array projection over `DynamicArray<Outer<T>>` is now covered across direct arguments, helper
  call results, inferred locals, and same-source-type ternary helper results.
- Matching negative coverage now pins missing Drop proof and mismatched ternary source-type boundaries for the same
  projection paths.
- The next DynamicArray lowering gap is outside this source-type matrix: broaden owned-element cleanup coverage toward
  computed-index and partial-ownership paths without weakening Drop-proof gates.
