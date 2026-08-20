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

This prints semantic descriptor origins, descriptor cleanup plans, descriptor lifetime plans, cleanup obligations,
sequence plans, verification, emission gate, capability proof, and production-readiness status in order. Ordinary
`--emit-llvm` keeps computed-index constructor moves rejected; this audit command explicitly enables the runtime-index
constructor-move, cleanup-emission, module-insertion, and module-mutation gates for inspection.

To inspect the unified descriptor origin and cleanup responsibility model, run:

```sh
build/tools/orisonc/orisonc --dynamic-array-descriptor-lifetime-plan tests/fixtures/dynamic_array_owned_parameter_forwarding_run.or
```

This report classifies each tracked descriptor as `origin local`, `origin parameter`, or `origin returned`, then pairs it
with the cleanup responsibility lowering should honor.

To inspect the default production gate without enabling constructor-move acceptance, run:

```sh
build/tools/orisonc/orisonc --runtime-indexed-constructor-move-production-readiness tests/fixtures/choice_constructor_multi_variant_computed_index_member_path_move_rejected.or
```

This report confirms the default runtime-index constructor-move path: cleanup proof, cleanup emission,
constructor-move acceptance, and ordinary LLVM emission are ready for the checked fixed-array and source-backed
DynamicArray fixtures. DynamicArray runtime-index owners use verified function-level cleanup CFG insertion with
skip-aware element drops and descriptor deallocation.

Ordinary `--emit-llvm` also accepts the checked source-backed `DynamicArray<T>` runtime-index member-transfer fixtures.
Member-granular cleanup inserts a verified cleanup walk that skips the moved computed index, drops the remaining whole
elements, drops live sibling members at the moved index through a finite helper, and deallocates the descriptor.

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

## Runtime-Index Member Cleanup Gap Analysis - 2026-08-13

- Report-only coverage now proves member scope, derives sibling cleanup targets, names insertion blocks, validates the
  CFG topology, renders an audit CFG slice, gates module mutation, and reports production blockers.
- Production emission is still blocked on real member Drop metadata binding, executable CFG insertion, module mutation
  authorization, and an enabled production member-cleanup gate.
- The next implementation step is to turn the report-only CFG slice into a verified function rewrite candidate that can
  splice member cleanup blocks without changing ordinary constructor-move acceptance.
- The promotion boundary should stay narrow: keep whole-element runtime-index moves working, keep member-path moves
  rejected on ordinary `--emit-llvm`, and expose any new readiness through audit or test-only seams first.

## Runtime-Index Member Cleanup Update - 2026-08-18

- Ordinary driver defaults now enable member-cleanup rewrite execution after typed proof, helper binding, mutation
  authorization, rewrite authorization, execution planning, and module IR-shape checks all report ready.
- Checked source-backed `DynamicArray<T>` computed-index member-transfer fixtures now compile, link, and run through
  the default driver path.
- Multi-owner member cleanup rewrites now select owner-specific lowered index operands, preventing one cleanup walk from
  reusing another owner/index temporary.

## DynamicArray Lowering Gap Analysis - 2026-08-18

- Strongest completed area: local source-backed `DynamicArray<T>` construction, append, indexing, owned-element cleanup,
  runtime-index whole-element cleanup, and checked runtime-index member cleanup now reach LLVM IR, object emission,
  host linking, and `run` for covered fixtures.
- Safety gates now reject missing direct sibling `Drop` helpers for member cleanup, malformed cleanup IR shape, reuse of
  moved elements, owner mismatches, and missing owned-element cleanup proofs.
- Remaining lowering gaps: owned `DynamicArray<T>` parameters and returns still need a unified ABI/lifetime model before
  broad production lowering, especially across function calls, branch joins, and forwarding paths.
- Remaining semantic gaps: borrow/exclusivity checks, interface constraint enforcement, access control, and concurrency
  transfer/share rules need dedicated passes rather than fixture-local lowering checks.
- Remaining backend gaps: dynamic C binding IR generation, portable target validation, runtime/standard-library
  integration, and richer source-span diagnostics are still incomplete.
- Next highest-value step: promote one owned `DynamicArray<T>` parameter/return path from fixture-specific coverage into
  a typed ABI/lifetime model, then use that model to simplify the current cleanup lowering seams.

## DynamicArray ABI Metadata Update - 2026-08-18

- Semantic descriptor origins now distinguish `origin local`, `origin parameter`, and `origin returned`, giving lowering
  a typed boundary for local descriptors, ABI-bound owned-parameter descriptors, and direct Orison function-call result
  descriptors.
- The `--dynamic-array-descriptor-lifetime-plan` diagnostic now consumes all three origin kinds and pairs them with
  cleanup responsibility metadata.
- Bound owned-parameter cleanup now uses a typed lowering-local lifetime plan helper before entering descriptor cleanup
  emission, retiring the duplicate inline descriptor-storage mutation on that path.
- Returned descriptor cleanup transfer now uses a typed lowering-local lifetime helper before suppressing callee-local
  cleanup, making caller-owned returned cleanup explicit at the lowering boundary.
- Pipeline descriptor lifetime reporting and lowering parameter/return cleanup decisions now share the same
  semantic-origin-backed lifetime helper, so origin kind selects cleanup responsibility in one lowering API.
- Negative coverage now proves missing or mismatched semantic descriptor origins cannot silently enable owned
  `DynamicArray<T>` parameter cleanup, and the returned-transfer helper rejects mismatched returned origins.
- Specialized generic function lowering now seeds parameter source types from concrete signature metadata, so checked
  owned `DynamicArray<T>` generic fixtures keep emitting concrete element cleanup once descriptor origin and Drop
  authorization both match. Descriptor-origin matching accepts concrete specializations of generic element patterns such
  as `DynamicArray<Box<T>>`.
- The descriptor lifetime report now includes `origin-blockers` and detail lines for missing cleanup plans or cleanup
  plans that lack matching semantic descriptor origins.
- DynamicArray cleanup production readiness now consumes descriptor-origin blocker state, so broader owned
  `DynamicArray<T>` forwarding paths stay blocked from production-ready status until every semantic origin is paired
  with cleanup responsibility metadata.
- Lowering now derives descriptor lifetime plans before function emission and passes them into returned descriptor
  cleanup transfer, so returned `DynamicArray<T>` cleanup handoff can use shared plan metadata instead of recomputing
  from semantic origins alone.
- Bound owned-parameter cleanup seeding and cleanup-plan emission now prefer the same lowering-owned descriptor
  lifetime plan vector, so parameter cleanup no longer relies on semantic-origin fallback when shared lifetime metadata
  is available.
- Descriptor-lifetime matching now lives in one DynamicArray lowering utility shared by function emission, cleanup-plan
  emission, and pipeline reporting.
- Negative pipeline coverage now injects mismatched shared lifetime metadata and verifies descriptor lifetime reporting
  blocks production-readiness instead of silently recomputing a cleanup-backed plan.
- DynamicArray cleanup production-readiness reports now emit explicit descriptor lifetime metadata diagnostics that name
  the blocker reason, owner, source type, element type, origin kind, and source line when available.
- Returned owned-element `DynamicArray<T>` choice payloads now report production-ready from actual emitted function
  cleanup capabilities when the caller-owned returned descriptor cleanup path is proven, and the checked fixture emits
  an object, host-links, and runs.
- Returned owned-element `DynamicArray<T>` values can now be forwarded into an owned parameter exactly once with
  production-ready cleanup metadata. The callee parameter owns the cleanup, and the caller's moved returned local does
  not emit stale cleanup.
- Returned owned-element `DynamicArray<T>` values can now cross a two-call forwarding chain:
  returned local -> forwarding parameter -> final consuming parameter. The final consumer owns cleanup, while the
  intermediate forwarder and caller do not emit stale cleanup.
- Returned owned-element `DynamicArray<T>` values can now enter a branch-join forwarding function where both branches
  pass the descriptor to the same consuming shape. The consumer owns cleanup, and neither the branch wrapper nor caller
  emits stale cleanup.
- Returned owned-element `DynamicArray<T>` choice payloads can now flow through branch forwarding into a single
  consuming owner without cleanup in the choice-return helper, branch wrapper, or caller.
- Returned owned-element `DynamicArray<T>` fields inside returned records can now move into a single consuming owner.
  The field-level returned origin is tracked as `returned.values`, and cleanup remains with the callee parameter.
- Nested returned record fields can now carry owned-element `DynamicArray<T>` descriptors into the same final-consumer
  cleanup path. The lifetime report tracks both `inner.values` and `returned.inner.values` returned origins.
- Next highest-value step: continue reducing owned `DynamicArray<T>` parameter/return ABI seams, starting with the
  next nested aggregate-field branch-forwarding path that can be proven end-to-end.
