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
Imported function summaries preserve the local binding, source package, exported declaration name, checked parameters,
return type, generic parameters, and async/unsafe classification. Imported direct-call expression facts preserve the
same source identity for lowering.

## Consequences

Lowering can migrate from repeated syntax walking to shared checked facts incrementally. The first slices cover
module-level declarations, visited expression facts, declared-owner facts, planned drop obligations, and checked
aggregate paths. Semantic planned-drop reports, semantic drop authorization, and semantic drop resolution reports now
consume the drop-obligation summary rather than the compatibility planned-drop vector. DynamicArray descriptor cleanup
planning now consumes semantic summary descriptor facts rather than the compatibility descriptor-origin vector.
DynamicArray lifetime planning, lowering cleanup matching, pipeline readiness reports, and CLI descriptor reports now
consume semantic summary descriptor facts directly. Direct lowering fixtures now construct semantic summary descriptor facts.
`SemanticAnalysisResult` no longer exposes the mirrored compatibility planned-drop or descriptor-origin vectors.
Semantic descriptor report helpers now use descriptor-summary terminology. The DynamicArray descriptor projection helper
and runtime origin overloads have been removed. Internal DynamicArray descriptor boundary fields and blockers now use
descriptor-summary binding terminology in typed C++ state.
Lowering consumes non-generic imported function summaries as fixed external LLVM declarations. Calls continue to use
the local binding for lookup and emit the exported declaration symbol. Imported generic calls stop with a targeted
lowering diagnostic until cross-package specialization is defined.

## Follow-up work

- Migrate the remaining report command/helper names away from descriptor-origin terminology only when the pinned CLI
  text can change intentionally.
- Define cross-package generic specialization and package-qualified source symbol policy before lowering imported
  generic functions or packages that export colliding declaration names.
