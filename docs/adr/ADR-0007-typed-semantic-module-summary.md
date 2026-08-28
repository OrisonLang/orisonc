# ADR-0007: Typed Semantic Module Summary

## Status
Accepted

## Context

Lowering currently re-derives many module-level facts from syntax and small semantic side tables. That makes each new
lowering path carry its own discovery logic for functions, methods, records, choices, and foreign declarations.

## Decision

Semantic analysis publishes an internal C++ `SemanticModuleSummary` as part of `SemanticAnalysisResult`. The initial
summary records checked function signatures, method owners, foreign import/export markers, record fields, and choice
variants. This is not public Orison syntax and does not alter the grammar.

## Consequences

Lowering can migrate from repeated syntax walking to shared checked facts incrementally. The first slice is intentionally
module-level only; resolved expression types, callable targets, ownership states, and lowering-ready aggregate paths will
be added in later iterations.

## Follow-up work

- Add expression-level type facts.
- Add callable target resolution.
- Add ownership/drop facts suitable for direct cleanup lowering.
- Migrate selected lowering decisions to consume the summary.
