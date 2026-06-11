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
- C foreign imports without an explicit library clause lower to LLVM external declarations.
- Host object generation uses position-independent relocation so global addresses link with default PIE toolchains.

## Consequences

- C string calls require no runtime allocation and have static storage duration.
- Embedded null bytes are representable, but C callees observe the first null as the string terminator.
- Foreign library selection, variadic declarations, non-C ABIs, and general `Text` representation remain future work.

## Follow-up work

- Add an explicit variadic FFI model before supporting additional `printf` arguments.
- Define foreign library propagation into the linker.
