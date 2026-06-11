# ADR-0003: C FFI String Literal Lowering

## Status
Accepted

## Context

The first runnable foreign-function example passes a string literal to a C function declared with a
`Pointer<Byte>` parameter. The backend needs a representation that is linkable without a runtime or heap allocation.

## Decision

- A string literal used as a `Pointer<Byte>` argument to a `foreign "c"` call lowers to a private, immutable,
  null-terminated LLVM byte-array global.
- The call passes the global address as LLVM `ptr`.
- This conversion is limited to C foreign-call arguments. Ordinary `Pointer<Byte>` parameters still require explicit
  pointer-like expressions.
- Supported C foreign imports lower to LLVM external declarations.
- Foreign declarations expose only finite, explicitly named parameters. Calls must match the declared arity and types.
- Orison does not expose C-style variadic parameters, spread syntax, or implicit extra arguments.
- A backend may adapt a fixed Orison declaration to a target ABI entry point that is variadic, but that adaptation must
  preserve the finite source contract and apply target-specific calling convention rules.
- Explicit foreign library names are deduplicated in source order and passed to the host linker as direct `-lname`
  arguments without shell command construction.
- Host object generation uses position-independent relocation so global addresses link with default PIE toolchains.

## Consequences

- C string calls require no runtime allocation and have static storage duration.
- Embedded null bytes are representable, but C callees observe the first null as the string terminator.
- C variadic source declarations are intentionally excluded. Non-C ABIs, nonstandard library search paths, safe
  target-specific adapters for variadic C entry points, and general `Text` representation remain future work.

## Follow-up work

- Add explicit backend adapter metadata before mapping fixed Orison signatures to variadic C ABI entry points.
- Define target-aware library naming and search paths for cross-compilation.
