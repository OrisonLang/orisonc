# Lowering Prep

## Status

Proposed next phase after aggregate-context frontend parity.

## Aggregate-context closure

The aggregate expected-context smoke matrix is now broad enough to stop expanding it by default. Current semantic and CLI coverage pins expected-type propagation through:

- annotated bindings and ordinary aggregate literals
- assignments
- function and receiver-method arguments
- explicit returns and implicit final expressions
- final `if ... else` branch containers
- direct nested arrays, choice-payload nested arrays, and holder-wrapped record fields
- generic `Maybe<T>` record fields, generic `Pointer<T>` record fields, and generic choice-constructor ternary arms

Additional aggregate smoke tests should now be added only for regressions or newly introduced frontend constructs.

## First LLVM IR emission target

The smallest useful lowering milestone should avoid records, choices, generics, arrays, ownership, and unsafe operations at first. A practical first target is:

```text
package demo.lowering

function main() -> UInt32
    1 as UInt32
```

That milestone needs only:

- module-level function discovery from the parsed `ModuleSyntax`
- function signature lowering for `Unit`, `Bool`, and fixed-width integer return types
- integer literal and same-width cast expression lowering
- explicit `return` and implicit final-expression return lowering
- a diagnostic or unsupported-node path for declarations and expressions not included in the milestone

## Required semantic facts

The current semantic pass primarily validates and diagnoses. First lowering should either consume or introduce a compact checked representation with:

- each function's declared return type
- the final value-producing statement selected for non-`Unit` functions
- inferred expression type names for literals, casts, names, calls, constructors, and low-level intrinsics used by lowering
- symbol resolution for local bindings and parameters
- a clear unsupported-feature boundary for aggregates, generics, choices, records, methods, concurrency, and unsafe operations

## Suggested implementation order

1. Add an internal lowering library target, separate from parser and semantics.
2. Define a minimal IR-emission API that accepts a parsed module plus successful semantic result.
3. Emit a single function returning a constant integer.
4. Add golden/snapshot tests for the emitted IR text.
5. Extend from constants to local immutable bindings and simple arithmetic only after the first IR path is stable.
