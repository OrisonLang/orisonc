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
- Backend adapter metadata is keyed by the external ABI symbol. The initial `printf` adapter requires an `Int32` return
  and a leading `Pointer<Byte>` parameter, emits the LLVM `ptr, ...` ABI declaration, and applies C integer promotions
  only to explicitly declared trailing parameters.
- Explicit trailing pointers and 64-bit integers retain their LLVM `ptr` and `i64` representations; only C's required
  narrow integer promotions alter an explicitly declared Orison argument type at the ABI boundary.
- Explicit trailing `Float32` arguments promote from LLVM `float` to `double`; `Float64` arguments remain `double`.
- Explicit foreign library names are deduplicated in source order and passed to the host linker as direct `-lname`
  arguments without shell command construction.
- Host object generation uses position-independent relocation so global addresses link with default PIE toolchains.

## Consequences

- C string calls require no runtime allocation and have static storage duration.
- Embedded null bytes are representable, but C callees observe the first null as the string terminator.
- C variadic source declarations are intentionally excluded. The initial adapter table is deliberately narrow;
  non-C ABIs, nonstandard library search paths, additional reviewed adapters, and general `Text` representation remain
  future work.

## Follow-up work

- Add reviewed adapter entries only when their fixed ABI prefix, promotions, and safety constraints are testable.
- Define target-aware library naming and search paths for cross-compilation.
