# Lowering Prep

## Status

Started: the lowering library scaffold now emits text LLVM IR for the initial constant-returning `UInt32`
function slice, leading immutable `let` bindings, simple fixed-width integer arithmetic, same-module function calls,
fixed-width integer comparisons returning `Bool`, fixed-width integer function parameters, and diagnostics for
unsupported shapes.

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

The first scaffold covers that slice through `orison_lowering`: it consumes a parsed `ModuleSyntax` plus a
successful semantic result, emits `define i32 @main() { ... ret i32 1 }` for `1 as UInt32`, and keeps unsupported
function shapes or return expressions diagnostic-only rather than silently generating partial IR.

The next slice adds a narrow symbol-to-IR value map for leading immutable bindings, so:

```text
function main() -> UInt32
    let value = 1 as UInt32
    value
```

emits `%value = add i32 0, 1` followed by `ret i32 %value`.

Simple binary `+`, `-`, `*`, and `/` now lower through that same value map, emitting temporaries such as
`%tmp0 = add i32 %left, %right`, `sub`, `mul`, unsigned `udiv`, and signed `sdiv` before the final return.

Integer comparisons now lower to `Bool`/`i1` via LLVM `icmp`. Lowered values preserve fixed-width integer
signedness, so `UInt*` ordering uses `ult`/`ule`/`ugt`/`uge`, `Int*` ordering uses `slt`/`sle`/`sgt`/`sge`,
and equality uses `eq`/`ne`.

Zero-argument same-module calls now use a precomputed function signature map and emit temporaries such as
`%tmp0 = call i32 @one()`, including when the call result is used as a `+` operand.

Fixed-width integer parameters now lower into function signatures and the local value map, so
`function increment(value: UInt32) -> UInt32` emits `define i32 @increment(i32 %value)` and calls emit
typed operands such as `call i32 @increment(i32 41)`. Multi-parameter calls are covered by the same vector path,
for example `define i32 @add(i32 %left, i32 %right)` and `call i32 @add(i32 40, i32 2)`.

## Required semantic facts

The current semantic pass primarily validates and diagnoses. First lowering should either consume or introduce a compact checked representation with:

- each function's declared return type
- the final value-producing statement selected for non-`Unit` functions
- inferred expression type names for literals, casts, names, calls, constructors, and low-level intrinsics used by lowering
- symbol resolution for local bindings and parameters
- a clear unsupported-feature boundary for aggregates, generics, choices, records, methods, concurrency, and unsafe operations

## Suggested implementation order

1. Extend constant integer lowering across the fixed-width integer types already mapped by the scaffold.
2. Add boolean literals and boolean operators after `Bool` return lowering is stable.
3. Add explicit CLI lowering output after the library emitter has enough useful expression coverage.
4. Introduce a checked lowering representation once expression type facts need to survive beyond the current narrow AST walk.
