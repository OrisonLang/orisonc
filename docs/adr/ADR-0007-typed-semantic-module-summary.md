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
Public top-level Orison functions use deterministic LLVM symbols in the form
`__orison_fn.<package>.<declaration>`. Imported fixed calls derive the same symbol from their preserved source identity.
Non-generic implementation and extension methods use deterministic LLVM symbols in the form
`__orison_method.<package>.<receiver>.<method>`. Generic method specializations use the same package-qualified base with
specialization suffixes: `__orison_method.<package>.<receiver>.<method>__<specialization-types>`.
The host entry point `main` and programmer-declared C ABI symbols retain their required external names.

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
Lowering consumes imported function summaries as external LLVM declarations. Calls continue to use the local binding
for lookup and emit the exported declaration symbol. Generic calls infer concrete substitutions through the existing
monomorphization path and declare
`__orison_fn.<source-package>.<source-declaration>__<specialization-types>` without emitting a consumer-side definition.
Producer and consumer modules can now emit separate object images and link them in one host-link invocation. This
proves the fixed imported-call ABI through execution while keeping dependency discovery and object selection outside
the language syntax.
Lowering also consumes reconstructed imported record and choice declarations when forming external function ABI
signatures. Fixed imported functions can pass and return records and choices across separately compiled objects.
Imported generic specialization requests may carry concrete record types; the producer uses its local aggregate layout
to emit the requested specialization, while the consumer uses the matching imported layout. These paths reuse existing
aggregate lowering and do not add source syntax.
Imported type metadata retains the consumer-local binding alongside source package and declaration identity. Generic
specialization requests canonicalize aggregate aliases from the target function's package back to their source
declaration names. Consumer declarations therefore keep local LLVM layout names while their external specialization
symbols and producer requests use producer-visible type names.
Distinct packages can export the same public function name and link into one executable without a symbol collision.
Public concrete generic specializations use the same package qualification. A consumer can link an imported generic
call when the concrete signature uses supported lowered ABI types. Consumer lowering publishes typed specialization
requests containing source package, exported declaration, and ordered concrete type arguments. Producer compilation
accepts those requests and emits each matching public generic specialization once, without requiring a local source
call to seed monomorphization. Specialization requests use a separate deterministic, versioned text artifact headed by
`orison-specialization-requests 1`; they are not source syntax and are not serialized into package-interface artifacts.
Version 1 stores each lowering type together with a recursive package-qualified type identity. Third-party aggregate
types can therefore use different aliases in producer and consumer modules while selecting one specialization symbol.
The generic producer uses an explicit source import when one exists. Otherwise, lowering may materialize a requested
public aggregate under a compiler-reserved local name from an available package interface. This materialization is
limited to specialization lowering and does not make the type visible to ordinary producer source analysis. Public
record fields and choice payloads recursively materialize same-package public aggregate dependencies, with every copied
type reference rewritten to its compiler-reserved local name. Package-interface format version 1 stores a recursive
package-qualified type identity beside each public record field and choice payload. Imported aliases therefore retain
their source package and declaration while recursive materialization crosses into explicitly available third-party
interfaces. Public alias declarations retain the same identity beside their target. Specialization-only layout
materialization expands alias chains before materializing the target record or choice, and ordinary imported aliases
project the resolved aggregate layout under the consumer-local name. Alias cycles terminate without recursive
materialization. All compiler-owned artifact formats remain version 1 before the first release; their schemas may
change in place during this phase.
Lowering records local type-alias targets and resolves them cycle-safely when forming function signatures, aggregate
fields, choice payloads, casts, and expression types. Aggregate aliases project the existing record or choice layout
under the source alias while retaining the target LLVM representation. Local and imported function parameters and
returns can therefore use distinct source aliases across separately compiled package objects without changing the
external calling convention.
Semantic choice-shape queries expand local alias chains with cycle detection. This permits variant construction,
payload binding, and exhaustiveness analysis through an alias while preserving nominal alias identity for ordinary
value compatibility and explicit conversions.
Semantic index validation and element-type inference expand local and imported alias chains with cycle detection.
Lowering applies the same resolution before fixed-array and view element queries, so alias-typed indexed expressions
retain their element type through LLVM emission. Ordinary assignability remains nominal.
Semantic member lookup expands local and imported alias chains before record and null-safe `Maybe` inspection.
Imported alias targets localize nested package types to the consumer's explicit import names. Lowering applies the
same cycle-safe resolution before record-layout lookup and throughout null-safe access planning. Alias identity remains
nominal for assignment and conversion.
An explicit alias-typed constructor context may select the resolved record or choice layout. This permits an underlying
record constructor or choice variant to initialize the declared alias directly, including nested constructor payloads
and renamed imported choice aliases. Values constructed outside that context still require explicit conversion before
entering the alias type, so ordinary compatibility remains nominal.
Semantic and lowering method lookup resolve the receiver through cycle-safe local and imported alias chains. Null-safe
calls expand the outer alias, unwrap `Maybe`, then resolve the payload alias. The selected method and emitted symbol
remain owned by the declared concrete receiver; alias identity remains unchanged for assignment and conversion.
Scalar operator inference expands cycle-safe local and imported aliases before checking integer, signed, or boolean
requirements. Value-producing integer operators retain the operand alias, including unannotated intermediate bindings;
comparisons and logical operators produce `Bool`. LLVM lowering selects the resolved representation and signedness.
Control-flow condition validation expands cycle-safe aliases and requires the resolved type to be `Bool` for `if`,
`guard`, `while`, and trailing `repeat while` conditions. Scalar switch value-pattern compatibility and Boolean
exhaustiveness use resolved aliases, while diagnostics retain source alias names. Switch literal casts lower through
the resolved representation. Alias identity remains nominal outside these type-directed operations.
Unsafe low-level validation expands cycle-safe aliases when identifying `Address`, `Pointer<T>`, pointer element types,
integer offsets, and raw or volatile write compatibility. Lowering resolves aliased pointer and pointee representations
before emitting address conversion, loads, stores, and pointer offsets. A read result still needs an explicit conversion
when it enters a distinct alias type, and ordinary assignment and return compatibility remain nominal.
The driver emits request artifacts with `--emit-specialization-requests` and accepts repeatable
`--specialization-requests` inputs, allowing build orchestration to aggregate consumer requests for producer
compilation. ADR-0008 defines explicit dependency scheduling and object selection through driver build plans.
Distinct packages can define the same non-generic receiver method and link into one executable without a symbol
collision. The compiler-reserved `__orison_` prefix prevents source and foreign declarations from claiming generated
method symbols. Generated owned-cleanup symbols use
`__orison_owned_cleanup.<mangled-type>.<package>`, so same-named record types from different packages can link in one
executable.
Distinct packages can also emit the same receiver and generic method specialization without a symbol collision.
Module-free synthetic lowering fixtures retain package-neutral generic method symbols.

## Follow-up work

- Migrate the remaining report command/helper names away from descriptor-origin terminology only when the pinned CLI
  text can change intentionally.
- Use the explicit build-plan orchestration defined by ADR-0008 for dependency scheduling and object selection.
