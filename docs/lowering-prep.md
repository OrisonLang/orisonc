# Lowering Prep

## Status

Started: the lowering library scaffold now emits text LLVM IR for the initial constant-returning `UInt32`
function slice, leading immutable `let` bindings, simple fixed-width integer arithmetic, same-module function calls,
fixed-width integer comparisons returning `Bool`, boolean literals and operators, fixed-width integer function
parameters, ternary expressions, final value-producing `if ... else` statements, and diagnostics for unsupported
shapes. Final scalar value-pattern `switch` statements now lower through LLVM case blocks and merge values too.

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

Boolean literals lower to `i1 1`/`i1 0`; `not` emits `xor i1 ..., true`; and `and`/`or` emit LLVM `i1`
operations. Boolean operands can include nested integer comparisons and same-module calls.

Ternary expressions lower through conditional branches into labeled then/else blocks and merge with an LLVM
`phi` value. The lowering state tracks each arm's actual exit block so nested ternaries produce valid predecessor
labels in outer `phi` nodes.

Final `if ... else` statements now reuse the same branch/merge model when each arm contains one value-producing
expression or explicit value return. Arms may begin with branch-local immutable bindings; lexical bindings are
restored between arms while function-wide SSA name counters keep repeated source names unique. Arm exit-block
tracking supports both ternaries and recursively nested final `if ... else` statements inside an arm.

Final scalar `switch` statements lower through LLVM's `switch` terminator, one block per value arm, and a merge
`phi`. Integer and boolean literal patterns are supported, including same-width integer cast patterns. Case-local
immutable bindings are isolated between arms, and exhaustive switches without `default` use an `unreachable`
fallback block. Final `if` and `switch` containers can recursively appear inside either kind of branch arm.

The compiler driver now exposes this lowering slice through `orisonc --emit-llvm <file>`. Successful emission
writes textual LLVM IR to stdout; source loading, parse, semantic, and lowering failures return path-aware
diagnostics on stderr without emitting partial IR.

`orison_lowering` now parses every generated module with LLVM's assembly parser and runs LLVM module verification
before publishing `ir_text`. Parser or verifier failures become lowering diagnostics, so both library callers and
`orisonc --emit-llvm` reject malformed backend output instead of forwarding it.
The current development environment supplies LLVM as a monolithic shared library; release builds still require
static LLVM archives to satisfy the toolchain's static-distribution requirement.

The first native code-generation boundary is now available through
`orisonc --emit-object <file> -o <output>`. It reparses the verified module, selects the host LLVM target,
assigns the target triple and data layout, runs the target object-emission pipeline, and writes the resulting
native object bytes only after code generation succeeds.

Host executables can now be produced with `orisonc --build <file> -o <executable>`. The compiler keeps linking
behind a separate `orison_link` library, writes a unique temporary object, invokes the CMake-discovered Clang/C
driver with `posix_spawn` argument vectors, and removes temporary or failed output artifacts. This is an initial
POSIX host-link path, not yet the final cross-platform or cross-target linker design.

Source loading, parsing, semantic analysis, verified IR emission, and native object emission now compose through
the reusable `orison_pipeline` library. `--parse`, `--emit-llvm`, `--emit-object`, and `--build` share those stage
boundaries rather than maintaining separate copies of the compiler flow. `examples/minimal.or` is the stable
zero-exit demo input for repeatedly exercising the end-to-end path.

`orisonc run <file>` now extends that shared path through temporary host linking and direct process execution.
The numbered `examples/tour_*.or` files are frontend-validated tour slices. The C `printf` hello-world example
remains frontend-only until lowering represents static string storage, `Pointer<Byte>` FFI arguments, and imported
foreign symbols.

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
2. Introduce a checked lowering representation once expression type facts need to survive beyond the current narrow AST walk.
3. Define choice representation before lowering constructor-pattern `switch` arms.
4. Replace the provisional host-link driver with explicit target selection and a cross-platform linker strategy.
