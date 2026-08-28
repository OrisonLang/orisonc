# ADR-0007: Typed Semantic Module Summary

## Status
Accepted

## Context

Lowering currently re-derives many module-level facts from syntax and small semantic side tables. That makes each new
lowering path carry its own discovery logic for functions, methods, records, choices, and foreign declarations.

## Decision

Semantic analysis publishes an internal C++ `SemanticModuleSummary` as part of `SemanticAnalysisResult`. The summary
records checked function signatures, method owners, foreign import/export markers, record fields, choice variants, and
visited expression facts. Expression facts include inferred type names and callable targets when the analyzer can resolve
them. This is not public Orison syntax and does not alter the grammar.

## Consequences

Lowering can migrate from repeated syntax walking to shared checked facts incrementally. The first slices cover
module-level declarations and visited expression facts; ownership states and lowering-ready aggregate paths will be added
in later iterations.

## Follow-up work

- Add ownership/drop facts suitable for direct cleanup lowering.
- Migrate selected lowering decisions to consume the summary.
