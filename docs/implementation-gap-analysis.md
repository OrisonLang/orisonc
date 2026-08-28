# Implementation Gap Analysis

Date: 2026-08-27

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
  production-path promotion.
- Name hygiene has a reusable lowering symbol registry for source functions, foreign declarations, generated helpers,
  runtime prelude declarations, Drop symbols, and record type identifiers.

## Remaining Gaps

- Lowering is broad but still fixture-driven; unsupported diagnostics remain the safe boundary for unproven source
  shapes.
- The semantic representation now exposes checked module-level facts, visited expression types, callable targets,
  ownership facts, drop obligations, aggregate paths, and DynamicArray descriptor facts. Semantic planned-drop reports,
  drop authorization reports, DynamicArray descriptor cleanup, lifetime planning, readiness reporting, and CLI
  descriptor reports consume summary-backed facts. Semantic descriptor report helpers use descriptor-summary
  terminology.
- DynamicArray production readiness is strongest for proven local, parameter, returned, branch, switch, and aggregate
  field paths; the next risk is broader computed-owner composition outside the audited shapes.
- FFI lowering supports fixed explicit parameters and selected library links; general C binding discovery and dynamic
  ABI generation are still future work.
- Host linking is functional for the current POSIX path; cross-target, cross-platform, and configurable toolchain
  selection remain open.
- Diagnostics are useful but still need richer source-span correlation for some lowering and cleanup failures.

## Suggested Next Step

- Migrate lowering cleanup consumers from projected compatibility records to typed summary facts where it simplifies the
  backend path.
