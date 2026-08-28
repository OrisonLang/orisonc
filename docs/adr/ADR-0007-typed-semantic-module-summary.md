# ADR-0007: Typed Semantic Module Summary

## Status
Accepted

## Context

Lowering currently re-derives many module-level facts from syntax and small semantic side tables. That makes each new
lowering path carry its own discovery logic for functions, methods, records, choices, and foreign declarations.

## Decision

Semantic analysis publishes an internal C++ `SemanticModuleSummary` as part of `SemanticAnalysisResult`. The summary
records checked function signatures, method owners, foreign import/export markers, record fields, choice variants,
visited expression facts, ownership facts, drop obligations, and aggregate paths. Expression facts include inferred type
names and callable targets when the analyzer can resolve them. Ownership facts classify declared owners and mark whether
they need cleanup. Drop obligations mirror planned cleanup calls. Aggregate paths describe checked member/index
projections rooted at named owners. DynamicArray descriptor facts include descriptor owner, descriptor source type,
element source type, origin kind, and source line. This is not public Orison syntax and does not alter the grammar.

## Consequences

Lowering can migrate from repeated syntax walking to shared checked facts incrementally. The first slices cover
module-level declarations, visited expression facts, declared-owner facts, planned drop obligations, and checked
aggregate paths. Semantic drop authorization and resolution reports now consume the drop-obligation summary rather than
the compatibility planned-drop vector. DynamicArray descriptor cleanup planning now consumes semantic summary descriptor
facts rather than the compatibility descriptor-origin vector. DynamicArray lifetime planning, lowering cleanup matching,
pipeline readiness reports, and CLI descriptor-origin reports now use the same summary-backed projection. Direct
lowering fixtures now construct semantic summary descriptor facts.

## Follow-up work

- Retire the compatibility descriptor-origin vector from `SemanticAnalysisResult` after downstream tests stop asserting
  it as mirrored output metadata.
