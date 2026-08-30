# Implementation Gap Analysis

Date: 2026-08-29

This note is an implementation snapshot. It does not define language syntax or semantics.

## Current Position

- `orisonc` has a native C++ pipeline for source loading, parsing, semantic checks, LLVM IR emission, object emission,
  host linking, and direct `run`.
- The examples suite marks the tour slices as backend-validated, including FFI, aggregate access, choices, pointers,
  views, dynamic arrays, unsafe operations, and scalar concurrency demos.
- `DynamicArray<T>` has strong coverage for descriptor ABI, append/grow, checked indexing, iteration, owned-element
  Drop cleanup, returned aggregate fields, nested fields, branch joins, switch joins, and final-control-flow cleanup
  composition.
- Runtime-index member cleanup has extensive proof, reporting, mutation-gate, and smoke coverage, including selected
  production-path promotion. Ordinary production `--emit-llvm` smoke coverage now pins generated member-cleanup helpers,
  sibling cleanup calls, descriptor deallocation, and clean diagnostics for source-backed nested-member and two-owner
  fixtures. Ordinary production object/link/run, `--emit-object`, and `--build` coverage now also covers source-backed
  nested-member, two-owner, and two nested-owner member-cleanup fixtures.
- Name hygiene has a reusable lowering symbol registry for source functions, foreign declarations, generated helpers,
  runtime prelude declarations, Drop symbols, and record type identifiers.

## Remaining Gaps

- Lowering is broad but still fixture-driven; unsupported diagnostics remain the safe boundary for unproven source
  shapes.
- The semantic representation now exposes checked module-level facts, visited expression types, callable targets,
  ownership facts, drop obligations, aggregate paths, and DynamicArray descriptor facts. Semantic planned-drop reports,
  drop authorization reports, DynamicArray descriptor cleanup, lifetime planning, readiness reporting, and CLI
  descriptor reports consume summary-backed facts. `SemanticAnalysisResult` no longer exposes compatibility
  planned-drop or descriptor-origin vectors. The descriptor projection helper has been removed. Internal DynamicArray
  descriptor lifetime/readiness state now uses summary binding terminology.
- DynamicArray production readiness is strongest for proven local, parameter, returned, branch, switch, and aggregate
  field paths; shared production defaults now cover construction, index, append, cleanup, computed `for`, and
  runtime-index member cleanup. Production defaults now explicitly enable descriptor cleanup planning. Source Drop now
  joins shared production defaults for audited aggregate cleanup paths, including runtime-index constructor move
  shape-fault coverage, module-rewrite audit paths, and staged member-cleanup gate checks. Abstract generic descriptors
  remain readiness metadata until concrete instantiation proves element layout and cleanup. Concrete generic
  owned-parameter cleanup can seed bound cleanup from generated and direct source Drop definitions after function
  emission resolves abstract descriptor parameter types from specialization suffixes; cleanup planning accepts the same
  concrete Drop-symbol proof when no concrete parameter summary exists. Returned descriptor lifetime plans now carry
  exact cleanup-owner proof, so return cleanup release consumes typed provenance directly. Computed ownership planning
  now unwraps source-proven descriptor-forwarding helper calls for returned descriptor iteration, and returned
  descriptors moved through local alias chains are covered by the same computed final-use cleanup path. Returned
  choices carrying DynamicArray payloads now bind caller-side switch payloads into that same cleanup path and reject
  later payload reuse after final-use cleanup. The returned/computed cleanup matrix is now pinned for direct returns,
  branch and switch joins, aggregate and nested aggregate fields, source-proven helper forwarding, local alias chains,
  returned choice payloads, missing-Drop boundaries, owner mismatch boundaries, and post-cleanup reuse diagnostics.
  Multi-candidate runtime-index cleanup fixtures now share the same production-default audit/module-rewrite helper.
  Single-candidate module-mutation and module-rewrite checks now use named option helpers. Runtime-index emission,
  insertion, mutation, Drop-surface, source-drop audit-only, and rewrite-execution-only staged checks now use named
  option helpers. Production `--emit-llvm` coverage now directly asserts the promoted member-cleanup IR shape for
  source-backed nested-member and two-owner fixtures, and production object/link/run, `--emit-object`, and `--build`
  coverage now covers the source-backed nested-member, two-owner, and two nested-owner fixtures. The remaining
  diagnostic member-cleanup run assertions now check only typed summary identity and helper metadata, and internal
  helper names use summary wording. The remaining runtime-index option-literal audit found no additional helper cleanup
  that would improve staged-gate clarity.
- FFI lowering supports fixed explicit parameters and selected library links; general C binding discovery and dynamic
  ABI generation are still future work.
- Host linking is functional for the current POSIX path; cross-target, cross-platform, and configurable toolchain
  selection remain open.
- Diagnostics are useful. Runtime-index splice-conflict blockers now include the conflicting source lines,
  production-readiness blocker reports now include the primary source line text, and member-cleanup mutation-stage
  audit/readiness reports now include source-line plus source-text metadata. Other lowering and cleanup failures still
  need richer source-span correlation.

## Suggested Next Step

- Reduce dependence on `--test-only-runtime-indexed-member-cleanup-run` by keeping execution-summary checks there and
  keeping production behavior checks on ordinary `run`, `--emit-llvm`, `--emit-object`, `--build`, object, and
  host-link paths. Continue shrinking that diagnostic seam to compact typed summary coverage.
