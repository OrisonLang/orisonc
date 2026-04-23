# Orison

Orison is a readable systems programming language with explicit ownership, explicit unsafe boundaries, modern tooling, and a surface syntax designed to welcome new systems engineers.

## Project goals

- Make systems programming more approachable without hiding core machine concepts.
- Keep memory, ownership, allocation, concurrency, and unsafe operations explicit.
- Provide a strong statically typed language with practical low-level control.
- Ship a serious toolchain with compiler, formatter, and language server support.
- Preserve a path toward zero-cost abstractions through representation and compiler design, not surface complexity.

## Current design direction

Orison currently aims for:

- static, explicit, mostly nominal typing
- ownership-aware access modes: owned, `shared`, `exclusive`, raw pointers
- algebraic data types via `record` and `choice`
- generics and constrained polymorphism
- interface-based abstraction via `interface` and `implements`
- indentation-based primary syntax
- explicit `unsafe` boundaries
- first-class raw addresses and MMIO support
- package imports and explicit FFI declarations
- OS threads, runtime tasks, and async functions integrated with the safety model

## Core documents

- `ORISON_SPEC.md` — language feature and keyword reference
- `ORISON_TOUR.md` — end-to-end syntax tour
- `AGENTS.md` — repository rules for AI-augmented development
- `MEMORY.md` — persistent project context and design learnings
- `docs/adr/` — architecture decision records

## Tooling plan

The intended tool suite is:

- `orisonc` — compiler driver
- `orisonls` — language server
- `orisonfmt` — formatter

Each tool should be distributed as a monolithic statically linked executable. Internally, the implementation should remain modular and library-oriented.

## Implementation stack

The initial implementation targets:

- `C++23` for the compiler and supporting tools
- `LLVM` for lowering, optimization, and machine code generation
- `CMake` for the native build workflow

## Repository expectations

The repository is being scaffolded toward a layout similar to:

- `docs/adr/`
- `compiler/`
- `runtime/`
- `tools/`
- `tests/`

Current bootstrap layout:

- `compiler/driver/` — shared compiler-driver library code
- `compiler/source/` — source file loading and path-aware source objects
- `compiler/diagnostics/` — explicit diagnostic collection and rendering
- `compiler/syntax/` — lexer and early parser infrastructure for module headers, block structure, and signature syntax
- `tools/orisonc/` — initial compiler executable entry point
- `runtime/` — reserved for runtime support
- `tests/` — native smoke and regression tests
- `docs/adr/` — architecture decision records

The exact layout may change during early implementation.

## Development standards

- Update the spec and syntax tour when language surface syntax changes.
- Add or update tests for every meaningful change.
- Record material design decisions in ADRs.
- Record ongoing project learnings in `MEMORY.md`.

## Status

This repository currently contains the initial language design documents plus an early native `C++23` frontend scaffold that can read a source file, lex a token stream, parse indentation-based blocks, build minimal signature/type syntax, and report explicit diagnostics.
