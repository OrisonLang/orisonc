# Orison

Orison is a readable systems programming language with explicit ownership, explicit unsafe boundaries, modern tooling, and a surface syntax designed to welcome new systems engineers.

## Project goals

- Make systems programming more approachable without hiding core machine concepts.
- Keep memory, ownership, allocation, access control, concurrency, and unsafe operations explicit.
- Provide a strong statically typed language with practical low-level control.
- Ship a serious toolchain with compiler, formatter, and language server support.
- Preserve a path toward zero-cost abstractions through representation and compiler design, not surface complexity.

## Current design direction

Orison currently aims for:

- static, explicit, mostly nominal typing
- ownership-aware access modes: owned, `shared`, `exclusive`, raw pointers
- algebraic data types via `record` and `choice`
- record construction with explicit field-order constructor calls
- generics and constrained polymorphism
- interface-based abstraction via `interface` and `implements`
- three-level access control: `public`, `package`, `private`
- indentation-based primary syntax
- word-based boolean operators: `and`, `or`, `not`
- named bitwise operators: `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`
- ternary conditional expressions with `?:`
- null-safe member access with `?.`
- explicit `unsafe` boundaries
- first-class raw addresses and MMIO support
- package imports and explicit FFI declarations
- OS threads, runtime tasks, and async functions integrated with the safety model

## Core documents

- `ORISON_SPEC.md` - language feature and keyword reference
- `ORISON_TOUR.md` - end-to-end syntax tour
- `docs/adr/` - architecture decision records

## Tooling plan

The intended tool suite is:

- `orisonc` - compiler driver
- `orisonls` - language server
- `orisonfmt` - formatter

Each tool should be distributed as a monolithic statically linked executable. Internally, the implementation should remain modular and library-oriented.

## Minimal compiler demo

The smallest checked-in program is `examples/minimal.or`:

```text
package demo.minimal

function main() -> UInt32
    0 as UInt32
```

Configure, build, compile, link, and run it with:

```sh
cmake -S . -B build
cmake --build build
build/tools/orisonc/orisonc run examples/minimal.or
```

The demo exits successfully with status `0`. Use `--build examples/minimal.or -o build/minimal` to retain the
executable. The same file can be inspected with `--emit-llvm` or compiled with `--emit-object`.

Focused examples for the language-tour sections live under `examples/tour_*.or`. See `examples/README.md` for
the feature and validation matrix, including the runnable C `printf` hello-world example.

## Repository expectations

The repository should evolve toward a layout similar to:

- `compiler/`
- `examples/`
- `runtime/`
- `tools/`
- `tests/`
- `docs/adr/`

The exact layout may change during early implementation.

## Development standards

- Update the spec and syntax tour when language surface syntax changes.
- Add or update tests for every meaningful change.
- Record material design decisions in ADRs.

For a quick compiler pipeline check, run the canonical demo smoke test:

```sh
ctest --test-dir build --output-on-failure -L canonical_pipeline
```

This covers `examples/minimal.or`, `examples/local_record_field_assignment.or`, and
`examples/pointer_record_field_assignment.or` across LLVM IR emission, object emission, `run`, and retained `--build`
paths.

## Status

This repository currently captures the initial language design and development conventions needed to begin implementation.
